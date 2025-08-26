#!/usr/bin/env python3

"""
Unit test for Path::resolveRelativeTo functionality
Tests the shader import path resolution logic
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from orkengine.core import Path
from ork import path as ork_path

def test_basic_path_operations():
    """Test basic path operations"""
    print("\n=== Testing Basic Path Operations ===")
    
    # Test absolute path detection
    abs_path = Path("/Users/test/test.txt")
    print(f"Path: {abs_path}")
    print(f"  isAbsolute: {abs_path.isAbsolute()}")
    print(f"  isRelative: {abs_path.isRelative()}")
    assert abs_path.isAbsolute() == True
    assert abs_path.isRelative() == False
    
    # Test relative path detection
    rel_path = Path("test.txt")
    print(f"Path: {rel_path}")
    print(f"  isAbsolute: {rel_path.isAbsolute()}")
    print(f"  isRelative: {rel_path.isRelative()}")
    assert rel_path.isAbsolute() == False
    assert rel_path.isRelative() == True
    
    # Test URL-based path
    url_path = Path("orkshader://test.txt")
    print(f"Path: {url_path}")
    print(f"  isAbsolute: {url_path.isAbsolute()}")
    print(f"  isRelative: {url_path.isRelative()}")
    print(f"  hasUrlBase: {url_path.hasUrlBase()}")
    assert url_path.isAbsolute() == True
    assert url_path.isRelative() == False

def test_resolve_relative_to():
    """Test Path::resolveRelativeTo functionality"""
    print("\n=== Testing resolveRelativeTo ===")
    
    # Test 1: Relative import from shader file
    print("\nTest 1: Relative shader import")
    container = Path("orkshader://basic.fxv2")
    import_file = Path("skintools.i2")
    
    print(f"Container: {container}")
    print(f"Import: {import_file}")
    
    resolved = import_file.resolveRelativeTo(container)
    print(f"Resolved: {resolved}")
    
    # Should resolve to orkshader://skintools.i2
    expected = "orkshader://skintools.i2"
    assert str(resolved) == expected, f"Expected {expected}, got {resolved}"
    print(f"✓ Correctly resolved to {resolved}")
    
    # Test 2: Absolute import (should remain unchanged)
    print("\nTest 2: Absolute shader import")
    container = Path("orkshader://basic.fxv2")
    import_file = Path("orkshader://mathtools.i2")
    
    print(f"Container: {container}")
    print(f"Import: {import_file}")
    
    resolved = import_file.resolveRelativeTo(container)
    print(f"Resolved: {resolved}")
    
    # Should remain orkshader://mathtools.i2
    expected = "orkshader://mathtools.i2"
    assert str(resolved) == expected, f"Expected {expected}, got {resolved}"
    print(f"✓ Correctly kept absolute path {resolved}")
    
    # Test 3: File system paths
    print("\nTest 3: File system paths")
    container = Path(f"{ork_path.root}/shaders/basic.fxv2")
    import_file = Path("skintools.i2")
    
    print(f"Container: {container}")
    print(f"Import: {import_file}")
    
    resolved = import_file.resolveRelativeTo(container)
    print(f"Resolved: {resolved}")
    
    # Should resolve to f{ork_path.root}/shaders/skintools.i2
    expected = f"{ork_path.root}/shaders/skintools.i2"
    assert str(resolved) == expected, f"Expected {expected}, got {resolved}"
    print(f"✓ Correctly resolved to {resolved}")
    
    # Test 4: Real-world case from pixelart.py
    print("\nTest 4: Real-world shader import case")
    container = Path(f"{ork_path.root}/ork.data/platform_lev2/shaders/fxv2/basic.fxv2")
    import_file = Path("skintools.i2")
    
    print(f"Container: {container}")
    print(f"Import: {import_file}")
    
    resolved = import_file.resolveRelativeTo(container)
    print(f"Resolved: {resolved}")
    
    # Should resolve to full path with skintools.i2
    expected = f"{ork_path.root}/ork.data/platform_lev2/shaders/fxv2/skintools.i2"
    assert str(resolved) == expected, f"Expected {expected}, got {resolved}"
    print(f"✓ Correctly resolved to {resolved}")

def test_to_absolute_folder():
    """Test toAbsoluteFolder functionality"""
    print("\n=== Testing toAbsoluteFolder ===")
    
    # Test with file path
    file_path = Path(f"{ork_path.root}/test.txt")
    folder = file_path.toAbsoluteFolder()
    print(f"File: {file_path}")
    print(f"Folder: {folder}")
    
    # Test with URL path
    url_path = Path("orkshader://test.fxv2")
    folder = url_path.toAbsoluteFolder()
    print(f"URL File: {url_path}")
    print(f"URL Folder: {folder}")

if __name__ == "__main__":
    print("=" * 60)
    print("Path Resolution Unit Tests")
    print("=" * 60)
    
    try:
        test_basic_path_operations()
        test_resolve_relative_to()
        test_to_absolute_folder()
        
        print("\n" + "=" * 60)
        print("All tests passed! ✓")
        print("=" * 60)
        
    except AssertionError as e:
        print(f"\n❌ Test failed: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)