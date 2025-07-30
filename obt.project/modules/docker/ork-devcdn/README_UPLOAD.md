# CDN Upload Support

This CDN now supports both GET (download) and PUT (upload) operations for asset management.

## Upload Endpoint

**URL**: `https://localhost:8443/upload/{path}`
**Method**: `PUT`
**Authentication**: API key via `X-API-Key` header

### Usage

Upload a file to the CDN:

```bash
curl -X PUT \
  -H "X-API-Key: your_api_key_here" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @local_file.pak \
  https://localhost:8443/upload/namespace/asset_name.pak
```

### Response

**Success (201 Created)**:
```json
{
  "status": "success",
  "path": "namespace/asset_name.pak",
  "size": 1048576,
  "md5": "d41d8cd98f00b204e9800998ecf8427e"
}
```

**Error (400/403/500)**:
```json
{
  "error": "Error description"
}
```

## Security Features

- **API Key Authentication**: Same authentication as downloads
- **Path Traversal Protection**: Prevents `../` attacks
- **Rate Limiting**: 2 uploads per second per IP
- **File Size Limits**: 1GB maximum upload size
- **Method Restriction**: Only PUT method allowed

## File Organization

Uploaded files are stored in `/cdn/content/` with the same structure as downloads:
- Upload to: `/upload/game/textures/player.pak`
- Stored at: `/cdn/content/game/textures/player.pak`
- Download from: `/download/game/textures/player.pak`

## Testing

Use the provided test script:

```bash
# Upload a file
python test_upload.py your_api_key local_file.pak remote/path/file.pak

# This will:
# 1. Upload the file
# 2. Verify MD5 hash
# 3. Download and compare for round-trip verification
```

## Integration with Asset Catalog

The upload endpoint is designed to work with the asset catalog uploader system:

1. **Asset Packager** creates encrypted `.tar.xz.enc` files
2. **Asset Uploader** uses PUT to upload to CDN
3. **Asset Catalog** downloads using GET with same paths

### Example Workflow

```python
import requests

# Upload asset
def upload_asset(api_key, local_file, namespace, asset_name):
    remote_path = f"{namespace}/{asset_name}"
    
    with open(local_file, 'rb') as f:
        data = f.read()
    
    response = requests.put(
        f"https://cdn.example.com/upload/{remote_path}",
        data=data,
        headers={'X-API-Key': api_key},
        verify=False
    )
    
    if response.status_code == 201:
        result = response.json()
        print(f"Uploaded: {result['path']}, MD5: {result['md5']}")
        return result['md5']
    else:
        print(f"Upload failed: {response.text}")
        return None

# Usage
md5_hash = upload_asset("your_key", "game_assets.tar.xz.enc", "game", "assets_v1.pak")
```

## Configuration

### Nginx Configuration Changes

- Added `/upload` location block
- Added upload rate limiting (2r/s)
- Increased client body size to 1GB
- Added Lua upload handler

### Docker Changes

- Content volume changed from `:ro` to `:rw` for write access
- Added `upload.lua` script mount

### File Permissions

Uploaded files are automatically set to `644` permissions for proper read access by the web server.

## Error Codes

| Code | Description |
|------|-------------|
| 201  | File uploaded successfully |
| 400  | Bad request (missing path, empty body) |
| 401  | Missing API key |
| 403  | Invalid API key or forbidden path |
| 405  | Method not allowed (not PUT) |
| 429  | Rate limit exceeded |
| 500  | Server error (disk full, permissions, etc.) |

## Monitoring and Logging

The CDN provides comprehensive logging for both uploads and downloads with real-time visibility in docker-compose.

### Interactive Monitoring

**Option 1: Direct docker-compose with visible logs**
```bash
# All logs visible in terminal with timestamps
docker-compose up

# Output shows real-time activity:
cdn_1  | 2024/01/01 12:00:00 [info] Auth attempt: 192.168.1.100 PUT /upload/test.pak key:test_key...
cdn_1  | 2024/01/01 12:00:00 [info] Auth success: 192.168.1.100 PUT /upload/test.pak key:test_key...
cdn_1  | 2024/01/01 12:00:00 [info] Upload started: 192.168.1.100 -> test.pak key:test_key...
cdn_1  | 2024/01/01 12:00:02 [info] Upload completed: 192.168.1.100 -> test.pak size:1048576 duration:2.00s rate:512.0KB/s
```

