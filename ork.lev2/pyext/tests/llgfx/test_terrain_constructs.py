#!/usr/bin/env python3
###############################################################################
# S0 explicit terrain CONSTRUCTS gate (owner sign-off L-A/V-A/G-A/S-B). Proves:
#   g2  T.loop -> nested LoopModule + cook cache: raising count N -> N+k reuses the
#       first-N iterations' Merkle results (nested-cook counters: 0/N warm, K/N grown),
#       only the k new tail iterations recompute.
#   V-A L.i binds a per-iteration value into the LoopModule's iter-feed TABLE, fed to
#       that iteration's plug VALUE (params stay plugs — A8; never inlined into shader text).
#   G-A @T.group: each call site expands independently (two calls -> two subgraphs).
#   S-B T.switch: eager named branches; the selector picks one (different selector
#       -> different bake; same selector -> identical bake).
#   ops-self-defend: a loop-body node that flows to no carry is a LOUD error.
#   g4  raw Python for/while imports FLATTENED with a visible one-line notice.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, io, time, math, json, ctypes, contextlib, hashlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.doc import DocNode, DocGroupCall, DocSwitch, to_json, from_json

DIM = 256
_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"
_COOK_RE = re.compile(r"\[cook\] cacheable bake:\s+(\d+) cache-loaded,\s+(\d+) demand-skipped,\s+(\d+) computed")
_libc = ctypes.CDLL(None)

# unique per-run frequency salt so the g2 loop chain starts cold (frequency IS in
# the fbm Merkle cook hash).
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)


def masked_sha(path):
    with open(path, "rb") as f:
        b = bytearray(f.read())
    i = b.find(_CAPDATE)
    if i >= 0:
        b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
    return hashlib.sha256(bytes(b)).hexdigest()


def capture_fd(fn):
    """Run fn() capturing C++ (fd 1) stdout — the [cook] line is a C printf."""
    tmp = f"/tmp/tered_cook_{os.getpid()}.txt"
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    saved = os.dup(1)
    sys.stdout.flush(); _libc.fflush(None)
    os.dup2(fd, 1)
    try:
        fn()
    finally:
        _libc.fflush(None)
        os.dup2(saved, 1)
        os.close(saved); os.close(fd)
    with open(tmp) as f:
        return f.read()


def cook_counts(text):
    m = _COOK_RE.search(text)
    return tuple(int(x) for x in m.groups()) if m else None


# ---- construct fixtures -----------------------------------------------------

class LoopErode(HeightField):
    def __init__(self, count=3, freq=6.0):
        super().__init__()
        h = T.Fbm(frequency=freq, octaves=6) * 0.5 + 0.5
        with T.loop(count, h=h) as L:
            L.h = T.erode_thermal(L.h, talus_deg=32.0, rate=0.15, iterations=6)
        self.capture(L.h, "height", cache=True)


class LoopIndexTerrace(HeightField):
    def __init__(self, count=4):
        super().__init__()
        h = T.Fbm(frequency=5.0, octaves=5) * 0.5 + 0.5
        with T.loop(count, h=h) as L:
            # V-A: sharpness varies per iteration; terrace sets m.inputs.sharpness = float(...)
            L.h = T.terrace(L.h, step_m=1.0/6.0, sharpness=2.0 + L.i * 0.5)
        self.capture(L.h, "height", cache=True)


@T.group
def ridge_stack(h, gain=1.0):
    r = T.Fbm(frequency=8.0, octaves=4) * gain
    return h + r


class GroupTwice(HeightField):
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=3.0, octaves=5) * 0.5 + 0.5
        h = ridge_stack(h, gain=0.2)     # expansion #1
        h = ridge_stack(h, gain=0.4)     # expansion #2 (independent)
        self.capture(h, "height")


class SwitchHF(HeightField):
    def __init__(self, which="smooth"):
        super().__init__()
        base = T.Fbm(frequency=4.0, octaves=6) * 0.5 + 0.5
        smooth = T.lpf(base, cutoff_texels=6.0)
        ridged = T.terrace(base, step_m=1.0/8.0, sharpness=6.0)
        self.capture(T.switch(which, smooth=smooth, ridged=ridged), "height")


class OrphanLoop(HeightField):
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=4.0, octaves=4) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            _orphan = T.Fbm(frequency=2.0, octaves=2)   # flows to NO carry -> loud error
            L.h = T.erode_thermal(L.h, iterations=4)
        self.capture(L.h, "height")


class LiReversedKwargs(HeightField):
    # only sharpness uses L.i; steps is plain — reversed kwarg order must still bind
    # per-iteration sharpness (no silent collapse to the i=0 constant).
    def __init__(self, count=4):
        super().__init__()
        h = T.Fbm(frequency=5.0, octaves=5) * 0.5 + 0.5
        with T.loop(count, h=h) as L:
            L.h = T.terrace(L.h, sharpness=2.0 + L.i * 0.5, step_m=1.0/6.0)
        self.capture(L.h, "height", cache=True)


