#!/usr/bin/env python3
###############################################################################
# JUL09 S1 gate gA — TerrainRuntime edit/rebake loop (headless).
#
# The runtime owns the structured terrain DOCUMENT + bake; every edit mutates the
# DOCUMENT (doc.set_param / DocLoop.set_count / DocSwitch.select) then rebake()
# re-elaborates a fresh GraphData and re-bakes (owner law L2). This gate proves:
#   * corpus load (voronoi) -> rebake -> re-rebake is byte-identical (determinism).
#   * set_param on a scalar -> rebake -> masked sha changes; setting it back ->
#     rebake -> the ORIGINAL sha returns AND the revert bake is cache-assisted
#     (cook cache-loaded > 0) — the Merkle cook cache absorbs re-derivation.
#   * DocSwitch.select(other) -> rebake -> sha changes and matches a DIRECT trace
#     of that branch (elaborate is the selection authority).
#   * DocLoop.set_count N->N+k -> rebake shows TAIL-ONLY recompute (first-N cache
#     point loads; computed grows by exactly k).
#   * dim is part of the cook context hash — preview + a larger dim cache
#     independently.
#   * doc-JSON save -> load -> rebake is sha-stable.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, time, ctypes, hashlib, tempfile, textwrap

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.editor.terrain_runtime import TerrainRuntime
from ork.hypergraph.dflow.terrain.doc import DocNode, DocLoop, DocSwitch

DIM = 256
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"
_COOK_RE = re.compile(r"\[cook\] cacheable bake:\s+(\d+) cache-loaded,\s+(\d+) demand-skipped,\s+(\d+) computed")
_libc = ctypes.CDLL(None)
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)


def masked_sha(path):
  with open(path, "rb") as f:
    b = bytearray(f.read())
  i = b.find(_CAPDATE)
  if i >= 0:
    s = i + len(_CAPDATE)
    b[s:s + 19] = b"\x00" * 19
  return hashlib.sha256(bytes(b)).hexdigest()


def capture_fd(fn):
  """Run fn() capturing C++ (fd 1) stdout — the [cook] line is a C printf."""
  tmp = f"/tmp/tered_rt_cook_{os.getpid()}.txt"
  fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
  saved = os.dup(1)
  sys.stdout.flush(); _libc.fflush(None)
  os.dup2(fd, 1)
  try:
    r = fn()
  finally:
    _libc.fflush(None)
    os.dup2(saved, 1)
    os.close(saved); os.close(fd)
  with open(tmp) as f:
    return r, f.read()


def cook_counts(text):
  m = _COOK_RE.search(text)
  return tuple(int(x) for x in m.groups()) if m else None


FIXTURE_SWITCH = textwrap.dedent("""
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T

    class LoopSwitchHF(HeightField):
        HEIGHT_M = 2000.0
        def __init__(self, which="smooth", count=3, freq=6.0):
            super().__init__()
            h = T.Fbm(frequency=freq, octaves=6) * 0.5 + 0.5
            with T.loop(count, h=h) as L:
                L.h = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=6)
            smooth = T.lpf(L.h, cutoff=6.0)
            ridged = T.terrace(L.h, steps=8.0, sharpness=6.0)
            self.capture(T.switch(which, smooth=smooth, ridged=ridged), "height", cache=True)
""")

FIXTURE_LOOP = textwrap.dedent("""
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T

    class LoopOnlyHF(HeightField):
        HEIGHT_M = 2000.0
        def __init__(self, count=3, freq=6.0):
            super().__init__()
            h = T.Fbm(frequency=freq, octaves=6) * 0.5 + 0.5
            with T.loop(count, h=h) as L:
                L.h = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=6)
            self.capture(L.h, "height", cache=True)
""")


def _write_fixture(src, name):
  d = tempfile.mkdtemp(prefix="tered_rt_")
  p = os.path.join(d, name)
  with open(p, "w") as f:
    f.write(src)
  return p


