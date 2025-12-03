#!/usr/bin/env python3
################################################################
# Test script for large file catalog import
# Tests the TAR write() chunking and LZ4 compression handling
# for files exceeding INT_MAX (2GB) and LZ4_MAX_INPUT_SIZE (2016MB)
################################################################

import argparse
import subprocess
import sys
import os
import tempfile
import shutil

def main():
    parser = argparse.ArgumentParser(description='Test large file catalog import')
    parser.add_argument('--sizemib', type=int, default=2500,
                        help='Size of test file in MiB (default: 2500)')
    parser.add_argument('--keep', action='store_true',
                        help='Keep test files after completion')
    parser.add_argument('--local-only', '-l', action='store_true',
                        help='Local only - do not upload to CDN')
    args = parser.parse_args()

    test_dir = '/tmp/catalog_test_large'
    test_file = os.path.join(test_dir, 'large_test.bin')

    # Get paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_file = os.path.join(script_dir, 'import_large.json')

    print(f"=" * 60)
    print(f"Large File Import Test")
    print(f"=" * 60)
    print(f"Test file size: {args.sizemib} MiB")
    print(f"Test directory: {test_dir}")
    print(f"Config file: {config_file}")
    print(f"=" * 60)

    # Create test directory
    os.makedirs(test_dir, exist_ok=True)

    # Remove old test file if exists
    if os.path.exists(test_file):
        print(f"Removing existing test file...")
        os.remove(test_file)

    # Create test file with dd
    print(f"Creating {args.sizemib} MiB test file...")
    result = subprocess.run(
        ['dd', 'if=/dev/zero', f'of={test_file}', 'bs=1M', f'count={args.sizemib}'],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"ERROR: Failed to create test file")
        print(result.stderr)
        return 1

    # Verify file size
    actual_size = os.path.getsize(test_file)
    expected_size = args.sizemib * 1024 * 1024
    print(f"Created test file: {actual_size} bytes ({actual_size / 1024 / 1024:.1f} MiB)")

    if actual_size != expected_size:
        print(f"WARNING: Expected {expected_size} bytes, got {actual_size} bytes")

    # Run catalog import
    print(f"\nRunning catalog import...")
    print(f"-" * 60)

    cmd = ['ork.catalog.import.py', '-c', config_file]
    if args.local_only:
        cmd.append('-l')

    result = subprocess.run(cmd)

    print(f"-" * 60)

    if result.returncode == 0:
        print(f"\nSUCCESS: Large file import completed!")
        print(f"FQ Asset ID: test_large|large_test")
    else:
        print(f"\nFAILED: Import returned exit code {result.returncode}")

    # Cleanup
    if not args.keep:
        print(f"\nCleaning up test files...")
        if os.path.exists(test_file):
            os.remove(test_file)
    else:
        print(f"\nKeeping test files in {test_dir}")

    return result.returncode

if __name__ == '__main__':
    sys.exit(main())