class LiSinStray(HeightField):
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=5.0) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            L.h = T.terrace(L.h, step_m=1.0/6.0, sharpness=math.sin(L.i))   # stray coercion
        self.capture(L.h, "height")


class LiPack(HeightField):
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=5.0) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            L.h = T.erode_thermal(L.h, talus_deg=30.0 + L.i * 1.0, iterations=4)  # pack path
        self.capture(L.h, "height")


class LiStashOverStash(HeightField):
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=5.0) * 0.5 + 0.5
        with T.loop(3, h=h) as L:
            # gradient sets dir = vec2(float(dir_x), float(dir_y)) — two L.i coercions
            # before one plug set => stash-over-stash.
            g = T.gradient(dir_x=L.i, dir_y=L.i)
            L.h = L.h + g
        self.capture(L.h, "height")


class RawForFlatten(HeightField):
    # g4 fixture: a DELIBERATE raw Python for-loop (not T.loop) — must import
    # FLATTENED with the visible one-line notice. Inline because the corpus
    # assets all use T.loop now (the notice AST-scans this __init__'s source).
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=5.0, octaves=4) * 0.5 + 0.5
        for _ in range(3):
            h = T.erode_thermal(h, talus_deg=32.0, rate=0.15, iterations=4)
        self.capture(h, "height")


def _bake(inst, tag, ctx):
    g = inst.generatedflow()
    inst.set_capture_path("height", f"/tmp/tered_ctor/{tag}.exr")
    lev2.terrain.bake_heightfield(g, ctx, DIM)
    return masked_sha(f"/tmp/tered_ctor/{tag}.exr")


def _bake_graph(g, tag, ctx):
    for cap in lev2.terrain.capture_modules(g):
        cap.path = f"/tmp/tered_ctor/{tag}.exr"
    lev2.terrain.bake_heightfield(g, ctx, DIM)
    return masked_sha(f"/tmp/tered_ctor/{tag}.exr")


