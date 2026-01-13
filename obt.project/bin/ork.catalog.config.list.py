#!/usr/bin/env ork.python

from orkengine import core
import os
from pathlib import Path
import obt.deco

deco = obt.deco.Deco()

app = core.Application.create(std_asset_catalog=True)
catalog = core.AssetCatalog.instance

print(deco.yellow("Configuration files loaded:") + "\n")

manifest_dirs_env = os.environ.get("ORKID_ASSET_MANIFEST_DIRS", "")
if manifest_dirs_env:
    manifest_dirs = manifest_dirs_env.split(':')

    config_count = 0
    for manifest_dir in manifest_dirs:
        config_file = Path(manifest_dir) / "config.json"
        if config_file.exists():
            print(deco.cyan(f"{config_file}"))
            config_count += 1

    print(f"\n{deco.key('Total:')} {deco.val(str(config_count))} config files")

    print(deco.yellow("\nConfiguration data topology:"))

    for manifest_dir in manifest_dirs:
        config_file = Path(manifest_dir) / "config.json"
        if config_file.exists():
            print(deco.magenta(f"\n{config_file}:"))
            try:
                import json
                with open(config_file, 'r') as f:
                    config_data = json.load(f)

                if 'namespaces' in config_data:
                    print(deco.key("  namespaces:"))
                    for ns_id, ns_config in sorted(config_data['namespaces'].items()):
                        print(deco.cyan(f"    {ns_id}:"))
                        if 'encryption_key' in ns_config:
                            print(deco.key(f"      encryption_key: ") + deco.red(f"{'*' * 8} (hidden)"))
                        if 'remote_location' in ns_config:
                            print(deco.key(f"      remote_location: ") + deco.val(ns_config['remote_location']))

                if 'namespace_keys' in config_data:
                    print(deco.key("  namespace_keys:"))
                    for ns_id, key in sorted(config_data['namespace_keys'].items()):
                        print(deco.cyan(f"    {ns_id}: ") + deco.red(f"{'*' * 8} (hidden)"))

                if 'locations' in config_data:
                    print(deco.key("  locations:"))
                    for loc_id, loc_config in sorted(config_data['locations'].items()):
                        if isinstance(loc_config, dict):
                            print(deco.cyan(f"    {loc_id}:"))
                            for k, v in sorted(loc_config.items()):
                                if k == 'api_key':
                                    print(deco.key(f"      {k}: ") + deco.red(f"{'*' * 8} (hidden)"))
                                else:
                                    print(deco.key(f"      {k}: ") + deco.val(str(v)))
                        else:
                            print(deco.cyan(f"    {loc_id}: ") + deco.val(str(loc_config)))

                if 'destinations' in config_data:
                    print(deco.key("  destinations:"))
                    for dest_id, dest_path in sorted(config_data['destinations'].items()):
                        print(deco.cyan(f"    {dest_id}: ") + deco.val(dest_path))

            except Exception as e:
                print(deco.red(f"  Error reading config: {e}"))
else:
    print(deco.red("ORKID_ASSET_MANIFEST_DIRS not set"))