def _find_scalar(doc):
  """First editable float param across the document (prefer 'frequency')."""
  best = None
  def walk(children):
    nonlocal best
    for ch in children:
      if isinstance(ch, DocNode):
        for (kind, name, value) in ch.editable_params():
          if isinstance(value, float) and not isinstance(value, bool):
            if name == "frequency":
              return (ch, kind, name, value)
            if best is None:
              best = (ch, kind, name, value)
      sub = getattr(ch, "children", None)
      if sub is not None:
        r = walk(sub)
        if r is not None:
          return r
    return None
  r = walk(doc._root)
  return r or best


def _first(cls_pred, doc):
  def walk(children):
    for ch in children:
      if cls_pred(ch):
        return ch
      sub = getattr(ch, "children", None)
      if sub is not None:
        r = walk(sub)
        if r is not None:
          return r
    return None
  return walk(doc._root)


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  sw_path = _write_fixture(FIXTURE_SWITCH, "fx_switch.py")
  lp_path = _write_fixture(FIXTURE_LOOP, "fx_loop.py")
  results = {}

  # ---- corpus load determinism (voronoi) ---------------------------------
  rt = TerrainRuntime(preview_dim=DIM)
  rt.load("voronoi")
  rt.set_context(ctx)
  rt.rebake(DIM); sha_v1 = masked_sha(rt.height_path)
  rt.rebake(DIM); sha_v2 = masked_sha(rt.height_path)
  det = (sha_v1 == sha_v2)
  print(f"[gA.load] voronoi rebake determinism: {det}  sha={sha_v1[:16]}", flush=True)
  results["load_determinism"] = det

  # ---- set_param scalar + revert (cache-assisted) ------------------------
  rt = TerrainRuntime(preview_dim=DIM)
  rt.load(sw_path, dsl_class="LoopSwitchHF", which="smooth", freq=_SALT)
  rt.set_context(ctx)
  rt.rebake(DIM); shaX = masked_sha(rt.height_path)
  node, kind, name, old = _find_scalar(rt.document)
  print(f"[gA.param] target = {node.clazz_name}.{name} ({kind}) = {old}", flush=True)
  node.set_param(kind, name, float(old) + 1.7)
  rt.rebake(DIM); shaY = masked_sha(rt.height_path)
  node.set_param(kind, name, float(old))                       # revert
  (_, revert_txt) = capture_fd(lambda: rt.rebake(DIM))
  shaX2 = masked_sha(rt.height_path)
  rc = cook_counts(revert_txt)
  param_ok = (shaY != shaX) and (shaX2 == shaX)
  cache_ok = (rc is not None and rc[0] > 0)                    # cache-loaded > 0 on revert
  print(f"[gA.param] X!=Y={shaY != shaX}  revert==X={shaX2 == shaX}  "
        f"revert cook(loaded,skipped,computed)={rc} -> cache-assisted={cache_ok}", flush=True)
  results["set_param_roundtrip"] = param_ok
  results["revert_cache_assisted"] = cache_ok

  # ---- switch: flip select -> matches a DIRECT ridged trace --------------
  rt_sm = TerrainRuntime(preview_dim=DIM)
  rt_sm.load(sw_path, dsl_class="LoopSwitchHF", which="smooth", freq=_SALT)
  rt_sm.set_context(ctx)
  rt_sm.rebake(DIM); sha_sm = masked_sha(rt_sm.height_path)
  sw = _first(lambda c: isinstance(c, DocSwitch), rt_sm.document)
  sw.select("ridged")
  rt_sm.rebake(DIM); sha_flip = masked_sha(rt_sm.height_path)
  rt_rg = TerrainRuntime(preview_dim=DIM)
  rt_rg.load(sw_path, dsl_class="LoopSwitchHF", which="ridged", freq=_SALT)
  rt_rg.set_context(ctx)
  rt_rg.rebake(DIM); sha_rg = masked_sha(rt_rg.height_path)
  switch_ok = (sha_flip != sha_sm) and (sha_flip == sha_rg)
  print(f"[gA.switch] flip-changed={sha_flip != sha_sm}  flip==direct_ridged={sha_flip == sha_rg} "
        f"-> {switch_ok}", flush=True)
  results["switch_select"] = switch_ok

  # ---- DocLoop.set_count N->N+k : tail-only recompute --------------------
  N, K = 3, 2
  rt_l = TerrainRuntime(preview_dim=DIM)
  rt_l.load(lp_path, dsl_class="LoopOnlyHF", count=N, freq=_SALT)
  rt_l.set_context(ctx)
  capture_fd(lambda: rt_l.rebake(DIM))                         # prime the first-N chain
  (_, txt_n) = capture_fd(lambda: rt_l.rebake(DIM))            # warm-N baseline
  loop = _first(lambda c: isinstance(c, DocLoop), rt_l.document)
  loop.set_count(N + K)
  (_, txt_nk) = capture_fd(lambda: rt_l.rebake(DIM))
  cn, cnk = cook_counts(txt_n), cook_counts(txt_nk)
  # tail-only: raising N->N+k reuses the first-N iterations' Merkle results (a first-N
  # cache point LOADS + its upstream demand-skips); only the k new tail iterations
  # recompute, plus the sink whose input moved from iteration N-1 to N+k-1. So computed
  # is K (or K+1 with the moved sink) — FAR below a cold bake of the full unrolled chain.
  loop_ok = (cn is not None and cnk is not None
             and cnk[0] >= 1                                   # a first-N cache point loads
             and K <= cnk[2] <= K + 1                          # only the k tail iters (+ moved sink)
             and cnk[2] < (N + K))                             # far below a cold full-chain bake
  print(f"[gA.loop] count={N} cook={cn}  count={N+K} cook={cnk} -> tail-only={loop_ok}", flush=True)
  results["loop_count_tail"] = loop_ok

  # ---- dim is part of the cook hash (preview vs larger cache separately) --
  (_, txt_dimA) = capture_fd(lambda: rt_l.rebake(DIM))         # warm at DIM
  (_, txt_dimB) = capture_fd(lambda: rt_l.rebake(DIM + 128))   # cold at a new dim
  ca, cb = cook_counts(txt_dimA), cook_counts(txt_dimB)
  dim_ok = (ca is not None and cb is not None
            and ca[2] <= 2                                     # warm at DIM: recompute just the sink
            and cb[2] >= 3)                                    # new dim recomputes the chain
  print(f"[gA.dim] dim={DIM} cook={ca}  dim={DIM+128} cook={cb} -> dim-in-hash={dim_ok}", flush=True)
  results["dim_in_cook_hash"] = dim_ok

  # ---- doc-JSON save -> load -> rebake sha-stable ------------------------
  rt_j = TerrainRuntime(preview_dim=DIM)
  rt_j.load(sw_path, dsl_class="LoopSwitchHF", which="smooth", freq=_SALT)
  rt_j.set_context(ctx)
  rt_j.rebake(DIM); sha_pre = masked_sha(rt_j.height_path)
  json_path = os.path.join(tempfile.mkdtemp(prefix="tered_rt_json_"), "doc.json")
  rt_j.save_doc_json(json_path)
  rt_r = TerrainRuntime(preview_dim=DIM)
  rt_r.load(json_path, extent_m=rt_j.extent_m, height_m=rt_j.height_m)
  rt_r.set_context(ctx)
  rt_r.rebake(DIM); sha_post = masked_sha(rt_r.height_path)
  json_ok = (sha_post == sha_pre)
  print(f"[gA.json] doc-JSON save/load/rebake sha-stable: {json_ok}  "
        f"pre={sha_pre[:16]} post={sha_post[:16]}", flush=True)
  results["doc_json_stable"] = json_ok

  ez.mainThreadEnd()
  ok = all(results.values())
  print(f"\n=== terrain runtime (gA) {'PASSED' if ok else 'FAILED'} ===", flush=True)
  for k, v in results.items():
    print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
