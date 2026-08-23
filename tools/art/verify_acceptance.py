#!/usr/bin/env python3
"""以合成輸入執行任務書要求的量化驗收，輸出 JSON。"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

import art_pipeline


ART_DIR = Path(__file__).resolve().parent
SCRIPT = ART_DIR / "art_pipeline.py"
PALETTE_PATH = ART_DIR / "placeholder_palette.json"


def directional_tile() -> Image.Image:
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 255))
    pixels = image.load()
    levels = (255, 170, 85, 0)
    for y in range(64):
        for x in range(64):
            value = levels[min(3, (x + y) // 32)]
            pixels[x, y] = (value, value, value, 255)
    pixels[12, 20] = (255, 0, 0, 255)
    return image


def run_cli(*arguments: object, hash_seed: str | None = None) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    if hash_seed is not None:
        environment["PYTHONHASHSEED"] = hash_seed
    return subprocess.run(
        [sys.executable, str(SCRIPT), *(str(argument) for argument in arguments)],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )


def write_manifest(path: Path, def_id: str = "terrain.test") -> None:
    path.write_text(
        json.dumps({"assets": [{"def_id": def_id, "size": [64, 64]}]}),
        encoding="utf-8",
    )


def gate_case(root: Path, label: str, image: Image.Image, relative: str = "terrain/test.png") -> dict[str, object]:
    case = root / label
    asset_root = case / "assets"
    path = asset_root / relative
    art_pipeline.save_png_deterministically(image, path)
    manifest = case / "manifest.json"
    write_manifest(manifest)
    result = run_cli("gate", "--asset-root", asset_root, "--manifest", manifest, "--palette", PALETTE_PATH)
    return {"exit_code": result.returncode, "message": result.stderr.strip().splitlines()[:-1]}


def main() -> int:
    palette = art_pipeline.load_palette(PALETTE_PATH)
    with tempfile.TemporaryDirectory(prefix="aetheria-art-acceptance-") as temporary:
        root = Path(temporary)

        source = directional_tile()
        source_path = root / "repro-source.png"
        source.save(source_path)
        separate_process_hashes: list[str] = []
        for index, seed in enumerate(("11", "97")):
            result = run_cli(
                "process",
                "--input",
                source_path,
                "--asset-root",
                root / f"repro-{index}",
                "--def-id",
                "terrain.repro",
                "--kind",
                "terrain",
                "--palette",
                PALETTE_PATH,
                hash_seed=seed,
            )
            if result.returncode != 0:
                print(result.stderr, file=sys.stderr)
                return result.returncode
            separate_process_hashes.append(json.loads(result.stdout)["sha256"])
        output_path = root / "repro-0/terrain/repro.png"
        with Image.open(output_path) as output:
            outside, visible = art_pipeline.palette_outside_count(output, palette)

        rotations = (None, Image.Transpose.ROTATE_90, Image.Transpose.ROTATE_180, Image.Transpose.ROTATE_270)
        light_hashes: list[str] = []
        for index, transpose in enumerate(rotations):
            rotated = source.copy() if transpose is None else source.transpose(transpose)
            path = root / f"light-{index}.png"
            rotated.save(path)
            result = run_cli(
                "process",
                "--input",
                path,
                "--asset-root",
                root / f"light-output-{index}",
                "--def-id",
                "terrain.light",
                "--kind",
                "terrain",
                "--palette",
                PALETTE_PATH,
            )
            if result.returncode != 0:
                print(result.stderr, file=sys.stderr)
                return result.returncode
            light_hashes.append(json.loads(result.stdout)["sha256"])

        seam_image = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
        for y in range(64):
            seam_image.putpixel((0, y), (0, 0, 0, 255))
            seam_image.putpixel((63, y), (255, 255, 255, 255))
        seam_before = art_pipeline.seam_metric(seam_image)
        seam_after = art_pipeline.seam_metric(art_pipeline.repair_seams(seam_image, palette))

        clean = Image.new("RGBA", (64, 64), (85, 85, 85, 255))
        wrong_size = Image.new("RGBA", (63, 64), (85, 85, 85, 255))
        outside_palette = clean.copy()
        for index in range(64):
            outside_palette.putpixel((index, 0), (1, 2, 3, 255))
        dirty_alpha = clean.copy()
        dirty_alpha.putpixel((0, 0), (85, 85, 85, 127))
        gate_results = {
            "dimension": gate_case(root, "gate-dimension", wrong_size),
            "palette": gate_case(root, "gate-palette", outside_palette),
            "alpha": gate_case(root, "gate-alpha", dirty_alpha),
            "naming": gate_case(root, "gate-naming", clean, "terrain/wrong.png"),
        }

        result = {
            "separate_process_hashes": separate_process_hashes,
            "separate_process_hashes_equal": len(set(separate_process_hashes)) == 1,
            "palette_outside_pixels": outside,
            "visible_pixels": visible,
            "four_direction_hashes": light_hashes,
            "four_direction_hashes_equal": len(set(light_hashes)) == 1,
            "seam_metric_before": seam_before,
            "seam_metric_after": seam_after,
            "gate_violations": gate_results,
        }
        print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))

        valid = (
            result["separate_process_hashes_equal"]
            and outside == 0
            and result["four_direction_hashes_equal"]
            and seam_after == 0
            and all(case["exit_code"] != 0 for case in gate_results.values())
        )
        return 0 if valid else 1


if __name__ == "__main__":
    raise SystemExit(main())
