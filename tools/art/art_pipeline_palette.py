"""色盤、參數解析與管線結果型別。"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


DEFAULT_SIZE = (64, 64)
MAX_OUTSIDE_PALETTE_RATIO = 0.01
_DEF_ID_RE = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
_BAYER_4X4 = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)

Rgb = tuple[int, int, int]




@dataclass(frozen=True)
class ProcessResult:
    output: str
    detected_light: str
    rotation_degrees_ccw: int
    light_warning: str | None
    seam_before: float | None
    seam_after: float | None
    sha256: str


@dataclass(frozen=True)
class GateResult:
    violations: list[str]
    warnings: list[str]


def parse_hex_color(value: str) -> Rgb:
    if not re.fullmatch(r"#[0-9a-fA-F]{6}", value):
        raise ValueError(f"無效色彩 {value!r}；必須是 #RRGGBB")
    return tuple(int(value[index : index + 2], 16) for index in (1, 3, 5))  # type: ignore[return-value]


def format_hex_color(color: Rgb) -> str:
    return "#{:02X}{:02X}{:02X}".format(*color)


def load_palette(path: Path) -> list[Rgb]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"無法讀取色盤 {path}: {error}") from error
    values = data.get("colors") if isinstance(data, dict) else None
    if not isinstance(values, list) or not values:
        raise ValueError(f"色盤 {path} 必須含非空的 colors 陣列")
    colors = [parse_hex_color(value) for value in values if isinstance(value, str)]
    if len(colors) != len(values):
        raise ValueError(f"色盤 {path} 的 colors 只能含 #RRGGBB 字串")
    if len(colors) != len(dict.fromkeys(colors)):
        raise ValueError(f"色盤 {path} 含重複顏色")
    return colors


def placeholder_palette_document() -> dict[str, object]:
    levels = (0, 85, 170, 255)
    colors = [format_hex_color((red, green, blue)) for red in levels for green in levels for blue in levels]
    return {
        "name": "placeholder-only-not-project-palette",
        "warning_zh_tw": "僅供管線測試；不是 Aetheria 專案色盤，禁止拿來產生正式素材。",
        "generator": "RGB 笛卡兒積；每通道依序取 0、85、170、255。",
        "colors": colors,
    }


def write_placeholder_palette(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(placeholder_palette_document(), ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    path.write_text(payload, encoding="utf-8", newline="\n")


def parse_size(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"([1-9][0-9]*)x([1-9][0-9]*)", value)
    if match is None:
        raise argparse.ArgumentTypeError("尺寸必須是 WIDTHxHEIGHT，例如 64x64")
    return int(match.group(1)), int(match.group(2))


def parse_rgb(value: str) -> Rgb:
    parts = value.split(",")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("背景色必須是 R,G,B")
    try:
        color = tuple(int(part) for part in parts)
    except ValueError as error:
        raise argparse.ArgumentTypeError("背景色必須是整數 R,G,B") from error
    if any(channel < 0 or channel > 255 for channel in color):
        raise argparse.ArgumentTypeError("背景色色頻必須介於 0 與 255")
    return color  # type: ignore[return-value]


def def_id_relative_path(def_id: str) -> Path:
    if _DEF_ID_RE.fullmatch(def_id) is None:
        raise ValueError(f"無效 def id {def_id!r}")
    return Path(*def_id.split(".")).with_suffix(".png")
