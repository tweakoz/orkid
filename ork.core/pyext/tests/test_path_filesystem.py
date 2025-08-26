#!/usr/bin/env python3

"""
Test Path::resolveRelativeTo with filesystem paths
This avoids the orkshader:// complexity
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

print("Importing orkengine.core...")
from orkengine.core import Path

def test_filesystem_paths():
    """Test resolveRelativeTo with regular filesystem paths"""
    print("\n=== Testing resolveRelativeTo with filesystem paths ===")
    
    # Test case: Resolve relative import from shader file
    print("\nTest: Relative import resolution")
    container = Path(f"{ork_path.root}/ork.data/platform_lev2/shaders/fxv2/basic.fxv2")
    import_file = Path("skintools.i2")
    
    print(f"Container: {container}")
    print(f"  isAbsolute: {container.isAbsolute()}")
    print(f"  isRelative: {container.isRelative()}")
    
    print(f"Import: {import_file}")
    print(f"  isAbsolute: {import_file.isAbsolute()}")
    print(f"  isRelative: {import_file.isRelative()}")
    
    print("\nCalling resolveRelativeTo...")
    try:
        resolved = import_file.resolveRelativeTo(container)
        print(f"Resolved: {resolved}")
        
        # Expected: f"{ork_path.root}/ork.data/platform_lev2/shaders/fxv2/skintools.i2
        expected = f"{ork_path.root}/ork.data/platform_lev2/shaders/fxv2/skintools.i2"
        if str(resolved) == expected:
            print(f"✓ SUCCESS: Correctly resolved to {resolved}")
        else:
            print(f"✗ FAILED: Expected {expected}, got {resolved}")
            
    except Exception as e:
        print(f"✗ ERROR: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    print("=" * 60)
    print("Path Resolution Test - Filesystem Paths")
    print("=" * 60)
    
    test_filesystem_paths()
    
    print("\n" + "=" * 60)
    print("Test completed")
    print("=" * 60)