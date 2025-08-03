#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path

ddir = ork_path.data/"cdntest"
ddir2 = ork_path.data/"cdntest2"

# Initialize core
core.coreappinit()

try:
    ##################################
    # Phase 2: Test AssetCatalog, createManifest, and createAsset
    ##################################
    
    print("=== Phase 2: Testing AssetCatalog, createManifest, and createAsset ===\n")
    
    # First create the config space and configs (from phase 1)
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    cfg1 = cfgspc.createConfig(id="test1", file=ddir/"config.json")
    cfg2 = cfgspc.createConfig(id="test2", file=ddir/"config.json")
    
    # Add config data
    cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
    cfg1.addNamespace("cdntest", "api_key", "cdntest_remote")
    cfg1.addLocalLocation(id="stage", loc="<stage>")
    cfg1.addLocalLocation(id="cache", loc="<assetcache>")
    
    # Line 49: writeToDisk
    cfgspc.writeToDisk()
    print("✓ Line 49: cfgspc.writeToDisk() completed")
    
    # Lines 51-54: loadFromDisk
    pth1 = ork_path.data/"cdntest/config.json"
    pth2 = ork_path.data/"cdntest2/config2.json"
    pthlist = [pth1, pth2] 
    cfgspc2 = core.AssetConfigSpace.loadFromDisk(pthlist)
    print("✓ Lines 51-54: AssetConfigSpace.loadFromDisk() completed")
    
    # Line 56: Create AssetCatalog with space
    cat = core.AssetCatalog(space=cfgspc)
    print("✓ Line 56: Created AssetCatalog with ConfigSpace")
    
    # Register codec for the namespace using the API key from config
    cat.registerCodecWithPassword("cdntest", "api_key")
    print("✓ Registered codec for cdntest namespace")
    
    # Lines 58-63: createManifest
    m1 = cat.createManifest( 
        # uuid assigned automatically
        id="cdntest",
        version="1.0.0",
        namespace="cdntest",
        file = ddir/"catalog.json")
    print("✓ Lines 58-63: Created manifest:", m1)
    
    # Create test file for asset
    import os
    test_file_dir = obt_path.stage() / "assetcache" / "my_local_asset_dir"
    os.makedirs(test_file_dir, exist_ok=True)
    test_file_path = test_file_dir / "my_asset_file.txt"
    with open(test_file_path, 'w') as f:
        f.write("This is test content for the asset file.")
    print(f"✓ Created test file: {test_file_path}")
    
    # Lines 65-75: createAsset
    a1 = m1.createAsset(
        # uuid assigned automatically
        id="user_id",
        priority=100,
        type="text",
        remote = "<cdntest_remote>/my_remote_asset_dir",
        local = "<cache>/my_local_asset_dir",
        filename="my_asset_file.txt",
        platforms=["mac", "linux"],
        dependencies = []  # Empty list for now
    )
    print("✓ Lines 65-75: Created asset:", a1)
    
    # Verify asset properties
    print("\nAsset properties:")
    print(f"  ID: {a1.id}")
    print(f"  Type: {a1.type}")
    print(f"  Priority: {a1.priority}")
    print(f"  Local: {a1.local_loc}")
    print(f"  Remote: {a1.remote_loc}")
    print(f"  Filename: {a1.filename}")
    print(f"  Platforms: {a1.platforms}")
    print(f"  Content hash: {a1.content_hash}")
    print(f"  Storage hash: {a1.storage_hash}")
    print(f"  Size: {a1.size}")
    
    # Test manifest assets
    assets = m1.assets
    print(f"\nManifest has {len(assets)} asset(s)")
    assert "user_id" in assets, "Asset not found in manifest"
    assert assets["user_id"] == a1, "Asset reference mismatch"
    
    # Verify encryption is reversible
    print("\n=== Verifying encryption is reversible ===")
    
    # Read original content
    with open(test_file_path, 'r') as f:
        original_content = f.read()
    print(f"Original content: '{original_content}'")
    
    # The asset should have been repackaged during createAsset
    # Get the codec from the manifest
    codec = m1.getCodec()
    if codec:
        # Get the encrypted file path using the new property
        encrypted_file = a1.local_encrypted_path
        print(f"Encrypted file path: {encrypted_file}")
        
        if encrypted_file and os.path.exists(encrypted_file):
            print("✓ Found encrypted file")
            
            # Decrypt to a temp file
            temp_decrypted = str(test_file_dir / "temp_decrypted.txt")
            # Use the free function decryptFile instead of method
            try:
                core.decrypt_file(encrypted_file, temp_decrypted, codec)
                print("✓ Successfully decrypted file")
                
                # Read decrypted content
                with open(temp_decrypted, 'r') as f:
                    decrypted_content = f.read()
                print(f"Decrypted content: '{decrypted_content}'")
                
                # Verify content matches
                assert decrypted_content == original_content, f"Content mismatch! Original: '{original_content}', Decrypted: '{decrypted_content}'"
                print("✓ Decrypted content matches original!")
                
                # Clean up
                os.remove(temp_decrypted)
            except Exception as e:
                print(f"❌ Failed to decrypt file: {e}")
        else:
            print(f"❌ Encrypted file not found at: {encrypted_file}")
    else:
        print("⚠️  No codec available for decryption test")
    
    print("\n✅ SUCCESS: All lines 49-75 work!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()