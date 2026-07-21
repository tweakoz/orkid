#!/usr/bin/env ork.python
###############################################################################
# E4_SKELETON_SPEC.md S6 gate — the missing #33 poke/evict oracle: no existing test
# exercises poke -> evict -> re-cook on a CACHEABLE LIVE hypermesh graph
# (test_hypermesh_cookcache.py covers only the re-authored-warm path;
# test_hypermesh_paramsink.py covers a plain plug poke with no cook-cache in play).
#
#   1. COLD:  materialize_live() a cacheable box->subdivide->assign_gid chain ->
#      every node COMPUTED + STORED.
#   2. WARM:  re-authoring the SAME chain (fresh python objects -> fresh uuids)
#      restores every node from <staging>/dflowcache (content-only hash contract).
#   3-5. POKE/EVICT: a SECOND fixture forks ONE shared box into TWO consumers in ONE
#      graph — a poke-target chain A (subdivide->assign_gid) plus an untouched sibling
#      chain B (assign_gid, fed by the SAME box, no edge to A) — so ONE
#      materialize_live() warm-loads BOTH into the SAME _cookLoaded epoch. Poking A's
#      mid-chain `level` int plug between live.recompute() calls must (a) un-freeze A +
#      its Merkle-downstream (_evictPokedCookNodes, hmdflow.cpp:608-640) — the
#      re-cooked terminal reflects the new level (a face-count change: the
#      onTopologyReady topology cascade re-running per hmdflow.cpp:675-695, not just a
#      value tweak) — while (b) B's disk-cached identity stays completely unperturbed
#      (recompute() never calls _cookStorePass — only materialize/materialize_live do).
#      [DEVIATION: the spec calls for a fully DISJOINT second chain (its own root). Two
#      independent roots turn out to make hypermesh's "last producing module" terminal
#      pick (bakeMesh/materializeLive, hmdflow.cpp) genuinely NON-DETERMINISTIC across
#      process runs — DgSorter enqueues tied-depth roots in std::set<shared_ptr> order,
#      which is allocation-address-dependent, not authoring-order — confirmed by hand
#      (~50/50 flip across repeated runs of an otherwise-unchanged two-root fixture; see
#      report). A SHARED root with two consumers avoids that: fan-out order comes off a
#      plain push_back connection list (OutPlugData::_connections), which IS
#      deterministic. This still exercises the exact property #33 needs — a sibling
#      consumer of a NON-evicted ancestor must not be swept into eviction — just via a
#      fork instead of two disjoint roots; reported as a separate hypermesh finding.
#      (b) also cannot be read off the SAME live instance directly: no python binding
#      exposes a LiveHypermesh's non-terminal outputs, and (at the time this fixture was
#      first written) the one verb that combines two independent mesh chains into one
#      terminal (hm.merge) was unusable here too — MergeMeshModuleInst.cookLoad() hard-set
#      _built=true, and both its compute() and onTopologyReady() early-returned while
#      _built stayed true, so a warm-loaded merge whose input was evicted by an upstream
#      #33 poke never rebuilt — the merged terminal stayed frozen at the pre-poke content
#      forever. Fixed (MeshComputeInst::onCookEvicted(), a virtual eviction hook the
#      driver calls on every freshly-evicted inst; MergeMesh's override resets _built) and
#      now exercised directly below in step 8. So (b) here is STILL checked via B's DISK
#      identity, not the merge join: a standalone re-materialization of B's exact content,
#      taken before and after A's poke, must warm-hit the SAME cache entries and dump
#      byte-identical.]
#   6. GPUUPDATE SEAM: lev2.dflow.gpuUpdate(graph, ctx) (the renamed recook() seam) on
#      the poked graph re-dispatches an independent one-shot bake — its output must be
#      byte-identical to the live graph's settled (post-poke) state.
#   8. MERGE-DOWNSTREAM EVICTION: the case (b)'s narration above flagged as unusable is now
#      the thing under test — a THIRD fixture forks the shared root into chain A
#      (subdivide, the poke target) and sibling B (assign_gid, untouched), terminating in
#      merge(A, B). MergeMeshModuleInst.cookLoad() hard-sets _built=true on the warm load;
#      poking A's `level` must evict A + cascade-evict the merge (case (b) above), and the
#      merge must reset ITS OWN _built (onCookEvicted) so onTopologyReady rebuilds instead
#      of skipping — else the merged terminal stays frozen at the pre-poke face count forever.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import struct
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh

