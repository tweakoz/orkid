#!/usr/bin/env ork.python

################################################################################
# Orkid Asset Catalog Upload Tool
# Uploads assets from catalog to configured remote locations using catalog APIs
################################################################################

import argparse
import os
import json
from pathlib import Path
from orkengine import core
from ork import assets as ork_assets
from obt import path as obt_path

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
        # Use the catalog's upload method
        receipt = catalog.upload(namespace_id)
        
        if receipt:
            print_upload_receipt(receipt)
            return receipt.success
        else:
            print(f"✗ Upload failed - no receipt returned")
            return False
            
    except Exception as e:
        print(f"✗ Upload error: {e}")
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
                print("  ✗ No receipt for this namespace")
                all_success = False
        
        return all_success
        
    except Exception as e:
        print(f"✗ Upload error: {e}")
        return False

def upload_single_asset(catalog, asset_id, dry_run=False):
    """Upload a single asset"""
   
    try:
        print(f"\nUploading asset '{asset_id}'...")
        
        # Get the specific asset entry
        entry = catalog.get_asset_info(asset_id)
        if not entry:
            print(f"✗ Asset not found: {asset_id}")
            return False
        
        # Get the namespace from the asset_id to determine destination
        namespace_id = asset_id.split('|')[0]
        
        # Get the merged config from the catalog
        config = catalog.merged_config
        if not config:
            print(f"✗ No configuration available")
            return False
        
        # Get the remote destination for this namespace
        destination_id = config.getRemoteLocationForNamespace(namespace_id)
        if not destination_id:
            print(f"✗ No upload destination configured for namespace: {namespace_id}")
            return False
        
        # Upload the individual asset using AssetEntry's upload method
        receipt = entry.upload(config, destination_id)
        print_upload_receipt(receipt, "  ")
        return receipt.success if receipt else False
            
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
    
    # Initialize core FIRST
    core.coreappinit()
    
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
            print(f"✗ Asset ID must be in format 'namespace|asset_id'")
            success = False
        else:
            success = upload_single_asset(catalog, args.asset, args.dry_run)
            
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
            print(f"\n✗ Failed to save receipt: {e}")
    
    core.coreappexit()
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())