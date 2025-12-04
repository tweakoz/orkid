#!/usr/bin/env ork.python
###############################################################################
# Large file round-trip test for asset catalog
# Tests packaging, fetching, and verifying files >2GB
###############################################################################

import os
import sys
import hashlib
import argparse
import multiprocessing
from pathlib import Path

def md5_file(filepath: Path) -> str:
    """Calculate MD5 hash of a file, handling large files efficiently."""
    hash_md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(8 * 1024 * 1024), b""):  # 8MB chunks
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def create_test_file(filepath: Path, size_mb: int) -> str:
    """Create a test file with random data and return its MD5 hash."""
    print(f"Creating {size_mb}MB test file: {filepath}")

    chunk_size = 1024 * 1024  # 1MB chunks
    hash_md5 = hashlib.md5()

    with open(filepath, "wb") as f:
        for i in range(size_mb):
            chunk = os.urandom(chunk_size)
            f.write(chunk)
            hash_md5.update(chunk)
            if (i + 1) % 256 == 0:
                print(f"  Written {i + 1}MB / {size_mb}MB")

    print(f"Created file: {filepath} ({size_mb}MB)")
    return hash_md5.hexdigest()

def run_command(cmd: str) -> int:
    """Run a shell command and return exit code."""
    print(f"Running: {cmd}")
    return os.system(cmd)

def fetch_asset(asset_id: str, result_queue):
    """Fetch asset in subprocess (fresh catalog load)."""
    try:
        from orkengine import core
        core.coreappinit()

        catalog = core.AssetCatalog.instance
        fetch_request = catalog.fetch(asset_id)

        if fetch_request and fetch_request.succeeded:
            result_queue.put(("success", None))
        else:
            error = fetch_request.error_detail if fetch_request else "Unknown error"
            result_queue.put(("error", error))
    except Exception as e:
        result_queue.put(("error", str(e)))

def main():
    parser = argparse.ArgumentParser(description="Test large file round-trip through asset catalog")
    parser.add_argument("-s", "--size", type=int, default=2560,
                        help="Size of test file in MB (default: 2560 = 2.5GB)")
    parser.add_argument("-k", "--keep", action="store_true",
                        help="Keep test files after completion")
    args = parser.parse_args()

    # Paths
    orkid_workspace = os.environ.get("ORKID_WORKSPACE_DIR")
    if not orkid_workspace:
        print("ERROR: ORKID_WORKSPACE_DIR not set")
        sys.exit(1)

    stage_dir = os.environ.get("OBT_STAGE")
    if not stage_dir:
        print("ERROR: OBT_STAGE not set")
        sys.exit(1)

    test_dir = Path(stage_dir) / "assetcache" / "ORKTEST"
    test_dir.mkdir(parents=True, exist_ok=True)

    test_file = test_dir / "large_test.bin"
    manifest_file = Path(orkid_workspace) / "ork.data" / "asset_manifests" / "test_large.json"
    import_config = Path(orkid_workspace) / "ork.data" / "catalog_test" / "import_large.json"

    print("=" * 60)
    print("Large File Round-Trip Test")
    print("=" * 60)
    print(f"Test file: {test_file}")
    print(f"Size: {args.size}MB ({args.size / 1024:.2f}GB)")
    print(f"Manifest: {manifest_file}")
    print(f"Import config: {import_config}")
    print("=" * 60)

    # Step 0: Delete manifest to force fresh packaging
    print("\n[Step 0] Clearing manifest to force fresh packaging...")
    if manifest_file.exists():
        manifest_file.unlink()
        print(f"Deleted manifest: {manifest_file}")
    else:
        print("No manifest to delete")

    # Step 1: Create test file
    print("\n[Step 1] Creating test file...")
    original_md5 = create_test_file(test_file, args.size)
    print(f"Original MD5: {original_md5}")

    # Step 2: Package the file
    print("\n[Step 2] Packaging file...")
    ret = run_command(f"ork.catalog.import.py -c {import_config}")
    if ret != 0:
        print("ERROR: Packaging failed")
        sys.exit(1)

    # Step 3: Delete local file
    print("\n[Step 3] Deleting local file...")
    test_file.unlink()
    print(f"Deleted: {test_file}")

    # Step 4: Fetch the file (fork to get fresh catalog)
    print("\n[Step 4] Fetching file...")
    result_queue = multiprocessing.Queue()
    fetch_proc = multiprocessing.Process(
        target=fetch_asset,
        args=("test_large|large_test", result_queue)
    )
    fetch_proc.start()
    fetch_proc.join()

    status, error = result_queue.get()
    if status != "success":
        print(f"ERROR: Fetch failed: {error}")
        sys.exit(1)

    print(f"Fetch complete")
    fetched_file = test_file
    print(f"Fetched to: {fetched_file}")

    # Step 5: Verify MD5
    print("\n[Step 5] Verifying MD5...")
    fetched_md5 = md5_file(fetched_file)
    print(f"Original MD5: {original_md5}")
    print(f"Fetched MD5:  {fetched_md5}")

    if original_md5 == fetched_md5:
        print("\n" + "=" * 60)
        print("SUCCESS: MD5 hashes match!")
        print("=" * 60)
        result = 0
    else:
        print("\n" + "=" * 60)
        print("FAILURE: MD5 hashes DO NOT match!")
        print("=" * 60)
        result = 1

    # Cleanup
    if not args.keep:
        print("\n[Cleanup] Removing test files...")
        if fetched_file.exists():
            fetched_file.unlink()
            print(f"Deleted: {fetched_file}")

    sys.exit(result)

if __name__ == "__main__":
    main()
