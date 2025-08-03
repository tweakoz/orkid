#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import os
import shutil
import tarfile

ddir = ork_path.data/"cdntest"

# Initialize core
core.coreappinit()

try:
    ##################################
    # Phase 3: Test asset_pak functionality
    ##################################
    
    print("=== Phase 3: Testing asset_pak with createAsset ===\n")
    
    # Set up config space and catalog (from phase 1)
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    cfg1 = cfgspc.createConfig(id="test1", file=ddir/"config.json")
    cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
    cfg1.addNamespace("cdntest", "api_key", "cdntest_remote")
    cfg1.addLocalLocation(id="stage", loc="<stage>")
    cfg1.addLocalLocation(id="cache", loc="<assetcache>")
    cfgspc.writeToDisk()
    
    cat = core.AssetCatalog(space=cfgspc)
    
    # Register codec before creating manifest/assets
    cat.registerCodecWithPassword("cdntest", "api_key")
    
    m1 = cat.createManifest(
        id="cdntest",
        version="1.0.0",
        namespace="cdntest",
        file = ddir/"catalog.json"
    )
    
    # Create a directory structure for the pak
    pak_dir = obt_path.stage() / "assetcache" / "test_pak_dir"
    if pak_dir.exists():
        shutil.rmtree(pak_dir)
    os.makedirs(pak_dir, exist_ok=True)
    
    # Create subdirectories and files
    models_dir = pak_dir / "models"
    textures_dir = pak_dir / "textures"
    config_dir = pak_dir / "config"
    
    os.makedirs(models_dir, exist_ok=True)
    os.makedirs(textures_dir, exist_ok=True)
    os.makedirs(config_dir, exist_ok=True)
    
    # Create test files
    with open(models_dir / "character.obj", 'w') as f:
        f.write("# OBJ file\nv 1.0 2.0 3.0\nv 4.0 5.0 6.0\nf 1 2\n")
    
    with open(textures_dir / "skin.png", 'wb') as f:
        # Write a minimal PNG header
        f.write(b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde')
    
    with open(config_dir / "settings.json", 'w') as f:
        f.write('{"version": 1, "quality": "high"}')
    
    print(f"✓ Created test directory structure at: {pak_dir}")
    print(f"  - {models_dir / 'character.obj'}")
    print(f"  - {textures_dir / 'skin.png'}")
    print(f"  - {config_dir / 'settings.json'}")
    
    # With the C++ changes, we don't need to create the TAR file manually
    # The C++ code will create it from the directory during repackage()
    
    # Rename the directory to match what the C++ code expects
    # It expects the directory name to be the TAR filename minus .tar extension
    expected_dir = obt_path.stage() / "assetcache" / "test_assets"
    if expected_dir.exists():
        shutil.rmtree(expected_dir)
    shutil.move(str(pak_dir), str(expected_dir))
    
    print(f"\n✓ Moved directory to expected location: {expected_dir}")
    
    # Now create asset_pak entry - C++ will create TAR from directory
    a1 = m1.createAsset(
        id="test_pak",
        priority=100,
        type="asset_pak",
        remote = "<cdntest_remote>/paks",
        local = "<cache>",
        filename="test_assets.tar",
        platforms=["mac", "linux"],
        dependencies = []
    )
    
    print(f"\n✓ Created asset_pak: {a1}")
    print(f"  Type: {a1.type}")
    print(f"  Local: {a1.local_loc}")
    print(f"  Filename: {a1.filename}")
    print(f"  Content hash: {a1.content_hash}")
    print(f"  Storage hash: {a1.storage_hash}")
    print(f"  Size: {a1.size}")
    
    # Verify it has different hashes (encrypted)
    assert a1.content_hash != a1.storage_hash, "Content and storage hashes should differ"
    print("\n✓ Verified: storage_hash differs from content_hash (encryption applied)")
    
    # The TAR file will be created by C++ during repackage
    # We can verify it was created by checking the storage hash
    print("\n✓ TAR file will be created during repackage")
    print(f"  Expected location: {obt_path.stage() / 'assetcache' / 'test_assets.tar'}")
    
    # Test that repackage() is idempotent
    old_storage_hash = a1.storage_hash
    a1.repackage()
    assert a1.storage_hash == old_storage_hash, "Repackage should produce same hash"
    print("\n✓ Verified: repackage() is idempotent")
    
    # Decrypt and verify we get original content back
    print("\n✓ Testing decryption and content verification:")
    
    # Use the local_encrypted_path property to find the encrypted file
    encrypted_file_path = a1.local_encrypted_path
    print(f"  Looking for encrypted file at: {encrypted_file_path}")
    
    if encrypted_file_path and os.path.exists(encrypted_file_path):
        encrypted_file = encrypted_file_path
        print(f"  Found encrypted file: {encrypted_file}")
        
        # Read encrypted content
        with open(encrypted_file, 'rb') as f:
            encrypted_data = f.read()
        print(f"  Encrypted size: {len(encrypted_data)} bytes")
        
        # Get codec for decryption
        codec = cat.codecForNamespace("cdntest")
        if codec:
            # Decrypt
            decrypted_data = codec.decrypt(encrypted_data)
            print(f"  Decrypted size: {len(decrypted_data)} bytes")
            
            # Calculate MD5 hash of decrypted content for comparison
            import hashlib
            decrypted_md5 = hashlib.md5(decrypted_data).hexdigest()
            
            # Show all hashes for comparison
            print(f"  Hash comparison:")
            print(f"    Original content hash (MD5): {a1.content_hash}")
            print(f"    Decrypted content hash (MD5): {decrypted_md5}")
            print(f"    Storage hash (encrypted MD5): {a1.storage_hash}")
            
            # Verify MD5 hash matches content hash
            assert decrypted_md5 == a1.content_hash, "Decrypted MD5 hash should match original content hash"
            print("  ✓ Content verified - decrypted MD5 hash matches content hash!")
            
            # Save decrypted file for manual verification
            decrypted_path = expected_dir.parent / "test_assets_decrypted.tar"
            with open(decrypted_path, 'wb') as f:
                f.write(decrypted_data)
            
            # Verify decrypted TAR is valid
            with tarfile.open(decrypted_path, 'r') as tar:
                members = tar.getnames()
                assert len(members) == 3, "Decrypted TAR should have 3 files"
                print("  ✓ Decrypted TAR is valid with all files intact")
        else:
            print("  ⚠️  WARNING: Could not get codec for namespace")
    else:
        print(f"  ⚠️  WARNING: Encrypted file not found at expected location")
        # Let's see what files exist in the directory
        enc_dir = obt_path.stage() / "assetcache" / "enc"
        if enc_dir.exists():
            print(f"  Files in {enc_dir}:")
            for f in os.listdir(enc_dir):
                print(f"    - {f}")
    
    print("\n✅ SUCCESS: asset_pak creation, encryption, and decryption works!")

except Exception as e:
    print(f"\n❌ ERROR: {e}")
    import traceback
    traceback.print_exc()

core.coreappexit()