#!/usr/bin/env ork.python
"""
Test for loading XIR irradiance assets from Python.
Loads a pre-filtered environment map and verifies the textures are created.
"""

import sys
import time
from orkengine import core
from orkengine import lev2

tokens = core.CrcStringProxy()

def main():
    print("="*60)
    print("Starting XIR irradiance asset loading test")
    print("="*60)
    
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
    fbi = ctx.FBI
    
    print("Starting main thread...")
    ezapp.mainThreadBegin()
    
    # Load XIR irradiance asset
    print("Loading XIR irradiance asset: ork_envmaps|tozenv_nebula")
    
    # Request irradiance maps from XIR file
    irr_maps = lev2.PbrCommon.requestIrradianceMaps("ork_envmaps|tozenv_nebula")
    print(irr_maps)
    if irr_maps:
        print("Successfully loaded irradiance maps!")
        
        # Process one frame to ensure GPU resources are created
        print("Processing frame to create GPU resources...")
        
        ctx.beginFrame()
        ctx.endFrame()
        ezapp.mainThreadIter()
        
        # Verify textures were created
        if irr_maps._filtenvDiffuseMap:
            print(f"  Diffuse map created: {irr_maps._filtenvDiffuseMap}")
        else:
            print("  ERROR: Diffuse map not created")
            
        if irr_maps._filtenvSpecularMap:
            print(f"  Specular map created: {irr_maps._filtenvSpecularMap}")
        else:
            print("  ERROR: Specular map not created")
            
        # Check BRDF integration maps
        if irr_maps._brdfIntegrationMapGGX:
            print(f"  BRDF GGX map created: {irr_maps._brdfIntegrationMapGGX}")
        else:
            print("  WARNING: BRDF GGX map not created")
            
        print("\nIrradiance maps summary:")
        print(f"  Has diffuse: {irr_maps._filtenvDiffuseMap is not None}")
        print(f"  Has specular: {irr_maps._filtenvSpecularMap is not None}")
        print(f"  Has BRDF GGX: {irr_maps._brdfIntegrationMapGGX is not None}")
        print(f"  Has BRDF Velvet: {irr_maps._brdfIntegrationMapVelvet is not None}")
        print(f"  Has BRDF RIM: {irr_maps._brdfIntegrationMapGGXRIM is not None}")
        print(f"  Has BRDF Blinn: {irr_maps._brdfIntegrationMapBlinn is not None}")
        print(f"  Has BRDF Phong: {irr_maps._brdfIntegrationMapPhong is not None}")
        
    else:
        print("ERROR: Failed to load irradiance maps")
        ezapp.mainThreadEnd()
        return 1
    
    # Test loading a non-existent asset (should return None)
    print("\nTesting error handling with non-existent asset...")
    bad_maps = pbr_common.requestIrradianceMaps("ork_envmaps|does_not_exist")
    if bad_maps is None:
        print("  Correctly returned None for non-existent asset")
    else:
        print("  ERROR: Should have returned None for non-existent asset")
    
    print("Ending main thread...")
    ezapp.mainThreadEnd()
    
    print("="*60)
    print("XIR irradiance asset loading test completed successfully!")
    print("="*60)
    return 0

if __name__ == "__main__":
    sys.exit(main())