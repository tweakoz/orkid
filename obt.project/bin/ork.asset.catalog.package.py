#!/usr/bin/env ork.python

################################################################################
# Orkid Asset Catalog Package Tool
# Packages (encrypts/compresses) assets and adds them to catalog manifests
################################################################################

import argparse
import sys
from ork.assets import package_asset


def main():
    parser = argparse.ArgumentParser(description='Package assets (encrypt/compress) and add them to Orkid catalog manifests')
    
    # Manifest args
    parser.add_argument('--namespace', required=True, help='Asset namespace')
    parser.add_argument('--output', '-o', required=True, help='Output manifest file')
    parser.add_argument('--version', default='1.0.0', help='Manifest version')
    parser.add_argument('--catalog-file', help='Catalog JSON file to load/update')
    
    # Asset args
    parser.add_argument('--asset-id', required=True, help='Asset identifier')
    parser.add_argument('--priority', type=int, default=100, help='Priority (lower wins)')
    parser.add_argument('--remote-loc', required=True, help='Remote location template (e.g., <orkid_cdn>)')
    parser.add_argument('--local-loc', required=True, help='Local location template')
    
    # Asset type + source (mutually exclusive)
    asset_group = parser.add_mutually_exclusive_group(required=True)
    asset_group.add_argument('--asset-pak', metavar='DIR', help='Create asset_pak from directory')
    asset_group.add_argument('--asset', metavar='FILE', help='Create asset from file')
    
    # Optional args
    parser.add_argument('--cache-dir', help='Cache directory for generated assets')
    parser.add_argument('--merge', type=lambda x: x.lower() == 'true', help='For asset_pak: merge with existing')
    parser.add_argument('--dependencies', nargs='*', help='Dependencies (namespace|asset_id)')
    parser.add_argument('--key', help='Encryption key (alternative to environment variable)')
    parser.add_argument('--filename', help='Override filename in manifest')
    parser.add_argument('--strip-leading', action='store_true', help='Strip base directory from tar archive paths')
    parser.add_argument('--filter', help='Wildcard pattern to filter files (only for --asset-pak)')
    parser.add_argument('--platforms', nargs='+', help='Target platforms (default: current platform)')
    
    args = parser.parse_args()
    
    # Call the package_asset function with named arguments
    try:
        result = package_asset(
            namespace=args.namespace,
            output=args.output,
            asset_id=args.asset_id,
            priority=args.priority,
            remote_loc=args.remote_loc,
            local_loc=args.local_loc,
            version=args.version,
            catalog_file=args.catalog_file,
            asset_pak=args.asset_pak,
            asset=args.asset,
            cache_dir=args.cache_dir,
            merge=args.merge,
            dependencies=args.dependencies,
            key=args.key,
            filename=args.filename,
            strip_leading=args.strip_leading,
            filter=args.filter,
            platforms=args.platforms
        )
        return 0
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    exit(main())