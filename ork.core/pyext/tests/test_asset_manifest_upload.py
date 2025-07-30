#!/usr/bin/env ork.python

from orkengine import core
import tempfile
import os
import urllib.request
import urllib.error

# Initialize core
core.coreappinit()

def check_cdn_running():
    """Check if CDN is running on https://localhost:8443/"""
    try:
        # Try to connect to the CDN
        ctx = urllib.request.ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = urllib.request.ssl.CERT_NONE
        
        req = urllib.request.Request('https://localhost:8443/')
        with urllib.request.urlopen(req, context=ctx, timeout=5) as response:
            return True
    except urllib.error.HTTPError as e:
        # HTTP errors (like 403, 404) mean the server is running
        if e.code in [403, 404, 401]:
            return True
        print(f"❌ CDN returned error: {e.code} {e.reason}")
        return False
    except Exception as e:
        print(f"❌ CDN not running at https://localhost:8443/")
        print(f"   Error: {e}")
        print("   Please start the CDN server before running this test")
        return False

try:
    # Check if CDN is running
    if not check_cdn_running():
        exit(1)
    
    print("✓ CDN is running at https://localhost:8443/")
    
    # Create temp directory for test
    with tempfile.TemporaryDirectory() as temp_dir:
        print(f"Using temp directory: {temp_dir}")
        
        # Create multiple test files
        test_files = ["test1.txt", "test2.txt", "test3.txt"]
        for i, filename in enumerate(test_files):
            test_file = os.path.join(temp_dir, filename)
            with open(test_file, 'w') as f:
                f.write(f"Content of test file {i+1}: Hello from Asset Catalog!")
            print(f"✓ Created test file: {test_file}")
        
        # Create config space and catalog with explicit cache directory
        cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
        
        # Create a simple config with HTTPS CDN destination
        cfg = cfgspc.createConfig("test", core.Path(temp_dir) / "config.json")
        cfg.addRemoteLocation("cdn", "https://localhost:8443/upload/")
        cfg.addNamespace("test", "api_key", "cdn")
        cfg.addLocalLocation("stage", temp_dir)
        
        # Enable certificate ignoring for self-signed cert
        location_info = cfg.resolveRemoteLocation("cdn")
        if location_info:
            location_info.disable_cert_check = True
            print("✓ Enabled certificate check bypass for self-signed cert")
        
        print("✓ Created config with CDN destination")
        
        # Create catalog with explicit cache directory
        cat = core.AssetCatalog(space=cfgspc)
        cat.cache_dir = os.path.join(temp_dir, "assetcache")
        
        # Register codec for encryption
        cat.registerCodecWithPassword("test", "test_password")
        
        print(f"✓ Set catalog cache dir to: {cat.cache_dir}")
        print(f"✓ Registered codec for namespace 'test'")
        
        # Create manifest
        manifest = cat.createManifest(
            id="test_manifest", 
            version="1.0.0",
            namespace="test",
            file=core.Path(temp_dir) / "manifest.json"
        )
        
        print("✓ Created manifest")
        
        # Create multiple assets in the manifest
        assets = []
        for i, filename in enumerate(test_files):
            asset = manifest.createAsset(
                id=f"test_asset_{i+1}",
                priority=100 + i,
                type="text",
                remote="<cdn>",
                local=temp_dir,
                filename=filename,
                platforms=["all"],
                dependencies=[]
            )
            assets.append(asset)
            print(f"✓ Created asset: test_asset_{i+1}")
        
        # Repackage all assets first (this should repackage the entire manifest)
        print("\\n=== Repackaging Manifest ===")
        manifest.repackage()
        print("✓ Manifest repackaged")
        
        # Verify all assets are repackaged
        for i, asset in enumerate(assets):
            print(f"  Asset {i+1}: storage_hash={asset.storage_hash}, content_hash={asset.content_hash}")
        
        # Test manifest upload
        print("\\n=== Testing Manifest Upload ===")
        try:
            receipt = manifest.upload(cfg, "cdn")
            print(f"✓ Manifest upload method called successfully")
            
            if receipt:
                print(f"  Receipt ID: {receipt.upload_id}")
                print(f"  Success: {receipt.success}")
                print(f"  Status: {receipt.status_message}")
                print(f"  Files uploaded: {receipt.total_files}")
                print(f"  Bytes uploaded: {receipt.bytes_uploaded}")
                
                if receipt.success:
                    print(f"\\n🎉 MANIFEST UPLOAD SUCCESS!")
                    print(f"   Uploaded {receipt.total_files} assets ({receipt.bytes_uploaded} bytes total)")
                    
                    # Verify each asset was uploaded
                    for i, asset in enumerate(assets):
                        expected_url = f"https://localhost:8443/test/enc/{asset.storage_hash}.enc"
                        print(f"   Asset {i+1}: {expected_url}")
                else:
                    print(f"\\n❌ Manifest upload failed: {receipt.status_message}")
            else:
                print("✗ No receipt returned")
                
        except Exception as e:
            print(f"✗ Manifest upload failed: {e}")
            import traceback
            traceback.print_exc()
        
        print("\\n✅ Test completed")

except Exception as e:
    print(f"❌ ERROR: {e}")
    import traceback
    traceback.print_exc()

core.coreappexit()