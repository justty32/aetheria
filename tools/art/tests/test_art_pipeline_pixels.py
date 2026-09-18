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


from conftest import _write_manifest, _run_cli, _save_gate_image, _directional_tile


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
