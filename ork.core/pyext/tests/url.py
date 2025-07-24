#!/usr/bin/env ork.python

import unittest
from orkengine.core import coreappinit, URL

coreappinit()

class TestURL(unittest.TestCase):
    """Test the URL class"""
    
    def test_url_parsing_basic(self):
        """Test basic URL parsing"""
        url = URL("https://example.com")
        self.assertEqual(url.scheme, "https")
        self.assertEqual(url.host, "example.com")
        self.assertEqual(url.port, -1)  # Default when not specified
        self.assertEqual(url.path, "")
        self.assertEqual(url.query, "")
        self.assertEqual(url.fragment, "")
    
    def test_url_parsing_full(self):
        """Test parsing URL with all components"""
        url = URL("https://user:pass@example.com:8080/path/to/resource?key=value&foo=bar#section")
        self.assertEqual(url.scheme, "https")
        self.assertEqual(url.userinfo, "user:pass")
        self.assertEqual(url.host, "example.com")
        self.assertEqual(url.port, 8080)
        self.assertEqual(url.path, "/path/to/resource")
        self.assertEqual(url.query, "key=value&foo=bar")
        self.assertEqual(url.fragment, "section")
    
    def test_url_parsing_no_scheme(self):
        """Test parsing URL without scheme"""
        url = URL("/path/to/file.txt")
        self.assertEqual(url.scheme, "")
        self.assertEqual(url.host, "")
        self.assertEqual(url.path, "/path/to/file.txt")
    
    def test_url_parsing_ipv4(self):
        """Test parsing URL with IPv4 address"""
        url = URL("http://192.168.1.1:8080/api")
        self.assertEqual(url.scheme, "http")
        self.assertEqual(url.host, "192.168.1.1")
        self.assertEqual(url.port, 8080)
        self.assertEqual(url.path, "/api")
    
    def test_url_parsing_ipv6(self):
        """Test parsing URL with IPv6 address"""
        url = URL("http://[2001:db8::1]:8080/api")
        self.assertEqual(url.scheme, "http")
        self.assertEqual(url.host, "[2001:db8::1]")
        self.assertEqual(url.port, 8080)
        self.assertEqual(url.path, "/api")
    
    def test_url_building_with_scheme(self):
        """Test building URL with scheme method"""
        url = URL("//example.com/path")
        url2 = url.with_scheme("https")
        self.assertEqual(url2.scheme, "https")
        self.assertEqual(url2.to_string(), "https://example.com/path")
    
    def test_url_building_with_host(self):
        """Test building URL with host method"""
        url = URL("https://old.com/path")
        url2 = url.with_host("new.com")
        self.assertEqual(url2.host, "new.com")
        self.assertEqual(url2.to_string(), "https://new.com/path")
    
    def test_url_building_with_port(self):
        """Test building URL with port method"""
        url = URL("https://example.com/path")
        url2 = url.with_port(8443)
        self.assertEqual(url2.port, 8443)
        self.assertEqual(url2.to_string(), "https://example.com:8443/path")
    
    def test_url_building_with_path(self):
        """Test building URL with path method"""
        url = URL("https://example.com")
        url2 = url.with_path("/api/v2/users")
        self.assertEqual(url2.path, "/api/v2/users")
        self.assertEqual(url2.to_string(), "https://example.com/api/v2/users")
    
    def test_url_building_with_query(self):
        """Test building URL with query method"""
        url = URL("https://api.example.com/search")
        url2 = url.with_query("q", "hello world")
        self.assertTrue("q=" in url2.query)
        # Check that spaces are encoded
        self.assertTrue("hello" in url2.to_string())
        
        # Add another query parameter
        url3 = url2.with_query("limit", "10")
        self.assertTrue("limit=10" in url3.query)
    
    def test_url_operator_slash(self):
        """Test URL path joining with / operator"""
        base = URL("https://api.example.com")
        
        # Simple append
        url1 = base / "v1"
        self.assertEqual(url1.path, "/v1")
        
        # Multiple appends
        url2 = url1 / "users" / "123"
        self.assertEqual(url2.path, "/v1/users/123")
        
        # With existing path
        base2 = URL("https://api.example.com/api/")
        url3 = base2 / "users"
        self.assertEqual(url3.path, "/api/users")
    
    def test_url_parent(self):
        """Test getting parent URL"""
        url = URL("https://example.com/path/to/resource")
        
        parent1 = url.parent()
        self.assertEqual(parent1.path, "/path/to")
        
        parent2 = parent1.parent()
        self.assertEqual(parent2.path, "/path")
        
        parent3 = parent2.parent()
        self.assertEqual(parent3.path, "/")
        
        # Parent of root should stay root
        parent4 = parent3.parent()
        self.assertEqual(parent4.path, "/")
    
    def test_url_encoding(self):
        """Test URL encoding functionality"""
        # Basic encoding
        encoded = URL.encode("hello world!")
        self.assertEqual(encoded, "hello%20world%21")
        
        # Keep safe characters
        encoded2 = URL.encode("hello-world_123.txt")
        self.assertEqual(encoded2, "hello-world_123.txt")
        
        # Special characters
        encoded3 = URL.encode("name=John&age=30")
        self.assertTrue("%3D" in encoded3)  # = encoded
        self.assertTrue("%26" in encoded3)  # & encoded
    
    def test_url_decoding(self):
        """Test URL decoding functionality"""
        # Basic decoding
        decoded = URL.decode("hello%20world%21")
        self.assertEqual(decoded, "hello world!")
        
        # Plus sign as space
        decoded2 = URL.decode("hello+world")
        self.assertEqual(decoded2, "hello world")
        
        # Mixed encoding
        decoded3 = URL.decode("name%3DJohn%26age%3D30")
        self.assertEqual(decoded3, "name=John&age=30")
    
    def test_url_path_encoding(self):
        """Test path-specific encoding"""
        # Path encoding should preserve slashes
        encoded = URL.encode_path("/path/with spaces/file.txt")
        self.assertTrue("/path/with%20spaces/file.txt" in encoded)
        self.assertTrue("/" in encoded)  # Slashes preserved
    
    def test_url_query_encoding(self):
        """Test query value encoding"""
        # Query encoding uses + for spaces
        encoded = URL.encode_query_value("hello world & more")
        self.assertTrue("hello+world" in encoded)
        self.assertTrue("%26" in encoded)  # & encoded
    
    def test_url_validation(self):
        """Test URL validation"""
        # Valid URLs
        self.assertTrue(URL("https://example.com").is_valid())
        self.assertTrue(URL("/path/to/file").is_valid())
        self.assertTrue(URL("file:///home/user/file.txt").is_valid())
        
        # Empty URL should be invalid
        self.assertFalse(URL("").is_valid())
    
    def test_url_is_absolute(self):
        """Test checking if URL is absolute"""
        # Absolute URLs have scheme and host
        self.assertTrue(URL("https://example.com/path").is_absolute())
        self.assertTrue(URL("ftp://ftp.example.com/file.txt").is_absolute())
        
        # Relative URLs
        self.assertFalse(URL("/path/to/file").is_absolute())
        self.assertFalse(URL("../relative/path").is_absolute())
        self.assertFalse(URL("file.txt").is_absolute())
    
    def test_real_world_urls(self):
        """Test with real-world URL examples"""
        # GitHub URL
        gh_url = URL("https://github.com/tweakoz/orkid/blob/master/README.md")
        self.assertEqual(gh_url.scheme, "https")
        self.assertEqual(gh_url.host, "github.com")
        self.assertEqual(gh_url.path, "/tweakoz/orkid/blob/master/README.md")
        
        # URL with complex query
        api_url = URL("https://api.example.com/v2/search?q=test&limit=10&offset=0&sort=desc")
        self.assertEqual(api_url.scheme, "https")
        self.assertEqual(api_url.host, "api.example.com")
        self.assertEqual(api_url.path, "/v2/search")
        self.assertTrue("q=test" in api_url.query)
        self.assertTrue("limit=10" in api_url.query)
        
        # File URL
        file_url = URL("file:///Users/michael/projects/orkid/README.md")
        self.assertEqual(file_url.scheme, "file")
        self.assertEqual(file_url.host, "")
        self.assertEqual(file_url.path, "/Users/michael/projects/orkid/README.md")
        
        # URL with authentication
        auth_url = URL("https://user:password@secure.example.com:8443/admin")
        self.assertEqual(auth_url.userinfo, "user:password")
        self.assertEqual(auth_url.host, "secure.example.com")
        self.assertEqual(auth_url.port, 8443)


if __name__ == '__main__':
    unittest.main(verbosity=2)