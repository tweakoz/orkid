#!/usr/bin/env ork.python

################################################################################
# test_dflowedit — headless model gates for the standalone dflow editor (JUL13 E3).
#
# Drives the SHELL's own family loaders (ork.editor.dflowedit) headless — no window —
# and asserts the resulting C0 node models + E2 property models:
#
#   Gate 1 (GraphData family): a .orj particle graph identical in shape to the shipped
#     test target (ork.data/tests/newrefl/particles/test1.orj — 8 nodes + 8 edges) is
#     built in-code, serialized to a .orj, then run through the SHELL's own _load_graphdata
#     loader: the GraphDataNodeGraphModel enumerates 8 nodes + 8 edges, exposes plug
#     metadata for a known module (GRAV), can_connect accepts a like-typed pair + rejects
#     a mismatch, the display flag round-trips through _output_node, and node positions
#     round-trip through the graph-level _editor_layout after a set_pos + Save (.orj
#     re-serialize) + reload.
#
#     NOTE the shipped test1.orj carries plug transform CHAINS the in-code stand-in does
#     not; deserializing its exact on-disk bytes SEGFAULTS the jul17 gate binary at
#     Object.deserializeJson (an ENGINE deserializer crash, before any shell code runs —
#     confirmed by an isolated probe; the in-code 8-node/8-edge graph round-trips CLEANLY).
#     Point DFLOWEDIT_TEST1_ORJ at a real test1.orj on a binary that deserializes it to
#     run the loader against the shipped asset directly (the loader path is IDENTICAL).
#
#   Gate 2 (particles-DSL leg): a particles DSL asset (CODE, resolved on the particles search
#     path) routes through _load_particles (resolve -> load_dsl_class -> generatedflow() ->
#     the SAME GraphData machinery as .orj): the model enumerates nodes + edges, the property
#     model binds a module, and Save EXPORTS a .orj (the .py source is never written in-place).
#
#   Gate 3 (terrain family, LIGHT path): load the corpus asset -> the TerrainNodeGraphModel
#     root node count matches the document's root-level tree_paths count, and a canvas
#     selection binds a node into the terrain property model.
#
# Init recipe = the terrain/hypermesh reflection-test precedent: full subsystem class
# registration (bare imports serialize/introspect EMPTY), always coreappexit().
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import sys
import tempfile

# Prefer THIS repo's obt.project/scripts (so a worktree/lane run shadows the env's default
# scripts dir), mirroring the ork.dflow.edit.py launcher so this gate tests the SHELL code
# sitting next to it. On the mainline this is a no-op re-order.
_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)

from orkengine import core                 # core before lev2
from orkengine import lev2                  # noqa: F401
from orkengine import ecs                   # headless_appinit (full class registration)


# the shipped test target's shape (parsed from test1.orj: 8 Modules + 8 zzz_connections).
TEST1_NODES = 8
TEST1_EDGES = 8


def _gen_test1_orj(dstdir):
  """Build the shipped test1.orj graph in-code (POOL -> EMITN -> EMITR -> GRAV -> TURB ->
  VORT -> STRK pool chain + GLOB.RelTime -> {EMITN,EMITR}.LifeSpan = 8 modules, 8 edges)
  and serialize it to a .orj on disk. A jul17-serializable stand-in for the shipped asset
  whose exact on-disk bytes crash the stale gate binary's deserializer (see module note)."""
  from orkengine.core import dataflow as _dflow
  P = lev2.particles
  g = _dflow.GraphData.createShared()
  pool  = g.create("POOL",  P.Pool)
  emitn = g.create("EMITN", P.NozzleEmitter)
  emitr = g.create("EMITR", P.RingEmitter)
  glob  = g.create("GLOB",  P.Globals)
  grav  = g.create("GRAV",  P.Gravity)
  turb  = g.create("TURB",  P.Turbulence)
  vort  = g.create("VORT",  P.Vortex)
  strk  = g.create("STRK",  P.StreakRenderer)
  g.connect(emitn.inputs.pool, pool.outputs.pool)
  g.connect(emitr.inputs.pool, emitn.outputs.pool)
  g.connect(grav.inputs.pool,  emitr.outputs.pool)
  g.connect(turb.inputs.pool,  grav.outputs.pool)
  g.connect(vort.inputs.pool,  turb.outputs.pool)
  g.connect(strk.inputs.pool,  vort.outputs.pool)
  g.connect(emitn.inputs.LifeSpan, glob.outputs.RelTime)
  g.connect(emitr.inputs.LifeSpan, glob.outputs.RelTime)
  path = os.path.join(dstdir, "test1_incode.orj")
  with open(path, "w") as f:
    f.write(g.serializeJson())
  return path


