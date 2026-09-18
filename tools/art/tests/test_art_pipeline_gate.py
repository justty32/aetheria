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
