#!/usr/bin/env python3
###############################################################################
# JUL09 S1.x gate — DSL constructor kwargs as editable "Terrain Parameters".
#
# The motivating case (warp.py): frequency / octaves / ring_amp_m / ring_period_m
# / center are ctor kwargs captured into a ptex3d hfbake closure — the document has
# no plug-backed params for them, so a plain propsheet is empty. TerrainRuntime now
# records the DSL class + current kwargs and exposes editable_dsl_kwargs(); a
# set_dsl_kwarg RE-TRACES the class (a fresh document, L2) and the rebake oracle bakes
# it. This gate proves (headless, bake oracle):
#   * load("warp") -> editable_dsl_kwargs lists the 5 with correct typed defaults.
#   * set_dsl_kwarg("ring_amp_m", 500.0) -> document REPLACED -> rebake sha CHANGES.
#   * set it back -> sha RETURNS and the revert bake is cache-assisted (cook loaded>0).
#   * a vec-ish kwarg (center tuple) coerces + re-traces; an unknown kwarg raises LOUD.
#   * a doc-JSON session returns EMPTY kwargs (feature absent, not erroring).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, ctypes, hashlib, tempfile, textwrap, time

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.editor.terrain_runtime import TerrainRuntime
from ork.hypergraph.dflow.terrain.doc import TerrainDocParamError

# SWEEP_DIM is the reduced resolution for the REPEATED byte-identity / cook-cache
# permutation bakes (set -> revert, prime -> warm -> edit -> revert): all the identity
# assertions are resolution-independent, so the sweeps run small. DIM is exercised once,
# by the single full-resolution smoke bake, so the full-dim path stays covered.
DIM = 256
SWEEP_DIM = 128
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)

