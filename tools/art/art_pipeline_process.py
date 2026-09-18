"""六步處理編排與確定性 PNG 輸出。"""

from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Sequence
from PIL import Image
from art_pipeline_palette import DEFAULT_SIZE, ProcessResult, Rgb, def_id_relative_path
from art_pipeline_pixels import crop_align, quantize_ordered, normalize_outline, seam_metric, repair_seams
from art_pipeline_lighting import normalize_lighting, object_light_warning


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
    warn_object_light: bool = True,
) -> ProcessResult:
    with Image.open(input_path) as source:
        image = crop_align(source, size, kind, background, background_tolerance)
    image = quantize_ordered(image, palette, dither_strength)
    image, detected_light, rotation = normalize_lighting(image, kind)
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
        light_warning=(
            object_light_warning(def_id, detected_light)
            if kind == "object" and warn_object_light
            else None
        ),
        seam_before=before,
        seam_after=after,
        sha256=sha256_file(output_path),
    )
