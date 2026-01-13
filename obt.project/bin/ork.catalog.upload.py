#!/usr/bin/env ork.python

################################################################################
# Orkid Asset Catalog Upload Tool
# Uploads assets from catalog to configured remote locations using catalog APIs
################################################################################

import argparse, os, json, time
from pathlib import Path
from orkengine import core
from ork import assets as ork_assets
from obt import path as obt_path

logger = core.Logger.instance()
logchan = logger.configureChannel("UPLOADER",core.vec3(1,1,0),True)

def print_upload_receipt(receipt, indent=""):
    """Pretty print an upload receipt"""
    if not receipt:
        print(f"{indent}No receipt returned")
        return

    print(f"{indent}Upload Receipt:")
    print(f"{indent}  Success: {receipt.success}")
    print(f"{indent}  Total files: {receipt.total_files}")
    print(f"{indent}  Successful files: {receipt.successful_files}")
    print(f"{indent}  Failed files: {receipt.failed_files}")
    print(f"{indent}  Bytes uploaded: {receipt.bytes_uploaded:,}")
    print(f"{indent}  Duration: {receipt.total_duration:.2f}s")

    if hasattr(receipt, 'status_message') and receipt.status_message:
        print(f"{indent}  Status: {receipt.status_message}")
    if hasattr(receipt, 'destination') and receipt.destination:
        print(f"{indent}  Destination: {receipt.destination}")

    if hasattr(receipt, 'errors') and receipt.errors:
        print(f"{indent}  Errors:")
        for error in receipt.errors:
            print(f"{indent}    - {error}")

def upload_namespace(catalog, namespace_id, dry_run=False):
    """Upload all assets in a namespace using catalog upload API"""

    print(f"\nUploading namespace '{namespace_id}'...")

    if dry_run:
        print("[DRY RUN MODE - No actual uploads will occur]")
        # TODO: Add dry run support to the API

    try:
        # Use the catalog's uploadNamespace method
        receipt = catalog.uploadNamespace(namespace_id)

        if receipt:
            print_upload_receipt(receipt)
            return receipt.success
        else:
            logchan.error(f"✗ Upload failed - no receipt returned")
            return False

    except Exception as e:
        logchan.error(f"✗ Upload error: {e}")
        return False

def upload_all_namespaces(catalog, dry_run=False):
    """Upload all namespaces using catalog uploadAllNamespaces API"""

    print("\nUploading all namespaces...")

    if dry_run:
        print("[DRY RUN MODE - No actual uploads will occur]")
        # TODO: Add dry run support to the API

    try:
        # Use the catalog's uploadAllNamespaces method
        results = catalog.uploadAllNamespaces()

        if not results:
            print("✗ No results returned from uploadAllNamespaces")
            return False

        all_success = True
        for namespace_id, receipt in results.items():
            print(f"\n{'='*60}")
            print(f"Namespace: {namespace_id}")
            print(f"{'='*60}")

            if receipt:
                print_upload_receipt(receipt, "  ")
                if not receipt.success:
                    all_success = False
            else:
                logchan.error("  ✗ No receipt for this namespace")
                all_success = False

        return all_success

    except Exception as e:
        logchan.error(f"✗ Upload error: {e}")
        return False

def upload_single_asset(app, catalog, asset_id, dry_run=False):
    """Upload a single asset using mainThreadLoop for async processing"""

    print(f"\nUploading asset '{asset_id}'...")

    if dry_run:
        print("[DRY RUN MODE - No actual uploads will occur]")
        return False

    # State for async upload
    state = {
        'completed': False,
        'receipt': None
    }

    def on_completed(r):
        state['receipt'] = r
        state['completed'] = True
        app.requestExit()  # Signal main loop to exit

    try:
        # Start async upload
        receipt = catalog.uploadAsset(asset_id, on_completed=on_completed)

        # Run main loop until upload completes or Ctrl-C
        def on_iter():
            pass  # Just let the loop run

        app.mainThreadLoop(on_iter=on_iter)

        # Check result
        if state['receipt']:
            print_upload_receipt(state['receipt'], "  ")
            return state['receipt'].success
        elif receipt:
            print_upload_receipt(receipt, "  ")
            return receipt.success
        else:
            logchan.error(f"✗ Upload failed - no receipt returned")
            return False

    except Exception as e:
        print(f"✗ Upload error: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Upload assets from Orkid catalog to remote locations')

    # What to upload
    upload_group = parser.add_mutually_exclusive_group(required=True)
    upload_group.add_argument('--namespace', help='Upload all assets in namespace')
    upload_group.add_argument('--asset', help='Upload single asset (namespace|asset_id)')
    upload_group.add_argument('--all', action='store_true', help='Upload all namespaces')

    # Upload options
    parser.add_argument('--dry-run', action='store_true',
                       help='Show what would be uploaded without actually uploading')
    parser.add_argument('--manifest-dir',
                       help='Additional manifest directory to load')
    parser.add_argument('--receipt-file',
                       help='Save upload receipt to JSON file')

    args = parser.parse_args()

    # Initialize Application with catalog
    app = core.Application.create(std_asset_catalog=True)

    # Create config space and catalog
    cfgspc, catalog = ork_assets.default_cfg_and_catalog()

    # Load additional manifest if specified
    if args.manifest_dir:
        catalog.load_manifests_from_path(args.manifest_dir)

    # Perform upload based on arguments
    success = False
    receipt = None

    if args.namespace:
        # Upload single namespace
        success = upload_namespace(catalog, args.namespace, args.dry_run)

    elif args.asset:
        # Upload single asset
        if '|' not in args.asset:
            logchan.error(f"✗ Asset ID must be in format 'namespace|asset_id'")
            success = False
        else:
            success = upload_single_asset(app, catalog, args.asset, args.dry_run)

    elif args.all:
        # Upload all namespaces
        success = upload_all_namespaces(catalog, args.dry_run)

    # Save receipt if requested
    if args.receipt_file and receipt:
        try:
            with open(args.receipt_file, 'w') as f:
                # TODO: Need to serialize receipt to JSON
                json.dump({"message": "Receipt serialization not yet implemented"}, f, indent=2)
            print(f"\nReceipt saved to: {args.receipt_file}")
        except Exception as e:
            logchan.error(f"\n✗ Failed to save receipt: {e}")

    return 0 if success else 1

if __name__ == "__main__":
    exit(main())
