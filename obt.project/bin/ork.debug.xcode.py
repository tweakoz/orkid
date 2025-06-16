#!/usr/bin/env python3

import os

from obt import path 
from obt import debug_helpers as dh

if __name__ == "__main__":
  args = dh.DebugArgParser()
  env_vars = dh.orkid_debug_env_vars()
  env_vars["DYLD_FALLBACK_LIBRARY_PATH"] = str(path.libs())
  env_vars["DYLD_LIBRARY_PATH"] = str(path.libs())+":/opt/homebrew/lib"
  #print(env_vars)
  #assert(False)
  exe_name = args.executable_name
  exe_path = args.executable_path
  exe_args = args.executable_args
  workspace_dir = os.path.join(env_vars["OBT_STAGE"], "tempdir", exe_name + ".xcworkspace")
  dh.create_xcode_structure(workspace_dir, args, env_vars, os.getcwd())
