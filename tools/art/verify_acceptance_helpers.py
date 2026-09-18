#!/usr/bin/env python3
"""驗收用合成輸入、CLI 呼叫與故障注入 helper。"""

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


from verify_acceptance_source import standalone_pipeline_source


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
    source = standalone_pipeline_source(SCRIPT)
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
