#!/usr/bin/env ork.python 

from orkengine import core
import os
from pathlib import Path
import obt.deco

deco = obt.deco.Deco()

core.coreappinit()

# Create config space and catalog
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)

# Load from global manifests
core.AssetCatalog.loadFromGlobalManifests(catalog)

print(deco.yellow("Manifests loaded:") + "\n")

# Get manifest directories
manifest_dirs_env = os.environ.get("ORKID_ASSET_MANIFEST_DIRS", "")
if manifest_dirs_env:
    manifest_dirs = manifest_dirs_env.split(':')
    
    manifest_count = 0
    for manifest_dir in manifest_dirs:
        if os.path.exists(manifest_dir):
            # List all .json files that are manifests
            manifest_files = []
            for file in Path(manifest_dir).glob("*.json"):
                if file.name != "config.json":  # Skip config files
                    manifest_files.append(file)
            
            if manifest_files:
                # Sanitize manifest directory path
                manifest_dir_path = core.Path(manifest_dir)
                sanitized_dir = manifest_dir_path.sanitized.toStdString()
                print(deco.magenta(f"{sanitized_dir}/"))
                for manifest_file in sorted(manifest_files):
                    print(deco.white(f"  └── ") + deco.cyan(manifest_file.name))
                    manifest_count += 1
    
    print(f"\n{deco.key('Total:')} {deco.val(str(manifest_count))} manifest files")
    
    # Also show which namespaces have manifests loaded
    namespaces_with_manifests = []
    try:
        all_namespaces = catalog.list_namespaces("*")
        for ns in all_namespaces:
            manifest = catalog.get_manifest(ns)
            if manifest:
                namespaces_with_manifests.append(ns)
    except:
        pass
    
    if namespaces_with_manifests:
        print(deco.yellow(f"\nNamespaces with manifests: ") + deco.cyan(', '.join(sorted(namespaces_with_manifests))))
else:
    print(deco.red("ORKID_ASSET_MANIFEST_DIRS not set"))

core.coreappexit()