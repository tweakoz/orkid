#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import json

ddir = ork_path.data/"cdntest"

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test AssetEntry.fromJson()
    ##################################
    
    print("=== Testing AssetEntry.fromJson() ===\n")
    
    # Create a sample JSON string with all fields
    asset_json = {
        "id": "test_asset",
        "namespace": "test_ns",
        "type": "model",
        "priority": 150,
        "remote": "https://cdn.example.com/assets",
        "local": "/tmp/assets",
        "filename": "test_model.gltf",
        "platforms": ["mac", "linux", "win32"],
        "dependencies": ["dep1", "dep2", "dep3"],
        "content_hash": "abcd1234567890ef",
        "storage_hash": "1234567890abcdef",
        "hash_algorithm": "xxhash64",
        "native_size": 1048576,
        "compressed_size": 524288,
        "chunks": {
            "chunk_size": 65536,
            "total_size": 1048576,
            "file_hash": 987654321,
            "compression": "lz4",
            "is_encrypted": True,
            "chunks": [
                {
                    "offset": 0,
                    "size": 65536,
                    "compressed_size": 32768,
                    "hash": 111111
                },
                {
                    "offset": 65536,
                    "size": 65536,
                    "compressed_size": 32768,
                    "hash": 222222
                }
            ]
        }
    }
    
    json_str = json.dumps(asset_json, indent=2)
    print("Input JSON:")
    print(json_str)
    
    # Test fromJson
    entry = core.AssetEntry.fromJson(json_str)
    
    if entry is None:
        print("❌ ERROR: fromJson returned None")
    else:
        print("\n✓ AssetEntry created successfully!")
        
        # Verify basic fields
        assert entry.id == "test_asset", f"Expected id='test_asset', got '{entry.id}'"
        assert entry.namespace_id == "test_ns", f"Expected namespace='test_ns', got '{entry.namespace_id}'"
        assert entry.type == "model", f"Expected type='model', got '{entry.type}'"
        assert entry.priority == 150, f"Expected priority=150, got {entry.priority}"
        assert entry.filename == "test_model.gltf", f"Expected filename='test_model.gltf', got '{entry.filename}'"
        
        # Verify platforms
        assert entry.platforms == ["mac", "linux", "win32"], f"Expected platforms=['mac', 'linux', 'win32'], got {entry.platforms}"
        
        # Verify hash info
        assert entry.content_hash == "abcd1234567890ef", f"Expected content_hash='abcd1234567890ef', got '{entry.content_hash}'"
        assert entry.storage_hash == "1234567890abcdef", f"Expected storage_hash='1234567890abcdef', got '{entry.storage_hash}'"
        assert entry.hash_algorithm == "xxhash64", f"Expected hash_algorithm='xxhash64', got '{entry.hash_algorithm}'"
        
        # Verify size info
        assert entry.size == 1048576, f"Expected size=1048576, got {entry.size}"
        assert entry.compressed_size == 524288, f"Expected compressed_size=524288, got {entry.compressed_size}"
        assert entry.is_compressed == True, f"Expected is_compressed=True, got {entry.is_compressed}"
        
        # Verify chunking
        assert entry.is_chunked() == True, f"Expected is_chunked()=True, got {entry.is_chunked()}"
        
        print("✓ All basic fields verified!")
        
        # Test round-trip (toJson -> fromJson)
        output_json = entry.toJson()
        print("\n=== Round-trip test ===")
        print("Output JSON:")
        print(output_json)
        
        # Parse output and verify
        parsed_output = json.loads(output_json)
        assert parsed_output["id"] == "test_asset"
        assert parsed_output["type"] == "model"
        assert parsed_output["priority"] == 150
        assert parsed_output["platforms"] == ["mac", "linux", "win32"]
        assert parsed_output["dependencies"] == ["dep1", "dep2", "dep3"]
        
        # Check chunk info preserved
        if "chunks" in parsed_output:
            chunks_info = parsed_output["chunks"]
            assert chunks_info["chunk_size"] == 65536
            assert chunks_info["total_size"] == 1048576
            assert chunks_info["compression"] == "lz4"
            assert chunks_info["is_encrypted"] == True
            assert len(chunks_info["chunks"]) == 2
            print("✓ Chunk info preserved in round-trip!")
        
        print("\n✅ SUCCESS: AssetEntry.fromJson() test passed!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()