**Option 2: Interactive startup script**
```bash
./start-interactive.sh

# Choose from:
# 1) Colored logs (if terminal supports it)
# 2) Simple logs
# 3) Background with monitor dashboard
```

**Option 3: Background with dedicated monitor**
```bash
# Start CDN in background
docker-compose up -d

# Run interactive monitor
python monitor_logs.py ./logs
```

### Log Files

- **access.log** - Standard nginx access log
- **cdn.log** - Detailed CDN operations log with API keys and timing
- **error.log** - Detailed application logs with upload/download events

### Log Formats

**Standard Access Log**:
```
192.168.1.100 - - [01/Jan/2024:12:00:00 +0000] "PUT /upload/game/assets.pak HTTP/1.1" 201 1048576
```

**Detailed CDN Log**:
```
192.168.1.100 - [01/Jan/2024:12:00:00 +0000] "PUT /upload/game/assets.pak HTTP/1.1" 201 1048576 "test_key..." "curl/7.68.0" rt=2.34 ul="" ut=""
```

**Application Error Log** (detailed events):
```
2024/01/01 12:00:00 [info] 123#0: Auth attempt: 192.168.1.100 PUT /upload/game/assets.pak key:test_key...
2024/01/01 12:00:00 [info] 123#0: Auth success: 192.168.1.100 PUT /upload/game/assets.pak key:test_key...
2024/01/01 12:00:00 [info] 123#0: Upload started: 192.168.1.100 -> game/assets.pak key:test_key...
2024/01/01 12:00:00 [info] 123#0: Upload target: 192.168.1.100 -> /cdn/content/game/assets.pak
2024/01/01 12:00:00 [info] 123#0: Read body directly: 1048576 bytes
2024/01/01 12:00:00 [info] 123#0: Writing file: /cdn/content/game/assets.pak (1048576 bytes)
2024/01/01 12:00:02 [info] 123#0: File written successfully: /cdn/content/game/assets.pak (1048576 bytes)
2024/01/01 12:00:02 [info] 123#0: MD5 calculated: d41d8cd98f00b204e9800998ecf8427e
2024/01/01 12:00:02 [info] 123#0: Upload completed: 192.168.1.100 -> game/assets.pak size:1048576 duration:2.34s rate:437.6KB/s md5:d41d8cd98f00b204e9800998ecf8427e
```

### Real-time Log Monitoring

Use the provided log monitor script for real-time monitoring:

```bash
# Monitor logs with statistics
python monitor_logs.py ./logs

# Output:
CDN Log Monitor Started
Monitoring: ./logs/error.log
Press Ctrl+C to stop
Legend: 🔐=Auth 📁=File ⬆️=Upload ⬇️=Download ✅=Success ❌=Failed ⚠️=Warning 🚨=Error

============================================================
CDN Statistics - 2024-01-01 12:00:00
============================================================
Uploads:   3 total, 3 success, 0 failed (100.0% success)
           3.2 MB uploaded
Downloads: 12 total, 12 success, 0 failed (100.0% success)
           15.7 MB downloaded
Auth:      15 attempts, 15 success, 0 failed (100.0% success)
```

### Security Features

- **API Key Logging**: Only first 8 characters logged for security
- **Rate Limiting Events**: Failed rate limit attempts are logged
- **Path Traversal Attempts**: Blocked attempts are logged with warnings
- **Authentication Failures**: Failed auth attempts logged with IP addresses

### Performance Metrics

Each upload/download includes:
- **Duration**: Total operation time
- **Transfer Rate**: KB/s transfer rate
- **File Size**: Bytes transferred
- **MD5 Hash**: Integrity verification (uploads only)

### Integration with Asset Catalog

The detailed logging integrates with the asset catalog workflow:

```python
# Upload with logging
def upload_with_monitoring(api_key, file_path, remote_path):
    # Upload triggers detailed logs:
    # - Authentication attempt/success
    # - Upload start with file size
    # - Directory creation if needed
    # - File write progress
    # - MD5 calculation
    # - Upload completion with metrics
    
    response = requests.put(
        f"https://cdn.example.com/upload/{remote_path}",
        data=open(file_path, 'rb'),
        headers={'X-API-Key': api_key}
    )
    
    # Response includes performance metrics
    if response.status_code == 201:
        result = response.json()
        print(f"Upload: {result['duration']}s @ {result['rate_kbps']}KB/s")
```