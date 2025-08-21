#!/usr/bin/env python3

"""
Test if orkshader:// context is registered and working
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

print("Testing orkshader:// context registration...")

# Import the graphics module which should register contexts
print("\n1. Importing orkengine.lev2...")
try:
    import orkengine.lev2 as lev2
    print("   ✓ Imported lev2")
except Exception as e:
    print(f"   ✗ Failed to import lev2: {e}")
    sys.exit(1)

# Try to initialize graphics environment
print("\n2. Initializing graphics environment...")
try:
    lev2.GfxEnv.staticInit()
    print("   ✓ GfxEnv initialized")
except Exception as e:
    print(f"   ✗ Failed to initialize GfxEnv: {e}")

# Now test path operations with orkshader://
print("\n3. Testing orkshader:// path operations...")
from orkengine.core import Path

test_path = Path("orkshader://basic.fxv2")
print(f"   Test path: {test_path}")
print(f"   isAbsolute: {test_path.isAbsolute()}")
print(f"   hasUrlBase: {test_path.hasUrlBase()}")

# This will likely crash if context isn't registered
print("\n4. Testing toAbsoluteFolder on orkshader:// path...")
try:
    abs_folder = test_path.toAbsoluteFolder()
    print(f"   ✓ toAbsoluteFolder succeeded: {abs_folder}")
except Exception as e:
    print(f"   ✗ toAbsoluteFolder failed: {e}")
    print("   This means orkshader:// context is not registered!")
    
print("\n5. Testing path resolution with orkshader://...")
import_path = Path("skintools.i2")
try:
    resolved = import_path.resolveRelativeTo(test_path)
    print(f"   ✓ Resolved '{import_path}' to '{resolved}'")
    
    # Now try toAbsoluteFolder on the resolved path
    print("\n6. Testing toAbsoluteFolder on resolved path...")
    try:
        abs_resolved = resolved.toAbsoluteFolder()
        print(f"   ✓ toAbsoluteFolder succeeded: {abs_resolved}")
    except Exception as e:
        print(f"   ✗ toAbsoluteFolder failed on resolved path: {e}")
        
except Exception as e:
    print(f"   ✗ Resolution failed: {e}")

print("\n" + "=" * 60)
print("Test completed")
print("=" * 60)