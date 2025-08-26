#!/usr/bin/env python3
"""
Test the cdntest directory setup without launching Docker
This verifies the file copying functionality works correctly
"""

import sys
import os
from pathlib import Path

# Mock the CDN setup to test file copying

# Import after path setup
from ork import path as ork_path
import shutil
import glob
import json
import time

sys.path.insert(0, f"{ork_path.root}/obt.project/modules/docker/ork-devcdn")

def test_setup_files():
    """Test the file setup functionality"""
    content_path = ork_path.cdntest
    
    # Clear existing cdntest directory
    if content_path.exists():
        print(f"Clearing existing cdntest directory: {content_path}")
        shutil.rmtree(content_path)
    
    # Create fresh cdntest directory
    content_path.mkdir(parents=True, exist_ok=True)
    
    # Define test file patterns to copy using wildcards
    ork_data = ork_path.data
    copy_patterns = [
        ("src/singularity/banks/CZ1/ROM1/*.bnk", "banks/cz1/"),
        ("src/singularity/banks/TX81Z/ROM/*.bnk", "banks/tx81z/"),
        ("platform_lev2/shaders/*.glfx", "shaders/"),
        ("src/environ/misc/frogs/*.dds", "textures/"),
        ("scripts/python/*.py", "scripts/"),
    ]
    
    copied_files = []
    print(f"Setting up test files in {content_path}")
    print(f"Copying from: {ork_data}")
    
    for pattern, dst_dir in copy_patterns:
        src_pattern = str(ork_data / pattern)
        dst_path = content_path / dst_dir
        dst_path.mkdir(parents=True, exist_ok=True)
        
        files_found = glob.glob(src_pattern)
        print(f"\nPattern: {pattern}")
        print(f"  Found {len(files_found)} files")
        
        for src_file in files_found[:5]:  # Limit to 5 files per pattern
            src = Path(src_file)
            dst = dst_path / src.name
            shutil.copy2(src, dst)
            size = dst.stat().st_size
            rel_path = str(dst.relative_to(content_path))
            copied_files.append((rel_path, size))
            print(f"  ✓ {src.name} -> {rel_path} ({size} bytes)")
    
    # Create manifest
    manifest = {
        "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
        "total_files": len(copied_files),
        "files": [{"path": path, "size": size} for path, size in copied_files]
    }
    
    manifest_path = content_path / "manifest.json"
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"\nCreated manifest.json with {len(copied_files)} test files")
    print(f"Total size: {sum(size for _, size in copied_files)} bytes")
    
    # List directory structure
    print(f"\nDirectory structure of {content_path}:")
    for root, dirs, files in os.walk(content_path):
        level = root.replace(str(content_path), '').count(os.sep)
        indent = ' ' * 2 * level
        print(f"{indent}{os.path.basename(root)}/")
        subindent = ' ' * 2 * (level + 1)
        for file in files[:3]:  # Show first 3 files per dir
            print(f"{subindent}{file}")
        if len(files) > 3:
            print(f"{subindent}... and {len(files) - 3} more files")

if __name__ == "__main__":
    try:
        test_setup_files()
        print("\n✅ Test completed successfully!")
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)