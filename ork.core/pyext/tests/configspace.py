#!/usr/bin/env ork.python

from orkengine import core

# Initialize core
print("Initializing core...")
core.coreappinit()

try:
    # Test line 16: cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    print("Creating AssetConfigSpace...")
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    print("Created AssetConfigSpace:", cfgspc)
    print("SUCCESS: Line 16 works!")
    
    # Test lines 17-21: createConfig with id and file
    print("\nTesting createConfig...")
    from ork import path as ork_path
    ddir = ork_path.data/"cdntest"
    cfg1 = cfgspc.createConfig(
        id="test1",
        file=ddir/"config.json"
    )
    print("Created config:", cfg1)
    print("SUCCESS: Lines 17-21 work!")
except Exception as e:
    print("ERROR:", e)
    import traceback
    traceback.print_exc()

print("Exiting...")
core.coreappexit()
print("Done.")