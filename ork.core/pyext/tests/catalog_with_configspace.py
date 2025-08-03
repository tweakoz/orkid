#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path

# Initialize core
core.coreappinit()

try:
    # Test line 56: cat = core.AssetCatalog(space=cfgspc)
    
    # First create a config space
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    print("✓ Created AssetConfigSpace")
    
    # Create catalog with config space
    cat = core.AssetCatalog(space=cfgspc)
    print("✓ Created AssetCatalog with ConfigSpace:", cat)
    
    # Also test default constructor
    cat2 = core.AssetCatalog()
    print("✓ Created AssetCatalog with default constructor:", cat2)
    
    # Test that catalog works
    namespaces = cat.list_namespaces()
    print("✓ Listed namespaces:", namespaces)
    
    print("\n✅ SUCCESS: AssetCatalog constructor with ConfigSpace works!")
    
except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()