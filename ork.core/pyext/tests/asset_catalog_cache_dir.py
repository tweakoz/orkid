#!/usr/bin/env ork.python

import unittest
import os
import tempfile
import shutil
from orkengine import core
from orkengine.core import *

class TestAssetCatalogCacheDir(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Initialize core application once for all tests"""
        core.coreappinit()
        
    @classmethod  
    def tearDownClass(cls):
        """Clean up core application"""
        core.coreappexit()
        
    def setUp(self):
        """Set up test environment"""
        # Create temporary directory for testing
        self.test_dir = tempfile.mkdtemp()
        print(f"Test directory: {self.test_dir}")
        
    def tearDown(self):
        """Clean up test environment"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
            
    def test_default_cache_dir(self):
        """Test default cache directory is set correctly"""
        catalog = AssetCatalog()
        
        # Check default cache dir is under stage
        cache_dir = catalog.cache_dir
        print(f"Default cache dir: {cache_dir}")
        self.assertTrue("assetcache" in cache_dir)
        
        # Check subdirectories
        print(f"Encrypted dir: {catalog.encrypted_dir}")
        print(f"Chunks dir: {catalog.chunks_dir}")
        print(f"Receipts dir: {catalog.receipts_dir}")
        print(f"Temp dir: {catalog.temp_dir}")
        
        self.assertTrue(catalog.encrypted_dir.endswith("assetcache/enc"))
        self.assertTrue(catalog.chunks_dir.endswith("assetcache/enc/chunks"))
        self.assertTrue(catalog.receipts_dir.endswith("assetcache/receipts"))
        self.assertTrue(catalog.temp_dir.endswith("assetcache/temp"))
        
    def test_custom_cache_dir(self):
        """Test setting custom cache directory"""
        catalog = AssetCatalog()
        
        # Set custom cache directory
        custom_cache = os.path.join(self.test_dir, "custom_cache")
        catalog.cache_dir = custom_cache
        
        # Verify it was set
        self.assertEqual(catalog.cache_dir, custom_cache)
        
        # Check subdirectories are updated
        self.assertTrue(catalog.encrypted_dir.startswith(custom_cache))
        self.assertTrue(catalog.chunks_dir.startswith(custom_cache))
        self.assertTrue(catalog.receipts_dir.startswith(custom_cache))
        self.assertTrue(catalog.temp_dir.startswith(custom_cache))
        
        # Verify directories were created
        self.assertTrue(os.path.exists(custom_cache))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "enc")))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "enc", "chunks")))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "receipts")))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "temp")))
        
    def test_cache_dir_structure_created(self):
        """Test that cache directory structure is created"""
        # Create catalog with custom cache
        custom_cache = os.path.join(self.test_dir, "test_cache")
        catalog = AssetCatalog()
        catalog.cache_dir = custom_cache
        
        # Verify all subdirectories were created
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "enc")))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "enc", "chunks")))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "receipts")))
        self.assertTrue(os.path.exists(os.path.join(custom_cache, "temp")))
        
if __name__ == "__main__":
    unittest.main()