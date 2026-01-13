#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import os
from collections import defaultdict
import obt.deco

deco = obt.deco.Deco()

def format_size(bytes_size):
    if bytes_size == 0:
        return "0 B"
    units = ['B', 'KB', 'MB', 'GB', 'TB']
    unit_index = 0
    size = float(bytes_size)
    while size >= 1024.0 and unit_index < len(units) - 1:
        size /= 1024.0
        unit_index += 1
    if unit_index == 0:
        return f"{int(size)} {units[unit_index]}"
    else:
        return f"{size:.1f} {units[unit_index]}"

def build_tree(catalog):
    tree = {}
    try:
        all_namespaces = catalog.list_namespaces("*")
    except:
        all_namespaces = []

    all_assets = catalog.list_assets("*")

    for fqid in all_assets:
        if '|' in fqid:
            namespace, asset_id = fqid.rsplit('|', 1)
            if namespace not in tree:
                tree[namespace] = {'assets': [], 'children': {}}
            asset_entry = catalog.findAssetEntry(fqid)
            asset_data = {'id': asset_id, 'entry': asset_entry}
            tree[namespace]['assets'].append(asset_data)

    for namespace in all_namespaces:
        if namespace not in tree:
            tree[namespace] = {'assets': [], 'children': {}}

    return tree

def print_flat_tree(tree):
    items = sorted(tree.items())
    for i, (namespace, node) in enumerate(items):
        print(deco.magenta(f"{namespace}/"))
        if isinstance(node, dict) and 'assets' in node:
            assets = sorted(node['assets'], key=lambda x: x['id'] if isinstance(x, dict) else x)
            for j, asset in enumerate(assets):
                is_last_asset = (j == len(assets) - 1)
                connector = "└── " if is_last_asset else "├── "
                if isinstance(asset, dict):
                    asset_id = asset['id']
                    asset_entry = asset['entry']
                    size_str = ""
                    if asset_entry and hasattr(asset_entry, 'archive_size'):
                        size = asset_entry.archive_size
                        if size > 0:
                            formatted_size = format_size(size)
                            is_local = False
                            if hasattr(asset_entry, 'resolved_local_path'):
                                resolved_path = asset_entry.resolved_local_path
                                if resolved_path:
                                    is_local = os.path.isdir(resolved_path) or os.path.isfile(resolved_path)
                            if is_local:
                                size_str = " " + deco.green(f"[{formatted_size}]")
                            else:
                                size_str = " " + deco.orange(f"[{formatted_size}]")
                    print(deco.white(f"  {connector}") + deco.cyan(asset_id) + size_str)
                else:
                    print(deco.white(f"  {connector}") + deco.cyan(asset))

app = core.Application.create(std_asset_catalog=True)
catalog = core.AssetCatalog.instance

tree = build_tree(catalog)

if not tree:
    print(deco.red("No namespaces or assets found in catalog."))
    print(deco.yellow("\nMake sure ORKID_ASSET_MANIFEST_DIRS is set and points to directories"))
    print(deco.yellow("containing config.json and manifest JSON files."))
else:
    print(deco.yellow("Asset Catalog Tree:") + "\n")
    print_flat_tree(tree)

    all_assets = catalog.list_assets("*")
    try:
        all_namespaces = catalog.list_namespaces("*")
        namespace_count = len(all_namespaces)
    except:
        namespace_count = len(tree)
    print(f"\n{deco.key('Total:')} {deco.val(str(namespace_count))} namespaces, {deco.val(str(len(all_assets)))} assets")