def _asset_path(dstdir):
  """The GraphData gate asset: the shipped test1.orj if DFLOWEDIT_TEST1_ORJ names a binary
  that can deserialize it, else the in-code stand-in (default on the jul17 gate binary)."""
  override = os.environ.get("DFLOWEDIT_TEST1_ORJ", "")
  if override and os.path.isfile(override):
    print(f"[gate:graphdata] using DFLOWEDIT_TEST1_ORJ={override}", flush=True)
    return override
  return _gen_test1_orj(dstdir)


def gate_graphdata():
  from ork.editor.dflowedit import _load_graphdata
  tmpdir = tempfile.mkdtemp(prefix="dflowedit_gate_")
  asset = _asset_path(tmpdir)
  print(f"[gate:graphdata] asset={asset}", flush=True)
  b = _load_graphdata(asset)              # the shell's own .orj loader (deserialize -> adapter)
  nm = b.node_model

  nodes = list(nm.nodes())
  edges = list(nm.edges())
  print(f"[gate:graphdata] nodes={len(nodes)} {sorted(nodes)}", flush=True)
  print(f"[gate:graphdata] edges={len(edges)}", flush=True)
  assert len(nodes) == TEST1_NODES, f"expected {TEST1_NODES} nodes, got {len(nodes)}"
  assert len(edges) == TEST1_EDGES, f"expected {TEST1_EDGES} edges, got {len(edges)}"

  # plug metadata for a known module (GRAV = GravityModuleData).
  gins = nm.inputs("GRAV")
  gouts = nm.outputs("GRAV")
  print(f"[gate:graphdata] GRAV type={nm.type_name('GRAV')} inputs={gins} outputs={gouts}",
        flush=True)
  assert len(gins) > 0, "GRAV has no input plug metadata (plugSpec empty?)"
  assert len(gouts) > 0, "GRAV has no output plug metadata (plugSpec empty?)"
  in_pool = next((t for (n, t) in gins if n == "pool"), None)
  out_pool = next((t for (n, t) in gouts if n == "pool"), None)
  assert in_pool is not None and out_pool is not None, "GRAV pool plugs missing"

  # can_connect (ENGINE verdict = GraphData::plugsCompatible: strict type + fan-out):
  # a like-typed pair WITH fan-out headroom connects. GLOB.RelTime is an unbounded float output;
  # GRAV.G is an unconnected float input. (pool<->pool outputs are max_fanout==1 and already
  # saturated by the chain, so the engine correctly refuses a SECOND consumer there.)
  ok, _r = nm.can_connect("GLOB", "RelTime", "GRAV", "G")
  assert ok, f"RelTime->G should connect: {_r}"
  # EMITN.LifeSpan is a float plug -> a pool -> LifeSpan wire is a genuine type mismatch.
  ok3, r3 = nm.can_connect("GRAV", "pool", "EMITN", "LifeSpan")
  print(f"[gate:graphdata] can_connect pool->LifeSpan ok={ok3} reason={r3!r}", flush=True)
  assert not ok3 and r3, "a type-mismatched connect must be rejected with a reason"
  self_ok, self_r = nm.can_connect("GRAV", "pool", "GRAV", "pool")
  assert not self_ok, "self-wire must be rejected"

  # display flag (exclusive _output_node marker) round-trips.
  nm.set_output("STRK", True)
  assert nm.is_output("STRK"), "set_output did not take"
  assert not nm.is_output("GRAV")

  # positions round-trip through _editor_layout after set_pos + Save + reload.
  nm.set_pos("GRAV", 123.0, 456.0)
  rp = nm.pos("GRAV")
  assert rp is not None and abs(rp[0] - 123.0) < 1e-4 and abs(rp[1] - 456.0) < 1e-4, \
      f"set_pos did not read back: {rp}"
  tmp = os.path.join(tempfile.mkdtemp(prefix="dflowedit_gate_"), "roundtrip.orj")
  b.save(tmp)
  b2 = _load_graphdata(tmp)
  rp2 = b2.node_model.pos("GRAV")
  print(f"[gate:graphdata] position round-trip -> {rp2}", flush=True)
  assert rp2 is not None and abs(rp2[0] - 123.0) < 1e-4 and abs(rp2[1] - 456.0) < 1e-4, \
      f"position did not round-trip through _editor_layout: {rp2}"

  # the E2 property model binds the module handle (object_for_nid = the module name).
  b.prop_model.set_object(nm.object_for_nid("GRAV"))
  prop_keys = b.prop_model.getChildren("")
  print(f"[gate:graphdata] GRAV propsheet rows={len(prop_keys)}", flush=True)
  assert len(prop_keys) > 0, "property sheet bound no rows for GRAV"

  # ---- authoring: node_types + add-node + delete-node + save/reload-after-edit ----
  # done on a FRESH load so it never perturbs the shape assertions above.
  bA = _load_graphdata(asset)
  nmA = bA.node_model
  types = list(nmA.node_types())
  print(f"[gate:graphdata] node_types={len(types)} sample={types[:3]}", flush=True)
  assert types, "node_types() empty for a populated particles graph (family filter/moduleClasses?)"
  assert all(t.lstrip("/").startswith("psys::") for t in types), \
      f"node_types leaked a non-particles class: {types}"
  add_type = "psys::GravityModuleData"
  assert ("/" + add_type) in types, f"{add_type!r} not offered by node_types(): {types}"

  base_n = len(list(nmA.nodes()))
  new_name = nmA.add_node(add_type, (10.0, 20.0))
  print(f"[gate:graphdata] add_node({add_type!r}) -> {new_name!r}", flush=True)
  assert new_name is not None, f"add_node returned None for {add_type!r}"
  after_add = list(nmA.nodes())
  assert len(after_add) == base_n + 1, \
      f"add did not grow the node set: {base_n} -> {len(after_add)}"
  assert new_name in after_add, f"{new_name!r} not in node set after add"

  # the edit persists across a Save (.orj re-serialize) + reload.
  tmp_add = os.path.join(tempfile.mkdtemp(prefix="dflowedit_gate_"), "after_add.orj")
  bA.save(tmp_add)
  bR = _load_graphdata(tmp_add)
  reloaded = list(bR.node_model.nodes())
  print(f"[gate:graphdata] reload-after-add nodes={len(reloaded)}", flush=True)
  assert len(reloaded) == base_n + 1, f"added node did not survive save/reload: {len(reloaded)}"
  assert new_name in reloaded, f"{new_name!r} missing after save/reload"

  # delete it back on the reloaded model -> node set shrinks; save/reload confirms.
  bR.node_model.delete_node(new_name)
  after_del = list(bR.node_model.nodes())
  assert len(after_del) == base_n, f"delete did not shrink the node set: {len(after_del)}"
  assert new_name not in after_del, f"{new_name!r} still present after delete"
  tmp_del = os.path.join(tempfile.mkdtemp(prefix="dflowedit_gate_"), "after_del.orj")
  bR.save(tmp_del)
  final = list(_load_graphdata(tmp_del).node_model.nodes())
  print(f"[gate:graphdata] reload-after-delete nodes={len(final)}", flush=True)
  assert len(final) == base_n, f"delete did not survive save/reload: {len(final)}"
  assert new_name not in final, f"{new_name!r} reappeared after delete save/reload"

  print("GATE_GRAPHDATA=PASS", flush=True)
  return True


