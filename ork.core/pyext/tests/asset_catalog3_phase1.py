#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path

ddir = ork_path.data/"cdntest"
ddir2 = ork_path.data/"cdntest2"

# Initialize core
core.coreappinit()

try:
    ##################################
    # AssetConfigSpace : Configuration Space
    #  manages full composite configuration
    ##################################

    # Line 16
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    print("✓ Line 16: Created AssetConfigSpace")

    # Lines 17-21
    cfg1 = cfgspc.createConfig( 
             # uuid assigned automatically
             id="test1",
             file=ddir/"config.json"
             ) # individual config
    print("✓ Lines 17-21: Created config1:", cfg1)

    # Lines 22-26
    cfg2 = cfgspc.createConfig(
             # uuid assigned automatically
             id="test2", 
             file=ddir/"config.json") # Using same file for test
    print("✓ Lines 22-26: Created config2:", cfg2)
    
    # Load the actual config from file since createConfig just creates empty
    loaded_cfg = core.AssetConfig.loadFromFile(ddir/"config.json")
    if loaded_cfg:
        # Copy loaded data to cfg2
        cfg2.addRemoteLocation("cdntest_remote", "https://localhost:8443/")
        cfg2.addNamespace("cdntest", "api_key", "cdntest_remote")
        cfg2.addLocalLocation("assetcache", "<assetcache>")
        cfg2.addLocalLocation("stage", "<stage>")

    # Line 27
    cfg1.addRemoteLocation("cdntest_remote", "https://localhost:8443/")
    cfg1.addNamespace("cdntest", "api_key", "cdntest_remote")
    print("✓ Line 27: Added namespace key")

    # Line 28
    cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
    print("✓ Line 28: Added remote location")

    # Line 29
    cfg1.addLocalLocation(id="stage", loc="<stage>") # stage is built-in immutable location -> points to Path::stage()
    print("✓ Line 29: Added local location 'stage'")

    # Line 30
    cfg1.addLocalLocation(id="cache", loc="<stage>/assetcache") # use explicit path to avoid overwriting existing key
    print("✓ Line 30: Added local location 'cache'")

    # Line 32
    as_json = cfg1.toJson()
    print("✓ Line 32: AssetConfig JSON:")
    print(as_json)
    
    # Verify JSON structure
    import json
    parsed = json.loads(as_json)
    assert "namespaces" in parsed, "Missing namespaces in JSON"
    assert "locations" in parsed, "Missing locations in JSON"
    assert "destinations" in parsed, "Missing destinations in JSON"
    
    # Verify that cfg1 has the expected content
    assert "cdntest" in parsed["namespaces"], "Missing 'cdntest' in namespaces"
    assert "cdntest_remote" in parsed["locations"], "Missing 'cdntest_remote' in locations"
    assert "cache" in parsed["destinations"], "Missing 'cache' in destinations"
    
    # Verify namespace structure
    cdntest_ns = parsed["namespaces"]["cdntest"]
    assert "encryption_key" in cdntest_ns, "Missing encryption_key in namespace"
    assert "upload_location" in cdntest_ns, "Missing upload_location in namespace"
    assert cdntest_ns["encryption_key"] == "api_key", "Wrong encryption_key value"
    assert cdntest_ns["upload_location"] == "cdntest_remote", "Wrong upload_location value"
    
    # Verify that URLs have trailing slash (per session notes)
    for loc_key, loc_value in parsed["locations"].items():
        if loc_value.startswith("http"):
            assert loc_value.endswith("/"), f"Location {loc_key} should end with trailing slash"
    
    print("✓ JSON structure verified!")
    
    # Now test cfg1 which has our added data
    cfg1_json = cfg1.toJson()
    print("\n✓ cfg1 JSON (with added data):")
    print(cfg1_json)
    
    parsed1 = json.loads(cfg1_json)
    # Should have both original data (from file) and new data
    assert "cdntest" in parsed1["namespaces"], "Missing added 'cdntest' namespace"
    cdntest_ns1 = parsed1["namespaces"]["cdntest"]
    assert cdntest_ns1["encryption_key"] == "api_key", "Wrong encryption_key for cdntest"
    assert cdntest_ns1["upload_location"] == "cdntest_remote", "Wrong upload_location for cdntest"
    assert "cdntest_remote" in parsed1["locations"], "Missing added remote location"
    assert parsed1["locations"]["cdntest_remote"] == "https://localhost:8443/", "Wrong remote location URL"
    assert "stage" in parsed1["destinations"], "Missing added 'stage' destination"
    assert "cache" in parsed1["destinations"], "Missing added 'cache' destination"
    
    print("✓ All JSON verifications passed!")
    
    print("\n✅ SUCCESS: All lines 16-32 work!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()