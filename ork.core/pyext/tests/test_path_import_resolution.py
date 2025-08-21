#!/usr/bin/env python3

"""
Unit test for shader import path resolution
Tests the specific case that's failing in pixelart.py
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from orkengine.core import Path

def test_shader_import_resolution():
    """Test the exact case that's failing"""
    print("\n=== Testing Shader Import Resolution ===")
    
    # Case 1: orkshader://basic.fxv2 imports "skintools.i2"
    print("\nCase 1: Relative import from orkshader://")
    container = Path("orkshader://basic.fxv2")
    import_file = Path("skintools.i2")
    
    print(f"Container path: '{container}'")
    print(f"  isAbsolute: {container.isAbsolute()}")
    print(f"  hasUrlBase: {container.hasUrlBase()}")
    
    print(f"Import path: '{import_file}'")
    print(f"  isAbsolute: {import_file.isAbsolute()}")
    print(f"  isRelative: {import_file.isRelative()}")
    
    # This should resolve to orkshader://skintools.i2
    print("\nResolving relative import...")
    try:
        resolved = import_file.resolveRelativeTo(container)
        print(f"✓ Resolved to: '{resolved}'")
        
        # Verify the result
        expected = "orkshader://skintools.i2"
        actual = str(resolved)
        if actual == expected:
            print(f"✓ PASS: Got expected result '{expected}'")
        else:
            print(f"✗ FAIL: Expected '{expected}', got '{actual}'")
            return False
            
    except Exception as e:
        print(f"✗ ERROR in resolveRelativeTo: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    # Case 2: Test with deeper path
    print("\n\nCase 2: Deeper orkshader path")
    container2 = Path("orkshader://subdir/shader.fxv2")
    import_file2 = Path("include.i2")
    
    print(f"Container path: '{container2}'")
    print(f"Import path: '{import_file2}'")
    
    try:
        resolved2 = import_file2.resolveRelativeTo(container2)
        print(f"✓ Resolved to: '{resolved2}'")
        
        expected2 = "orkshader://subdir/include.i2"
        actual2 = str(resolved2)
        if actual2 == expected2:
            print(f"✓ PASS: Got expected result '{expected2}'")
        else:
            print(f"✗ FAIL: Expected '{expected2}', got '{actual2}'")
            return False
            
    except Exception as e:
        print(f"✗ ERROR in resolveRelativeTo: {e}")
        return False
    
    # Case 3: Absolute import should remain unchanged
    print("\n\nCase 3: Absolute import")
    container3 = Path("orkshader://basic.fxv2")
    import_file3 = Path("orkshader://mathtools.i2")
    
    print(f"Container path: '{container3}'")
    print(f"Import path: '{import_file3}' (already absolute)")
    
    try:
        resolved3 = import_file3.resolveRelativeTo(container3)
        print(f"✓ Resolved to: '{resolved3}'")
        
        expected3 = "orkshader://mathtools.i2"
        actual3 = str(resolved3)
        if actual3 == expected3:
            print(f"✓ PASS: Absolute path unchanged as expected")
        else:
            print(f"✗ FAIL: Expected '{expected3}', got '{actual3}'")
            return False
            
    except Exception as e:
        print(f"✗ ERROR in resolveRelativeTo: {e}")
        return False
    
    return True

def test_filesystem_paths():
    """Test with regular filesystem paths as a control"""
    print("\n=== Testing Filesystem Paths (Control) ===")
    
    container = Path("/Users/test/shaders/basic.fxv2")
    import_file = Path("include.i2")
    
    print(f"Container: '{container}'")
    print(f"Import: '{import_file}'")
    
    try:
        resolved = import_file.resolveRelativeTo(container)
        print(f"Resolved: '{resolved}'")
        
        expected = "/Users/test/shaders/include.i2"
        actual = str(resolved)
        if actual == expected:
            print(f"✓ PASS: Filesystem path resolution works")
            return True
        else:
            print(f"✗ FAIL: Expected '{expected}', got '{actual}'")
            return False
            
    except Exception as e:
        print(f"✗ ERROR: {e}")
        return False

if __name__ == "__main__":
    print("=" * 60)
    print("Path Import Resolution Unit Test")
    print("=" * 60)
    
    # Track which tests pass
    all_passed = True
    
    # Test filesystem paths first (should work)
    if not test_filesystem_paths():
        all_passed = False
        print("\n⚠️  Filesystem path test failed")
    
    # Test shader import resolution (the actual problem)
    if not test_shader_import_resolution():
        all_passed = False
        print("\n⚠️  Shader import resolution test failed")
    
    print("\n" + "=" * 60)
    if all_passed:
        print("✅ All tests PASSED")
    else:
        print("❌ Some tests FAILED")
    print("=" * 60)
    
    sys.exit(0 if all_passed else 1)