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
                
    print("\nIrradiance maps summary:")
    print(f"  diffuse: {irr_maps.diffuse}")
    print(f"  specular: {irr_maps.specular}")
    print(f"  BRDF GGX: {irr_maps.brdf_ggx}")
    sys.exit(0)
    
else:
    print("ERROR: Failed to load irradiance maps")
    ezapp.mainThreadEnd()
    sys.exit(1)
    
