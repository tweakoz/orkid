#!/usr/bin/env ork.python
"""
Test for loading XIR Xir (Radiance) assets from Python.
Loads a pre-filtered environment map and verifies the textures are created.
"""

import sys, time, argparse
from orkengine import core
from orkengine import lev2

parser = argparse.ArgumentParser(description='XIR Radiance asset loading test')
parser.add_argument("--id", type=str, default="ork_envmaps|tozenv_nebula", help="XIR asset ID")

tokens = core.CrcStringProxy()

print("="*60)
print("Starting XIR Radiance asset loading test")
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

# Load XIR Radiance asset
print("Loading XIR Radiance asset: ork_envmaps|tozenv_nebula")

# Request Radiance maps from XIR file
ibl_maps = lev2.PbrCommon.requestRadianceMaps("ork_envmaps|tozenv_nebula")
print(ibl_maps)
if ibl_maps:
    print("Successfully loaded Radiance maps!")
    
    # Process one frame to ensure GPU resources are created
    print("Processing frame to create GPU resources...")
    
    ctx.beginFrame()
    ctx.endFrame()
    ezapp.mainThreadIter()
                
    print("\nRadiance maps summary:")
    print(f"  diffuse: {ibl_maps.diffuse}")
    print(f"  specular: {ibl_maps.specular}")
    print(f"  BRDF GGX: {ibl_maps.brdf_ggx}")
    sys.exit(0)
    
else:
    print("ERROR: Failed to load Radiance maps")
    ezapp.mainThreadEnd()
    sys.exit(1)
    