def gate_particles(name="fireball"):
  """Particles-DSL leg: a particles DSL asset (CODE, not an .orj) routes through the SHELL's
  own _load_particles loader — resolve_dsl_file -> load_dsl_class -> generatedflow() -> wrapped
  in the SAME GraphData machinery the .orj family uses. Asserts the model enumerates the graph
  (nodes + edges), the E2 property model binds a module, and the DSL Save policy EXPORTS a .orj
  (the .py source is CODE, never written in-place) that reloads through _load_graphdata."""
  from ork.editor.dflowedit import _load_particles, _load_graphdata
  print(f"[gate:particles] asset={name}", flush=True)
  b = _load_particles(name)
  assert b.family == "particles", f"expected family 'particles', got {b.family!r}"
  assert b.dsl_source and b.dsl_source.endswith(".py"), \
      f"DSL-sourced binding must record its .py source, got {b.dsl_source!r}"
  nm = b.node_model

  nodes = list(nm.nodes())
  edges = list(nm.edges())
  print(f"[gate:particles] nodes={len(nodes)} edges={len(edges)} {sorted(nodes)}", flush=True)
  assert len(nodes) > 0, "particles DSL produced an empty graph (generatedflow() populated nothing?)"
  assert len(edges) > 0, "particles DSL produced no edges (the pool chain should wire modules)"

  # the E2 property model binds a module handle (object_for_nid = the module name).
  b.prop_model.set_object(nm.object_for_nid(nodes[0]))
  prop_rows = b.prop_model.getChildren("")
  print(f"[gate:particles] {nodes[0]!r} propsheet rows={len(prop_rows)}", flush=True)
  assert len(prop_rows) > 0, f"property sheet bound no rows for {nodes[0]!r}"

  # Save policy: DSL code is never written in-place — Save EXPORTS a doc-less .orj that
  # reloads through the .orj loader with the same shape (the .py is untouched by design).
  tmp = os.path.join(tempfile.mkdtemp(prefix="dflowedit_ptc_"), f"{b.title}.orj")
  b.save(tmp)
  assert os.path.isfile(tmp), f"particles Save did not write the exported .orj: {tmp}"
  reloaded = list(_load_graphdata(tmp).node_model.nodes())
  print(f"[gate:particles] exported .orj reload nodes={len(reloaded)}", flush=True)
  assert len(reloaded) == len(nodes), \
      f"exported .orj lost nodes on reload: {len(nodes)} -> {len(reloaded)}"

  # ---- E7 follow-up: BYPASS semantics + DISPLAY-affordance capability (particles) ----
  # (1) DISPLAY affordance ABSENT: particles have no per-node display marker (render terminals
  #     are chosen by BYPASS, every non-bypassed renderer draws), so NO node offers one.
  assert not any(nm.has_display_flag(n) for n in nodes), \
      "particles must NOT offer a display flag on any node (affordance absent, not merely N/A)"

  # (2) role-based bypassability: OP + RENDERER bypassable; POOL + EMITTER not.
  from ork.hypergraph.dflow.particles.capabilities import classify
  by_kind = {}
  for n in nodes:
    by_kind.setdefault(classify(nm._class_of(n)), []).append(n)
  print(f"[gate:particles] roles={ {k: sorted(v) for k, v in by_kind.items()} }", flush=True)
  assert by_kind.get("pool") and by_kind.get("emitter") and by_kind.get("op") \
      and by_kind.get("renderer"), f"fireball must carry pool/emitter/op/renderer: {by_kind}"
  for n in by_kind["op"] + by_kind["renderer"]:
    assert nm.has_bypass_flag(n), f"{n!r} (op/renderer) must be bypassable"
  for n in by_kind["pool"] + by_kind["emitter"]:
    assert not nm.has_bypass_flag(n), f"{n!r} (pool/emitter) must NOT be bypassable"

  # (3) refusal: a bypass on POOL/EMITTER is refused LOUDLY, the flag never marked.
  for n in by_kind["pool"] + by_kind["emitter"]:
    r = nm.set_bypassed(n, True)
    assert isinstance(r, tuple) and r[0] == "refused" and r[1], \
        f"bypass on {n!r} must be refused with a reason, got {r!r}"
    assert not nm.is_bypassed(n), f"refused bypass on {n!r} must not mark the flag"

  # (4) EFFECTIVE graph: bypass the two chain ops -> the CLONE wires the pool source around
  #     them (their upstream connects to their consumer) and drops them; the shared graph
  #     stays PRISTINE. Bypass a renderer -> it is OMITTED from the render terminals.
  caps = b.viewport_host._caps
  live = b.document.elaborate()
  ops = by_kind["op"]
  for n in ops:
    nm.set_bypassed(n, True)
  eff = caps.build_effective_graph(live)
  eff_mods = {m for (m, _c) in _mods_of(eff)}
  live_mods = {m for (m, _c) in _mods_of(live)}
  print(f"[gate:particles] effective(ops bypassed) modules={sorted(eff_mods)}", flush=True)
  assert not (set(ops) & eff_mods), f"bypassed ops must be dropped from the effective graph: {ops}"
  assert live_mods == {m for (m, _c) in _mods_of(live)}, "live graph must stay pristine"
  # the chain must stay connected through the wire-around (POOL still reaches a renderer).
  eff_edges = {(e["out_module"], e["in_module"]) for e in eff.edges() if e["out_plug"] == "pool"}
  rend = by_kind["renderer"][0]
  assert any(dst == rend for (_s, dst) in eff_edges), \
      f"the renderer {rend!r} must still be fed after the ops are wired around: {eff_edges}"
  for n in ops:                                    # un-bypass -> effective graph == live
    nm.set_bypassed(n, False)
  assert {m for (m, _c) in _mods_of(caps.build_effective_graph(live))} == live_mods, \
      "un-bypass must restore the full effective graph"
  # renderer omit
  nm.set_bypassed(rend, True)
  eff_r = caps.build_effective_graph(live)
  assert rend not in {m for (m, _c) in _mods_of(eff_r)}, \
      f"a bypassed renderer {rend!r} must be omitted from the effective graph"
  assert rend not in caps.render_terminals(eff_r), "omitted renderer must not be a render terminal"
  nm.set_bypassed(rend, False)

  print("GATE_PARTICLES=PASS", flush=True)
  return True


