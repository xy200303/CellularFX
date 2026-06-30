#!/usr/bin/env python3
"""Package the CellularFX addon into a release zip.

The output zip contains only the plugin files and pre-built DLLs; intermediate
files such as .exp, .lib, .pdb and backup ~*.dll are excluded.
"""

import os
import re
import sys
import zipfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
PLUGIN_DIR = PROJECT_ROOT / "addons" / "cellular_automata_engine"
OUT_DIR = PROJECT_ROOT
OUT_ZIP = OUT_DIR / "cellular_automata_engine.zip"

EXCLUDED_SUFFIXES = {".exp", ".lib", ".pdb"}
BACKUP_DLL_RE = re.compile(r"^~.*\.dll$")


def should_include(path: Path) -> bool:
    if path.suffix.lower() in EXCLUDED_SUFFIXES:
        return False
    if BACKUP_DLL_RE.match(path.name):
        return False
    return True


def main() -> int:
    if not PLUGIN_DIR.is_dir():
        print(f"Error: plugin directory not found: {PLUGIN_DIR}", file=sys.stderr)
        return 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(OUT_ZIP, "w", zipfile.ZIP_DEFLATED) as zf:
        for file_path in sorted(PLUGIN_DIR.rglob("*")):
            if not file_path.is_file():
                continue
            if not should_include(file_path):
                continue
            arcname = file_path.relative_to(PROJECT_ROOT / "addons")
            zf.write(file_path, arcname)

    print(f"Packaged: {OUT_ZIP} ({OUT_ZIP.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
