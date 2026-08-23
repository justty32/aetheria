#!/usr/bin/env python3
"""以合成輸入執行量化驗收，並用內建故障注入證明數字有偵測力。"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Sequence

from PIL import Image

import art_pipeline


ART_DIR = Path(__file__).resolve().parent
SCRIPT = ART_DIR / "art_pipeline.py"
PALETTE_PATH = ART_DIR / "placeholder_palette.json"
HASH_SEEDS = ("11", "97")
ROTATIONS = (None, Image.Transpose.ROTATE_90, Image.Transpose.ROTATE_180, Image.Transpose.ROTATE_270)


def directional_tile(size: tuple[int, int] = (64, 64)) -> Image.Image:
    image = Image.new("RGBA", size, (0, 0, 0, 255))
    pixels = image.load()
    levels = (255, 170, 85, 0)
    denominator = max(1, (size[0] + size[1]) // 4)
    for y in range(size[1]):
        for x in range(size[0]):
            value = levels[min(3, (x + y) // denominator)]
            pixels[x, y] = (value, value, value, 255)
    pixels[min(12, size[0] - 1), min(20, size[1] - 1)] = (255, 0, 0, 255)
    return image


def run_cli(
    script: Path,
    *arguments: object,
    hash_seed: str | None = None,
) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    if hash_seed is not None:
        environment["PYTHONHASHSEED"] = hash_seed
    return subprocess.run(
        [sys.executable, str(script), *(str(argument) for argument in arguments)],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )


def fault_script(root: Path, label: str, replacements: Sequence[tuple[str, str]]) -> Path:
    source = SCRIPT.read_text(encoding="utf-8")
    for old, new in replacements:
        if source.count(old) != 1:
            raise RuntimeError(
                f"故障注入 {label} 找不到唯一目標：{old}。"
                "找不到目標多半代表被注入的那一行已被修改，注入器也要跟著更新。"
            )
        source = source.replace(old, new)
    path = root / f"fault-{label}.py"
    path.write_text(source, encoding="utf-8")
    return path


def parse_process(result: subprocess.CompletedProcess[str]) -> dict[str, object]:
    if result.returncode != 0:
        raise RuntimeError(f"process 退出 {result.returncode}: {result.stderr}")
    return json.loads(result.stdout)


def write_tie_fixture(root: Path) -> tuple[Path, Path]:
    palette = root / "tie-palette.json"
    palette.write_text(
        json.dumps({"colors": ["#000000", "#020000", "#000200", "#000002"]}),
        encoding="utf-8",
    )
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    for y in range(8, 56):
        for x in range(8, 56):
            image.putpixel((x, y), (1, 0, 0, 255))
    source = root / "tie-source.png"
    image.save(source)
    return source, palette


def reproducibility_hashes(
    script: Path,
    root: Path,
    label: str,
    source: Path,
    palette: Path,
) -> list[str]:
    hashes: list[str] = []
    for index, seed in enumerate(HASH_SEEDS):
        result = run_cli(
            script,
            "process",
            "--input",
            source,
            "--asset-root",
            root / f"{label}-{index}",
            "--def-id",
            "object.repro",
            "--kind",
            "object",
            "--palette",
            palette,
            "--dither-strength",
            0,
            "--object-light-policy",
            "ignore",
            hash_seed=seed,
        )
        hashes.append(str(parse_process(result)["sha256"]))
    return hashes


def process_light_directions(
    script: Path,
    root: Path,
    label: str,
    kind: str,
    source: Image.Image,
) -> tuple[list[str], list[str], list[int]]:
    hashes: list[str] = []
    directions: list[str] = []
    rotations: list[int] = []
    for index, transpose in enumerate(ROTATIONS):
        rotated = source.copy() if transpose is None else source.transpose(transpose)
        path = root / f"{label}-source-{index}.png"
        rotated.save(path)
        result = run_cli(
            script,
            "process",
            "--input",
            path,
            "--asset-root",
            root / f"{label}-output-{index}",
            "--def-id",
            f"{kind}.light",
            "--kind",
            kind,
            "--palette",
            PALETTE_PATH,
            "--object-light-policy",
            "ignore",
        )
        payload = parse_process(result)
        hashes.append(str(payload["sha256"]))
        directions.append(str(payload["detected_light"]))
        rotations.append(int(payload["rotation_degrees_ccw"]))
    return hashes, directions, rotations


def write_manifest(path: Path, def_id: str = "terrain.test", kind: str | None = None) -> None:
    entry: dict[str, object] = {"def_id": def_id, "size": [64, 64]}
    if kind is not None:
        entry["kind"] = kind
    path.write_text(json.dumps({"assets": [entry]}), encoding="utf-8")


def gate_case(
    script: Path,
    root: Path,
    label: str,
    image: Image.Image,
    relative: str = "terrain/test.png",
) -> dict[str, object]:
    case = root / label
    asset_root = case / "assets"
    art_pipeline.save_png_deterministically(image, asset_root / relative)
    manifest = case / "manifest.json"
    write_manifest(manifest)
    result = run_cli(script, "gate", "--asset-root", asset_root, "--manifest", manifest, "--palette", PALETTE_PATH)
    lines = result.stderr.strip().splitlines()
    if lines and lines[-1].startswith("入庫閘拒絕"):
        lines = lines[:-1]
    return {"exit_code": result.returncode, "messages": lines, "stdout": result.stdout.strip()}


def main() -> int:
    palette = art_pipeline.load_palette(PALETTE_PATH)
    with tempfile.TemporaryDirectory(prefix="aetheria-art-acceptance-") as temporary:
        root = Path(temporary)

        tie_source, tie_palette = write_tie_fixture(root)
        baseline_hashes = reproducibility_hashes(SCRIPT, root, "repro-baseline", tie_source, tie_palette)
        unordered_script = fault_script(
            root,
            "unordered-palette",
            [
                (
                    "colors = [parse_hex_color(value) for value in values if isinstance(value, str)]",
                    "colors = [parse_hex_color(value) for value in set(values) if isinstance(value, str)]",
                )
            ],
        )
        unordered_hashes = reproducibility_hashes(
            unordered_script,
            root,
            "repro-unordered",
            tie_source,
            tie_palette,
        )
        restored_hashes = reproducibility_hashes(SCRIPT, root, "repro-restored", tie_source, tie_palette)

        source = directional_tile()
        terrain_hashes, terrain_directions, terrain_rotations = process_light_directions(
            SCRIPT, root, "terrain", "terrain", source
        )
        no_rotation_script = fault_script(
            root,
            "skip-terrain-rotation",
            [("normalized = image.copy() if transpose is None else image.transpose(transpose)", "normalized = image.copy()")],
        )
        broken_terrain_hashes, _, _ = process_light_directions(
            no_rotation_script, root, "terrain-broken", "terrain", source
        )

        object_hashes, object_directions, object_rotations = process_light_directions(
            SCRIPT, root, "object", "object", source
        )
        object_preserved: list[bool] = []
        for transpose in ROTATIONS:
            rotated = source.copy() if transpose is None else source.transpose(transpose)
            prepared = art_pipeline.quantize_ordered(rotated, palette)
            unchanged, _, _ = art_pipeline.normalize_lighting(prepared, "object")
            object_preserved.append(unchanged.tobytes() == prepared.tobytes())

        non_square = directional_tile((64, 96)).transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        non_square_output, non_square_direction, non_square_rotation = art_pipeline.normalize_lighting(
            non_square, "object"
        )

        terrain_output = root / "terrain-output-0/terrain/light.png"
        with Image.open(terrain_output) as opened:
            accepted_output = opened.convert("RGBA")
        outside, visible = art_pipeline.palette_outside_count(accepted_output, palette)
        foreign_color = accepted_output.copy()
        foreign_color.putpixel((1, 1), (1, 2, 3, 255))
        outside_fault, visible_after_color_fault = art_pipeline.palette_outside_count(foreign_color, palette)
        invisible_pixel = accepted_output.copy()
        invisible_pixel.putpixel((1, 1), (0, 0, 0, 0))
        _, visible_fault = art_pipeline.palette_outside_count(invisible_pixel, palette)

        seam_image = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
        for y in range(64):
            seam_image.putpixel((0, y), (0, 0, 0, 255))
            seam_image.putpixel((63, y), (255, 255, 255, 255))
        seam_before = art_pipeline.seam_metric(seam_image)
        seam_after = art_pipeline.seam_metric(art_pipeline.repair_seams(seam_image, palette))
        seam_after_when_repair_skipped = art_pipeline.seam_metric(seam_image.copy())

        clean = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
        wrong_size = Image.new("RGBA", (63, 64), (85, 85, 85, 255))
        outside_palette = clean.copy()
        for index in range(64):
            outside_palette.putpixel((index, 0), (1, 2, 3, 255))
        dirty_alpha = clean.copy()
        dirty_alpha.putpixel((0, 0), (85, 85, 85, 127))
        gate_results = {
            "clean": gate_case(SCRIPT, root, "gate-clean", clean),
            "dimension": gate_case(SCRIPT, root, "gate-dimension", wrong_size),
            "palette": gate_case(SCRIPT, root, "gate-palette", outside_palette),
            "alpha": gate_case(SCRIPT, root, "gate-alpha", dirty_alpha),
            "naming": gate_case(SCRIPT, root, "gate-naming", clean, "terrain/wrong.png"),
        }

        disabled_gate_scripts = {
            "dimension": fault_script(root, "skip-dimension-gate", [("if image.size != expected_size:", "if False:")]),
            "palette": fault_script(
                root,
                "relax-palette-gate",
                [("MAX_OUTSIDE_PALETTE_RATIO = 0.01", "MAX_OUTSIDE_PALETTE_RATIO = 1.0")],
            ),
            "alpha": fault_script(
                root,
                "skip-alpha-gate",
                [("if semitransparent or polluted_transparent:", "if False:")],
            ),
            "naming": fault_script(
                root,
                "skip-naming-gate",
                [
                    ("for relative_path in sorted(actual.keys() - expected.keys()):", "for relative_path in ():"),
                    ("for relative_path in sorted(expected.keys() - actual.keys()):", "for relative_path in ():"),
                ],
            ),
        }
        disabled_gate_results = {
            "dimension": gate_case(disabled_gate_scripts["dimension"], root, "gate-disabled-dimension", wrong_size),
            "palette": gate_case(disabled_gate_scripts["palette"], root, "gate-disabled-palette", outside_palette),
            "alpha": gate_case(disabled_gate_scripts["alpha"], root, "gate-disabled-alpha", dirty_alpha),
            "naming": gate_case(
                disabled_gate_scripts["naming"], root, "gate-disabled-naming", clean, "terrain/wrong.png"
            ),
        }

        expected_directions = {"top-left", "top-right", "bottom-left", "bottom-right"}
        gate_fragments = {
            "dimension": "尺寸不符",
            "palette": "色盤外顏色",
            "alpha": "alpha 邊緣不乾淨",
            "naming": "命名不符 def",
        }
        audit = {
            "separate_process_hashes_equal": "有效：內建 set 迭代注入會令兩個 hash seed 的雜湊分歧。",
            "terrain_four_direction_hashes_equal": "有效：內建跳過旋轉注入會令四個雜湊分歧。",
            "object_orientation_preserved": "有效：光源步驟前後逐位元比對，四方向都相同。",
            "lighting_directions": "有效：terrain 與 object 都必須偵測出四個預期方向。",
            "rotation_degrees": "有效但不能單看：terrain 角度須為 0/270/180/90，object 須全為 0，並與雜湊或逐位元比對合看。",
            "non_square_object_size": "有效：64×96 前後尺寸與像素都必須完全相同，方向仍須偵測為 top-right。",
            "palette_outside_pixels": "有效：注入 1 個色盤外可見像素後由 0 變 1。",
            "visible_pixels": "只有分母診斷力：注入透明像素會變，但它本身不是通過條件。",
            "seam_metric_after": "有效：跳過修補時維持正值，修補後才是 0。",
            "seam_metric_before": "只是一項 fixture 前置條件，不是管線成果；現已要求必須大於 0。",
            "gate_exit_codes": "有效：四種單一違規皆退出 2；各自停用規則後都變成 0。",
            "gate_messages": "診斷資料；驗收只檢查必要片段，不把完整文案當正確性證明。",
            "individual_sha256_values": "觀察資料；偵測條件是相等／分歧關係，單一雜湊值本身不是獨立門檻。",
        }
        result = {
            "reproducibility": {
                "baseline_hashes": baseline_hashes,
                "baseline_equal": len(set(baseline_hashes)) == 1,
                "injected_unordered_palette_hashes": unordered_hashes,
                "injected_hashes_diverge": len(set(unordered_hashes)) > 1,
                "restored_hashes": restored_hashes,
                "restored_equal": len(set(restored_hashes)) == 1,
            },
            "terrain_lighting": {
                "directions": terrain_directions,
                "rotations_ccw": terrain_rotations,
                "hashes": terrain_hashes,
                "hashes_equal": len(set(terrain_hashes)) == 1,
                "skip_rotation_hashes": broken_terrain_hashes,
                "skip_rotation_hashes_diverge": len(set(broken_terrain_hashes)) > 1,
            },
            "object_lighting": {
                "directions": object_directions,
                "rotations_ccw": object_rotations,
                "hashes": object_hashes,
                "hashes_all_different": len(set(object_hashes)) == 4,
                "orientation_preserved": object_preserved,
            },
            "non_square_object": {
                "direction": non_square_direction,
                "rotation_ccw": non_square_rotation,
                "size_before": list(non_square.size),
                "size_after": list(non_square_output.size),
                "pixels_preserved": non_square_output.tobytes() == non_square.tobytes(),
            },
            "palette": {
                "outside_pixels": outside,
                "visible_pixels": visible,
                "foreign_pixel_probe_outside": outside_fault,
                "foreign_pixel_probe_visible": visible_after_color_fault,
                "transparent_pixel_probe_visible": visible_fault,
            },
            "seam": {
                "metric_before": seam_before,
                "metric_after": seam_after,
                "metric_after_when_repair_skipped": seam_after_when_repair_skipped,
            },
            "gate": {"normal": gate_results, "rule_disabled_controls": disabled_gate_results},
            "detection_power_audit": audit,
        }
        print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))

        gate_messages_valid = all(
            any(fragment in message for message in gate_results[label]["messages"])
            for label, fragment in gate_fragments.items()
        )
        valid = (
            len(set(baseline_hashes)) == 1
            and len(set(unordered_hashes)) > 1
            and len(set(restored_hashes)) == 1
            and baseline_hashes == restored_hashes
            and set(terrain_directions) == expected_directions
            and terrain_rotations == [0, 270, 180, 90]
            and len(set(terrain_hashes)) == 1
            and len(set(broken_terrain_hashes)) > 1
            and set(object_directions) == expected_directions
            and object_rotations == [0, 0, 0, 0]
            and len(set(object_hashes)) == 4
            and all(object_preserved)
            and non_square_output.size == non_square.size
            and non_square_output.tobytes() == non_square.tobytes()
            and non_square_direction == "top-right"
            and non_square_rotation == 0
            and outside == 0
            and outside_fault == 1
            and visible_after_color_fault == visible
            and visible_fault == visible - 1
            and seam_before > 0
            and seam_after == 0
            and seam_after_when_repair_skipped > 0
            and gate_results["clean"]["exit_code"] == 0
            and all(gate_results[label]["exit_code"] == 2 for label in gate_fragments)
            and gate_messages_valid
            and all(case["exit_code"] == 0 for case in disabled_gate_results.values())
        )
        return 0 if valid else 1


if __name__ == "__main__":
    raise SystemExit(main())