def _mods_of(graph):
  """[(name, class)] for a live graph (reflection-JSON module walk)."""
  import json
  root = json.loads(graph.serializeJson())
  mods = root["root"]["object"]["properties"].get("Modules", {}) or {}
  return [(name, entry.get("object", {}).get("class", "")) for name, entry in mods.items()]


def gate_terrain(name="xxx"):
  from ork.editor.dflowedit import _load_terrain
  from ork.hypergraph.dflow.terrain.doc import tree_paths, DocNode
  print(f"[gate:terrain] asset={name}", flush=True)
  b = _load_terrain(name)
  nm = b.node_model
  doc = b.document

  root_nodes = list(nm.nodes())
  root_paths = [k for (pk, k, _o) in tree_paths(doc) if pk == ""]
  print(f"[gate:terrain] model root nodes={len(root_nodes)} "
        f"root tree_paths={len(root_paths)}", flush=True)
  assert len(root_nodes) == len(root_paths), \
      f"model root node count {len(root_nodes)} != root tree_paths {len(root_paths)}"

  # a canvas selection binds a node into the terrain property model. Pick a DocNode.
  picked = None
  for nid in root_nodes:
    obj = nm.object_for_nid(nid)
    if isinstance(obj, DocNode):
      picked = (nid, obj)
      break
  assert picked is not None, "no root DocNode to select (expected at least one leaf node)"
  nid, obj = picked
  b.prop_model.set_object(obj)
  print(f"[gate:terrain] selected {nid!r} ({obj.clazz_name}) "
        f"propsheet rows={len(b.prop_model.getChildren(''))}", flush=True)
  assert b.prop_model._obj is obj, "property model did not bind the selected node"

  # terrain KEEPS its display affordance (the display flag picks which node materializes) —
  # the capability mask is per-family: particles drop it, terrain does not.
  assert any(nm.has_display_flag(n) for n in root_nodes), \
      "terrain must still offer the display affordance (per-family capability, not global)"

  print("GATE_TERRAIN=PASS", flush=True)
  return True


