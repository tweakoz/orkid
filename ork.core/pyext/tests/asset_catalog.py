#!/usr/bin/env ork.python

from orkengine import core
import tempfile
import json
import os

# Initialize core
core.coreappinit()

# Test AssetManifest
print("Testing AssetManifest...")

# Create a test manifest JSON
# This references the same asset as SINGUL_PAK_STD but with a different ID
test_manifest = {
    "namespace": "test",
    "version": "1.0.0",
    "assets": {
        "test_std": {
            "type": "asset_pak",
            "priority": 100,
            "dst_loc": "<temp>/test_singularity",
            "src_loc": "<orkid_std>",
            "filename": "SINGUL_PAK_STD",
            "md5": "43be06d16db50d01be0187c2c7bcf831",
            "dependencies": {}
        }
    }
}

# Save to temp directory (fetcher will look for JSON files in manifest directories)
manifest_dir = tempfile.mkdtemp()
temp_manifest = os.path.join(manifest_dir, "test_manifest.json")
with open(temp_manifest, 'w') as f:
    json.dump(test_manifest, f)

# Test loading
manifest = core.AssetManifest.load_from_file(core.Path(temp_manifest))
if not manifest:
    print("Failed to load manifest")
    sys.exit(1)

# Test AssetConfig
print("\nTesting AssetConfig...")

# Create a test config JSON with locations
test_config = {
    "namespace_keys": {
        "test": "singularity_rulez"  # Use same key as singularity for testing
    },
    "locations": {
        "orkid_std": "https://www.tweakoz.com/resources"
    },
    "destinations": {
        "temp": "<temp>"
    }
}

# Save config to same directory as manifest
config_file = os.path.join(manifest_dir, "test_config.json")
with open(config_file, 'w') as f:
    json.dump(test_config, f)

# Load config from directory
config = core.AssetConfig.load_from_directory(core.Path(manifest_dir))
if not config:
    print("Failed to load config from directory")
    sys.exit(1)

# Test AssetFetcher
print("\nTesting AssetFetcher...")

# Create fetchers silently

# Create fetchers for testing
orkid_manifest_dir = core.Path(os.environ.get('ORKID_WORKSPACE_DIR', '')) / "ork.data" / "asset_manifests"
fetcher_orkid = core.AssetFetcher()
fetcher_orkid.set_manifest_directories([orkid_manifest_dir])
fetcher_orkid.reload()

# Create test fetcher with our custom directory
fetcher = core.AssetFetcher()
fetcher.set_manifest_directories([core.Path(manifest_dir)])
fetcher.reload()

# Test callbacks
def on_progress(asset_id, downloaded, total):
    # Print progress on same line using carriage return
    progress_msg = f"Progress: {asset_id} - {downloaded}/{total} ({downloaded/1024/1024:.1f}MB/{total/1024/1024:.1f}MB)"
    # Pad with spaces to clear any previous longer text
    print(f"\r{progress_msg:<80}", end='', flush=True)

def on_complete(asset_id, success):
    # Clear the progress line and print completion on new line
    print(f"\r{' ':<80}\r", end='')  # Clear the line
    print(f"Complete: {asset_id} - {'Success' if success else 'Failed'}")

fetcher.on_asset_progress(on_progress)
fetcher.on_asset_complete(on_complete)

# Test downloading an asset
print("\nTesting asset download...")

# Try with the Orkid fetcher and real asset
print("Attempting to fetch singularity.std asset with Orkid fetcher...")
fetch_count = fetcher_orkid.fetch_pak("singularity.std")
print(f"Fetched {fetch_count} assets")

# Try fetching by full ID with our test fetcher
print("\nAttempting to fetch test.test_std asset...")
fetch_count = fetcher.fetch_pak("test.test_std")
print(f"Fetched {fetch_count} assets")

# Also try fetching by namespace only
print("\nAttempting to fetch all 'test' namespace assets...")
fetch_count = fetcher.fetch_pak("test")
print(f"Fetched {fetch_count} assets")

# Clean up
import shutil
shutil.rmtree(manifest_dir)

print("\nAsset catalog bindings test complete!")