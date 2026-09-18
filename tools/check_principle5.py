#!/usr/bin/env python3
"""Enforce principle 5: content kinds are data, not populated C++ enums."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys


from check_principle5_scan import (
    HEADER_SUFFIXES,
    SECTIONS,
    ENUM_PATTERN,
    EnumDefinition,
    mask_comments_and_literals,
    find_closing_brace,
    scan_headers,
)


def read_allowlist(path: Path) -> tuple[dict[tuple[str, str], str], dict[tuple[str, str], str], list[str]]:
    entries: dict[str, dict[tuple[str, str], str]] = {section: {} for section in SECTIONS}
    errors: list[str] = []
    section: str | None = None

    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            candidate = line[1:-1].strip()
            if candidate not in SECTIONS:
                errors.append(f"{path}:{line_number}: unknown section [{candidate}]")
                section = None
            else:
                section = candidate
            continue
        if section is None:
            errors.append(f"{path}:{line_number}: entry appears outside a known section")
            continue

        identity, separator, reason = line.partition("|")
        enum_path, key_separator, enum_name = identity.strip().rpartition("::")
        if not separator or not reason.strip():
            errors.append(f"{path}:{line_number}: every entry needs a non-empty reason after '|'")
            continue
        if not key_separator or not enum_path.startswith("core/") or not enum_name:
            errors.append(
                f"{path}:{line_number}: identity must be formatted as core/path/header.h::EnumName"
            )
            continue

        key = (enum_path, enum_name)
        if any(key in section_entries for section_entries in entries.values()):
            errors.append(f"{path}:{line_number}: duplicate allowlist entry {enum_path}::{enum_name}")
            continue
        entries[section][key] = reason.strip()

    return entries["mechanism"], entries["debt"], errors


def parse_args() -> argparse.Namespace:
    script_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=script_root / "core")
    parser.add_argument(
        "--allowlist", type=Path, default=script_root / "tests" / "principle5_allowlist.txt"
    )
    parser.add_argument("--project-root", type=Path, default=script_root)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_root = args.project_root.resolve()
    core_dir = args.core.resolve()
    allowlist_path = args.allowlist.resolve()

    try:
        definitions, errors = scan_headers(core_dir, project_root)
        mechanisms, debt, allowlist_errors = read_allowlist(allowlist_path)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"principle5 scan failed: {error}", file=sys.stderr)
        return 1

    errors.extend(allowlist_errors)
    discovered = {definition.key: definition for definition in definitions}
    allowed = mechanisms.keys() | debt.keys()

    for definition in definitions:
        if definition.key not in allowed:
            errors.append(
                f"{definition.path}:{definition.line}: enum class {definition.name} has enumerators "
                "but is not allowlisted; add it to [mechanism] only for fixed state/behavior, "
                "or to [debt] for a content kind that must be data-driven"
            )

    for enum_path, enum_name in sorted(debt.keys() - discovered.keys()):
        errors.append(
            f"{enum_path}: debt enum class {enum_name} is gone; remove it from the [debt] allowlist"
        )

    if errors:
        print("principle5 check failed:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1

    print(
        "principle5 check passed: "
        f"{len(definitions)} populated enums ({len(mechanisms)} mechanisms, {len(debt)} debt)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
