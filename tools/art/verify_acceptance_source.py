"""把管線入口及同目錄依賴展成單一故障注入副本。"""

from __future__ import annotations

import ast
from pathlib import Path


def standalone_pipeline_source(script: Path) -> str:
    """只展開本管線的 import，保留每段原始碼與入口的 __doc__。"""
    visited: set[Path] = set()

    def expand(path: Path) -> str:
        if path in visited:
            return ""
        visited.add(path)
        source = path.read_text(encoding="utf-8")
        lines = source.splitlines(keepends=True)
        parts: list[str] = []
        cursor = 0
        for index, node in enumerate(ast.parse(source).body):
            replacement: str | None = None
            if isinstance(node, ast.ImportFrom) and (node.module or "").startswith("art_pipeline_"):
                replacement = expand(path.with_name(node.module + ".py"))
            elif path != script:
                if isinstance(node, ast.ImportFrom) and node.module == "__future__":
                    replacement = ""
                elif (
                    index == 0
                    and isinstance(node, ast.Expr)
                    and isinstance(node.value, ast.Constant)
                    and isinstance(node.value.value, str)
                ):
                    replacement = ""
            if replacement is not None:
                parts.append("".join(lines[cursor:node.lineno - 1]))
                parts.append(replacement)
                cursor = node.end_lineno
        parts.append("".join(lines[cursor:]))
        return "".join(parts)

    return expand(script)
