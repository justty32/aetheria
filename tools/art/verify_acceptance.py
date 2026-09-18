#!/usr/bin/env python3
"""以合成輸入執行量化驗收，並用內建故障注入證明數字有偵測力。"""

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


from verify_acceptance_helpers import (
    directional_tile,
    run_cli,
    fault_script,
    parse_process,
    write_tie_fixture,
    reproducibility_hashes,
    process_light_directions,
    write_manifest,
    gate_case,
)
from verify_acceptance_probes import image_probes, gate_probes


def main() -> int:
    palette = art_pipeline.load_palette(PALETTE_PATH)
    with tempfile.TemporaryDirectory(prefix="aetheria-art-acceptance-") as temporary:
        root = Path(temporary)

        (
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
        ) = image_probes(root, palette)
        gate_results, disabled_gate_results = gate_probes(root)

        expected_directions = {"top-left", "top-right", "bottom-left", "bottom-right"}
        gate_fragments = {
            "dimension": "尺寸不符",
            "palette": "色盤外顏色",
            "alpha": "alpha 邊緣不乾淨",
            "naming": "命名不符 def",
        }
        audit = {
            "separate_process_hashes_equal": "有效：內建 set 迭代注入會令兩個 hash seed 的雜湊分歧。",
            "terrain_four_direction_hashes_equal": "有效：內建跳過旋轉注入會令四個雜湊分歧。",
            "object_orientation_preserved": "有效：光源步驟前後逐位元比對，四方向都相同。",
            "lighting_directions": "有效：terrain 與 object 都必須偵測出四個預期方向。",
            "rotation_degrees": "有效但不能單看：terrain 角度須為 0/270/180/90，object 須全為 0，並與雜湊或逐位元比對合看。",
            "non_square_object_size": "有效：64×96 前後尺寸與像素都必須完全相同，方向仍須偵測為 top-right。",
            "palette_outside_pixels": "有效：注入 1 個色盤外可見像素後由 0 變 1。",
            "visible_pixels": "只有分母診斷力：注入透明像素會變，但它本身不是通過條件。",
            "seam_metric_after": "有效：跳過修補時維持正值，修補後才是 0。",
            "seam_metric_before": "只是一項 fixture 前置條件，不是管線成果；現已要求必須大於 0。",
            "gate_exit_codes": "有效：四種單一違規皆退出 2；各自停用規則後都變成 0。",
            "gate_messages": "診斷資料；驗收只檢查必要片段，不把完整文案當正確性證明。",
            "individual_sha256_values": "觀察資料；偵測條件是相等／分歧關係，單一雜湊值本身不是獨立門檻。",
        }
        result = {
            "reproducibility": {
                "baseline_hashes": baseline_hashes,
                "baseline_equal": len(set(baseline_hashes)) == 1,
                "injected_unordered_palette_hashes": unordered_hashes,
                "injected_hashes_diverge": len(set(unordered_hashes)) > 1,
                "restored_hashes": restored_hashes,
                "restored_equal": len(set(restored_hashes)) == 1,
            },
            "terrain_lighting": {
                "directions": terrain_directions,
                "rotations_ccw": terrain_rotations,
                "hashes": terrain_hashes,
                "hashes_equal": len(set(terrain_hashes)) == 1,
                "skip_rotation_hashes": broken_terrain_hashes,
                "skip_rotation_hashes_diverge": len(set(broken_terrain_hashes)) > 1,
            },
            "object_lighting": {
                "directions": object_directions,
                "rotations_ccw": object_rotations,
                "hashes": object_hashes,
                "hashes_all_different": len(set(object_hashes)) == 4,
                "orientation_preserved": object_preserved,
            },
            "non_square_object": {
                "direction": non_square_direction,
                "rotation_ccw": non_square_rotation,
                "size_before": list(non_square.size),
                "size_after": list(non_square_output.size),
                "pixels_preserved": non_square_output.tobytes() == non_square.tobytes(),
            },
            "palette": {
                "outside_pixels": outside,
                "visible_pixels": visible,
                "foreign_pixel_probe_outside": outside_fault,
                "foreign_pixel_probe_visible": visible_after_color_fault,
                "transparent_pixel_probe_visible": visible_fault,
            },
            "seam": {
                "metric_before": seam_before,
                "metric_after": seam_after,
                "metric_after_when_repair_skipped": seam_after_when_repair_skipped,
            },
            "gate": {"normal": gate_results, "rule_disabled_controls": disabled_gate_results},
            "detection_power_audit": audit,
        }
        print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))

        gate_messages_valid = all(
            any(fragment in message for message in gate_results[label]["messages"])
            for label, fragment in gate_fragments.items()
        )
        valid = (
            len(set(baseline_hashes)) == 1
            and len(set(unordered_hashes)) > 1
            and len(set(restored_hashes)) == 1
            and baseline_hashes == restored_hashes
            and set(terrain_directions) == expected_directions
            and terrain_rotations == [0, 270, 180, 90]
            and len(set(terrain_hashes)) == 1
            and len(set(broken_terrain_hashes)) > 1
            and set(object_directions) == expected_directions
            and object_rotations == [0, 0, 0, 0]
            and len(set(object_hashes)) == 4
            and all(object_preserved)
            and non_square_output.size == non_square.size
            and non_square_output.tobytes() == non_square.tobytes()
            and non_square_direction == "top-right"
            and non_square_rotation == 0
            and outside == 0
            and outside_fault == 1
            and visible_after_color_fault == visible
            and visible_fault == visible - 1
            and seam_before > 0
            and seam_after == 0
            and seam_after_when_repair_skipped > 0
            and gate_results["clean"]["exit_code"] == 0
            and all(gate_results[label]["exit_code"] == 2 for label in gate_fragments)
            and gate_messages_valid
            and all(case["exit_code"] == 0 for case in disabled_gate_results.values())
        )
        return 0 if valid else 1



if __name__ == "__main__":
    raise SystemExit(main())