def run(ez, ctx):
    os.makedirs("/tmp/tered_ctor", exist_ok=True)
    results = {}

    # ---- g2: loop count-growth cache reuse (composite LoopModule) ----------
    # A T.loop now elaborates to a LoopModule whose per-iteration body cooks NESTED —
    # so the recompute/load accounting lives in the nested-cook counters (the host
    # [cook] line only sees the loop NODE), read via last_subgraph_cook_counts().
    N, K = 3, 2
    _bake(LoopErode(count=N, freq=_SALT), "loopN_prime", ctx)   # warm the first-N chain (cold)
    _bake(LoopErode(count=N, freq=_SALT), "loopN", ctx)
    warm = lev2.terrain.last_subgraph_cook_counts()             # (computes, loads)
    _bake(LoopErode(count=N + K, freq=_SALT), "loopNK", ctx)
    grown = lev2.terrain.last_subgraph_cook_counts()
    print(f"[g2] count={N}  nested(computes,loads)={warm}", flush=True)
    print(f"[g2] count={N+K} nested(computes,loads)={grown}", flush=True)
    # warm-N: every iteration cache-hits (0 computes, N loads). Growing by K: the first N
    # iterations load (same per-iteration salts — salt excludes count), only the K new tail
    # iterations recompute -> (K computes, N loads). The exact N->N+k Merkle reuse.
    g2_ok = (warm == (0, N)) and (grown == (K, N))
    print(f"[g2] loop cache reuse: {g2_ok}", flush=True)
    results["g2_loop_cache"] = g2_ok

    # ---- V-A: L.i per-iteration plug value (LoopModule feed TABLE) ---------
    # The loop no longer unrolls to loop_0/i{k}/terr_0 — the body is ONE nested module
    # (loop_0.subgraph/terr_0) and the per-iteration sharpness lives in the LoopModule's
    # iter-feed value TABLE (A8: still a plug value the C++ feeds each iteration).
    li = LoopIndexTerrace(count=4)
    g_li = li.generatedflow()
    loop_m = g_li.findModule("loop_0")
    terr_present = (loop_m is not None and loop_m.subgraph is not None
                    and loop_m.subgraph.findModule("terr_0") is not None)
    feed = next((f for f in loop_m.iter_feeds
                 if f.inner_module == "terr_0" and f.inner_plug == "sharpness"), None) if loop_m else None
    sharps = [round(v, 4) for v in feed.values] if feed else None
    li_ok = terr_present and (sharps == [2.0, 2.5, 3.0, 3.5])   # 2.0 + k*0.5 per iteration
    print(f"[V-A] nested terr present={terr_present} feed table={sharps} -> {li_ok}", flush=True)
    results["li_per_iter"] = li_ok

    # ---- G-A: @T.group two independent expansions -------------------------
    gt = GroupTwice()
    g_gt = gt.generatedflow()
    calls = [c for c in gt.document()._root if isinstance(c, DocGroupCall)]
    group_ok = (len(calls) == 2) and (calls[0].path != calls[1].path)
    for call in calls:
        kids = [ch.local_name for ch in call.children if isinstance(ch, DocNode)]
        # each call's inner nodes materialize under ITS distinct path prefix.
        group_ok = group_ok and len(kids) > 0 and all(
            g_gt.findModule(f"{call.path}/{ln}") is not None for ln in kids)
    print(f"[G-A] group calls={[c.path for c in calls]} independent-expand -> {group_ok}", flush=True)
    results["group_expand"] = group_ok

    # ---- S-B: T.switch selection ------------------------------------------
    sha_sm = _bake(SwitchHF(which="smooth"), "sw_smooth", ctx)
    sha_rg = _bake(SwitchHF(which="ridged"), "sw_ridged", ctx)
    sha_rg2 = _bake(SwitchHF(which="ridged"), "sw_ridged2", ctx)
    sw_ok = (sha_sm != sha_rg) and (sha_rg == sha_rg2)
    print(f"[S-B] switch smooth!=ridged={sha_sm != sha_rg} ridged==ridged={sha_rg == sha_rg2} -> {sw_ok}", flush=True)
    results["switch_select"] = sw_ok

    # ---- FIX 1: switch survives doc-JSON; elaborate re-selects ------------
    # (a document loaded from doc-JSON has NO source to re-trace — elaborate is the
    # selection authority; flipping DocSwitch.selected re-routes the other branch.)
    src = SwitchHF(which="smooth"); src.generatedflow()
    doc_l = from_json(json.loads(json.dumps(to_json(src.document()))))
    sw = [c for c in doc_l._root if isinstance(c, DocSwitch)][0]
    g_sm, _ = doc_l.elaborate()
    sha_load_sm = _bake_graph(g_sm, "sw_rt_smooth", ctx)
    sw.selected = "ridged"                        # flip on the JSON-loaded document
    g_rg, _ = doc_l.elaborate()
    sha_load_rg = _bake_graph(g_rg, "sw_rt_ridged", ctx)
    rt_ok = (sha_load_sm != sha_load_rg          # flip changed the bake
             and sha_load_sm == sha_sm           # as-loaded matches original smooth
             and sha_load_rg == sha_rg)          # flipped matches a direct ridged trace
    print(f"[FIX1] doc-JSON switch: loaded_smooth==direct_smooth={sha_load_sm == sha_sm} "
          f"flip->ridged==direct_ridged={sha_load_rg == sha_rg} "
          f"flip-changed-bake={sha_load_sm != sha_load_rg} -> {rt_ok}", flush=True)
    results["switch_docjson_reselect"] = rt_ok

    # ---- ops-self-defend: loop orphan is a LOUD error ---------------------
    orphan_ok = False
    try:
        OrphanLoop()
        print("[orphan] ERROR: no exception raised", flush=True)
    except RuntimeError as e:
        orphan_ok = "orphan" in str(e).lower() and "fbm" in str(e).lower()
        print(f"[orphan] raised: {e}", flush=True)
    results["orphan_loud"] = orphan_ok

    # ---- FIX 2: L.i pending hygiene — LOUD, never a silent i=0 collapse ----
    def _raises(cls):
        try:
            cls()
            return False
        except (RuntimeError, TypeError):
            return True
    # (i) multi-scalar-param op, reversed kwargs, only one uses L.i: still binds
    # per-iteration (no silent collapse to the i=0 constant).
    g_lir = LiReversedKwargs(count=4).generatedflow()
    loop_lir = g_lir.findModule("loop_0")
    feed_lir = next((f for f in loop_lir.iter_feeds
                     if f.inner_module == "terr_0" and f.inner_plug == "sharpness"), None) if loop_lir else None
    rvals = [round(v, 4) for v in feed_lir.values] if feed_lir else None
    li_multi_ok = (rvals == [2.0, 2.5, 3.0, 3.5])
    sin_ok = _raises(LiSinStray)      # (ii) math.sin(L.i) stray coercion
    pack_ok = _raises(LiPack)         # (iii) L.i through the ParamPack path
    sos_ok = _raises(LiStashOverStash)  # (a) two coercions before a plug set
    print(f"[FIX2] reversed-kwargs per-iter={rvals}->{li_multi_ok}  "
          f"sin-raises={sin_ok}  pack-raises={pack_ok}  stashx2-raises={sos_ok}", flush=True)
    results["li_reversed_kwargs"] = li_multi_ok
    results["li_sin_raises"] = sin_ok
    results["li_pack_raises"] = pack_ok
    results["li_stashover_raises"] = sos_ok

    # ---- g4: raw-for flatten notice (trace-only, python print) -------------
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        RawForFlatten().generatedflow()
    notice = "flattened on import" in buf.getvalue()
    print(f"[g4] raw-for flatten notice emitted: {notice}", flush=True)
    print(f"     notice: {buf.getvalue().strip()[:160]}", flush=True)
    results["g4_flatten_notice"] = notice

    return results


def main():
    ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = all(results.values())
    print(f"\n=== terrain constructs {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
        print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
