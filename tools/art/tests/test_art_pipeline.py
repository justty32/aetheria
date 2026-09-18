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
