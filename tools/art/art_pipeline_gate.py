"""素材 manifest 載入與入庫檢查。"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Sequence
from PIL import Image
from art_pipeline_palette import MAX_OUTSIDE_PALETTE_RATIO, GateResult, Rgb, def_id_relative_path
from art_pipeline_lighting import detect_light_direction, object_light_warning


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
        kind = entry.get("kind")
        if not isinstance(def_id, str):
            raise ValueError(f"manifest assets[{index}].def_id 必須是字串")
        def_id_relative_path(def_id)
        if (
            not isinstance(size, list)
            or len(size) != 2
            or not all(isinstance(value, int) and value > 0 for value in size)
        ):
            raise ValueError(f"manifest assets[{index}].size 必須是兩個正整數")
        if kind is not None and kind not in ("terrain", "object"):
            raise ValueError(f"manifest assets[{index}].kind 必須是 terrain 或 object")
        validated.append({"def_id": def_id, "size": (size[0], size[1]), "kind": kind})
    return validated


def run_gate(
    asset_root: Path,
    manifest_path: Path,
    palette: Sequence[Rgb],
    warn_object_light: bool = True,
) -> GateResult:
    entries = _load_manifest(manifest_path)
    expected = {def_id_relative_path(str(entry["def_id"])).as_posix(): entry for entry in entries}
    actual_paths = sorted(path for path in asset_root.rglob("*.png") if path.is_file())
    actual = {path.relative_to(asset_root).as_posix(): path for path in actual_paths}
    violations: list[str] = []
    warnings: list[str] = []
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
        if entry["kind"] == "object" and warn_object_light:
            warning = object_light_warning(relative_path, detect_light_direction(image))
            if warning is not None:
                warnings.append(warning)
    return GateResult(violations=violations, warnings=warnings)
