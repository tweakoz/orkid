#!/usr/bin/env ork.python
"""
Generate XIR (pre-filtered environment map) from an HDRI source image.
Supports .exr, .hdr, .png, .dds input formats.

By default uses synchronous Python GPU pipeline.
Use --async for the original C++ async processor.
"""

import os, sys, time, signal, argparse
from pathlib import Path
from orkengine import core
from orkengine import lev2

parser = argparse.ArgumentParser(description="Generate XIR from environment map")
parser.add_argument("-i", "--input", required=True, help="Source environment map (.exr, .hdr, .png, .dds)")
parser.add_argument("-o", "--output", required=True, help="Output XIR file path")
parser.add_argument("-d", "--debug", default=None, help="Directory for debug images (specular roughness + diffuse mips)")
parser.add_argument("--async", dest="use_async", action="store_true", help="Use original C++ async processor")
args = parser.parse_args()

def main_async():
    """Original C++ async processing path."""
    source_file = Path(args.input).resolve()
    dest_file = Path(args.output).resolve()

    if not source_file.exists():
        print(f"ERROR: Source file not found: {source_file}")
        return 1

    os.makedirs(str(dest_file.parent), exist_ok=True)

    print(f"Source:  {source_file}")
    print(f"Output:  {dest_file}")
    print(f"Mode:    async (C++)")

    ezapp = lev2.lev2appinit()

    ok_to_exit = False
    def onCtrlC(signum, frame):
        nonlocal ok_to_exit
        print("\nInterrupted by user")
        ok_to_exit = True
    signal.signal(signal.SIGINT, onCtrlC)

    ezapp.mainThreadBegin()

    future = lev2.EnvMapProcessor.processToXIRDataBlockAsync(str(source_file))
    if not future:
        print("ERROR: Failed to start processing")
        ezapp.mainThreadEnd()
        return 1

    start_time = time.time()
    report_time = start_time
    while not future.isReady() and not ok_to_exit:
        time.sleep(0.1)
        ezapp.mainThreadIterCommandLine()
        now = time.time()
        if now - report_time > 5:
            print(f"  Processing... ({int(now - start_time)}s elapsed)")
            report_time = now

    if ok_to_exit:
        ezapp.mainThreadEnd()
        return 1

    result = future.get()
    if not result:
        print("ERROR: Processing returned null result")
        ezapp.mainThreadEnd()
        return 1

    with open(str(dest_file), 'wb') as f:
        f.write(result.bytes)

    elapsed = time.time() - start_time
    print(f"SUCCESS: {dest_file.name} ({elapsed:.1f}s)")

    debug_dir = None
    if args.debug:
        debug_dir = Path(args.debug).resolve()
        os.makedirs(str(debug_dir), exist_ok=True)
        ext = source_file.suffix.lower()
        debug_ext = ".exr" if ext in (".exr", ".hdr") else ".png"
        saved = 0
        for i, img in enumerate(future.specular_images or []):
            if img:
                img.writeToFile(str(debug_dir / f"specular_roughness_{i}{debug_ext}"))
                saved += 1
        for i, img in enumerate(future.diffuse_images or []):
            if img:
                img.writeToFile(str(debug_dir / f"diffuse_mip_{i}{debug_ext}"))
                saved += 1
        if saved:
            print(f"Saved {saved} debug images to {debug_dir}")

    ezapp.mainThreadEnd()
    return 0

def main_sync():
    """Synchronous Python GPU pipeline."""
    from ork.envmap import process_envmap

    source_file = Path(args.input).resolve()
    dest_file = Path(args.output).resolve()

    if not source_file.exists():
        print(f"ERROR: Source file not found: {source_file}")
        return 1

    os.makedirs(str(dest_file.parent), exist_ok=True)

    debug_dir = None
    if args.debug:
        debug_dir = str(Path(args.debug).resolve())
        os.makedirs(debug_dir, exist_ok=True)

    print(f"Source:  {source_file}")
    print(f"Output:  {dest_file}")
    print(f"Mode:    sync (Python)")

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    ezapp.mainThreadBegin()

    start_time = time.time()
    ok = process_envmap(
        source_file, dest_file, ctx, ezapp,
        debug_dir=debug_dir, verbose=True)
    elapsed = time.time() - start_time

    if ok:
        print(f"SUCCESS: {dest_file.name} ({elapsed:.1f}s)")
    else:
        print(f"FAILED after {elapsed:.1f}s")

    ezapp.mainThreadEnd()
    return 0 if ok else 1

if __name__ == "__main__":
    if args.use_async:
        exit_code = main_async()
    else:
        exit_code = main_sync()
    os._exit(exit_code)
