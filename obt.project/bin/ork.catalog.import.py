#!/usr/bin/env ork.python
"""
Import assets into the Orkid asset catalog.

Usage:
    ork.catalog.import.py -c import.json [-u] [-n] [-l] [-v]
    ork.catalog.import.py -N ns -s DIR -L LOC -m FILE -a ID PATTERN ...
"""

import sys
import argparse
from orkengine import core
from ork import catalog_import

def build_parser():
    parser = argparse.ArgumentParser(
        description='Import assets into Orkid asset catalog',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Import from config file
  ork.catalog.import.py -c singularity.json -u

  # List what would be imported
  ork.catalog.import.py -c envmaps.json -l

  # Dry run
  ork.catalog.import.py -c models.json -n

  # CLI with explicit assets
  ork.catalog.import.py -N singularity -s "<stage>/share" -L "<stage>/share" \\
      -m '${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/singularity.json' \\
      -a casiocz "singularity/casioCZ/*" \\
      -a irs "singularity/IRs/*"

  # Export CLI args to config file
  ork.catalog.import.py -N singularity -s "<stage>/share" -L "<stage>/share" \\
      -m singularity.json -a casiocz "singularity/casioCZ/*" -e singularity_import.json
"""
    )

    parser.add_argument('-c', '--config',
                        help='Import config JSON file')

    parser.add_argument('-N', '--namespace',
                        help='Asset namespace')
    parser.add_argument('-s', '--source-dir',
                        help='Source directory (supports <stage>, ${VAR})')
    parser.add_argument('-L', '--local-loc',
                        help='Local deployment location (supports <stage>, ${VAR})')
    parser.add_argument('-m', '--manifest',
                        help='Manifest file path (supports ${VAR})')
    parser.add_argument('-k', '--key',
                        help='Encryption key or ${VAR} reference')
    parser.add_argument('-p', '--platforms', nargs='+', default=['mac', 'linux'],
                        help='Target platforms (default: mac linux)')

    parser.add_argument('-a', '--asset', nargs=2, action='append',
                        metavar=('ID', 'PATTERN'),
                        help='Asset definition: -a casiocz "singularity/casioCZ/*"')
    parser.add_argument('-x', '--exclude', nargs='+',
                        help='Exclude patterns')

    parser.add_argument('-u', '--upload', action='store_true',
                        help='Upload after packaging')
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help='Show what would be done (no-op)')
    parser.add_argument('-l', '--list', action='store_true',
                        help='List matching files')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')

    parser.add_argument('-e', '--export-config', metavar='FILE',
                        help='Export args as config JSON (no import executed)')

    return parser

def main():
    parser = build_parser()
    args = parser.parse_args()

    if len(sys.argv) == 1:
        parser.print_help()
        return 0

    # Initialize Application
    app = core.Application.create(std_asset_catalog=True)

    if args.config:
        config = catalog_import.load_config(args.config)
    else:
        required_for_cli = [args.namespace, args.source_dir, args.local_loc, args.manifest]

        if not args.export_config:
            if not all(required_for_cli):
                parser.error("Without --config, requires: --namespace (-N), --source-dir (-s), --local-loc (-L), --manifest (-m)")
            if not args.asset:
                parser.error("Without --config, requires at least one --asset (-a)")

        config = catalog_import.config_from_args(args)

    if args.export_config:
        catalog_import.export_config(config, args.export_config)
        print(f"Config exported to: {args.export_config}")
        return 0

    result = catalog_import.run_import(
        config,
        upload=args.upload,
        dry_run=args.dry_run,
        list_only=args.list,
        verbose=args.verbose
    )

    return 0 if result.failed_count == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