# unique-per-run identity (leftover disk state from an earlier gate run can't fake a hit) +
# non-overlapping integer bands so the 2 fixtures below never accidentally share a content hash.
SALT    = struct.unpack("<I", os.urandom(4))[0] % 1000
SIZE    = 1.0 + SALT * 1e-4   # SimpleChain
SIZE_PF = 3.0 + SALT * 1e-4   # PokeFixture's shared root box / BOnly
SIZE_MF = 5.0 + SALT * 1e-4   # MergeFixture's shared root box
GID_A   = 5
GID_B   = 9


class SimpleChain(Hypermesh):
  """steps 1-2: the plain 3-node cacheable chain (cookcache test's pattern, via materialize_live)."""
  def __init__(self):
    super().__init__()
    b = self.box(size=SIZE)
    s = self.subdivide(b, level=1)
    g = self.assign_gid(s, gid=GID_A)
    self.output(g)


class PokeFixture(Hypermesh):
  """steps 3-5: ONE shared root box forks into chain A (subdivide->assign_gid, the poke
  target) and sibling chain B (assign_gid, never touched) -- exercises
  _evictPokedCookNodes' cascade scoping: poking A must not evict B, a fellow consumer of
  the SAME non-evicted ancestor. `assign_gid_B` is connected FIRST and `subdivide` SECOND
  so chain A -- the poke target -- is deterministically the readable terminal (the
  "last producing module" pick walks a plug's OutPlugData::_connections in that push_back
  order, unlike two independent roots -- see the file header)."""
  def __init__(self, level=1):
    super().__init__()
    root = self.box(size=SIZE_PF)
    self.term_b = self.assign_gid(root, gid=GID_B)     # connected FIRST -> untouched sibling
    self.subdiv = self.subdivide(root, level=level)     # connected SECOND: POKE TARGET
    self.term_a = self.assign_gid(self.subdiv, gid=GID_A)
    self.output(self.term_a)


class BOnly(Hypermesh):
  """B's exact content in isolation (same root size + gid as PokeFixture's sibling chain) --
  its materialize_live() terminal IS directly readable, unlike B's instance inside
  PokeFixture (no python binding exposes a LiveHypermesh's non-terminal outputs)."""
  def __init__(self):
    super().__init__()
    b = self.box(size=SIZE_PF)
    g = self.assign_gid(b, gid=GID_B)
    self.output(g)


class MergeFixture(Hypermesh):
  """step 8: shared root forks into chain A (subdivide, the POKE TARGET) and sibling B
  (assign_gid, untouched), terminating in merge(A, B) -- merge is unambiguously the topo-last
  node (it depends on both branches), so no connection-order trick is needed to make it the
  readable terminal (contrast PokeFixture above)."""
  def __init__(self, level=1):
    super().__init__()
    root     = self.box(size=SIZE_MF)
    self.a   = self.subdivide(root, level=level)              # POKE TARGET
    b        = self.assign_gid(root, gid=GID_B)                # untouched sibling
    self.m   = self.merge(self.a, b, gid_a=GID_A, gid_b=None)  # gid_b=None preserves B's stamp
    self.output(self.m)


def _obj(mesh, ctx, tag):
  path = "/tmp/hm_pokeevict_%s.obj" % tag
  lev2.hypermesh.dump_obj(mesh, ctx, path)
  with open(path) as f:
    return f.read()


def _gids(hm, mesh, ctx):
  return set((t >> 20) & 0xFFF for t in hm.read_face_tags(mesh, ctx))


