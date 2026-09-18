from __future__ import annotations

import sys
from pathlib import Path


ART_DIR = Path(__file__).resolve().parents[1]
if str(ART_DIR) not in sys.path:
    sys.path.insert(0, str(ART_DIR))


import json
import subprocess

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
