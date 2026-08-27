#!/usr/bin/env python3
"""Resolve Debian package metadata from a pinned Packages.xz index."""

import json
import lzma
import sys


def parse_stanza(text):
    result = {}
    for line in text.splitlines():
        if ": " in line:
            key, value = line.split(": ", 1)
            result[key] = value
    return result


def main():
    if len(sys.argv) < 3:
        print("usage: resolve_debian_packages.py Packages.xz package...", file=sys.stderr)
        return 2

    requested = set(sys.argv[2:])
    with lzma.open(sys.argv[1], mode="rt", encoding="utf-8") as package_file:
        stanzas = (parse_stanza(block) for block in package_file.read().split("\n\n"))
        resolved = {
            stanza.get("Package"): {
                "version": stanza.get("Version"),
                "filename": stanza.get("Filename"),
                "sha256": stanza.get("SHA256"),
            }
            for stanza in stanzas
            if stanza.get("Package") in requested
        }

    missing = sorted(requested.difference(resolved))
    if missing:
        print("missing packages: {}".format(", ".join(missing)), file=sys.stderr)
        return 1
    print(json.dumps(resolved, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
