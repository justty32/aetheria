"""重現性、影像處理與入庫規則的正向／故障控制。"""

from __future__ import annotations

from pathlib import Path
from PIL import Image
import art_pipeline
from verify_acceptance_helpers import (
    SCRIPT,
    ROTATIONS,
    directional_tile,
    write_tie_fixture,
    reproducibility_hashes,
    fault_script,
    process_light_directions,
    gate_case,
)


def image_probes(root: Path, palette: list[art_pipeline.Rgb]) -> tuple:
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

    return (
        baseline_hashes,
        broken_terrain_hashes,
        non_square,
        non_square_direction,
        non_square_output,
        non_square_rotation,
        object_directions,
        object_hashes,
        object_preserved,
        object_rotations,
        outside,
        outside_fault,
        restored_hashes,
        seam_after,
        seam_after_when_repair_skipped,
        seam_before,
        terrain_directions,
        terrain_hashes,
        terrain_rotations,
        unordered_hashes,
        visible,
        visible_after_color_fault,
        visible_fault,
    )


def gate_probes(root: Path) -> tuple:
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

    return (
        gate_results,
        disabled_gate_results,
    )
