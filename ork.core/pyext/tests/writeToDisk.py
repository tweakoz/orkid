#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import tempfile
import json
import os

# Initialize core
core.coreappinit()

try:
    # Create a temporary directory for testing
    with tempfile.TemporaryDirectory() as temp_dir:
        # Create test config files
        config1_path = os.path.join(temp_dir, "config1.json")
        config2_path = os.path.join(temp_dir, "config2.json")
        
        # Write initial configs
        config1_data = {
            "namespace_keys": {"test1": "key1"},
            "locations": {"loc1": "https://test1.com"},
            "destinations": {"dest1": "/path1"}
        }
        config2_data = {
            "namespace_keys": {"test2": "key2"},
            "locations": {"loc2": "https://test2.com"},
            "destinations": {"dest2": "/path2"}
        }
        
        with open(config1_path, 'w') as f:
            json.dump(config1_data, f, indent=2)
        with open(config2_path, 'w') as f:
            json.dump(config2_data, f, indent=2)
        
        print(f"Created test configs in {temp_dir}")
        
        # Create config space and load configs
        cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
        cfg1 = cfgspc.createConfig("test1", config1_path)
        cfg2 = cfgspc.createConfig("test2", config2_path)
        
        print("✓ Loaded configs")
        
        # Modify configs
        cfg1.addNamespaceKey("newkey", "newvalue")
        cfg2.addRemoteLocation("newloc", "https://newloc.com")
        
        print("✓ Modified configs")
        
        # Write back to disk
        cfgspc.writeToDisk()
        
        print("✓ Called writeToDisk()")
        
        # Verify files were updated
        with open(config1_path, 'r') as f:
            saved_config1 = json.load(f)
        with open(config2_path, 'r') as f:
            saved_config2 = json.load(f)
        
        # Check that modifications were saved
        assert "newkey" in saved_config1["namespace_keys"], "New namespace key not saved"
        assert saved_config1["namespace_keys"]["newkey"] == "newvalue", "New namespace key value incorrect"
        assert "newloc" in saved_config2["locations"], "New location not saved"
        assert saved_config2["locations"]["newloc"] == "https://newloc.com/", "New location URL incorrect (should have trailing slash)"
        
        print("✓ Verified files were updated correctly")
        print("\n✅ SUCCESS: writeToDisk() works correctly!")
        
except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()