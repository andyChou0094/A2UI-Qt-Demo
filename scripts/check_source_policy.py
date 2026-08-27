#!/usr/bin/env python3
"""Reject source constructs outside the Qt 5.12.8/C++14 demo contract."""

from __future__ import print_function

import re
import sys
import os
from pathlib import Path


IGNORED_PARTS = {".git", ".cache", ".venv", "build", "__pycache__"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".cmake"}
FORBIDDEN = {
    "Qt 6 dependency": re.compile(r"(?:Qt6::|find_package\s*\(\s*Qt6)"),
    "QtSql dependency": re.compile(r"(?:Qt5::Sql|#\s*include\s*[<\"]QtSql)"),
    "runtime precompiled plugin loading": re.compile(r"\b(?:QPluginLoader|QLibrary)\b"),
    "C++17 standard library API": re.compile(
        r"\bstd::(?:any|filesystem|optional|string_view|variant)\b"
    ),
    "C++17 if constexpr": re.compile(r"\bif\s+constexpr\b"),
    "C++17 filesystem header": re.compile(r"#\s*include\s*<filesystem>"),
}


def is_source(path):
    return path.name == "CMakeLists.txt" or path.suffix.lower() in SOURCE_SUFFIXES


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    violations = []
    for directory, dirnames, filenames in os.walk(str(root)):
        dirnames[:] = [
            name
            for name in dirnames
            if name not in IGNORED_PARTS and not name.startswith("build-")
        ]
        for filename in filenames:
            path = Path(directory) / filename
            if not is_source(path):
                continue
            relative = path.relative_to(root)
            if relative.as_posix() == "cmake/A2uiBuildPolicy.cmake":
                continue
            text = path.read_text(encoding="utf-8")
            for line_number, line in enumerate(text.splitlines(), 1):
                for label, pattern in FORBIDDEN.items():
                    if pattern.search(line):
                        violations.append((relative, line_number, label))

    for path, line_number, label in violations:
        print("{}:{}: forbidden {}".format(path, line_number, label), file=sys.stderr)
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
