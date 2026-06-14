#!/usr/bin/env python3
###############################################################################
# E.7/M0 gate — the sdfgrid family floor.
#
#   1. C++ ORACLE (lev2.sdfgrid_selftest): hand-built SdfEval graphs through the
#      REAL dispatch machinery; dense-brick readback vs the ANALYTIC distance at
#      EVERY voxel (sphere / two-sphere union), x-fastest layout, sign at known
#      inside/outside points, and a runtime dim poke (brick reallocs + recomputes,
#      no recompile).
#   2. DSL algebra: sphere/box/union/subtract/smooth_union emit deterministic
#      GLSL; spot-check the emitted expression text.
#   3. serdes: a hypermesh graph carrying an SdfEval round-trips byte-identical
#      (expression + dim/extent/center plug values are reflected state).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.dflow import sdf as S


class SdfAsset(Hypermesh):
  def __init__(self):
    super().__init__()
    expr = S.sphere(1.25, center=(0.5, 0, 0)) | S.sphere(0.8, center=(-1.0, 0, 0))
    self._sdf = self.sdf_eval(expr, dim=32, extent=4.0)
    b = self.box(size=1.0)        # a mesh terminal so the graph stays a normal hypermesh graph
    self.output(b)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  try:
    _body(ezapp, ctx)
  except SystemExit:
    raise                       # _body already tore down; don't double-teardown
  except BaseException:
    import traceback; traceback.print_exc()
    # ALWAYS tear down — a skipped headless_exit spins the process forever
    # (the coreappexit teardown trap)
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    sys.exit(1)


