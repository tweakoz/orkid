#!/usr/bin/env python3
"""
Clear and set up the cdntest directory with test files
"""

from pathlib import Path
import shutil
import glob
import json
import time
from ork import path as ork_path

def setup_cdntest():
    """Clear and set up test files in cdntest directory"""
    content_path = ork_path.cdntest
    
    # Clear existing cdntest directory
    if content_path.exists():
        print(f"Clearing existing cdntest directory: {content_path}")
        shutil.rmtree(content_path)
    
    # Create fresh cdntest directory
    content_path.mkdir(parents=True, exist_ok=True)
    
    # Create upload directory (needed for uploads)
    upload_path = content_path / "upload"
    upload_path.mkdir(parents=True, exist_ok=True)
    print(f"Created upload directory: {upload_path}")
    
    # Define test file patterns to copy using wildcards
    ork_data = ork_path.data
    copy_patterns = [
        ("platform_lev2/shaders/glfx/*.glfx", "shaders/"),
        ("misc/*.dds", "textures/"),
        ("misc/*.png", "images/"),
        ("tests/pbr2/*.png", "pbr/"),
        ("tests/hython_test/*.py", "scripts/"),
    ]
    
    copied_files = []
    print(f"Setting up test files in {content_path}")
    
    for pattern, dst_dir in copy_patterns:
        src_pattern = str(ork_data / pattern)
        dst_path = content_path / dst_dir
        dst_path.mkdir(parents=True, exist_ok=True)
        
        for src_file in glob.glob(src_pattern)[:5]:  # Limit to 5 files per pattern
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
    
    with open(content_path / "manifest.json", 'w') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"\nCreated manifest.json with {len(copied_files)} test files")
    print(f"Total size: {sum(size for _, size in copied_files)} bytes")

if __name__ == "__main__":
    setup_cdntest()