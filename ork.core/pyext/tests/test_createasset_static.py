#!/usr/bin/env ork.python

import os, sys
import json
from pathlib import Path
from orkengine.core import *

# Test the static factory pattern for createAsset
print("Testing AssetManifest.createAsset static factory pattern...")

# Create a catalog and manifest
catalog = AssetCatalog()
manifest = AssetCatalog.createManifest(
    catalog,
    "test_manifest",
    "1.0.0", 
    "test_namespace",
    Path("/tmp/test_manifest.json")
)

# Create test file
test_file = Path("/tmp/test_asset.txt")
test_file.write_text("test content")

# Create asset using the manifest instance method (which internally calls static)
asset = manifest.createAsset(
    id="test_asset",
    priority=100,
    type="asset",
    remote="https://cdn.example.com/",
    local="/tmp",
    filename="test_asset.txt",
    platforms=["darwin", "linux"],
    dependencies=["dep1", "dep2"]
)

print(f"✓ Created asset: {asset}")
print(f"  - ID: {asset.id}")
print(f"  - Namespace: {asset.namespace_id}")
print(f"  - Content hash: {asset.content_hash}")
print(f"  - Storage hash: {asset.storage_hash}")
print(f"  - Size: {asset.size}")

# Verify parent reference was set
if hasattr(asset, '_parent_manifest'):
    print("✓ Parent manifest reference is set (internal)")
else:
    print("✗ Parent manifest reference NOT set")

# Test repackage (should now have access to parent)
try:
    asset.repackage()
    print("✓ Repackage called successfully")
    print(f"  - Updated content hash: {asset.content_hash}")
    print(f"  - Updated storage hash: {asset.storage_hash}")
except Exception as e:
    print(f"✗ Repackage failed: {e}")

print("\n✅ Static factory pattern test complete!")