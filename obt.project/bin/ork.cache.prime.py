#!/usr/bin/env ork.python
################################################################
# ork.cache.prime.py - Pre-cache assets (shaders, envmaps, models, BRDF maps)
#
# Runs headless (no window) using the ECS headless lifecycle
# (ecs.headless_appinit + bindGfxToCurrentThread). Triggers
# DataBlockCache population so subsequent app launches hit warm
# caches for expensive operations like Assimp->XGM conversion, BRDF
# integration map computation, shader compilation, and environment
# map processing.
#
# Usage:
#   ork.cache.prime.py                 # prime everything
#   ork.cache.prime.py --shaders       # shaders only
#   ork.cache.prime.py --envmaps       # environment maps only
#   ork.cache.prime.py --models        # models only
#   ork.cache.prime.py --brdf          # BRDF integration maps only
#   ork.cache.prime.py --list          # list what would be primed
################################################################

import sys, time, argparse
from orkengine import core, lev2
from orkengine import ecs

tokens = core.CrcStringProxy()

###############################################################################
# Asset lists
###############################################################################

SHADERS = [
    "orkshader://solid",
    "orkshader://pbr",
    "orkshader://ui",
    "orkshader://ui2",
    "orkshader://prim_canvas",
    "orkshader://particle",
    "orkshader://grid",
    "orkshader://graphview",
    "orkshader://imposters",
    "orkshader://blit",
    "orkshader://sdf_ui",
]

ENVMAPS = [
    "ork_envmaps|tozenv_nebula",
    "ork_envmaps|tozenv_hellscape",
    "ork_envmaps|tozenv_basic",
    "ork_envmaps|pillars4k",
    "ork_envmaps|cold4k",
    "ork_envmaps|ocean4k",
    "ork_envmaps|arena4k",
    "ork_envmaps|club4k",
    "ork_envmaps|desert4k",
    "ork_envmaps|canyon4k",
    "ork_envmaps|crossroads4k",
    "ork_envmaps|futcity4k",
    "ork_envmaps|ethereal4k",
]

MODELS = [
    "data://tests/pbr_calib.glb",
    "data://tests/pbr_calib_lopoly.glb",
    "data://tests/monkey_pbr.glb",
]

BRDF_TYPES = ["GGX", "GGXVELVET", "GGXRIM", "BLINN", "PHONG"]

###############################################################################

def _prime_shaders(ctx):
    print("\n--- Priming shaders ---")
    for path in SHADERS:
        t0 = time.monotonic()
        try:
            mtl = lev2.FreestyleMaterial()
            mtl.gpuInit(ctx, path)
            dt = time.monotonic() - t0
            print(f"  OK  {path}  ({dt:.3f}s)")
        except Exception as e:
            print(f"  FAIL {path}: {e}")

def _prime_envmaps(ctx):
    print("\n--- Priming environment maps ---")
    catalog = core.AssetCatalog.instance
    for name in ENVMAPS:
        t0 = time.monotonic()
        try:
            ibl = lev2.PbrCommon.requestRadianceMaps(name)
            if ibl:
                dt = time.monotonic() - t0
                print(f"  OK  {name}  ({dt:.3f}s)")
            else:
                print(f"  SKIP {name} (not available)")
        except Exception as e:
            print(f"  FAIL {name}: {e}")

def _prime_models(ctx):
    print("\n--- Priming models ---")
    for path in MODELS:
        t0 = time.monotonic()
        try:
            model = lev2.XgmModel(path)
            dt = time.monotonic() - t0
            print(f"  OK  {path}  ({dt:.3f}s)")
        except Exception as e:
            print(f"  FAIL {path}: {e}")

def _prime_brdf(ctx):
    print("\n--- Priming BRDF integration maps ---")
    for brdf_type in BRDF_TYPES:
        t0 = time.monotonic()
        try:
            tex = lev2.PBRMaterial.brdfIntegrationMap(ctx, brdf_type)
            dt = time.monotonic() - t0
            print(f"  OK  {brdf_type}  ({dt:.3f}s)")
        except Exception as e:
            print(f"  FAIL {brdf_type}: {e}")

def _list_assets():
    print("Shaders:")
    for s in SHADERS:
        print(f"  {s}")
    print(f"\nEnvironment maps:")
    for e in ENVMAPS:
        print(f"  {e}")
    print(f"\nModels:")
    for m in MODELS:
        print(f"  {m}")
    print(f"\nBRDF integration maps:")
    for b in BRDF_TYPES:
        print(f"  {b}")

###############################################################################

def main():
    parser = argparse.ArgumentParser(description="Pre-cache Orkid assets (headless)")
    parser.add_argument("--shaders", action="store_true", help="Prime shaders only")
    parser.add_argument("--envmaps", action="store_true", help="Prime environment maps only")
    parser.add_argument("--models", action="store_true", help="Prime models only")
    parser.add_argument("--brdf", action="store_true", help="Prime BRDF integration maps only")
    parser.add_argument("--list", action="store_true", help="List assets and exit")
    args = parser.parse_args()

    if args.list:
        _list_assets()
        return 0

    prime_all = not (args.shaders or args.envmaps or args.models or args.brdf)

    # ECS headless lifecycle — inline GPU work on a bound context (no render loop).
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    print("=" * 60)
    print("ork.cache.prime - headless asset pre-caching")
    print("=" * 60)
    t_start = time.monotonic()

    if prime_all or args.shaders:
        _prime_shaders(ctx)
    if prime_all or args.envmaps:
        _prime_envmaps(ctx)
    if prime_all or args.models:
        _prime_models(ctx)
    if prime_all or args.brdf:
        _prime_brdf(ctx)

    dt_total = time.monotonic() - t_start
    print(f"\n{'=' * 60}")
    print(f"Cache priming complete in {dt_total:.1f}s")
    print("=" * 60)

    ezapp.mainThreadEnd()
    ecs.headless_exit()
    return 0

if __name__ == "__main__":
    sys.exit(main())
