#!/usr/bin/env python3
"""Aetheria 的確定性美術後處理管線與入庫閘。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence

from PIL import Image


DEFAULT_SIZE = (64, 64)
MAX_OUTSIDE_PALETTE_RATIO = 0.01
_DEF_ID_RE = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
_BAYER_4X4 = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)

Rgb = tuple[int, int, int]


@dataclass(frozen=True)
class ProcessResult:
    output: str
    detected_light: str
    rotation_degrees_ccw: int
    seam_before: float | None
    seam_after: float | None
    sha256: str


def parse_hex_color(value: str) -> Rgb:
    if not re.fullmatch(r"#[0-9a-fA-F]{6}", value):
        raise ValueError(f"無效色彩 {value!r}；必須是 #RRGGBB")
    return tuple(int(value[index : index + 2], 16) for index in (1, 3, 5))  # type: ignore[return-value]


def format_hex_color(color: Rgb) -> str:
    return "#{:02X}{:02X}{:02X}".format(*color)


def load_palette(path: Path) -> list[Rgb]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"無法讀取色盤 {path}: {error}") from error
    values = data.get("colors") if isinstance(data, dict) else None
    if not isinstance(values, list) or not values:
        raise ValueError(f"色盤 {path} 必須含非空的 colors 陣列")
    colors = [parse_hex_color(value) for value in values if isinstance(value, str)]
    if len(colors) != len(values):
        raise ValueError(f"色盤 {path} 的 colors 只能含 #RRGGBB 字串")
    if len(colors) != len(dict.fromkeys(colors)):
        raise ValueError(f"色盤 {path} 含重複顏色")
    return colors


def placeholder_palette_document() -> dict[str, object]:
    levels = (0, 85, 170, 255)
    colors = [format_hex_color((red, green, blue)) for red in levels for green in levels for blue in levels]
    return {
        "name": "placeholder-only-not-project-palette",
        "warning_zh_tw": "僅供管線測試；不是 Aetheria 專案色盤，禁止拿來產生正式素材。",
        "generator": "RGB 笛卡兒積；每通道依序取 0、85、170、255。",
        "colors": colors,
    }


def write_placeholder_palette(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(placeholder_palette_document(), ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    path.write_text(payload, encoding="utf-8", newline="\n")


def parse_size(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"([1-9][0-9]*)x([1-9][0-9]*)", value)
    if match is None:
        raise argparse.ArgumentTypeError("尺寸必須是 WIDTHxHEIGHT，例如 64x64")
    return int(match.group(1)), int(match.group(2))


def parse_rgb(value: str) -> Rgb:
    parts = value.split(",")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("背景色必須是 R,G,B")
    try:
        color = tuple(int(part) for part in parts)
    except ValueError as error:
        raise argparse.ArgumentTypeError("背景色必須是整數 R,G,B") from error
    if any(channel < 0 or channel > 255 for channel in color):
        raise argparse.ArgumentTypeError("背景色色頻必須介於 0 與 255")
    return color  # type: ignore[return-value]


def def_id_relative_path(def_id: str) -> Path:
    if _DEF_ID_RE.fullmatch(def_id) is None:
        raise ValueError(f"無效 def id {def_id!r}")
    return Path(*def_id.split(".")).with_suffix(".png")


def _remove_background(image: Image.Image, background: Rgb | None, tolerance: int) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels: list[tuple[int, int, int, int]] = []
    limit_squared = tolerance * tolerance
    for red, green, blue, alpha in rgba.get_flattened_data():
        if background is not None:
            distance_squared = sum((channel - target) ** 2 for channel, target in zip((red, green, blue), background))
            if distance_squared <= limit_squared:
                alpha = 0
        clean_alpha = 255 if alpha >= 128 else 0
        pixels.append((red, green, blue, clean_alpha) if clean_alpha else (0, 0, 0, 0))
    rgba.putdata(pixels)
    return rgba


def crop_align(
    image: Image.Image,
    size: tuple[int, int],
    kind: str,
    background: Rgb | None,
    background_tolerance: int,
) -> Image.Image:
    rgba = _remove_background(image, background, background_tolerance)
    alpha = rgba.getchannel("A")
    box = alpha.getbbox()
    if box is None:
        raise ValueError("去背後沒有任何可見像素")
    cropped = rgba.crop(box)
    if kind == "terrain":
        return cropped.resize(size, Image.Resampling.NEAREST)
    scale = min(size[0] / cropped.width, size[1] / cropped.height, 1.0)
    resized_size = (max(1, int(cropped.width * scale)), max(1, int(cropped.height * scale)))
    resized = cropped.resize(resized_size, Image.Resampling.NEAREST)
    aligned = Image.new("RGBA", size, (0, 0, 0, 0))
    offset = ((size[0] - resized.width) // 2, (size[1] - resized.height) // 2)
    aligned.alpha_composite(resized, offset)
    return aligned


def _nearest_palette_color(color: Rgb, palette: Sequence[Rgb]) -> Rgb:
    return min(
        palette,
        key=lambda candidate: (
            (color[0] - candidate[0]) ** 2
            + (color[1] - candidate[1]) ** 2
            + (color[2] - candidate[2]) ** 2
        ),
    )


def quantize_ordered(image: Image.Image, palette: Sequence[Rgb], strength: int = 16) -> Image.Image:
    if not palette:
        raise ValueError("色盤不可為空")
    if strength < 0 or strength > 64:
        raise ValueError("抖動強度必須介於 0 與 64")
    source = image.convert("RGBA")
    output = Image.new("RGBA", source.size)
    result: list[tuple[int, int, int, int]] = []
    for y in range(source.height):
        for x in range(source.width):
            red, green, blue, alpha = source.getpixel((x, y))
            if alpha == 0:
                result.append((0, 0, 0, 0))
                continue
            matrix_value = _BAYER_4X4[y % 4][x % 4]
            offset = ((matrix_value * 2 - 15) * strength) // 16
            adjusted = tuple(max(0, min(255, channel + offset)) for channel in (red, green, blue))
            result.append((*_nearest_palette_color(adjusted, palette), alpha))
    output.putdata(result)
    return output


def _luminance(pixel: tuple[int, int, int, int]) -> int:
    return 54 * pixel[0] + 183 * pixel[1] + 19 * pixel[2]


def detect_light_direction(image: Image.Image) -> str:
    rgba = image.convert("RGBA")
    gradient_x = 0
    gradient_y = 0
    samples = 0
    for y in range(1, rgba.height - 1):
        for x in range(1, rgba.width - 1):
            left = rgba.getpixel((x - 1, y))
            right = rgba.getpixel((x + 1, y))
            top = rgba.getpixel((x, y - 1))
            bottom = rgba.getpixel((x, y + 1))
            if min(left[3], right[3], top[3], bottom[3]) == 0:
                continue
            gradient_x += _luminance(right) - _luminance(left)
            gradient_y += _luminance(bottom) - _luminance(top)
            samples += 1
    if samples == 0 or (gradient_x == 0 and gradient_y == 0):
        return "undetermined"
    horizontal = "right" if gradient_x > 0 else "left"
    vertical = "bottom" if gradient_y > 0 else "top"
    return f"{vertical}-{horizontal}"


def normalize_lighting(image: Image.Image) -> tuple[Image.Image, str, int]:
    direction = detect_light_direction(image)
    rotations = {
        "top-left": (None, 0),
        "top-right": (Image.Transpose.ROTATE_90, 90),
        "bottom-right": (Image.Transpose.ROTATE_180, 180),
        "bottom-left": (Image.Transpose.ROTATE_270, 270),
        "undetermined": (None, 0),
    }
    transpose, degrees = rotations[direction]
    normalized = image.copy() if transpose is None else image.transpose(transpose)
    if normalized.size != image.size:
        scale = min(image.width / normalized.width, image.height / normalized.height)
        resized = normalized.resize(
            (max(1, int(normalized.width * scale)), max(1, int(normalized.height * scale))),
            Image.Resampling.NEAREST,
        )
        canvas = Image.new("RGBA", image.size, (0, 0, 0, 0))
        canvas.alpha_composite(
            resized,
            ((image.width - resized.width) // 2, (image.height - resized.height) // 2),
        )
        normalized = canvas
    return normalized, direction, degrees


def _opaque_boundary(mask: Sequence[bool], width: int, height: int) -> list[bool]:
    boundary = [False] * (width * height)
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if not mask[index]:
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if nx < 0 or nx >= width or ny < 0 or ny >= height or not mask[ny * width + nx]:
                    boundary[index] = True
                    break
    return boundary


def normalize_outline(image: Image.Image, kind: str, outline_color: Rgb, line_width: int = 1) -> Image.Image:
    if line_width < 1:
        raise ValueError("輪廓線寬必須至少為 1")
    rgba = image.convert("RGBA")
    pixels = list(rgba.get_flattened_data())
    mask = [pixel[3] > 0 for pixel in pixels]
    layers: list[list[bool]] = []
    remaining = mask.copy()
    for _ in range(line_width):
        layer = _opaque_boundary(remaining, rgba.width, rgba.height)
        layers.append(layer)
        remaining = [visible and not edge for visible, edge in zip(remaining, layer)]
    boundary = [any(layer[index] for layer in layers) for index in range(len(mask))]
    output = pixels.copy()
    if kind == "object":
        for index, is_boundary in enumerate(boundary):
            if is_boundary:
                output[index] = (*outline_color, 255)
    else:
        interior_indices = [index for index, visible in enumerate(remaining) if visible]
        for index, is_boundary in enumerate(boundary):
            if not is_boundary or not interior_indices:
                continue
            x, y = index % rgba.width, index // rgba.width
            nearest = min(
                interior_indices,
                key=lambda candidate: (
                    abs(x - candidate % rgba.width) + abs(y - candidate // rgba.width),
                    candidate,
                ),
            )
            output[index] = (*pixels[nearest][:3], pixels[index][3])
    rgba.putdata(output)
    return rgba


def seam_metric(image: Image.Image) -> float:
    rgba = image.convert("RGBA")
    difference = 0
    comparisons = 0
    for y in range(rgba.height):
        left = rgba.getpixel((0, y))
        right = rgba.getpixel((rgba.width - 1, y))
        difference += sum(abs(a - b) for a, b in zip(left, right))
        comparisons += 4
    for x in range(rgba.width):
        top = rgba.getpixel((x, 0))
        bottom = rgba.getpixel((x, rgba.height - 1))
        difference += sum(abs(a - b) for a, b in zip(top, bottom))
        comparisons += 4
    return difference / comparisons if comparisons else 0.0


def repair_seams(image: Image.Image, palette: Sequence[Rgb], seam_width: int = 1) -> Image.Image:
    if seam_width < 1 or seam_width * 2 > min(image.size):
        raise ValueError("接縫修補寬度超出影像範圍")
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    for offset in range(seam_width):
        opposite_x = rgba.width - 1 - offset
        for y in range(rgba.height):
            left = pixels[offset, y]
            right = pixels[opposite_x, y]
            average = tuple((left[channel] + right[channel]) // 2 for channel in range(3))
            color = _nearest_palette_color(average, palette)
            alpha = 0 if left[3] == 0 and right[3] == 0 else 255
            repaired = (0, 0, 0, 0) if alpha == 0 else (*color, alpha)
            pixels[offset, y] = repaired
            pixels[opposite_x, y] = repaired
        opposite_y = rgba.height - 1 - offset
        for x in range(rgba.width):
            top = pixels[x, offset]
            bottom = pixels[x, opposite_y]
            average = tuple((top[channel] + bottom[channel]) // 2 for channel in range(3))
            color = _nearest_palette_color(average, palette)
            alpha = 0 if top[3] == 0 and bottom[3] == 0 else 255
            repaired = (0, 0, 0, 0) if alpha == 0 else (*color, alpha)
            pixels[x, offset] = repaired
            pixels[x, opposite_y] = repaired
    return rgba


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def save_png_deterministically(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG", optimize=False, compress_level=9)


def process_asset(
    input_path: Path,
    asset_root: Path,
    def_id: str,
    kind: str,
    palette: Sequence[Rgb],
    size: tuple[int, int] = DEFAULT_SIZE,
    background: Rgb | None = None,
    background_tolerance: int = 0,
    dither_strength: int = 16,
    outline_width: int = 1,
    seam_width: int = 1,
) -> ProcessResult:
    with Image.open(input_path) as source:
        image = crop_align(source, size, kind, background, background_tolerance)
    image = quantize_ordered(image, palette, dither_strength)
    image, detected_light, rotation = normalize_lighting(image)
    image = normalize_outline(image, kind, palette[0], outline_width)
    before = seam_metric(image) if kind == "terrain" else None
    if kind == "terrain":
        image = repair_seams(image, palette, seam_width)
    after = seam_metric(image) if kind == "terrain" else None
    output_path = asset_root / def_id_relative_path(def_id)
    save_png_deterministically(image, output_path)
    return ProcessResult(
        output=output_path.as_posix(),
        detected_light=detected_light,
        rotation_degrees_ccw=rotation,
        seam_before=before,
        seam_after=after,
        sha256=sha256_file(output_path),
    )


def palette_outside_count(image: Image.Image, palette: Sequence[Rgb]) -> tuple[int, int]:
    allowed = frozenset(palette)
    visible = [pixel for pixel in image.convert("RGBA").get_flattened_data() if pixel[3] > 0]
    outside = sum(pixel[:3] not in allowed for pixel in visible)
    return outside, len(visible)


def alpha_edge_issue_counts(image: Image.Image) -> tuple[int, int]:
    pixels = image.convert("RGBA").get_flattened_data()
    semitransparent = sum(0 < pixel[3] < 255 for pixel in pixels)
    polluted_transparent = sum(pixel[3] == 0 and pixel[:3] != (0, 0, 0) for pixel in pixels)
    return semitransparent, polluted_transparent


def _load_manifest(path: Path) -> list[dict[str, object]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"無法讀取 manifest {path}: {error}") from error
    assets = data.get("assets") if isinstance(data, dict) else None
    if not isinstance(assets, list):
        raise ValueError("manifest 必須含 assets 陣列")
    validated: list[dict[str, object]] = []
    for index, entry in enumerate(assets):
        if not isinstance(entry, dict):
            raise ValueError(f"manifest assets[{index}] 必須是物件")
        def_id = entry.get("def_id")
        size = entry.get("size", [64, 64])
        if not isinstance(def_id, str):
            raise ValueError(f"manifest assets[{index}].def_id 必須是字串")
        def_id_relative_path(def_id)
        if (
            not isinstance(size, list)
            or len(size) != 2
            or not all(isinstance(value, int) and value > 0 for value in size)
        ):
            raise ValueError(f"manifest assets[{index}].size 必須是兩個正整數")
        validated.append({"def_id": def_id, "size": (size[0], size[1])})
    return validated


def run_gate(asset_root: Path, manifest_path: Path, palette: Sequence[Rgb]) -> list[str]:
    entries = _load_manifest(manifest_path)
    expected = {def_id_relative_path(str(entry["def_id"])).as_posix(): entry for entry in entries}
    actual_paths = sorted(path for path in asset_root.rglob("*.png") if path.is_file())
    actual = {path.relative_to(asset_root).as_posix(): path for path in actual_paths}
    violations: list[str] = []
    for relative_path in sorted(actual.keys() - expected.keys()):
        violations.append(f"{relative_path}: 命名不符 def：檔案未列於 manifest")
    for relative_path in sorted(expected.keys() - actual.keys()):
        def_id = expected[relative_path]["def_id"]
        violations.append(f"{def_id}: 命名不符 def：預期 {relative_path} 不存在")
    for relative_path in sorted(expected.keys() & actual.keys()):
        path = actual[relative_path]
        entry = expected[relative_path]
        with Image.open(path) as opened:
            image = opened.convert("RGBA")
        expected_size = entry["size"]
        if image.size != expected_size:
            violations.append(
                f"{relative_path}: 尺寸不符：實際 {image.width}x{image.height}，"
                f"預期 {expected_size[0]}x{expected_size[1]}"
            )
        outside, visible = palette_outside_count(image, palette)
        ratio = outside / visible if visible else 0.0
        if ratio > MAX_OUTSIDE_PALETTE_RATIO:
            violations.append(
                f"{relative_path}: 色盤外顏色 {outside}/{visible} ({ratio:.3%})，"
                f"門檻 {MAX_OUTSIDE_PALETTE_RATIO:.3%}"
            )
        semitransparent, polluted_transparent = alpha_edge_issue_counts(image)
        if semitransparent or polluted_transparent:
            violations.append(
                f"{relative_path}: alpha 邊緣不乾淨：半透明像素 {semitransparent}，"
                f"透明 RGB 污染像素 {polluted_transparent}"
            )
    return violations


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    process = subparsers.add_parser("process", help="依六步管線處理一張素材")
    process.add_argument("--input", type=Path, required=True)
    process.add_argument("--asset-root", type=Path, required=True)
    process.add_argument("--def-id", required=True)
    process.add_argument("--kind", choices=("terrain", "object"), required=True)
    process.add_argument("--palette", type=Path, required=True)
    process.add_argument("--size", type=parse_size, default=DEFAULT_SIZE)
    process.add_argument("--background", type=parse_rgb)
    process.add_argument("--background-tolerance", type=int, default=0)
    process.add_argument("--dither-strength", type=int, default=16)
    process.add_argument("--outline-width", type=int, default=1)
    process.add_argument("--seam-width", type=int, default=1)

    gate = subparsers.add_parser("gate", help="執行入庫閘自動檢查")
    gate.add_argument("--asset-root", type=Path, required=True)
    gate.add_argument("--manifest", type=Path, required=True)
    gate.add_argument("--palette", type=Path, required=True)

    generate = subparsers.add_parser("generate-placeholder-palette", help="重建測試專用佔位色盤")
    generate.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "generate-placeholder-palette":
            write_placeholder_palette(args.output)
            print(args.output)
            return 0
        palette = load_palette(args.palette)
        if args.command == "process":
            if args.background_tolerance < 0 or args.background_tolerance > 441:
                raise ValueError("背景容差必須介於 0 與 441")
            result = process_asset(
                input_path=args.input,
                asset_root=args.asset_root,
                def_id=args.def_id,
                kind=args.kind,
                palette=palette,
                size=args.size,
                background=args.background,
                background_tolerance=args.background_tolerance,
                dither_strength=args.dither_strength,
                outline_width=args.outline_width,
                seam_width=args.seam_width,
            )
            print(json.dumps(asdict(result), ensure_ascii=False, sort_keys=True))
            return 0
        violations = run_gate(args.asset_root, args.manifest, palette)
        if violations:
            for violation in violations:
                print(violation, file=sys.stderr)
            print(f"入庫閘拒絕：共 {len(violations)} 項", file=sys.stderr)
            return 2
        print("入庫閘通過")
        return 0
    except (OSError, ValueError) as error:
        print(f"錯誤：{error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
