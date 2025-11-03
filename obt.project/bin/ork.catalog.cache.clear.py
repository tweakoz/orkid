#!/usr/bin/env ork.python

import os 
from ork import path as ork_path

cdir = ork_path.assetcache
print(f"[DEBUG] Clearing asset catalog cache directory: {cdir}")
os.system(f"rm -rf {cdir}/*")



