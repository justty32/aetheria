#!/usr/bin/env python3
"""Enforce principle 5: content kinds are data, not populated C++ enums."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys


HEADER_SUFFIXES = frozenset({".h", ".hh", ".hpp", ".hxx"})
SECTIONS = frozenset({"mechanism", "debt"})
ENUM_PATTERN = re.compile(
    r"\benum\s+class\s+([A-Za-z_]\w*)\s*(?::[^;{}]+)?\s*([;{])",
    re.MULTILINE,
)


@dataclass(frozen=True, order=True)
class EnumDefinition:
    path: str
    name: str
    line: int

    @property
    def key(self) -> tuple[str, str]:
        return (self.path, self.name)


def mask_comments_and_literals(source: str) -> str:
    """Replace C++ comments and literals with spaces while preserving line numbers."""
    masked = list(source)
    index = 0
    size = len(source)

    def blank(start: int, end: int) -> None:
        for position in range(start, end):
            if masked[position] != "\n":
                masked[position] = " "

    while index < size:
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            end = size if end == -1 else end
            blank(index, end)
            index = end
            continue

        if source.startswith("/*", index):
            closing = source.find("*/", index + 2)
            end = size if closing == -1 else closing + 2
            blank(index, end)
            index = end
            continue

        if source.startswith('R"', index):
            delimiter_end = source.find("(", index + 2, min(size, index + 19))
            if delimiter_end != -1:
                delimiter = source[index + 2 : delimiter_end]
                terminator = ")" + delimiter + '"'
                closing = source.find(terminator, delimiter_end + 1)
                end = size if closing == -1 else closing + len(terminator)
                blank(index, end)
                index = end
                continue

        quote_is_digit_separator = (
            source[index] == "'"
            and index > 0
            and index + 1 < size
            and source[index - 1].isalnum()
            and source[index + 1].isalnum()
            and not (
                source[index - 1] in {"L", "u", "U"}
                and (index < 2 or not (source[index - 2].isalnum() or source[index - 2] == "_"))
            )
            and not (
                index >= 2
                and source[index - 2 : index] == "u8"
                and (index < 3 or not (source[index - 3].isalnum() or source[index - 3] == "_"))
            )
        )
        if source[index] in {'"', "'"} and not quote_is_digit_separator:
            quote = source[index]
            end = index + 1
            while end < size:
                if source[end] == "\\":
                    end = min(size, end + 2)
                    continue
                end += 1
                if source[end - 1] == quote:
                    break
            blank(index, end)
            index = end
            continue

        index += 1

    return "".join(masked)


def find_closing_brace(source: str, opening: int) -> int | None:
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def scan_headers(core_dir: Path, project_root: Path) -> tuple[list[EnumDefinition], list[str]]:
    definitions: list[EnumDefinition] = []
    errors: list[str] = []

    headers = sorted(
        path for path in core_dir.rglob("*") if path.is_file() and path.suffix in HEADER_SUFFIXES
    )
    for header in headers:
        source = header.read_text(encoding="utf-8")
        masked = mask_comments_and_literals(source)
        relative_path = header.relative_to(project_root).as_posix()

        for match in ENUM_PATTERN.finditer(masked):
            if match.group(2) == ";":
                continue
            opening = match.end(2) - 1
            closing = find_closing_brace(masked, opening)
            line = masked.count("\n", 0, match.start()) + 1
            if closing is None:
                errors.append(f"{relative_path}:{line}: enum class {match.group(1)} has no closing brace")
                continue
            if masked[opening + 1 : closing].strip():
                definitions.append(EnumDefinition(relative_path, match.group(1), line))

    return definitions, errors


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
