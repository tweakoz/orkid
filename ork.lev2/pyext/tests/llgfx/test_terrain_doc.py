#!/usr/bin/env python3
###############################################################################
# S0 terrain DOCUMENT model gate. The DSL trace now builds a structured Python
# document (nodes/params/connections/captures/groups); a separate elaborate()
# DERIVES the flat dflow.GraphData (owner law L2 — nothing else constructs it).
# This gate proves:
#   g1 (determinism): a corpus file import -> document -> elaborate -> bake is
#       byte-identical run-to-run through the document path (per-channel sha256,
#       masking only the EXR capDate wall-clock stamp — the sole cross-run field).
#       (Equivalence to the pre-refactor DIRECT-TRACE bake was verified out of
#       band: the g0 baseline of the unmodified tree and this document path give
#       byte-identical masked sha256 for voronoi/hf1/erox.)
#   g3 (doc-JSON round-trip): document -> doc-JSON -> document -> elaborate ->
#       bake is byte-identical to the direct elaborate bake.
#   serializeJson of the elaborated graph is byte-stable across re-traces of the
#       same source (masking only the per-object uuid, which is not part of the
#       deterministic Merkle cook hash).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, json, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
from ork.hypergraph.dflow.terrain.doc import to_json, from_json

DIM = 256
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"   # followed by 19-byte "YYYY:MM:DD HH:MM:SS"
_UUID_RE = re.compile(r'"uuid"\s*:\s*"[0-9a-fA-F-]+"')


def masked_sha(path):
    """sha256 of an EXR with the capDate wall-clock stamp zeroed (the only field that
    varies across bakes of an identical field)."""
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        s = i + len(_CAPDATE)
        b[s:s + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


def _load(dsl_file):
    cls = load_dsl_class(resolve_dsl_file(dsl_file), None)
    return cls()


def _bake(inst, outdir, tag, ctx):
    g = inst.generatedflow()
    shas = {}
    for ch in inst.channels:
        p = os.path.join(outdir, f"{tag}__{ch}.exr")
        inst.set_capture_path(ch, p)
    lev2.terrain.bake_heightfield(g, ctx, DIM)
    for ch in inst.channels:
        p = os.path.join(outdir, f"{tag}__{ch}.exr")
        shas[ch] = masked_sha(p) if os.path.exists(p) else "MISSING"
    return g, shas


def _sj(g):
    return _UUID_RE.sub('"uuid":"X"', g.serializeJson())


def run(ez, ctx):
    outdir = "/tmp/tered_doc"
    os.makedirs(outdir, exist_ok=True)

    results = {}
    for dsl_file in ("voronoi", "hf1"):
        print(f"\n=== {dsl_file} ===", flush=True)

        # g1-internal: two independent traces -> elaborate -> bake must match.
        _, sha_a = _bake(_load(dsl_file), outdir, f"{dsl_file}_a", ctx)
        _, sha_b = _bake(_load(dsl_file), outdir, f"{dsl_file}_b", ctx)
        det = (sha_a == sha_b)
        print(f"  determinism (2 traces): {det}  sha={sha_a}", flush=True)
        results[f"{dsl_file}_determinism"] = det

        # serializeJson byte-stability across re-traces (uuid masked).
        g1, _ = _bake(_load(dsl_file), outdir, f"{dsl_file}_sj1", ctx)
        g2, _ = _bake(_load(dsl_file), outdir, f"{dsl_file}_sj2", ctx)
        sj_stable = (_sj(g1) == _sj(g2))
        print(f"  serializeJson stable across re-traces: {sj_stable}", flush=True)
        results[f"{dsl_file}_sj_stable"] = sj_stable

        # g3: doc-JSON round-trip -> elaborate -> bake identical.
        inst = _load(dsl_file)
        _, sha_direct = _bake(inst, outdir, f"{dsl_file}_direct", ctx)
        dj = json.loads(json.dumps(to_json(inst.document())))   # doc -> JSON text -> doc
        doc2 = from_json(dj)
        g_rt, capmap = doc2.elaborate()
        for cap in lev2.terrain.capture_modules(g_rt):
            for ch in cap.channel.split(","):
                cap.path = os.path.join(outdir, f"{dsl_file}_rt__{ch}.exr")
        lev2.terrain.bake_heightfield(g_rt, ctx, DIM)
        sha_rt = {}
        for cap in lev2.terrain.capture_modules(g_rt):
            for ch in cap.channel.split(","):
                p = os.path.join(outdir, f"{dsl_file}_rt__{ch}.exr")
                sha_rt[ch] = masked_sha(p) if os.path.exists(p) else "MISSING"
        rt_ok = (sha_rt == sha_direct)
        print(f"  doc-JSON round-trip bake identical: {rt_ok}", flush=True)
        print(f"    doc-JSON bytes={len(json.dumps(dj))}", flush=True)
        results[f"{dsl_file}_roundtrip"] = rt_ok

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain doc-model {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
