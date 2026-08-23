from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

import pytest
from PIL import Image

import art_pipeline


ART_DIR = Path(__file__).resolve().parents[1]
SCRIPT = ART_DIR / "art_pipeline.py"
PLACEHOLDER = ART_DIR / "placeholder_palette.json"


def _write_manifest(
    path: Path,
    def_id: str = "terrain.test",
    size: tuple[int, int] = (64, 64),
    kind: str | None = None,
) -> None:
    entry: dict[str, object] = {"def_id": def_id, "size": list(size)}
    if kind is not None:
        entry["kind"] = kind
    path.write_text(
        json.dumps({"assets": [entry]}),
        encoding="utf-8",
    )


def _run_cli(*arguments: object, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *(str(argument) for argument in arguments)],
        check=False,
        capture_output=True,
        text=True,
        env=env,
    )


def _save_gate_image(root: Path, image: Image.Image, relative: str = "terrain/test.png") -> Path:
    path = root / relative
    art_pipeline.save_png_deterministically(image, path)
    return path


def _directional_tile() -> Image.Image:
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 255))
    pixels = image.load()
    levels = (255, 170, 85, 0)
    for y in range(64):
        for x in range(64):
            value = levels[min(3, (x + y) // 32)]
            pixels[x, y] = (value, value, value, 255)
    pixels[12, 20] = (255, 0, 0, 255)
    return image


def test_placeholder_palette_matches_deterministic_generator(tmp_path: Path) -> None:
    generated = tmp_path / "generated.json"
    art_pipeline.write_placeholder_palette(generated)
    assert generated.read_bytes() == PLACEHOLDER.read_bytes()
    assert len(art_pipeline.load_palette(PLACEHOLDER)) == 64


def test_acceptance_script_proves_its_detection_power() -> None:
    environment = os.environ.copy()
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    result = subprocess.run(
        [sys.executable, str(ART_DIR / "verify_acceptance.py")],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(result.stdout)
    assert payload["reproducibility"]["injected_hashes_diverge"] is True
    assert payload["terrain_lighting"]["skip_rotation_hashes_diverge"] is True
    assert all(case["exit_code"] == 0 for case in payload["gate"]["rule_disabled_controls"].values())


def test_def_id_maps_dots_to_slashes() -> None:
    assert art_pipeline.def_id_relative_path("terrain.grassland") == Path("terrain/grassland.png")
    with pytest.raises(ValueError, match="無效 def id"):
        art_pipeline.def_id_relative_path("Terrain/Grass")


def test_crop_align_removes_background_and_hardens_alpha() -> None:
    image = Image.new("RGB", (20, 10), (255, 255, 255))
    for y in range(2, 8):
        for x in range(5, 15):
            image.putpixel((x, y), (255, 0, 0))
    aligned = art_pipeline.crop_align(image, (64, 64), "object", (255, 255, 255), 0)
    assert aligned.size == (64, 64)
    assert set(aligned.getchannel("A").get_flattened_data()) == {0, 255}
    assert aligned.getchannel("A").getbbox() == (27, 29, 37, 35)


def test_ordered_quantization_uses_only_palette_colors() -> None:
    image = Image.new("RGBA", (17, 13))
    image.putdata([(x * 13 % 256, y * 29 % 256, (x + y) * 17 % 256, 255) for y in range(13) for x in range(17)])
    palette = art_pipeline.load_palette(PLACEHOLDER)
    output = art_pipeline.quantize_ordered(image, palette)
    outside, visible = art_pipeline.palette_outside_count(output, palette)
    assert (outside, visible) == (0, 221)


def test_four_light_directions_normalize_to_same_hash(tmp_path: Path) -> None:
    palette = art_pipeline.load_palette(PLACEHOLDER)
    base = _directional_tile()
    rotations = (
        None,
        Image.Transpose.ROTATE_90,
        Image.Transpose.ROTATE_180,
        Image.Transpose.ROTATE_270,
    )
    hashes: list[str] = []
    for index, transpose in enumerate(rotations):
        source = base.copy() if transpose is None else base.transpose(transpose)
        source_path = tmp_path / f"source-{index}.png"
        source.save(source_path)
        result = art_pipeline.process_asset(
            source_path,
            tmp_path / f"output-{index}",
            "terrain.light",
            "terrain",
            palette,
        )
        hashes.append(result.sha256)
    assert len(set(hashes)) == 1, hashes


def test_object_four_directions_are_detected_without_rotation(tmp_path: Path) -> None:
    palette = art_pipeline.load_palette(PLACEHOLDER)
    base = _directional_tile()
    rotations = (
        None,
        Image.Transpose.ROTATE_90,
        Image.Transpose.ROTATE_180,
        Image.Transpose.ROTATE_270,
    )
    hashes: list[str] = []
    directions: list[str] = []
    for index, transpose in enumerate(rotations):
        source = base.copy() if transpose is None else base.transpose(transpose)
        quantized = art_pipeline.quantize_ordered(source, palette)
        unchanged, direction, degrees = art_pipeline.normalize_lighting(quantized, "object")
        assert unchanged.tobytes() == quantized.tobytes()
        assert degrees == 0
        directions.append(direction)
        source_path = tmp_path / f"object-{index}.png"
        source.save(source_path)
        result = art_pipeline.process_asset(
            source_path,
            tmp_path / f"objects-{index}",
            "object.light",
            "object",
            palette,
        )
        hashes.append(result.sha256)
        assert result.rotation_degrees_ccw == 0
        assert (result.light_warning is None) == (result.detected_light == "top-left")
    assert set(directions) == {"top-left", "top-right", "bottom-left", "bottom-right"}
    assert len(set(hashes)) == 4


def test_object_lighting_preserves_non_square_pixels_and_size() -> None:
    image = _directional_tile().resize((64, 96), Image.Resampling.NEAREST)
    image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    unchanged, direction, degrees = art_pipeline.normalize_lighting(image, "object")
    assert direction == "top-right"
    assert degrees == 0
    assert unchanged.size == (64, 96)
    assert unchanged.tobytes() == image.tobytes()


def test_object_outline_is_uniform_palette_color() -> None:
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for y in range(3, 13):
        for x in range(4, 12):
            image.putpixel((x, y), (170, 85, 0, 255))
    outlined = art_pipeline.normalize_outline(image, "object", (0, 0, 0), 1)
    assert outlined.getpixel((4, 5)) == (0, 0, 0, 255)
    assert outlined.getpixel((5, 5)) == (170, 85, 0, 255)


def test_terrain_outline_color_is_removed() -> None:
    image = Image.new("RGBA", (9, 9), (0, 0, 0, 0))
    for y in range(1, 8):
        for x in range(1, 8):
            image.putpixel((x, y), (0, 0, 0, 255) if x in (1, 7) or y in (1, 7) else (85, 170, 85, 255))
    normalized = art_pipeline.normalize_outline(image, "terrain", (0, 0, 0), 1)
    assert normalized.getpixel((1, 4)) == (85, 170, 85, 255)


def test_seam_repair_reduces_metric_to_zero() -> None:
    palette = art_pipeline.load_palette(PLACEHOLDER)
    image = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
    for y in range(64):
        image.putpixel((0, y), (0, 0, 0, 255))
        image.putpixel((63, y), (255, 255, 255, 255))
    before = art_pipeline.seam_metric(image)
    repaired = art_pipeline.repair_seams(image, palette)
    assert before > 0
    assert art_pipeline.seam_metric(repaired) == 0


def test_pipeline_is_bit_reproducible_across_processes_and_hash_seeds(tmp_path: Path) -> None:
    palette_path = tmp_path / "tie-palette.json"
    palette_path.write_text(
        json.dumps({"colors": ["#000000", "#020000", "#000200", "#000002"]}),
        encoding="utf-8",
    )
    source = Image.new("RGBA", (64, 64), (1, 0, 0, 0))
    for y in range(8, 56):
        for x in range(8, 56):
            source.putpixel((x, y), (1, 0, 0, 255))
    source_path = tmp_path / "source.png"
    source.save(source_path)
    hashes: list[str] = []
    for index, hash_seed in enumerate(("11", "97")):
        environment = os.environ.copy()
        environment["PYTHONHASHSEED"] = hash_seed
        result = _run_cli(
            "process",
            "--input",
            source_path,
            "--asset-root",
            tmp_path / f"run-{index}",
            "--def-id",
            "object.test",
            "--kind",
            "object",
            "--palette",
            palette_path,
            "--dither-strength",
            0,
            env=environment,
        )
        assert result.returncode == 0, result.stderr
        hashes.append(json.loads(result.stdout)["sha256"])
    assert hashes[0] == hashes[1]


def test_gate_accepts_clean_asset(tmp_path: Path) -> None:
    root = tmp_path / "assets"
    manifest = tmp_path / "manifest.json"
    _write_manifest(manifest)
    _save_gate_image(root, Image.new("RGBA", (64, 64), (85, 85, 85, 255)))
    result = _run_cli("gate", "--asset-root", root, "--manifest", manifest, "--palette", PLACEHOLDER)
    assert result.returncode == 0
    assert result.stdout.strip() == "入庫閘通過"


def test_gate_warns_for_object_light_without_rejecting_and_can_ignore(tmp_path: Path) -> None:
    root = tmp_path / "assets"
    manifest = tmp_path / "manifest.json"
    _write_manifest(manifest, def_id="object.test", kind="object")
    image = _directional_tile().transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    _save_gate_image(root, image, "object/test.png")
    warned = _run_cli("gate", "--asset-root", root, "--manifest", manifest, "--palette", PLACEHOLDER)
    assert warned.returncode == 0
    assert warned.stdout.strip() == "入庫閘通過（警告 1 項）"
    assert "警告：object/test.png: 主光方向 top-right（object 僅警告、不旋轉）" in warned.stderr

    ignored = _run_cli(
        "gate",
        "--asset-root",
        root,
        "--manifest",
        manifest,
        "--palette",
        PLACEHOLDER,
        "--object-light-policy",
        "ignore",
    )
    assert ignored.returncode == 0
    assert ignored.stdout.strip() == "入庫閘通過"
    assert ignored.stderr == ""


def test_gate_rejects_wrong_dimensions(tmp_path: Path) -> None:
    root = tmp_path / "assets"
    manifest = tmp_path / "manifest.json"
    _write_manifest(manifest)
    _save_gate_image(root, Image.new("RGBA", (63, 64), (85, 85, 85, 255)))
    result = _run_cli("gate", "--asset-root", root, "--manifest", manifest, "--palette", PLACEHOLDER)
    assert result.returncode != 0
    assert "terrain/test.png: 尺寸不符：實際 63x64，預期 64x64" in result.stderr


def test_gate_rejects_palette_ratio_above_threshold(tmp_path: Path) -> None:
    root = tmp_path / "assets"
    manifest = tmp_path / "manifest.json"
    _write_manifest(manifest)
    image = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
    for index in range(64):
        image.putpixel((index, 0), (1, 2, 3, 255))
    _save_gate_image(root, image)
    result = _run_cli("gate", "--asset-root", root, "--manifest", manifest, "--palette", PLACEHOLDER)
    assert result.returncode != 0
    assert "terrain/test.png: 色盤外顏色 64/4096 (1.562%)，門檻 1.000%" in result.stderr


def test_gate_rejects_unclean_alpha_edge(tmp_path: Path) -> None:
    root = tmp_path / "assets"
    manifest = tmp_path / "manifest.json"
    _write_manifest(manifest)
    image = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
    image.putpixel((0, 0), (85, 85, 85, 127))
    _save_gate_image(root, image)
    result = _run_cli("gate", "--asset-root", root, "--manifest", manifest, "--palette", PLACEHOLDER)
    assert result.returncode != 0
    assert "terrain/test.png: alpha 邊緣不乾淨：半透明像素 1，透明 RGB 污染像素 0" in result.stderr


def test_gate_rejects_name_not_matching_def(tmp_path: Path) -> None:
    root = tmp_path / "assets"
    manifest = tmp_path / "manifest.json"
    _write_manifest(manifest)
    _save_gate_image(root, Image.new("RGBA", (64, 64), (85, 85, 85, 255)), "terrain/wrong.png")
    result = _run_cli("gate", "--asset-root", root, "--manifest", manifest, "--palette", PLACEHOLDER)
    assert result.returncode != 0
    assert "terrain/wrong.png: 命名不符 def：檔案未列於 manifest" in result.stderr
    assert "terrain.test: 命名不符 def：預期 terrain/test.png 不存在" in result.stderr