def _body(ezapp, ctx, hm):

  ##############################################################
  # 1) COLD -- materialize_live(): a simple cacheable chain, everything computes+stores
  ##############################################################
  g1 = SimpleChain().generatedflow()
  assert g1.cacheable, "DSL did not mark the static graph cacheable"
  live1 = hm.materialize_live(g1, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 0, "cold run claims %d hits (unique-per-run salt broken?)" % hits
  assert stores == 3, "cold run stored %d nodes, expected 3 (box/subdiv/gid)" % stores
  print("#33 step1 COLD PASS (3 computed+stored, %d faces)" % live1.mesh.num_faces, flush=True)

  ##############################################################
  # 2) WARM -- re-authored (fresh uuids) -> every node loads via materialize_live()
  ##############################################################
  g2 = SimpleChain().generatedflow()
  live2 = hm.materialize_live(g2, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 3, "warm run hit %d/3 — content-only hashing broken (uuid leak?)" % hits
  assert stores == 0, "warm run stored %d nodes, expected 0" % stores
  print("#33 step2 WARM PASS (3/3 loaded via materialize_live)", flush=True)

  ##############################################################
  # 3) POKE-FIXTURE COLD -- the shared root + both forked consumers (4 nodes) compute+store
  ##############################################################
  pf1 = PokeFixture(level=1)
  gpf1 = pf1.generatedflow()
  assert gpf1.cacheable, "DSL did not mark the poke fixture cacheable"
  livepf1 = hm.materialize_live(gpf1, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 0, "poke-fixture cold claims %d hits" % hits
  assert stores == 4, "poke-fixture cold stored %d nodes, expected 4 (root/subdivA/gidA/gidB)" % stores
  assert livepf1.mesh.num_faces == 24, "level=1 terminal has %d faces, expected 24 (6*4**1)" % livepf1.mesh.num_faces
  print("#33 step3a POKE-FIXTURE COLD PASS (4 computed+stored, terminal=chainA 24 faces)", flush=True)

  # B's disk identity, captured now (the COLD bake above just wrote it) -- the pre-poke reference.
  bref1 = BOnly().generatedflow()
  livebref1 = hm.materialize_live(bref1, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 2 and stores == 0, \
      "BOnly should warm-hit the poke-fixture's sibling-B entries: hits=%d stores=%d" % (hits, stores)
  obj_b_ref = _obj(livebref1.mesh, ctx, "bref_pre")

  ##############################################################
  # 4) POKE-FIXTURE WARM -- re-authored (fresh uuids); this is the live graph we poke
  ##############################################################
  pf2 = PokeFixture(level=1)
  gpf2 = pf2.generatedflow()
  live_pf2 = hm.materialize_live(gpf2, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 4 and stores == 0, \
      "poke-fixture warm hit %d/4 stored %d — both forked consumers must share one cook-load epoch" % (hits, stores)
  assert live_pf2.mesh.num_faces == 24
  tags_warm = _gids(hm, live_pf2.mesh, ctx)
  assert tags_warm == {GID_A}, "warm terminal carries wrong gid: %s" % tags_warm
  obj_warm = _obj(live_pf2.mesh, ctx, "pf_warm")
  print("#33 step3b POKE-FIXTURE WARM PASS (4/4 loaded, terminal=chainA)", flush=True)

  ##############################################################
  # 5) POKE a mid-chain input plug BETWEEN recomputes -> evict + Merkle-downstream + cascade
  ##############################################################
  pf2.subdiv.inputs.level = 2
  live_pf2.recompute(ctx)
  obj_poked = _obj(live_pf2.mesh, ctx, "pf_poked")
  assert obj_poked != obj_warm, "poke had no observable effect on the terminal — eviction didn't fire"
  assert live_pf2.mesh.num_faces == 96, \
      "poked terminal has %d faces, expected 96 (6*4**2) — onTopologyReady cascade didn't re-run post-eviction" \
      % live_pf2.mesh.num_faces
  tags_poked = _gids(hm, live_pf2.mesh, ctx)
  assert tags_poked == {GID_A}, "poked terminal carries wrong gid: %s" % tags_poked
  print("#33 step4+5 POKE PASS (level 1->2: 24->96 faces, terminal reflects the poke, cascade re-ran)", flush=True)

  ##############################################################
  # 6) the UNTOUCHED sibling's disk identity stays unperturbed by A's poke
  ##############################################################
  bref2 = BOnly().generatedflow()
  livebref2 = hm.materialize_live(bref2, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 2 and stores == 0, \
      "sibling B's disk cache entries were disturbed by A's poke: hits=%d stores=%d" % (hits, stores)
  obj_b_after = _obj(livebref2.mesh, ctx, "bref_post")
  assert obj_b_after == obj_b_ref, "sibling B's content diverged across A's poke (untouched consumer got swept in)"
  print("#33 step3c UNTOUCHED-SIBLING PASS (B's disk identity + geometry byte-identical across A's poke)", flush=True)

  ##############################################################
  # 7) the gpuUpdate() seam reproduces the poked live graph's settled state
  ##############################################################
  mesh_gu = lev2.dflow.gpuUpdate(gpf2, ctx)
  obj_gu = _obj(mesh_gu, ctx, "pf_gpuupdate")
  assert obj_gu == obj_poked, "gpuUpdate(poked graph) diverged from the live graph's settled state"
  tags_gu = _gids(hm, mesh_gu, ctx)
  assert tags_gu == tags_poked, "gpuUpdate face tags diverged from the live graph's settled state"
  print("#33 step6 GPUUPDATE PASS (gpuUpdate(graph, ctx) byte-identical to the live settled state)", flush=True)

  ##############################################################
  # 8) MERGE-DOWNSTREAM EVICTION -- poking A through a merge() terminal
  ##############################################################
  mf1 = MergeFixture(level=1)
  gmf1 = mf1.generatedflow()
  assert gmf1.cacheable, "DSL did not mark the merge fixture cacheable"
  livemf1 = hm.materialize_live(gmf1, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 0, "merge-fixture cold claims %d hits" % hits
  assert stores == 4, "merge-fixture cold stored %d nodes, expected 4 (root/subdivA/gidB/merge)" % stores
  assert livemf1.mesh.num_faces == 30, \
      "level=1 merge terminal has %d faces, expected 30 (24 subdiv + 6 box)" % livemf1.mesh.num_faces
  print("#33 step8a MERGE-FIXTURE COLD PASS (4 computed+stored, terminal=merge 30 faces)", flush=True)

  mf2 = MergeFixture(level=1)
  gmf2 = mf2.generatedflow()
  live_mf2 = hm.materialize_live(gmf2, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 4 and stores == 0, \
      "merge-fixture warm hit %d/4 stored %d — every node (incl. the merge) must warm-load" % (hits, stores)
  assert live_mf2.mesh.num_faces == 30
  obj_mf_warm = _obj(live_mf2.mesh, ctx, "mf_warm")
  print("#33 step8b MERGE-FIXTURE WARM PASS (4/4 loaded, terminal=merge)", flush=True)

  mf2.a.inputs.level = 2
  live_mf2.recompute(ctx)
  obj_mf_poked = _obj(live_mf2.mesh, ctx, "mf_poked")
  assert obj_mf_poked != obj_mf_warm, \
      "poke through the merge had no observable effect — the merge never un-froze (onCookEvicted missing?)"
  assert live_mf2.mesh.num_faces == 102, \
      "poked merge terminal has %d faces, expected 102 (96 subdiv + 6 box) — merge stayed frozen at cook-load content" \
      % live_mf2.mesh.num_faces
  tags_mf_poked = _gids(hm, live_mf2.mesh, ctx)
  assert tags_mf_poked == {GID_A, GID_B}, "poked merge terminal carries wrong gids: %s" % tags_mf_poked
  print("#33 step8c MERGE-DOWNSTREAM POKE PASS (level 1->2 THROUGH merge: 30->102 faces, terminal un-froze)", flush=True)

  # verdict table printed above, BEFORE teardown (known: offscreen EzApp teardown can SIGSEGV on
  # exit on some linux fleet nodes -- a filed LoaderThread race, unrelated to this test -- so the gate reads PASS
  # off these printed lines, not the process rc).
  ezapp.mainThreadEnd()
  print("=== hypermesh poke/evict #33 oracle PASSED ===", flush=True)
  ecs.headless_exit()
  sys.exit(0)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  hm = lev2.hypermesh
  try:
    _body(ezapp, ctx, hm)
  except SystemExit:
    raise                       # _body already tore down; don't double-teardown
  except BaseException:
    import traceback; traceback.print_exc()
    # ALWAYS tear down — a skipped headless_exit spins the process forever
    # (the coreappexit teardown trap)
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    sys.exit(1)


if __name__ == "__main__":
  main()