def _body(ezapp, ctx):

  ##############################################################
  # 1) the C++ analytic oracle (M0)
  ##############################################################
  print("running sdfgrid oracle ...", flush=True)
  fails = lev2.sdfgrid_selftest(ctx)
  assert fails == 0, "sdfgrid selftest: %d failures" % fails

  ##############################################################
  # 1b) the M1 voxelize oracle (openvdb reference + mode equivalence)
  ##############################################################
  print("running sdf voxelize oracle ...", flush=True)
  fails = lev2.sdf_voxelize_selftest(ctx)
  assert fails == 0, "sdf voxelize selftest: %d failures" % fails

  print("running sdf csg oracle ...", flush=True)
  fails = lev2.sdf_csg_selftest(ctx)
  assert fails == 0, "sdf csg selftest: %d failures" % fails

  print("running sdf mesh oracle ...", flush=True)
  fails = lev2.sdf_mesh_selftest(ctx)
  assert fails == 0, "sdf mesh selftest: %d failures" % fails

  print("running sdf redistance oracle (M4a) ...", flush=True)
  fails = lev2.sdf_redistance_selftest(ctx)
  assert fails == 0, "sdf redistance selftest: %d failures" % fails

  ##############################################################
  # 1c) DIRTY input -> the AUTO mode falls back to winding and the
  #     inside/outside classification SURVIVES a deleted face: compare
  #     the broken box's sign field against the closed box's (resolved
  #     region; the immediate surface band exempt).
  ##############################################################
  hm = lev2.hypermesh
  from ork.hypergraph.dflow.hypermesh import sel_normal_dir, replace, group
  from orkengine.core import vec3 as _v3

  class ClosedBox(Hypermesh):
    def __init__(self):
      super().__init__()
      b = self.box(size=1.6)
      self._sdf = self.mesh_to_sdf(b, dim=32, extent=2.6)
      self.output(b)

  class HoleyBox(Hypermesh):
    def __init__(self):
      super().__init__()
      b = self.box(size=1.6)
      s = self.select(b, sel_normal_dir(n=_v3(0, 1, 0), t=0.5), op=replace(group(0)))
      d = self.delete_faces(s, slot=0)             # the +Y face is GONE -> boundary edges
      self._sdf = self.mesh_to_sdf(d, dim=32, extent=2.6)
      self.output(d)

  lc = hm.materialize_live(ClosedBox().generatedflow(), ctx)
  lh = hm.materialize_live(HoleyBox().generatedflow(), ctx)
  (dc, oc, vc, valc) = lev2.sdf.read_brick(lc, ctx)
  (dh, oh, vh, valh) = lev2.sdf.read_brick(lh, ctx)
  assert dc == dh and abs(vc - vh) < 1e-9, "broken-box brick frame diverged"
  mism = sum(1 for a, b in zip(valc, valh)
             if abs(a) > 0.35 * vc and abs(b) > 0.35 * vc and (a < 0) != (b < 0))
  frac = mism / float(len(valc))
  assert frac < 0.005, "winding fallback lost inside/outside on a holey box: %.4f%% mismatched" % (100 * frac)
  print("sdf dirty-input winding fallback PASS (%d/%d voxels disagree)" % (mism, len(valc)), flush=True)

  ##############################################################
  # 2) DSL emission spot-checks (deterministic GLSL)
  ##############################################################
  e = S.sphere(1.0)
  assert e._glsl == "length(p - vec3(0, 0, 0)) - 1", "sphere emission drifted: %r" % e._glsl
  u = S.sphere(1.0) | S.sphere(0.5, center=(2, 0, 0))
  assert u._glsl.startswith("min("), "union must emit min(): %r" % u._glsl
  d = S.box(1.0) - S.sphere(0.9)
  assert d._glsl.startswith("max(") and "-(" in d._glsl, "subtract must emit max(a,-b): %r" % d._glsl
  k = S.smooth_union(S.sphere(1.0), S.sphere(0.5), k=0.25)
  assert "mix(" in k._glsl and "clamp(" in k._glsl, "smooth_union emission drifted"
  print("sdf DSL emission PASS", flush=True)

  ##############################################################
  # 3) serdes — byte-identical round trip with an SdfEval aboard
  ##############################################################
  g = SdfAsset().generatedflow()
  js = g.serializeJson()
  clone = core.Object.deserializeJson(js)
  assert clone.serializeJson() == js, "sdf graph re-serialization diverged"
  print("sdf serdes PASS (%d bytes byte-identical)" % len(js), flush=True)

  ##############################################################
  # 3b) M4a — a graph carrying a Redistance node round-trips byte-identical
  ##############################################################
  class RedistAsset(Hypermesh):
    def __init__(self):
      super().__init__()
      s    = self.sdf(dim=48, extent=4.0)
      blob = s.sphere(1.0).smooth_union(s.sphere(0.7, center=(1.0, 0, 0)), k=0.3)
      self.output(blob.redistance().to_mesh(weld=True))
  gr = RedistAsset().generatedflow()
  jsr = gr.serializeJson()
  assert core.Object.deserializeJson(jsr).serializeJson() == jsr, "redistance graph re-serialization diverged"
  assert '"class": ""' not in jsr, "redistance graph has an UNTOUCHED (empty-class) node — reflection gap"
  print("sdf redistance DSL+serdes PASS (%d bytes byte-identical)" % len(jsr), flush=True)

  ##############################################################
  # 3c) M4b — conform an icosphere onto an ANALYTIC sphere SDF (cross-family).
  #     The field (length(p)-1) is already a true |grad|=1 SDF, so conform must
  #     land EVERY vertex on the unit sphere (the shrinkwrap/retopo proof).
  ##############################################################
  import tempfile, math as _math

  # ---- OBJ parse helpers: verts, per-vertex normals (vn), polygon faces (1-indexed) ----
  def _parse_obj(path):
    verts, norms, faces = [], [], []
    with open(path) as f:
      for ln in f:
        if ln.startswith("v "):
          pp = ln.split(); verts.append((float(pp[1]), float(pp[2]), float(pp[3])))
        elif ln.startswith("vn "):
          pp = ln.split(); norms.append((float(pp[1]), float(pp[2]), float(pp[3])))
        elif ln.startswith("f "):
          faces.append([int(tok.split("/")[0]) - 1 for tok in ln.split()[1:]])
    return verts, norms, faces

  # coefficient of variation (std/mean) of all polygon edge lengths — a distribution-uniformity metric
  def _edge_cov(verts, faces):
    lens = []
    for face in faces:
      for i in range(len(face)):
        a = verts[face[i]]; b = verts[face[(i + 1) % len(face)]]
        lens.append(_math.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2 + (a[2]-b[2])**2))
    mean = sum(lens) / len(lens)
    var  = sum((x - mean) ** 2 for x in lens) / len(lens)
    return _math.sqrt(var) / mean

  # SURFACE roughness, CURVATURE-INSENSITIVE. h[v] = normal-dir offset of v from its 1-ring centroid;
  # on a curved surface h has a smooth CURVATURE baseline (~R*theta^2/2) that fairing can't (and
  # shouldn't) remove. The high-freq RIDGING is the part of h that ALTERNATES between neighbours, so we
  # subtract each vertex's neighbour-averaged h (kills the smooth curvature trend) and report mean of
  # the residual. Distinct from edge-CoV (= distribution). Taubin fairing must DROP this.
  def _surface_roughness(verts, faces):
    nv   = len(verts)
    vn   = [[0.0, 0.0, 0.0] for _ in range(nv)]   # area-weighted vertex normals
    nbrs = [set() for _ in range(nv)]             # 1-ring neighbour sets
    for face in faces:
      a, b, c = verts[face[0]], verts[face[1]], verts[face[2]]
      e1 = (b[0]-a[0], b[1]-a[1], b[2]-a[2]); e2 = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
      fn = (e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0])  # area-weighted
      n = len(face)
      for i in range(n):
        vi = face[i]
        vn[vi][0] += fn[0]; vn[vi][1] += fn[1]; vn[vi][2] += fn[2]
        nbrs[vi].add(face[(i + 1) % n]); nbrs[vi].add(face[(i - 1) % n])
    h = [0.0] * nv; ok = [False] * nv
    for v in range(nv):
      if not nbrs[v]: continue
      nl = _math.sqrt(vn[v][0]**2 + vn[v][1]**2 + vn[v][2]**2)
      if nl < 1e-9: continue
      k = len(nbrs[v])
      cx = sum(verts[j][0] for j in nbrs[v]) / k
      cy = sum(verts[j][1] for j in nbrs[v]) / k
      cz = sum(verts[j][2] for j in nbrs[v]) / k
      h[v] = ((verts[v][0]-cx)*vn[v][0] + (verts[v][1]-cy)*vn[v][1] + (verts[v][2]-cz)*vn[v][2]) / nl
      ok[v] = True
    tot = 0.0; cnt = 0
    for v in range(nv):
      if not ok[v]: continue
      hn = [h[j] for j in nbrs[v] if ok[j]]
      if not hn: continue
      tot += abs(h[v] - sum(hn) / len(hn)); cnt += 1   # residual after removing the smooth curvature trend
    return tot / cnt if cnt else 0.0

  class ConformGate(Hypermesh):
    def __init__(self):
      super().__init__()
      s     = self.sdf(dim=64, extent=4.0)
      field = s.sphere(1.0)                                 # true |grad|=1 analytic field
      base  = self.icosphere(radius=1.8, subdivisions=3)    # clean uniform base, OUTSIDE the field surface
      self.output(self.displace_by_sdf(base, field=field, mode="conform", conform_steps=4))

  lconf = hm.materialize_live(ConformGate().generatedflow(), ctx)
  cobj  = os.path.join(tempfile.gettempdir(), "sdf_conform_gate.obj")
  lev2.hypermesh.dump_obj(lconf.mesh, ctx, cobj)
  cverts, cnorms, _ = _parse_obj(cobj)
  maxoff   = 0.0
  min_ndot = 1.0   # worst-case dot(normalized SDF-gradient normal, radial outward dir) over all verts
  for (x, y, z), (nx, ny, nz) in zip(cverts, cnorms):
    r = _math.sqrt(x*x + y*y + z*z)
    maxoff = max(maxoff, abs(r - 1.0))
    nl = _math.sqrt(nx*nx + ny*ny + nz*nz)
    if r > 1e-6 and nl > 1e-6:
      min_ndot = min(min_ndot, (x*nx + y*ny + z*nz) / (r * nl))
  nvconf = len(cverts)
  assert nvconf > 200 and maxoff < 0.08, "conform: %d verts, max |r-1| = %.4f (should land on unit sphere)" % (nvconf, maxoff)
  print("sdf displace_by_sdf conform PASS (%d verts, max off-sphere %.4f)" % (nvconf, maxoff), flush=True)
  # NORMALS: on the analytic unit sphere the SDF gradient IS the radial outward direction.
  # cs_sdf_normal writes NORMAL = normalize(grad phi); every vertex normal must point radially.
  assert min_ndot > 0.95, \
      "conform NORMALS wrong: worst dot(N, radial) = %.4f (SDF-gradient normal should point radially out)" % min_ndot
  print("sdf displace_by_sdf normals PASS (worst dot(N,radial) %.4f over %d verts)" % (min_ndot, nvconf), flush=True)

  jsd = ConformGate().generatedflow().serializeJson()
  assert core.Object.deserializeJson(jsd).serializeJson() == jsd, "displace_by_sdf graph re-serialization diverged"
  assert '"class": ""' not in jsd, "displace_by_sdf graph has an UNTOUCHED (empty-class) node — reflection gap"
  print("sdf displace_by_sdf serdes PASS (%d bytes byte-identical)" % len(jsd), flush=True)

  ##############################################################
  # 3d) M4a+M4b CHAIN — conform onto a REDISTANCED smooth_union blob. The field's
  #     |grad| dips below 1 at the blend, so the conform step must NOT overshoot
  #     and fling verts (the bug this chain exposed). Assert NO flung verts: every
  #     conformed vertex stays near the blob (radius << the brick edge).
  ##############################################################
  class ConformBlob(Hypermesh):
    def __init__(self, relax_steps=0):
      super().__init__()
      s    = self.sdf(dim=80, extent=6.0)   # MARGIN: brick (+-3) comfortably contains the blob + base
      blob = s.sphere(0.9).smooth_union(s.sphere(0.7, center=(1.1, 0.3, 0.0)), k=0.4).redistance()
      base = self.icosphere(radius=2.0, subdivisions=3)   # encloses the blob, well inside the brick
      self.output(self.displace_by_sdf(base, field=blob, mode="conform",
                                        conform_steps=6, relax_steps=relax_steps))

  def _conform_blob_stats(relax_steps, tag):
    lblob = hm.materialize_live(ConformBlob(relax_steps).generatedflow(), ctx)
    bobj  = os.path.join(tempfile.gettempdir(), "sdf_conform_blob_%s.obj" % tag)
    lev2.hypermesh.dump_obj(lblob.mesh, ctx, bobj)
    verts, _, faces = _parse_obj(bobj)
    maxr = max(_math.sqrt(x*x + y*y + z*z) for (x, y, z) in verts)
    return len(verts), maxr, _edge_cov(verts, faces), _surface_roughness(verts, faces)

  # without relax: conform lands every vert on the bumpy redistanced field -> SURFACE ridging + uneven spacing
  nvb, maxr, cov0, rough0 = _conform_blob_stats(0, "norelax")
  # the blob lives within radius ~1.8; the brick is +-2. A flung vert lands far outside.
  assert nvb > 200 and maxr < 2.3, \
      "conform-on-redistanced-blob: %d verts, MAX RADIUS %.3f — FLUNG verts (redistance/conform overshoot)" % (nvb, maxr)
  print("sdf conform-on-redistanced-blob PASS (%d verts, max radius %.3f — no flung verts)" % (nvb, maxr), flush=True)

  # with SURFACE FAIRING: BOTH the surface ridging (normal-dir roughness) AND the spacing must improve,
  # and the shape must NOT collapse/shrink away (Taubin is non-shrinking) — verts stay near the blob.
  nvr, maxrr, cov1, rough1 = _conform_blob_stats(16, "relax")
  assert nvr == nvb and 1.0 < maxrr < 2.3, \
      "fairing flung/collapsed verts: %d->%d verts, max radius %.3f" % (nvb, nvr, maxrr)
  assert rough1 < 0.7 * rough0, \
      "surface fairing did NOT smooth the SURFACE: roughness %.5f (faired) >= 0.7*%.5f (raw)" % (rough1, rough0)
  assert cov1 < cov0, \
      "surface fairing did NOT improve distribution: edge-length CoV %.4f (faired) >= %.4f (raw)" % (cov1, cov0)
  print("sdf surface-fairing PASS (roughness %.5f -> %.5f = %.0f%% smoother; edge-CoV %.3f -> %.3f)"
        % (rough0, rough1, 100.0 * (rough0 - rough1) / rough0, cov0, cov1), flush=True)

  ##############################################################
  # 3e) M4c (hash) — COOK-CACHE COLLISION GUARD. Two DIFFERENT `<sdf>.to_mesh()`
  #     static graphs in the SAME process must produce DIFFERENT meshes. Without a
  #     content-distinct SDF cook hash, the SdfToMesh cook key collides (SDF nodes
  #     contribute 0 to input_hashes) and the cache serves a STALE mesh.
  ##############################################################
  def _mesh_nv(radius):
    h2 = Hypermesh()
    ss = h2.sdf(dim=48, extent=4.0)
    h2.output(ss.sphere(radius).to_mesh())
    lv  = hm.materialize_live(h2.generatedflow(), ctx)
    pth = os.path.join(tempfile.gettempdir(), "sdf_cache_%g.obj" % radius)
    lev2.hypermesh.dump_obj(lv.mesh, ctx, pth)
    return sum(1 for ln in open(pth) if ln.startswith("v "))
  nv_big   = _mesh_nv(1.2)
  nv_small = _mesh_nv(0.5)
  assert nv_big > 100 and nv_small > 100 and nv_big != nv_small, \
      "COOK-CACHE COLLISION: sphere(1.2) and sphere(0.5) meshes have the same vert count (%d/%d) — SDF content missing from the cook hash" % (nv_big, nv_small)
  print("sdf cook-cache distinct-content PASS (sphere1.2=%d verts != sphere0.5=%d verts)" % (nv_big, nv_small), flush=True)

  ##############################################################
  # 3f) BOUNDARY DECOUPLING — the base mesh must NOT have to live inside the field's brick. The brick
  #     is sized to the FIELD (a unit sphere, extent=6 -> +-3), but the base icosphere is radius 4.0,
  #     so its verts START OUTSIDE the brick (radius 4 > the +-3 half-extent along the axes). The OLD
  #     sampler clamped out-of-brick coords to the boundary voxel -> every exterior vert read the same
  #     pinned cell and "locked to the extent" (spikes). With out-of-brick gradient EXTRAPOLATION the
  #     conform must pull EVERY vertex onto the unit sphere with NO spikes. Also exercises the fluent
  #     MeshNode.conformToSdf(...) wrapper.
  ##############################################################
  class ConformOutside(Hypermesh):
    def __init__(self):
      super().__init__()
      s     = self.sdf(dim=128, extent=6.0)                 # TIGHT brick (+-3): the radius-4 base is OUTSIDE it
      field = s.sphere(1.0)                                 # true |grad|=1 analytic unit sphere
      base  = self.icosphere(radius=4.0, subdivisions=3)    # STARTS outside the brick (the decoupling proof)
      self.output(base.conformToSdf(sdf=field, conform_steps=10, relax_steps=4))

  lout = hm.materialize_live(ConformOutside().generatedflow(), ctx)
  oobj = os.path.join(tempfile.gettempdir(), "sdf_conform_outside.obj")
  lev2.hypermesh.dump_obj(lout.mesh, ctx, oobj)
  overts, _, _ = _parse_obj(oobj)
  rmin, rmax, nvo = 1e9, 0.0, len(overts)
  for (x, y, z) in overts:
    r = _math.sqrt(x*x + y*y + z*z)
    rmin = min(rmin, r); rmax = max(rmax, r)
  # a "locked to extent" spike sits at the brick boundary radius (~3) or stays flung at the start (~4);
  # a converged vert sits at radius ~1. Tight bounds on BOTH ends prove no vert is pinned/flung.
  assert nvo > 200 and rmin > 0.95 and rmax < 1.05, \
      "boundary-decoupling: %d verts, radius range [%.3f, %.3f] — a vert pinned at the brick edge / flung (clamp-collapse)" % (nvo, rmin, rmax)
  print("sdf boundary-decoupling PASS (radius-4 base onto extent-6 brick -> %d verts in r=[%.3f,%.3f], no spikes)"
        % (nvo, rmin, rmax), flush=True)

  ##############################################################
  # 3g) TEMPORAL SMOOTH — inter-frame EMA damps cross-frame jitter. Drive a conformed mesh with a
  #     1-frame SQUARE WAVE (sphere offset alternates +-0.3 in x each frame = maximal temporal jitter).
  #     Without temporal_smooth the conformed verts alternate at full amplitude; with it the EMA must
  #     attenuate the per-vertex temporal VARIANCE substantially. Also exercises live recompute() +
  #     the .temporalSmooth() fluent method + its persistent inter-frame buffer.
  ##############################################################
  class JitterField(Hypermesh):
    def __init__(self, alpha=None):
      super().__init__()
      s          = self.sdf(dim=48, extent=6.0)
      self.sph   = s.sphere(1.0)                               # offset poked each frame -> the jitter source
      base       = self.icosphere(radius=1.8, subdivisions=3)
      skin       = base.conformToSdf(self.sph, conform_steps=8)  # relax_steps=0: isolate the temporal effect
      if alpha is not None:
        skin = skin.temporalSmooth(alpha=alpha)
      self.output(skin)
    def onUpdate(self, updinfo):
      pass   # marks the asset ANIMATED -> graph is NOT cacheable -> recompute() actually re-evaluates
             # (a static/cacheable graph serves the cooked mesh -> recompute is a no-op -> frozen)

  def _jitter_var(alpha, tag):
    asset = JitterField(alpha=alpha)
    live  = lev2.hypermesh.materialize_live(asset.generatedflow(), ctx)
    sums = sumsq = None; nf = 0
    for fr in range(12):                                        # 4 warm/settle frames, then 8 measured
      asset.sph.inputs.offset = core.vec3(0.3 if (fr % 2 == 0) else -0.3, 0.0, 0.0)
      live.recompute(ctx)
      if fr < 4: continue
      jp = os.path.join(tempfile.gettempdir(), "sdf_jitter_%s_%d.obj" % (tag, fr))
      lev2.hypermesh.dump_obj(live.mesh, ctx, jp)
      vts, _, _ = _parse_obj(jp)
      if sums is None:
        sums  = [[0.0, 0.0, 0.0] for _ in vts]; sumsq = [[0.0, 0.0, 0.0] for _ in vts]
      for i, (x, y, z) in enumerate(vts):
        for k, c in enumerate((x, y, z)):
          sums[i][k] += c; sumsq[i][k] += c * c
      nf += 1
    tot = 0.0
    for i in range(len(sums)):
      for k in range(3):
        mean = sums[i][k] / nf
        tot += max(0.0, sumsq[i][k] / nf - mean * mean)        # per-vertex temporal variance (xyz summed)
    return tot / len(sums)

  var_raw = _jitter_var(None, "raw")
  var_sm  = _jitter_var(0.25, "smooth")
  assert var_raw > 1e-4 and var_sm < 0.35 * var_raw, \
      "temporal smoothing did NOT damp jitter: per-vertex temporal variance %.5f (smoothed) vs %.5f (raw)" % (var_sm, var_raw)
  print("sdf temporal-smooth PASS (per-vertex temporal variance %.5f -> %.5f = %.0f%% damped)"
        % (var_raw, var_sm, 100.0 * (var_raw - var_sm) / var_raw), flush=True)

  ##############################################################
  # 3h) BLOCKY / CUBERILLE — to_mesh(blocky=True) extracts pure axis-aligned voxel faces (no
  #     interpolation/smoothing). The DEFINING property: every face normal is a unit AXIS vector
  #     (±x/±y/±z). Assert all normals are axis-aligned + verts bounded near the surface.
  ##############################################################
  class BlockyGate(Hypermesh):
    def __init__(self):
      super().__init__()
      s = self.sdf(dim=40, extent=4.0)
      self.output(s.sphere(1.2).to_mesh(blocky=True))   # forces weld off internally

  lblk = hm.materialize_live(BlockyGate().generatedflow(), ctx)
  bkobj = os.path.join(tempfile.gettempdir(), "sdf_blocky.obj")
  lev2.hypermesh.dump_obj(lblk.mesh, ctx, bkobj)
  bverts, bnorms, _ = _parse_obj(bkobj)
  nbad = 0
  for (nx, ny, nz) in bnorms:
    comps = sorted([abs(nx), abs(ny), abs(nz)])        # [min, mid, max]
    if not (comps[2] > 0.99 and comps[1] < 0.02 and comps[0] < 0.02):
      nbad += 1                                        # a non-axis-aligned normal = not cuberille
  maxr = max(_math.sqrt(x*x + y*y + z*z) for (x, y, z) in bverts) if bverts else 0.0
  assert len(bverts) > 200 and nbad == 0 and maxr < 1.2 + 0.3, \
      "blocky/cuberille: %d verts, %d non-axis-aligned normals, max radius %.3f (expect all axis normals, r<1.5)" \
      % (len(bverts), nbad, maxr)
  print("sdf blocky-cuberille PASS (%d verts, ALL %d normals axis-aligned, max radius %.3f)"
        % (len(bverts), len(bnorms), maxr), flush=True)

  ezapp.mainThreadEnd()
  print("=== sdfgrid gate PASSED ===", flush=True)
  ecs.headless_exit()
  sys.exit(0)


if __name__ == "__main__":
  main()