def gate_multidoc(terrain_name="xxx", particles_name="fireball"):
  """Multi-document leg (model level): TWO sources loaded through the SHELL loaders open as
  independent bindings, each with its OWN node model + property model (selection state is
  per-binding, so a focus switch just rebinds the propsheet — the focus-follow contract).
  Also exercises the terrain COMPOSE seam: a second terrain runtime folded into the first as
  a contributor lowers a SECOND terrain entity/asset into ONE composed SceneData."""
  from ork.editor.dflowedit import _load_terrain, load_family

  # ---- per-binding independence + focus-follow (terrain + particles, mixed families) ----
  b_terr = load_family(terrain_name)     # payload-bearing (terrain viewport host)
  b_ptc = load_family(particles_name)    # payload-bearing (E7: particles viewport host)
  print(f"[gate:multidoc] loaded [{b_terr.family}:{b_terr.title}] + "
        f"[{b_ptc.family}:{b_ptc.title}]", flush=True)
  assert b_terr.family == "terrain" and b_terr.viewport_host is not None, \
      "first binding should be a payload-bearing terrain"
  # E7: particles now carry a live viewport payload (a running particle system) that folds over
  # the terrain primary as a CONTRIBUTOR — it exposes the compose + visibility-oracle seams.
  assert b_ptc.family == "particles" and b_ptc.viewport_host is not None, \
      "second binding should be a payload-bearing particles family (E7)"
  assert b_ptc.viewport_compose is not None, \
      "the particles payload must expose the CONTRIBUTOR compose seam"
  assert hasattr(b_ptc.viewport_host, "setComposedVisible"), \
      "the particles payload must expose the visibility-oracle suppression seam"
  assert b_terr.prop_model is not b_ptc.prop_model, \
      "each binding MUST own an independent property model (per-binding selection)"

  terr_nodes = list(b_terr.node_model.nodes())
  ptc_nodes = list(b_ptc.node_model.nodes())
  assert terr_nodes and ptc_nodes, "both bindings must enumerate a non-empty node set"
  # focus binding A: bind A's propsheet to a node from A; then focus binding B independently.
  b_terr.prop_model.set_object(b_terr.node_model.object_for_nid(terr_nodes[0]))
  b_ptc.prop_model.set_object(b_ptc.node_model.object_for_nid(ptc_nodes[0]))
  rows_terr = b_terr.prop_model.getChildren("")
  rows_ptc = b_ptc.prop_model.getChildren("")
  print(f"[gate:multidoc] focus-follow: terr propsheet rows={len(rows_terr)} "
        f"ptc propsheet rows={len(rows_ptc)}", flush=True)
  assert rows_terr and rows_ptc, "each focused binding binds its own non-empty propsheet"

  # ---- terrain COMPOSE seam: fold a 2nd terrain runtime into the 1st -> ONE scene ----
  primary = _load_terrain(terrain_name).viewport_host.runtime
  contrib = _load_terrain(terrain_name).viewport_host.runtime
  # single-source scene has ONE terrain asset ("terra"); compose adds a suffixed second.
  js_solo = primary.export_scene_json()
  assert '"terra"' in js_solo and '"terra_1"' not in js_solo, \
      "a single-source scene must carry exactly the primary terrain asset"
  primary.add_contributor(contrib)
  js_composed = primary.export_scene_json()
  print(f"[gate:multidoc] compose: solo_bytes={len(js_solo)} composed_bytes={len(js_composed)}",
        flush=True)
  assert '"terra"' in js_composed and '"terra_1"' in js_composed, \
      "composed scene must carry BOTH the primary and the contributor terrain assets"
  assert '"terrain0"' in js_composed and '"terrain1"' in js_composed, \
      "composed scene must carry BOTH terrain entities"
  assert len(js_composed) > len(js_solo), "composed scene must be strictly larger than solo"

  print("GATE_MULTIDOC=PASS", flush=True)
  return True


