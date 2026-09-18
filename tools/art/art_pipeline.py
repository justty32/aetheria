#!/usr/bin/env python3
"""Aetheria 的確定性美術後處理管線與入庫閘。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence

from PIL import Image


from art_pipeline_palette import (
    ProcessResult,
    GateResult,
    parse_hex_color,
    format_hex_color,
    load_palette,
    placeholder_palette_document,
    write_placeholder_palette,
    parse_size,
    parse_rgb,
    def_id_relative_path,
    DEFAULT_SIZE,
    MAX_OUTSIDE_PALETTE_RATIO,
    _DEF_ID_RE,
    _BAYER_4X4,
    Rgb,
)
from art_pipeline_pixels import (
    _remove_background,
    crop_align,
    _nearest_palette_color,
    quantize_ordered,
    _opaque_boundary,
    normalize_outline,
    seam_metric,
    repair_seams,
)
from art_pipeline_lighting import (
    _luminance,
    detect_light_direction,
    normalize_lighting,
    object_light_warning,
)
from art_pipeline_process import (
    sha256_file,
    save_png_deterministically,
    process_asset,
)
from art_pipeline_gate import (
    palette_outside_count,
    alpha_edge_issue_counts,
    _load_manifest,
    run_gate,
)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    process = subparsers.add_parser("process", help="依六步管線處理一張素材")
    process.add_argument("--input", type=Path, required=True)
    process.add_argument("--asset-root", type=Path, required=True)
    process.add_argument("--def-id", required=True)
    process.add_argument("--kind", choices=("terrain", "object"), required=True)
    process.add_argument("--palette", type=Path, required=True)
    process.add_argument("--size", type=parse_size, default=DEFAULT_SIZE)
    process.add_argument("--background", type=parse_rgb)
    process.add_argument("--background-tolerance", type=int, default=0)
    process.add_argument("--dither-strength", type=int, default=16)
    process.add_argument("--outline-width", type=int, default=1)
    process.add_argument("--seam-width", type=int, default=1)
    process.add_argument("--object-light-policy", choices=("warn", "ignore"), default="warn")

    gate = subparsers.add_parser("gate", help="執行入庫閘自動檢查")
    gate.add_argument("--asset-root", type=Path, required=True)
    gate.add_argument("--manifest", type=Path, required=True)
    gate.add_argument("--palette", type=Path, required=True)
    gate.add_argument("--object-light-policy", choices=("warn", "ignore"), default="warn")

    generate = subparsers.add_parser("generate-placeholder-palette", help="重建測試專用佔位色盤")
    generate.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "generate-placeholder-palette":
            write_placeholder_palette(args.output)
            print(args.output)
            return 0
        palette = load_palette(args.palette)
        if args.command == "process":
            if args.background_tolerance < 0 or args.background_tolerance > 441:
                raise ValueError("背景容差必須介於 0 與 441")
            result = process_asset(
                input_path=args.input,
                asset_root=args.asset_root,
                def_id=args.def_id,
                kind=args.kind,
                palette=palette,
                size=args.size,
                background=args.background,
                background_tolerance=args.background_tolerance,
                dither_strength=args.dither_strength,
                outline_width=args.outline_width,
                seam_width=args.seam_width,
                warn_object_light=args.object_light_policy == "warn",
            )
            if result.light_warning is not None:
                print(f"警告：{result.light_warning}", file=sys.stderr)
            print(json.dumps(asdict(result), ensure_ascii=False, sort_keys=True))
            return 0
        gate_result = run_gate(
            args.asset_root,
            args.manifest,
            palette,
            warn_object_light=args.object_light_policy == "warn",
        )
        for warning in gate_result.warnings:
            print(f"警告：{warning}", file=sys.stderr)
        if gate_result.violations:
            for violation in gate_result.violations:
                print(violation, file=sys.stderr)
            print(f"入庫閘拒絕：共 {len(gate_result.violations)} 項", file=sys.stderr)
            return 2
        suffix = f"（警告 {len(gate_result.warnings)} 項）" if gate_result.warnings else ""
        print(f"入庫閘通過{suffix}")
        return 0
    except (OSError, ValueError) as error:
        print(f"錯誤：{error}", file=sys.stderr)
        return 2



if __name__ == "__main__":
    raise SystemExit(main())
