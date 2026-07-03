#!/usr/bin/env python3
# Wipe the dataflow cook cache AND everything derived from it. A partial clean
# (dflowcache only) is a trap: the capture-currency sidecars left in
# assetcache/terrain/<name>/*.cookhash still claim the bake outputs are current,
# so the intended "cold" recook silently skips every sink and computes nothing
# (bit the divergence-reference generation, 2026-07-02).

from obt import path, command

targets = [
    # per-node cook blobs (terrain + hypermesh graphs)
    path.stage() / "dflowcache",
    # terrain bake products: channel EXRs, .terrain.json manifests, scatter
    # .ogeo, and the capture-currency .cookhash sidecars
    path.stage() / "assetcache" / "terrain",
    # captured proctex atlases (derived from the materials + terrain products;
    # keyed by material hash, so stale entries would otherwise just accumulate)
    path.stage() / "assetcache" / "ptex3d_capture",
]

for t in targets:
    print("cleaning<%s>" % t)
    command.system(["rm", "-rf", "%s/*" % t])
