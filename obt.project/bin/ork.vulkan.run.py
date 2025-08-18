#!/usr/bin/env python3
"""
Orkid Vulkan Launcher
Sets up Vulkan environment variables and launches an Orkid program.
Usage: ork.vulkan.run.py <program> [args...]
"""

import os
import sys
import subprocess
import obt.path
import obt.host

def setup_vulkan_env():
    """Set up environment variables for Vulkan, same as ork.use.vulkan()"""
    
    # Get library paths
    obt_dyld_fallback_library_path = str(obt.path.libs())
    dyld_library_path = str(obt.path.libs())
    
    if obt.host.IsDarwin:
        dyld_library_path += ":/opt/homebrew/lib"
    
    # Get current environment values
    env_dyld_library_path = os.environ.get("DYLD_LIBRARY_PATH", "")
    env_obt_dyld_fallback_library_path = os.environ.get("OBT_DYLD_FALLBACK_LIBRARY_PATH", "")
    
    # Set up environment variables
    os.environ["ORKID_GRAPHICS_API"] = "VULKAN"
    
    # Combine existing and new library paths
    if env_dyld_library_path:
        os.environ["DYLD_LIBRARY_PATH"] = f"{env_dyld_library_path}:{dyld_library_path}"
    else:
        os.environ["DYLD_LIBRARY_PATH"] = dyld_library_path
    
    if env_obt_dyld_fallback_library_path:
        os.environ["DYLD_FALLBACK_LIBRARY_PATH"] = f"{env_obt_dyld_fallback_library_path}:{obt_dyld_fallback_library_path}"
    else:
        os.environ["DYLD_FALLBACK_LIBRARY_PATH"] = obt_dyld_fallback_library_path
    
    print(f"Vulkan environment set up:")
    print(f"  ORKID_GRAPHICS_API={os.environ['ORKID_GRAPHICS_API']}")
    print(f"  DYLD_LIBRARY_PATH={os.environ['DYLD_LIBRARY_PATH']}")
    print(f"  DYLD_FALLBACK_LIBRARY_PATH={os.environ['DYLD_FALLBACK_LIBRARY_PATH']}")

def main():
    if len(sys.argv) < 2:
        print("Usage: ork.vulkan.run.py <program> [args...]")
        print("Example: ork.vulkan.run.py ork.example.lev2.gfx.arraytex2.exe")
        sys.exit(1)
    
    # Set up Vulkan environment
    setup_vulkan_env()
    
    # Get program and arguments
    program = sys.argv[1]
    args = sys.argv[2:]
    
    print(f"Launching: {program}")
    if args:
        print(f"Arguments: {' '.join(args)}")
    
    try:
        # Launch the program with the modified environment
        result = subprocess.run([program] + args, env=os.environ)
        sys.exit(result.returncode)
    except FileNotFoundError:
        print(f"Error: Program '{program}' not found")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nProgram interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"Error launching program: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 