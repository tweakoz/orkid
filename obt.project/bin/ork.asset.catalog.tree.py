#!/usr/bin/env ork.python 

from orkengine import core
from ork import path as ork_path
import os
from collections import defaultdict
import obt.deco

deco = obt.deco.Deco()

def build_tree(catalog):
    """Build a tree structure of namespaces and assets"""
    tree = {}
    
    # Get all namespaces (including empty ones)
    try:
        all_namespaces = catalog.list_namespaces("*")
    except:
        all_namespaces = []
    
    # Get all assets
    all_assets = catalog.list_assets("*")
    
    # Build tree from assets (simpler approach)
    for fqid in all_assets:
        if '|' in fqid:
            namespace, asset_id = fqid.rsplit('|', 1)
            
            # Ensure namespace exists in tree
            if namespace not in tree:
                tree[namespace] = {'assets': [], 'children': {}}
            
            # Add asset
            tree[namespace]['assets'].append(asset_id)
    
    # Add any empty namespaces from the namespace list
    for namespace in all_namespaces:
        if namespace not in tree:
            tree[namespace] = {'assets': [], 'children': {}}
    
    return tree

def print_flat_tree(tree):
    """Print the tree structure (flat namespace view)"""
    items = sorted(tree.items())
    
    for i, (namespace, node) in enumerate(items):
        is_last_namespace = (i == len(items) - 1)
        
        # Print the namespace
        print(deco.magenta(f"{namespace}/"))
        
        # Print assets in this namespace
        if isinstance(node, dict) and 'assets' in node:
            assets = sorted(node['assets'])
            for j, asset in enumerate(assets):
                is_last_asset = (j == len(assets) - 1)
                connector = "└── " if is_last_asset else "├── "
                print(deco.white(f"  {connector}") + deco.cyan(asset))

core.coreappinit()

# Create config space and catalog
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)

# Load from global manifests
core.AssetCatalog.loadFromGlobalManifests(catalog)

# Build and print the tree
tree = build_tree(catalog)

if not tree:
    print(deco.red("No namespaces or assets found in catalog."))
    print(deco.yellow("\nMake sure ORKID_ASSET_MANIFEST_DIRS is set and points to directories"))
    print(deco.yellow("containing config.json and manifest JSON files."))
else:
    print(deco.yellow("Asset Catalog Tree:") + "\n")
    print_flat_tree(tree)
    
    # Print summary
    all_assets = catalog.list_assets("*")
    try:
        all_namespaces = catalog.list_namespaces("*")
        namespace_count = len(all_namespaces)
    except:
        namespace_count = len(tree)
    print(f"\n{deco.key('Total:')} {deco.val(str(namespace_count))} namespaces, {deco.val(str(len(all_assets)))} assets")

core.coreappexit()