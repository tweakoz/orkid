#!/usr/bin/env ork.python
import os, argparse
from pathlib import Path
from obt import path as obt_path
from ork import path as ork_path
from ork import assets

parser = argparse.ArgumentParser(description="Package Singularity assets into asset_pak files.")
parser.add_argument("--upload", action="store_true",
                    help="Upload packaged assets to CDN after packaging")
args = parser.parse_args()
upload_enabled = args.upload

# Source directory (same as local_loc for symmetry)
source_dir = Path(obt_path.stage()) / "share"

# Output manifest
manifest_file = ork_path.data / "asset_manifests" / "singularity.json"
manifest_file.parent.mkdir(parents=True, exist_ok=True)

# Local location (deployment location after unpacking)
local_loc = "<stage>/share"

# Package each subdirectory as a separate asset_pak
subdirs = [
    ("casioCZ", "casiocz", "singularity/casioCZ/*"),
    ("IRs", "irs", "singularity/IRs/*"),
    ("kurzweil", "kurzweil", "singularity/kurzweil/*.krz"),
    ("midifiles", "midifiles", "singularity/midifiles/*.mid"),
    ("tx81z", "tx81z", "singularity/tx81z/*"),
    ("wavs", "wavs", "singularity/wavs/*")
]

upload_failed = 0
upload_success = 0

for src_name, asset_id, filter in subdirs:
    # Check if source exists
    src_path = source_dir / "singularity" / src_name
    if not src_path.exists():
        print(f"Skipping {src_name} - not found")
        continue

    print(f"\nPackaging {src_name} as {asset_id}...")
    
    # Use filter to include everything under tar_root
    # No strip_prefix - keep full paths in TAR
    result = assets.build_assetpak(
        namespace="singularity",
        output=str(manifest_file),
        asset_id=asset_id,
        source_dir=str(source_dir),  # Same as resolved local_loc
        filters=[filter],   # Use tar_root in filter
        priority=0,
        local_loc=local_loc,
        key="singularity_rulez",
        platforms=["mac","linux"],
        write_manifest=True
    )
    print(f"  Storage hash: {result['storage_hash']}")
    if upload_enabled:
        spc, catalog = assets.default_cfg_and_catalog()
        print("  Uploading assets to CDN...")
        fqid = f"singularity|{asset_id}"
        print(f"Uploading {fqid}...")
            
        try:
            # Upload this specific asset using the new uploadAsset method
            receipt = catalog.uploadAsset(fqid)
            if receipt and receipt.success:
                print(f"  ✓ Uploaded successfully")
                upload_success += 1
            else:
                print(f"  ✗ Upload failed")
                upload_failed += 1
                
        except Exception as e:
            print(f"  ✗ Upload error: {e}")
            upload_failed += 1
    

print("\nPackaging complete!")
print(f"Manifest saved to: {manifest_file}")
print(f"TAR files will be created from: {source_dir}")
print(f"Encrypted files are in: {obt_path.stage()}/assetcache/enc")

# if singularity/kurzweil/*.bin exists, package it as well to singularity_ext namespace

bin_assets = (source_dir / "singularity" / "kurzweil").glob("*.bin")
do_bin_assets_exist = len(list(bin_assets)) > 0
if do_bin_assets_exist:
  manifest_file = ork_path.data / "asset_manifests" / "singularity_ext.json"
  manifest_file.parent.mkdir(parents=True, exist_ok=True)
  result = assets.build_assetpak(
    namespace="singularity_ext",
    output=str(manifest_file),
    asset_id="bin_assets",
    source_dir=str(source_dir),  # Same as resolved local_loc
    filters=["singularity/kurzweil/*.bin"],   # Use tar_root in filter
    priority=0,
    local_loc=local_loc,
    key="singularity_rulez",
    platforms=["mac","linux"],
    write_manifest=True
  )
  print(f"  BIN: Storage hash: {result['storage_hash']}")
  print("\nBIN: Packaging complete!")
  print(f"BIN: Manifest saved to: {manifest_file}")
  print(f"BIN: TAR files will be created from: {source_dir}")
  print(f"BIN: Encrypted files are in: {obt_path.stage()}/assetcache/enc")
if upload_enabled:
    spc, catalog = assets.default_cfg_and_catalog()
    print("  Uploading assets to CDN...")
    fqid = f"singularity_ext|bin_assets"
    print(f"Uploading {fqid}...")
        
    try:
        # Upload this specific asset using the new uploadAsset method
        receipt = catalog.uploadAsset(fqid)
        if receipt and receipt.success:
            print(f"  ✓ Uploaded successfully")
            upload_success += 1
        else:
            print(f"  ✗ Upload failed")
            upload_failed += 1
            
    except Exception as e:
        print(f"  ✗ Upload error: {e}")
        upload_failed += 1
  