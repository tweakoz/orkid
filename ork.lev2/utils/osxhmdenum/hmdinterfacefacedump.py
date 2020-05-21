#!/usr/bin/env python3
import os
from ork.path import Path

root = Path("/")
frameworks = root/"System"/"Library"/"Frameworks"
appkit = frameworks/"Appkit.framework"/"Versions"/"Current"/"AppKit"

os.system("nm %s | grep HMD | c++filt"%appkit)
