#!/usr/bin/env ork.python
"""
Orkid Shared Memory Cleanup Utility

Cleans up shared memory segments created by Orkid applications.
All Orkid segments are prefixed with "ork.shm." for easy identification.
"""

import sys
import argparse

try:
    from orkengine.core import (
        list_shmobjects,
        remove_shmobject,
        cleanup_shmobjects
    )
except ImportError:
    print("Error: Could not import orkengine.core")
    print("Make sure Orkid Python bindings are built and in your PYTHONPATH")
    sys.exit(1)

def list_segments(pattern=None):
    """List shared memory segments"""
    if pattern is None:
        pattern = "ork\\.shm\\..*"
    
    print(f"Searching for segments matching pattern: {pattern}")
    segments = list_shmobjects(pattern)
    
    if not segments:
        print("No matching segments found.")
    else:
        print(f"\nFound {len(segments)} segment(s):")
        for seg in segments:
            print(f"  • {seg}")
    
    return segments

def cleanup_segments(verbose=True):
    """Clean up all Orkid shared memory segments"""
    print("=" * 60)
    print("Orkid Shared Memory Cleanup")
    print("=" * 60)
    
    removed = cleanup_shmobjects(verbose)
    
    if removed > 0:
        print(f"\n✅ Successfully cleaned up {removed} segment(s)")
    else:
        print("\n✅ No segments needed cleanup")
    
    return removed

def remove_specific(name):
    """Remove a specific shared memory segment"""
    success = remove_shmobject(name)
    if success:
        print(f"✅ Removed: {name}")
    else:
        print(f"❌ Failed to remove: {name}")
    return success

def main():
    parser = argparse.ArgumentParser(
        description="Orkid Shared Memory Cleanup Utility",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  ork.shmobject.cleanup.py              # Clean up all Orkid segments
  ork.shmobject.cleanup.py --list       # List segments without removing
  ork.shmobject.cleanup.py --pattern "test.*"  # List segments matching pattern
  ork.shmobject.cleanup.py --remove NAME       # Remove specific segment
        """
    )
    
    parser.add_argument(
        "--list", "-l",
        action="store_true",
        help="List segments without removing them"
    )
    
    parser.add_argument(
        "--pattern", "-p",
        type=str,
        help="Regex pattern for listing segments (default: ork\\.shm\\..*)"
    )
    
    parser.add_argument(
        "--remove", "-r",
        type=str,
        help="Remove a specific segment by name"
    )
    
    parser.add_argument(
        "--quiet", "-q",
        action="store_true",
        help="Suppress verbose output"
    )
    
    args = parser.parse_args()
    
    if args.remove:
        # Remove specific segment
        success = remove_specific(args.remove)
        return 0 if success else 1
    
    elif args.list or args.pattern:
        # List segments
        pattern = args.pattern if args.pattern else None
        segments = list_segments(pattern)
        return 0
    
    else:
        # Default: cleanup all Orkid segments
        verbose = not args.quiet
        removed = cleanup_segments(verbose)
        return 0

if __name__ == "__main__":
    sys.exit(main())
