#!/usr/bin/env ork.python

import tempfile
import shutil
import os
import time
from pathlib import Path as PyPath
from orkengine.core import coreappinit, URL, Download, DownloadManager, DownloadGroup, DownloadState, Path

coreappinit()

# Test downloading SINGUL_PAK_STD multiple times as a group
print("Testing download group...")

SINGUL_PAK_URL = "https://www.tweakoz.com/resources/SINGUL_PAK_STD"
temp_dir = tempfile.mkdtemp()
manager = DownloadManager()

group_complete = False
all_success = False

try:
  group = DownloadGroup()
  
  # Add multiple downloads of the same file to the group
  num_downloads = 3
  for i in range(num_downloads):
    url = URL(SINGUL_PAK_URL)
    dest_path = Path(os.path.join(temp_dir, f"SINGUL_PAK_STD_group_{i}"))
    group.add_download(url, dest_path)
  
  def on_group_complete(success):
    global group_complete, all_success
    group_complete = True
    all_success = success
    print(f"Group complete: {'Success' if success else 'Failed'}")
  
  def on_group_progress(completed, total):
    print(f"Group progress: {completed}/{total} downloads")
  
  group.on_complete(on_group_complete)
  group.on_progress(on_group_progress)
  
  # Start the group download
  manager.downloadGroup(group)
  
  # Wait for group completion
  timeout = 300  # 5 minutes
  start_time = time.time()
  last_completed = -1
  
  while not group_complete and (time.time() - start_time) < timeout:
    time.sleep(1)
    # Only print when progress changes
    if group.completed_count != last_completed:
      print(f"Group progress: {group.completed_count}/{num_downloads} completed, "
            f"{group.failed_count} failed, {group.progress_percentage():.1f}%")
      last_completed = group.completed_count
  
  assert group_complete, "Group download did not complete"
  assert all_success, "Not all downloads in group succeeded"
  assert group.completed_count == num_downloads
  assert group.failed_count == 0
  
  # Verify all files exist
  for i in range(num_downloads):
    file_path = os.path.join(temp_dir, f"SINGUL_PAK_STD_group_{i}")
    assert os.path.exists(file_path), f"Group file {i} was not downloaded"
  
  print("✓ Download group test passed")
  
finally:
  manager.shutdown()
  shutil.rmtree(temp_dir, ignore_errors=True)