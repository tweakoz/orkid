#!/usr/bin/env python3

"""
Simple test to debug Path issues
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

print("Importing orkengine.core...")
from orkengine.core import Path, FileEnv

# Register the orkshader:// context like FxShader::RegisterLoaders does
print("\nRegistering orkshader:// context...")
lev2_ctx = FileEnv.contextForUriProto("lev2://")
if lev2_ctx:
    shader_base = Path("platform_lev2/shaders/fxv2")
    shader_path = lev2_ctx.filesystemBaseAbs / shader_base
    print(f"  Shader path: {shader_path}")
    
    # Create the orkshader:// context
    shader_ctx = FileEnv.createContextForUriBase("orkshader://", shader_path)
    if shader_ctx:
        shader_ctx.setFilesystemBaseEnable(True)
        print("  orkshader:// context registered successfully")
    else:
        print("  ERROR: Failed to create orkshader:// context")
else:
    print("  WARNING: lev2:// context not found, trying to register manually...")
    # Register lev2:// first if needed
    import orkengine.lev2 as lev2
    lev2.GfxEnv.staticInit()
    lev2_ctx = FileEnv.contextForUriProto("lev2://")
    if lev2_ctx:
        shader_base = Path("platform_lev2/shaders/fxv2")
        shader_path = lev2_ctx.filesystemBaseAbs / shader_base
        shader_ctx = FileEnv.createContextForUriBase("orkshader://", shader_path)
        if shader_ctx:
            shader_ctx.setFilesystemBaseEnable(True)
            print("  orkshader:// context registered successfully")

print("\n1. Creating absolute path...")
abs_path = Path("/Users/test/test.txt")
print(f"  Created: {abs_path}")

print("\n2. Testing isAbsolute...")
is_abs = abs_path.isAbsolute()
print(f"  isAbsolute: {is_abs}")

print("\n3. Testing isRelative...")
is_rel = abs_path.isRelative()
print(f"  isRelative: {is_rel}")

print("\n4. Creating relative path...")
rel_path = Path("test.txt")
print(f"  Created: {rel_path}")
print(f"  isAbsolute: {rel_path.isAbsolute()}")
print(f"  isRelative: {rel_path.isRelative()}")

print("\n5. Testing hasUrlBase...")
url_path = Path("orkshader://test.txt")
print(f"  Created: {url_path}")
print(f"  hasUrlBase: {url_path.hasUrlBase()}")

print("\n6. Testing toAbsoluteFolder...")
container = Path("orkshader://basic.fxv2")
print(f"  Container: {container}")
print(f"  Container isAbsolute: {container.isAbsolute()}")

# This might crash
print("\n7. Getting absolute folder...")
try:
    folder = container.toAbsoluteFolder()
    print(f"  Folder: {folder}")
except Exception as e:
    print(f"  ERROR: {e}")

print("\n8. Testing resolveRelativeTo...")
import_file = Path("skintools.i2")
print(f"  Import: {import_file}")

try:
    resolved = import_file.resolveRelativeTo(container)
    print(f"  Resolved: {resolved}")
except Exception as e:
    print(f"  ERROR: {e}")

print("\nTest completed!")