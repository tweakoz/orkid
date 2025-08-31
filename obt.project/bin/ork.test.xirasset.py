#!/usr/bin/env ork.python
"""
Test for loading XIR (Radiance) assets from Python.
Loads a pre-filtered environment map and verifies the textures are created.
Supports short names, category names, or fully qualified asset IDs.
"""

import sys, time, argparse
from orkengine import core
from orkengine import lev2
from oka import envmaps

parser = argparse.ArgumentParser(description='XIR Radiance asset loading test')
parser.add_argument("source", type=str, nargs='?', 
                    help="Short name (e.g. 'desert4k'), category name to test first in category, or fully qualified ID (e.g. 'ork_envmaps|desert4k')")
parser.add_argument("--list", action="store_true",
                    help="List all available environment maps")
args = parser.parse_args()

def list_available():
    """List all available environment maps."""
    print("Available XIR Assets for Testing")
    print("=" * 60)
    
    categories = envmaps.all
    for cat_name, category in categories.items():
        print(f"\n{cat_name.upper()} ({len(category.assets)} maps):")
        if category.assets:
            sorted_assets = sorted(category.assets.keys())
            for asset_name in sorted_assets:
                print(f"  - {asset_name}")
    
    print("\n" + "=" * 60)
    print("Usage:")
    print("  oka.test.xirasset.py <short_name>    # Test single map")
    print("  oka.test.xirasset.py <category>      # Test first in category")
    print("  oka.test.xirasset.py --list          # Show this list")


def main():
    tokens = core.CrcStringProxy()
    
    # Handle --list option
    if args.list:
        list_available()
        return 0
    
    # Determine asset ID to test
    if not args.source:
        # Default to first available asset
        for cat_name, category in envmaps.all.items():
            if category.assets:
                first_name = sorted(category.assets.keys())[0]
                asset_id = f"ork_envmaps|{first_name}"
                print(f"No source specified, using first available: {first_name}")
                break
        else:
            print("ERROR: No environment maps found")
            return 1
    elif '|' in args.source:
        # Fully qualified ID provided
        asset_id = args.source
    else:
        # Use enumeration to find the asset
        files = envmaps.enumerate(args.source)
        
        if not files:
            print(f"ERROR: '{args.source}' not found (not a valid short name or category)")
            print("Use --list to see available maps")
            return 1
        
        # Use first asset if category, or the single asset if short name
        short_name = files[0][0]
        asset_id = f"ork_envmaps|{short_name}"
        
        if len(files) > 1:
            print(f"Testing first asset from category '{args.source}': {short_name}")
    
    print("="*60)
    print("XIR Radiance Asset Loading Test")
    print("="*60)
    print(f"Asset ID: {asset_id}")
    print()
    
    # Initialize lev2 app with offscreen graphics context
    print("Initializing lev2 app...")
    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    
    ##########################
    # Initialize asset catalog
    ##########################
    catalog = core.AssetCatalog.instance
    
    ##########################
    print(f"Graphics context: {ctx}")
        
    # Load XIR Radiance asset
    print(f"Loading XIR Radiance asset: {asset_id}")
    
    # Request Radiance maps from XIR file
    start = time.time()
    ibl_maps = lev2.PbrCommon.requestRadianceMaps(asset_id)
    ezapp.mainThreadBegin()
    ctx.beginFrame()
    ctx.endFrame()
    ezapp.mainThreadIter()
    ezapp.mainThreadEnd()
    end = time.time()
    interval = end - start
    print(ibl_maps)
    if None==ibl_maps:
        print("✗ ERROR: requestRadianceMaps returned None")
        print("✗ ERROR: Failed to load Radiance maps")
        print("  Check that:")
        print(f"  1. Asset '{asset_id}' exists in catalog")
        print("  2. XIR file was properly generated")
        print("  3. Asset has been packaged and is available")
        return 1    

    print("✓ Successfully loaded Radiance maps!")
    
    # Process one frame to ensure GPU resources are created
    print("Processing frame to create GPU resources...")
    

    print("\nRadiance maps summary:")
    print(f"  Diffuse texture: {ibl_maps.diffuse}")
    print(f"  Specular texture: {ibl_maps.specular}")
    print(f"  BRDF GGX LUT: {ibl_maps.brdf_ggx}")
    
    # Additional validation
    if ibl_maps.diffuse and ibl_maps.specular:
        print("\n✓ All required textures loaded successfully")
        print(f"Time taken: {interval:.2f} seconds")
        return 0
    else:
        print("\n✗ Some textures failed to load")
        return 1

if __name__ == "__main__":
    sys.exit(main())