# a T.* (Merkle-cached) terrain, ctor-parameterized by freq — proves the kwargs-edit
# path integrates with the disk cook cache (a set_dsl_kwarg revert cache-LOADS). warp's
# own hfbake bake uses a demand-skip single output slot (no multi-version Merkle load),
# so its revert recomputes 2 cheap ptex3d nodes — quoted, not asserted, below.
FIXTURE_LOOP = textwrap.dedent("""
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T

    class KwLoopHF(HeightField):
        HEIGHT_M = 2000.0
        def __init__(self, count=3, freq=6.0):
            super().__init__()
            h = T.Fbm(frequency=freq, octaves=6) * 0.5 + 0.5
            with T.loop(count, h=h) as L:
                L.h = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=6)
            self.capture(L.h, "height", cache=True)
""")
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"
_COOK_RE = re.compile(r"\[cook\] cacheable bake:\s+(\d+) cache-loaded,\s+(\d+) demand-skipped,\s+(\d+) computed")
_libc = ctypes.CDLL(None)


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
  tmp = f"/tmp/tered_kw_cook_{os.getpid()}.txt"
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


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  # ALL work runs under try/finally so a failed assertion / stale-API error dies LOUD
  # (traceback to the console) with the engine ALWAYS torn down — otherwise the update
  # thread never joins and the process wedges in the exit handler (the filed hang).
  results = {}
  try:
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    # ---- editable_dsl_kwargs lists warp's 5 ctor kwargs with correct defaults ----
    rt = TerrainRuntime(preview_dim=SWEEP_DIM)
    rt.load("warp")
    rt.set_context(ctx)
    kw = rt.editable_dsl_kwargs()
    expect = {"frequency": 3.0, "octaves": 2, "ring_amp_m": 50.0,
              "ring_period_m": 250.0, "center": (0.0, 0.0)}
    kwargs_ok = (set(kw.keys()) == set(expect.keys())
                 and all(kw[k] == expect[k] and type(kw[k]) is type(expect[k]) for k in expect))
    print(f"[kw.list] editable_dsl_kwargs={kw} -> {kwargs_ok}", flush=True)
    results["editable_kwargs_list"] = kwargs_ok

    # ---- set_dsl_kwarg -> document replaced -> rebake sha CHANGES ----------------
    # The set/revert byte-identity sweep bakes at SWEEP_DIM (the shas are resolution-
    # independent); a single full-dim smoke below covers the full-resolution path.
    doc0 = rt.document
    rt.rebake(SWEEP_DIM); sha_a = masked_sha(rt.height_path)
    rt.set_dsl_kwarg("ring_amp_m", 500.0)
    doc1 = rt.document
    replaced = (doc1 is not doc0) and (rt.editable_dsl_kwargs()["ring_amp_m"] == 500.0)
    rt.rebake(SWEEP_DIM); sha_b = masked_sha(rt.height_path)
    changed = (sha_b != sha_a)
    print(f"[kw.set] doc_replaced={replaced} sha_changed={changed} "
          f"a={sha_a[:12]} b={sha_b[:12]}", flush=True)
    results["set_replaces_document"] = replaced
    results["rebake_sha_changes"] = changed

    # ---- set it back -> sha RETURNS (faithful re-trace) -------------------------
    # warp bakes via hfbake (ptex3d expr) using a DEMAND-SKIP single output slot, so the
    # revert recomputes 2 cheap nodes (no multi-version Merkle load) — the counts are
    # QUOTED here; the Merkle cache-assist is proven on a T.* terrain below.
    rt.set_dsl_kwarg("ring_amp_m", 50.0)
    (_, revert_txt) = capture_fd(lambda: rt.rebake(SWEEP_DIM))
    sha_a2 = masked_sha(rt.height_path)
    rc = cook_counts(revert_txt)
    returns = (sha_a2 == sha_a)
    print(f"[kw.revert] warp sha_returns={returns} revert cook(loaded,skipped,computed)={rc} "
          f"(warp uses demand-skip; not a Merkle load)", flush=True)
    results["revert_sha_returns"] = returns

    # ---- ONE full-dim case: the full-resolution bake produces a height ----------
    rt.rebake(DIM)
    fulldim_ok = (rt.height_path is not None and os.path.getsize(rt.height_path) > 0)
    print(f"[kw.fulldim] full-dim({DIM}) bake -> {rt.height_path} size>0={fulldim_ok}",
          flush=True)
    results["fulldim_bake"] = fulldim_ok

    # ---- vec-ish kwarg (center tuple) coerces + re-traces ------------------------
    before_center_doc = rt.document
    coerced = rt.set_dsl_kwarg("center", (100.0, -50.0))
    center_ok = (coerced == (100.0, -50.0)
                 and rt.editable_dsl_kwargs()["center"] == (100.0, -50.0)
                 and rt.document is not before_center_doc)
    rt.set_dsl_kwarg("center", (0.0, 0.0))         # restore
    print(f"[kw.vec] center coerced={coerced} -> {center_ok}", flush=True)
    results["vecish_tuple_kwarg"] = center_ok

    # ---- unknown kwarg raises LOUD ----------------------------------------------
    raised = False
    try:
      rt.set_dsl_kwarg("does_not_exist", 1.0)
    except TerrainDocParamError:
      raised = True
    print(f"[kw.unknown] set_dsl_kwarg(unknown) raised={raised}", flush=True)
    results["unknown_kwarg_raises"] = raised

    # ---- doc-JSON session returns EMPTY kwargs (feature absent) ------------------
    # heights are TRUE METERS end-to-end (natural units) — there is no vertical-scale
    # override, so load() takes only extent_m for a doc-JSON session.
    json_path = os.path.join(tempfile.mkdtemp(prefix="tered_kw_json_"), "warp_doc.json")
    rt.save_doc_json(json_path)
    rt_j = TerrainRuntime(preview_dim=SWEEP_DIM)
    rt_j.load(json_path, extent_m=rt.extent_m)
    empty = (rt_j.editable_dsl_kwargs() == {})
    raised_json = False
    try:
      rt_j.set_dsl_kwarg("frequency", 5.0)
    except TerrainDocParamError:
      raised_json = True
    json_ok = empty and raised_json
    print(f"[kw.json] doc-JSON editable_dsl_kwargs empty={empty} set raises={raised_json} "
          f"-> {json_ok}", flush=True)
    results["docjson_empty_kwargs"] = json_ok

    # ---- kwargs-edit path integrates with the Merkle cook cache (T.* terrain) ----
    # set_dsl_kwarg re-traces the WHOLE graph; on a Merkle-cached terrain a revert to the
    # prior value cache-LOADS (loaded>0) — the kwargs edit is a first-class cook-cache client.
    # SWEEP_DIM keeps the (cold, salted) erode_thermal permutation bakes cheap.
    lp_dir = tempfile.mkdtemp(prefix="tered_kw_loop_")
    lp_path = os.path.join(lp_dir, "kwloop.py")
    with open(lp_path, "w") as f:
      f.write(FIXTURE_LOOP)
    rt_l = TerrainRuntime(preview_dim=SWEEP_DIM)
    rt_l.load(lp_path, dsl_class="KwLoopHF", freq=_SALT)
    rt_l.set_context(ctx)
    capture_fd(lambda: rt_l.rebake(SWEEP_DIM))                     # prime freq=SALT
    capture_fd(lambda: rt_l.rebake(SWEEP_DIM))                     # warm
    rt_l.set_dsl_kwarg("freq", _SALT + 2.0)
    capture_fd(lambda: rt_l.rebake(SWEEP_DIM))                     # edit -> cold tail
    rt_l.set_dsl_kwarg("freq", _SALT)                              # revert
    (_, rv_txt) = capture_fd(lambda: rt_l.rebake(SWEEP_DIM))
    rlc = cook_counts(rv_txt)
    merkle_ok = (rlc is not None and rlc[0] > 0)                   # cache-loaded > 0
    print(f"[kw.cache] T.* kwargs-edit revert cook(loaded,skipped,computed)={rlc} "
          f"-> cache-assisted={merkle_ok}", flush=True)
    results["kwarg_edit_cache_assisted"] = merkle_ok

    ok = all(results.values())
    print(f"\n=== terrain DSL-kwargs (S1.x) {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
      print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    sys.exit(0 if ok else 1)
  finally:
    ez.mainThreadEnd()
    ecs.headless_exit()


main()
