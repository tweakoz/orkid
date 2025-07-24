#!/usr/bin/env ork.python

import tempfile
import shutil
import os
import time
from pathlib import Path as PyPath
from orkengine.core import coreappinit, URL, Download, DownloadManager, DownloadState, Path

coreappinit()

# Test downloading SINGUL_PAK_STD
print("Testing single download...")

SINGUL_PAK_URL = "https://www.tweakoz.com/resources/SINGUL_PAK_STD"
temp_dir = tempfile.mkdtemp()
manager = DownloadManager()

download_complete = False
download_success = False
final_size = 0

try:
  url = URL(SINGUL_PAK_URL)
  dest_path = Path(os.path.join(temp_dir, "SINGUL_PAK_STD_single"))
 
  def on_complete(success, path):
    global download_complete, download_success, final_size
    download_complete = True
    download_success = success
    if success and os.path.exists(str(path)):
      final_size = os.path.getsize(str(path))
      print(f"Downloaded {final_size} bytes to {path}")
  
  def on_progress(downloaded, total):
    if total > 0:
      percent = downloaded/total*100
      # Only print at major milestones: 25%, 50%, 75%, 100%
      if percent >= 25 and not hasattr(on_progress, 'printed_25'):
        print(f"Progress: 25%")
        on_progress.printed_25 = True
      elif percent >= 50 and not hasattr(on_progress, 'printed_50'):
        print(f"Progress: 50%")
        on_progress.printed_50 = True
      elif percent >= 75 and not hasattr(on_progress, 'printed_75'):
        print(f"Progress: 75%")
        on_progress.printed_75 = True
  
  dl = manager.download(url, dest_path)
  dl.on_complete(on_complete)
  dl.on_progress(on_progress)
  
  # Wait for download to complete (longer timeout for large file)
  timeout = 120  # 2 minutes
  start_time = time.time()
  while not download_complete and (time.time() - start_time) < timeout:
    time.sleep(0.5)
  
  assert download_complete, "Download did not complete"
  assert download_success, "Download failed"
  assert os.path.exists(str(dest_path)), "Downloaded file does not exist"
  assert final_size > 1000000, "Downloaded file seems too small"  # Should be ~72MB
  
  print("✓ Single download test passed")
  
finally:
  manager.shutdown()
  shutil.rmtree(temp_dir, ignore_errors=True)

# Test SINGUL_PAK_STD download with detailed progress tracking
print("\nTesting download with progress tracking...")

temp_dir2 = tempfile.mkdtemp()
manager2 = DownloadManager()

try:
  url = URL(SINGUL_PAK_URL)
  dest_path = Path(os.path.join(temp_dir2, "SINGUL_PAK_STD_progress"))
  
  progress_updates = []
  
  def on_progress(downloaded, total):
    progress_updates.append((downloaded, total))
    # Only print at major milestones
    if total > 0:
      percent = downloaded / total * 100
      if percent >= 25 and not hasattr(on_progress, 'printed_25'):
        print(f"Progress tracking: 25% ({downloaded:,}/{total:,} bytes)")
        on_progress.printed_25 = True
      elif percent >= 50 and not hasattr(on_progress, 'printed_50'):
        print(f"Progress tracking: 50% ({downloaded:,}/{total:,} bytes)")
        on_progress.printed_50 = True
      elif percent >= 75 and not hasattr(on_progress, 'printed_75'):
        print(f"Progress tracking: 75% ({downloaded:,}/{total:,} bytes)")
        on_progress.printed_75 = True
  
  def on_complete(success, path):
    if success:
      print(f"Download complete: {path}")
    else:
      print("Download failed")
  
  dl = manager2.download(url, dest_path)
  dl.on_progress(on_progress)
  dl.on_complete(on_complete)
  
  # Wait for completion
  timeout = 120
  start_time = time.time()
  download_complete = False
  
  while not download_complete and (time.time() - start_time) < timeout:
    time.sleep(0.5)
    # Check if download is complete by checking file existence and non-zero size
    if os.path.exists(str(dest_path)) and os.path.getsize(str(dest_path)) > 0:
      # Give it a bit more time to ensure callbacks are called
      time.sleep(1)
      download_complete = True
  
  assert download_complete, "Download did not complete"
  assert len(progress_updates) > 0, "No progress updates received"
  assert os.path.exists(str(dest_path)), "Downloaded file does not exist"
  
  # Check that progress updates made sense
  if progress_updates:
    last_downloaded, last_total = progress_updates[-1]
    assert last_downloaded > 0, "Final downloaded bytes should be > 0"
    print(f"Received {len(progress_updates)} progress updates")
    print(f"Final size: {last_downloaded:,} bytes")
  
  print("✓ Progress tracking test passed")
  
finally:
  manager2.shutdown()
  shutil.rmtree(temp_dir2, ignore_errors=True)