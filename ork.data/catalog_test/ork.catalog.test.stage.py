#!/usr/bin/env ork.python
"""
Stage test assets for catalog import testing.
Copies test files from ork.data/catalog_test/ to <stage>/assetcache/catalogtest/
"""

import os
import sys
import shutil
from pathlib import Path
from obt import path as obt_path

def main():
    # Source: test assets in orkid repo
    orkid_dir = Path(os.environ.get("ORKID_WORKSPACE_DIR", ""))
    if not orkid_dir.exists():
        print("Error: ORKID_WORKSPACE_DIR not set")
        return 1

    source_dir = orkid_dir / "ork.data" / "catalog_test"
    if not source_dir.exists():
        print(f"Error: Source directory not found: {source_dir}")
        return 1

    # Destination: staging area
    dest_dir = Path(obt_path.stage()) / "assetcache" / "catalogtest"

    # Clean and recreate destination
    if dest_dir.exists():
        print(f"Cleaning existing: {dest_dir}")
        shutil.rmtree(dest_dir)

    print(f"Staging test assets...")
    print(f"  From: {source_dir}")
    print(f"  To:   {dest_dir}")
    print()

    # Copy entire tree
    shutil.copytree(source_dir, dest_dir)

    # Count files
    file_count = sum(1 for _ in dest_dir.rglob("*") if _.is_file())

    print(f"Staged {file_count} test files")
    print()
    print("Test directories:")
    for subdir in sorted(dest_dir.iterdir()):
        if subdir.is_dir():
            count = sum(1 for _ in subdir.rglob("*") if _.is_file())
            print(f"  {subdir.name}/ ({count} files)")

    print()
    print("Ready for import testing. Example commands:")
    print()
    print("  # Test single-file mode (auto)")
    print("  ork.catalog.import.py -c ${ORKID_WORKSPACE_DIR}/ork.data/catalog_test/import_single.json -l")
    print()
    print("  # Test batched mode (explicit assets)")
    print("  ork.catalog.import.py -c ${ORKID_WORKSPACE_DIR}/ork.data/catalog_test/import_batched.json -l")
    print()
    print("  # Test hierarchical mode (auto with relative_stem)")
    print("  ork.catalog.import.py -c ${ORKID_WORKSPACE_DIR}/ork.data/catalog_test/import_hierarchical.json -l")

    return 0

if __name__ == "__main__":
    sys.exit(main())