def gate_compose_visibility():
  """VISIBILITY leg (compose placement): the contributor-into-terrain-primary placement policy
  (ork.editor.hypermesh_viewport_host.plan_surface_anchor) must make a hypermesh mesh VISIBLE BY
  DEFAULT on a large terrain — NOT buried (raw origin overlap sits ~thousands of meters below the
  surface) and NOT sub-pixel (native size << the primary extent). Pure math, so it is deterministic
  and quotes BOTH the default box AND the owner's size=1000 case: each normalizes to the same visible
  footprint and sits its bounds-bottom ABOVE the sampled surface. Also asserts a bogus envmap override
  is refused LOUDLY at the shell (never reaching the engine's crash-on-missing path)."""
  from ork.editor.hypermesh_viewport_host import plan_surface_anchor, _FIT_FRAC
  from ork.editor.dflowedit import resolve_envmap_override

  EXTENT = 32768.0
  SURFACE = 7266.0
  surf_fn = lambda x, z: SURFACE                    # flat synthetic surface at the erosion-terrain height

  target = _FIT_FRAC * EXTENT
  for half in (1.0, 1000.0):                        # size=1 (default box) and the owner's size=1000
    lo = (-half, -half, -half)
    hi = (half, half, half)
    scale, tx, ty, tz = plan_surface_anchor(lo, hi, EXTENT, surf_fn)
    footprint = scale * (2.0 * half)
    bottom_y = ty + scale * lo[1]                   # world Y of the scaled bounds-bottom
    print(f"[gate:visibility] half={half}: scale={scale:.4g} footprint={footprint:.1f}m "
          f"originY={ty:.1f} bottomY={bottom_y:.1f} (surface={SURFACE})", flush=True)
    assert abs(footprint - target) < 1.0, \
        f"footprint {footprint} != target {target} (auto-fit must normalize any native size)"
    assert bottom_y > SURFACE, \
        f"bounds-bottom {bottom_y} is not ABOVE the surface {SURFACE} (still buried)"
    assert abs(tx) < 1e-6 and abs(tz) < 1e-6, \
        f"XZ center must land at the terrain center (origin), got ({tx},{tz})"

  # bogus envmap override -> loud shell refusal (no engine crash path).
  try:
    resolve_envmap_override("no_such_envmap_xyz")
    raise AssertionError("bogus envmap override did NOT raise a shell-level error")
  except FileNotFoundError as ex:
    print(f"[gate:visibility] bogus envmap refused loudly: {ex}", flush=True)
  # a present envmap resolves to the house .xir form.
  present = resolve_envmap_override("blender_courtyard")
  assert present == "<ork_envmaps2>/blender_courtyard.xir", \
      f"present envmap resolved wrong: {present!r}"
  print(f"[gate:visibility] present envmap resolves -> {present}", flush=True)

  print("GATE_VISIBILITY=PASS", flush=True)
  return True


def main(argv):
  terrain_name = argv[0] if argv else "xxx"
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ezapp.bindGfxToCurrentThread()
  r1 = r2 = r3 = r4 = r5 = False
  try:
    r1 = gate_graphdata()
    r2 = gate_particles()
    r3 = gate_terrain(terrain_name)
    r4 = gate_multidoc(terrain_name)
    r5 = gate_compose_visibility()
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()          # ALWAYS exit cleanly (a skipped coreappexit hangs teardown)
  ok = bool(r1 and r2 and r3 and r4 and r5)
  print(f"DFLOWEDIT_MODEL_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
