"""C++ 標頭的內容 enum 掃描與註解、字串遮罩。"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


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
