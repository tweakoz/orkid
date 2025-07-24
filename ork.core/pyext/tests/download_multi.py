#!/usr/bin/env ork.python

import tempfile
import shutil
import os
import time
from pathlib import Path as PyPath
from orkengine.core import coreappinit, URL, Download, DownloadManager, DownloadState, Path

coreappinit()

# Test downloading SINGUL_PAK_STD multiple times concurrently
print("Testing concurrent downloads...")

SINGUL_PAK_URL = "https://www.tweakoz.com/resources/SINGUL_PAK_STD"
temp_dir = tempfile.mkdtemp()
manager = DownloadManager()

try:
  # Set a reasonable concurrent limit
  manager.max_concurrent_downloads = 3
  
  # Create multiple downloads of the same file
  num_downloads = 5
  downloads = []
  completed = []
  
  def make_on_complete(index):
    def on_complete(success, path):
      if success:
        completed.append(index)
        print(f"Download {index} completed: {path}")
    return on_complete
  
  for i in range(num_downloads):
    url = URL(SINGUL_PAK_URL)
    dest_path = Path(os.path.join(temp_dir, f"SINGUL_PAK_STD_{i}"))
    
    dl = manager.download(url, dest_path)
    dl.on_complete(make_on_complete(i))
    downloads.append((i, dl, dest_path))
  
  # Wait for all downloads to complete
  timeout = 300  # 5 minutes for multiple downloads
  start_time = time.time()
  last_completed_count = -1
  
  while len(completed) < num_downloads and (time.time() - start_time) < timeout:
    time.sleep(1)
    # Check active downloads
    active = manager.active_download_count()
    # Only print occasionally to reduce spam
    if len(completed) != last_completed_count:
      print(f"Active downloads: {active}, Completed: {len(completed)}/{num_downloads}")
      last_completed_count = len(completed)
    assert active <= 3, "Too many concurrent downloads"
  
  # Verify all completed
  assert len(completed) == num_downloads, "Not all downloads completed"
  
  # Verify all files exist and have reasonable size
  for i, dl, dest_path in downloads:
    assert os.path.exists(str(dest_path)), f"Download {i} file missing"
    size = os.path.getsize(str(dest_path))
    assert size > 1000000, f"Download {i} file too small"
  
  print("✓ Concurrent downloads test passed")
  
finally:
  manager.shutdown()
  shutil.rmtree(temp_dir, ignore_errors=True)