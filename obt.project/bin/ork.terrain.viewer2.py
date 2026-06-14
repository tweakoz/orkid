#!/usr/bin/env ork.python

################################################################################
# ork.terrain.viewer2.py
#
# This is geoclipmesh_basic.py cloned VERBATIM, with exactly ONE thing changed:
# the height source. Where the example computed terrain_height() from procedural
# sin/cos math, this samples a DSL-baked heightfield instead —
#   * CPU: bilinear lookup into the baked field (camera terrain-follow)
#   * GPU: terrain_viewer.fxv2 (geoclipmesh_basic.fxv2 with the same height swap)
#
# EVERYTHING else (StandardSceneGraphComponent camera, near=0.3/far=10000,
# the geoclip clipmap, _onUpdate, _onUiEvent) is the known-good example,
# unchanged — so the camera feel and depth precision match it exactly.
#
# Usage:
#   ork.terrain.viewer2.py xxx
#   ork.terrain.viewer2.py xxx --dim 2048 -x 8192 -H 600
################################################################################

import math, sys, os, argparse
from pathlib import Path
from unittest import case
from orkengine.core import vec2, vec3, vec4, quat, thisdir, CrcStringProxy
from orkengine.lev2 import PBRMaterial, Image, Texture, GeoClipMapDrawable, XgmModel, CameraData, CameraDataLut, ui
from orkengine.lev2 import RigidPrimitive, MicroMesh
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, list_dsl_files, load_dsl_class
from ork.hypergraph.ecs.scene.assets import HeightField
from lev2utils.cameras import setupUiCameraX

tokens = CrcStringProxy()

SHADER_PATH = Path(__file__).parent.parent / "scripts" / "ork" / "terrain" / "terrain_viewer.fxv2"

# ---- terrain placement -------------------------------------------------------
# The terrain DSL OWNS its PHYSICAL world scale: EXTENT_M / HEIGHT_M are class
# attributes on the HeightField subclass (defaulted on the base). This viewer READS
# them from the loaded class; -x/--extent and -H/--height-scale override per run.
# (The erosion exaggeration is authored INSIDE the DSL graph on the erode ops, so it
#  never reaches the viewer/bake call — the bake env runs at the PHYSICAL height.)

# ---- camera terrain-follow ---------------------------------------------------
CLEARANCE_RADIUS_M = 10.0   # hold clearance over the local MAX height within this radius
# the clearance taps: center + 8-point ring, as (dx,dz) multiples of the radius.
CLEARANCE_TAPS = ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1),
                  (1, 1), (1, -1), (-1, 1), (-1, -1))
SHOW_SAMPLE_MARKERS = True  # draw debug spheres at the clearance sample points
MARKER_RADIUS_M    = 1.0    # sphere marker size (m); keep < CLEARANCE_RADIUS so taps stay distinct
# ---- camera-follow height smoothing (ring of probes around the camera XZ) ----
SMOOTH_RADIUS_M = 10.0      # probe-ring radius (m) the follow height is averaged over
SMOOTH_PROBES   = 12        # probes evenly spaced around that ring (+ the center sample)

################################################################################

class GeoClipMapApp(ComponentizedApplication):

  def __init__(self, dsl_path, class_name, name, dim, extent_m, height_m,
               dsl_kwargs=None, fullscreen=False, material="hmview", material_params=None,
               material_class=None, ssaa=1, chunk=128):
    super().__init__()
    self._fullscreen = fullscreen
    self._material   = material   # material NAME (resolved from the DSL folder, then materials/)
    self._material_params = material_params or {}  # DSL-declared material kwargs (strata_freq, ...)
    self._material_class  = material_class         # explicit Ptex3d subclass (DSL MATERIAL_CLASS), else None
    self._ssaa       = int(ssaa)  # SSAA supersample multiplier (1 = off, 2, 4, ...)

    # terrain DSL bake parameters (the ONLY thing this viewer adds over the example)
    self._dsl_path   = dsl_path
    self._class_name = class_name
    self._name       = name
    self._dim        = dim
    self._chunk      = int(chunk)        # GPU cull chunk size (cells/side); smaller = finer cull
    self._extent     = float(extent_m)
    self._hscale     = float(height_m)   # FINAL physical vertical scale (mesh + material + bake)
    self._dsl_kwargs = dsl_kwargs or {}
    self._cpu_hf     = None
    self._cpu_dim    = 0

    # Skim clearance held above the LOCAL ground. Keep this LOW so the camera flies
    # close enough to read the debug markers (a 10 m ring directly below). At 0.35 of
    # the (auto-exposed) relief you sit ~1 km up and the markers are an invisible dot;
    # 0.02 (~60 m at hscale 3000) is a proper skim. Tunable — raise for higher flyover.
    self.cam_above   = 8.0 #max(8.0, self._hscale * 0.02)

    # Configure scenegraph parameters
    sg_params = {
      "preset": "ForwardPBR",
      "SkyboxIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "DiffuseIntensity": 1.0,
      "AmbientLight": vec3(1),
      "DepthFogDistance": 90000.0,
      "DepthFogPower": 2.0,
      "SkyboxTexPathStr": "<ork_envmaps2>/desert4k.xir",
      "ssaa": self._ssaa
    }

    # Add standard scenegraph component with camera (near/far verbatim → good depth)
    self.SGC = self.addComponent(
      "std_scenegraph",
      StandardSceneGraphComponent,
      sg_params=sg_params,
      eye=vec3(0, 1, 0),
      tgt=vec3(0, 1, 0.1),
      up=vec3(0, 1, 0),
      near = 2.0,
      far = 100000.0,
      grid_variant=None,
      enable_ui_camera=True,   # we drive a CameraData directly; no EzUiCam orbit model
      explicit_near_far=True
    )

    self.createEzApp(ssaa=self._ssaa, fullscreen=self._fullscreen)

    # ---- first-person walking camera state (ported from ork.terrain.viewer.py) ----
    # WASD = fly in the camera-relative XZ plane; arrow keys = look (yaw/pitch).
    # The camera itself is self.SGC.uicam (an EzUiCam), available after _onGpuInit.
    self._move       = vec2(0, 0)                      # (strafe, forward) in [-1,1]
    self._speed      = max(3.0, self._extent / 300.0)  # m/s, scaled to terrain size
    self._offset     = vec3(0, 0, 0)                   # world XZ (Y from terrain-follow)
    self._cam_y      = self.cam_above                  # smoothed follow height (seeded in _onGpuInit)
    self._yaw_vel    = 0.0                             # arrow LEFT/RIGHT -> -1/+1
    self._pitch_vel  = 0.0                             # arrow UP/DOWN    -> +1/-1
    self._yaw_rate   = 0.9                             # rad/s at full deflection
    self._pitch_rate = 0.7
    
  ################################################
  # height source (THE one change): bilinear sample of the baked heightfield.
  # EXACT same mapping as the GPU shader (terrain_viewer.fxv2): uv = xz/extent + 0.5,
  # textureLod(height_map, uv) — NO flip on either axis (Vulkan top-left origin, PNG
  # scanline 0 -> numpy row 0), bilinear, scaled to meters by the vertical scale.
  # The camera MUST sample the field exactly where the GPU displaces it.
  ################################################

  def terrain_height(self, x, z):
    if self._cpu_hf is None:
      return 0.0
    d  = self._cpu_dim
    u  = x / self._extent + 0.5
    v  = z / self._extent + 0.5
    fx = u * d - 0.5  # texel-center convention (sampler2D LINEAR), NOT u*(d-1)
    fy = v * d - 0.5                       
    x0 = math.floor(fx);
    y0 = math.floor(fy)
    x1 = min(max(x0 + 1, 0), d - 1);
    y1 = min(max(y0 + 1, 0), d - 1)
    tx = (fx - x0); x0 = min(max(x0, 0), d - 1);
    ty = (fy - y0); y0 = min(max(y0, 0), d - 1)
    hf = self._cpu_hf
    h0 = hf[y0, x0] * (1.0 - tx) + hf[y0, x1] * tx
    h1 = hf[y1, x0] * (1.0 - tx) + hf[y1, x1] * tx
    return float(h0 * (1.0 - ty) + h1 * ty) * self._hscale

  def clearance_points(self, x, z, radius):
    """The clearance sample positions (center + 8-point ring at `radius`) as a list
    of (px, pz) — shared by the follow and the debug-sphere markers so they agree."""
    return [(x + dx * radius, z + dz * radius) for dx, dz in CLEARANCE_TAPS]

  def terrain_height_clearance(self, x, z, radius):
    """Max terrain height over clearance_points(x,z,radius) — so the camera clears
    nearby PEAKS, not just the point directly below it (keeps you above the rim in
    deep/narrow valleys)."""
    h = 0
    c = 0
    for px, pz in self.clearance_points(x, z, radius):
      h += self.terrain_height(px, pz)
      c += 1.0
    return h/c

  def terrain_height_smoothed(self, x, z, radius=SMOOTH_RADIUS_M, n=SMOOTH_PROBES):
    """Average the terrain height over the center + a ring of `n` probes at `radius`
    around (x,z). The camera-follow rides the LOCAL MEAN ground instead of twitching
    on every fine bump directly under it (radius 10m -> ~20m-wide smoothing kernel)."""
    h = self.terrain_height(x, z)
    for i in range(n):
      a = (2.0 * math.pi) * i / n
      h += self.terrain_height(x + math.cos(a) * radius, z + math.sin(a) * radius)
    return h / (n + 1)

  ################################################
  # build a triangle mesh of the baked heightfield from the SAME CPU samples
  # terrain_height() reads — so the RENDERED surface IS the camera-follow source
  # (no CPU/GPU divergence possible). Vertices sit at texel centers (matching the
  # texel-center sample convention); the grid is stride-subsampled to keep the
  # face list bounded for large bakes. Returns numpy (verts,norms,binorms,colors)
  # and a flat python faces list ([3,i0,i1,i2, ...]) for MicroMesh.
  ################################################

  def _build_terrain_mesh_arrays(self, np):
    MESH_N = 4096                                   # ~verts per side cap
    NORMAL_STENCIL_R = 0                            # Scharr gradient neighbor radius in texels (larger = smoother)
    hf   = self._cpu_hf
    if hf.ndim == 3:                # single-channel Image view (H,W,1) -> (H,W)
      hf = hf[..., 0]
    d    = self._cpu_dim
    ext  = self._extent
    step = max(1, d // MESH_N)
    idx  = np.arange(0, d, step)
    n    = len(idx)
    # texel-center world coord, shared by x (cols) and z (rows): ((i+0.5)/d - 0.5)*ext
    world = ((idx.astype(np.float32) + 0.5) / d - 0.5) * ext
    X, Z  = np.meshgrid(world, world)              # (n,n): X varies along cols, Z along rows
    Y     = hf[np.ix_(idx, idx)] * self._hscale    # (n,n) heights in meters
    verts = np.stack([X, Y, Z], axis=-1).reshape(-1, 3).astype(np.float32)

    minxyz = verts.min(axis=0)
    maxxyz = verts.max(axis=0)
    print(f"terrain mesh: {verts.shape[0]} verts, XZ extent {maxxyz[0]-minxyz[0]:.1f}m x {maxxyz[2]-minxyz[2]:.1f}m, Y range {minxyz[1]:.1f}m to {maxxyz[1]:.1f}m", flush=True)

    # High-quality heightfield normals via a SCHARR gradient operator — an
    # 8-neighbor, rotationally-symmetric weighted central difference (an optimized
    # Sobel). Far better than a 2-point np.gradient: it averages across three
    # rows/cols, so the 16-bit png height quantization (~hscale/65535 m steps) no
    # longer bands the mirror-lit normals, while the Scharr weights keep slope
    # direction accurate in every orientation. Neighbor radius R widens the stencil
    # for extra smoothing. Vertex POSITIONS stay on the RAW samples (camera-follow
    # match); only the gradient uses the wider stencil.
    dw = (step / d) * ext
    def _scharr_grad(a, R):
      ap = np.pad(a, R, mode='edge')
      h, w = a.shape
      cL, cC, cR = slice(0, w), slice(R, R + w), slice(2 * R, 2 * R + w)
      rT, rC, rB = slice(0, h), slice(R, R + h), slice(2 * R, 2 * R + h)
      TL, T0, TR = ap[rT, cL], ap[rT, cC], ap[rT, cR]
      L0,     R0 = ap[rC, cL],             ap[rC, cR]
      BL, B0, BR = ap[rB, cL], ap[rB, cC], ap[rB, cR]
      inv = 1.0 / (32.0 * R * dw)                       # span between -R and +R taps = 2*R*dw
      gx = (-3.0 * TL + 3.0 * TR - 10.0 * L0 + 10.0 * R0 - 3.0 * BL + 3.0 * BR) * inv
      gz = (-3.0 * TL - 10.0 * T0 - 3.0 * TR + 3.0 * BL + 10.0 * B0 + 3.0 * BR) * inv
      return gx, gz
    gx, gz = _scharr_grad(Y, max(1, NORMAL_STENCIL_R))
    N = np.stack([-gx, np.ones_like(gx), -gz], axis=-1)
    N /= np.linalg.norm(N, axis=-1, keepdims=True)
    norms = N.reshape(-1, 3).astype(np.float32)
    T = np.stack([np.ones_like(gx), gx, np.zeros_like(gx)], axis=-1)   # +x tangent
    T /= np.linalg.norm(T, axis=-1, keepdims=True)
    binorms = T.reshape(-1, 3).astype(np.float32)
    colors  = np.ones((n * n, 4), dtype=np.float32)   # white (BGRA all 1)
    # uv0 = texel-center [0,1], IDENTICAL to the height sampler mapping (uv = xz/ext + 0.5,
    # no flip on either axis — see line ~130). world = ((i+0.5)/d - 0.5)*ext, so
    # uv = world/ext + 0.5 = (i+0.5)/d. This makes ctx.uv in the ptex3d material align any
    # baked channel (FlowMap / discharge / metrics) exactly with terrain_height().
    uaxis  = (idx.astype(np.float32) + 0.5) / d
    UU, VV = np.meshgrid(uaxis, uaxis)                # UU follows X (cols), VV follows Z (rows)
    uvs    = np.stack([UU, VV], axis=-1).reshape(-1, 2).astype(np.float32)
    # two CCW (+Y) tris per quad; vertex index(r,c) = r*n + c
    r = np.arange(n - 1); c = np.arange(n - 1)
    R, C = np.meshgrid(r, c, indexing='ij')
    a  = (R * n + C).ravel()
    b  = a + 1
    cc = a + n
    dd = cc + 1
    three = np.full_like(a, 3)
    t1 = np.stack([three, a,  cc, b ], axis=1)        # (a,c,b) -> normal +Y
    t2 = np.stack([three, b,  cc, dd], axis=1)        # (b,c,d) -> normal +Y
    faces = np.concatenate([t1, t2], axis=1).reshape(-1).tolist()
    return verts, norms, binorms, colors, uvs, faces

  ##############################################
  # gpu data init:
  #  called on main thread when graphics context is made available
  ##############################################

  def _onGpuInit(self, ctx):

    # ---- bake the DSL HeightField, load it (GPU texture + CPU array) ----------
    from ork.hypergraph.ecs.scene.assets import HeightField as HFAsset
    print(f"terrain viewer2: baking {self._name} (dim={self._dim}, extent={self._extent:g}m) ...", flush=True)
    hf = HFAsset(dsl_file=str(self._dsl_path), dsl_class=self._class_name,
                 dimension=self._dim, extent_m=self._extent,
                 height_scale_m=self._hscale, ctx=ctx, **self._dsl_kwargs)
    hf.gendata.asset_name = self._name
    artifacts = hf.build(ext="exr")  # png16: GPU texture + easy CPU readback
    # consume the bake's SCALE CONTRACT (authoritative, self-describing). We set up
    # extent/height from the DSL class attrs pre-bake; the manifest is the bake-time
    # truth (same values + per-channel fit ranges) -> adopt it as the source for the
    # CPU sampler + mesh, and keep it for metric queries (segmentation) later.
    from ork.hypergraph.dflow.terrain.manifest import TerrainManifest
    self._manifest = TerrainManifest.load(artifacts["manifest"])
    self._extent = self._manifest.extent_m
    self._hscale = self._manifest.height_m
    _hs = self._manifest.channels.get("height")
    print(f"terrain viewer2:   scale contract -> extent={self._extent:g}m height={self._hscale:g}m"
          + (f"  height[min={_hs.min:.4f} max={_hs.max:.4f}]" if _hs else ""), flush=True)
    height_path = artifacts["height"]
    img = Image.createFromFile(str(height_path))
    print(img)
    print(img.width, img.height, img.format_name, flush=True)
    print(f"terrain viewer2:   baked height -> {height_path}", flush=True)
    # create texture from image 
    self._height_tex = Texture("height")
    ctx.TXI.updateTexture(self._height_tex, img, False)
    import numpy
    arr = numpy.array(img.numpy, dtype=numpy.float32)   # writable copy
    if arr.ndim == 3:
      arr = arr[..., 0]                                 # (H,W,C) -> (H,W); take channel 0
    self._cpu_hf = arr
    # Integer formats are unorm-encoded -> bring back to [0,1]. Float (R32F EXR) is
    # already the raw field value, so leave it as-is.
    match img.format_name:
      case "R16" | "RG16" | "RGBA16":
        self._cpu_hf /= 65535.0
      case "R8" | "RG8" | "RGBA8":
        self._cpu_hf /= 255.0
      case _:
        pass   # R32F / RGBA32F: raw float field
    self._cpu_dim = self._cpu_hf.shape[0]
    # SPAWN HIGH: seed the camera-follow height ABOVE the highest peak so the whole
    # terrain is visible on launch. The _onUpdate terrain-follow (exp, tau~1s) then
    # smoothly descends toward local-ground + cam_above -> low skim flight.
    terrain_max = float(self._cpu_hf.max()) * self._hscale
    self.smoothed_height = terrain_max + self.cam_above
    # seed the walking camera at ground level over the spawn point (0,0) + clearance.
    self._cam_y = self.terrain_height_smoothed(0.0, 0.0) + self.cam_above

    # Get scenegraph and layer from StandardSceneGraphComponent
    self.scene = self.SGC.scenegraph
    self.layer_fwd = self.SGC.layer_fwd

    #######################################
    # ground material 
    #   hmview : height/slope ptex3d blend (dirt/grass/rock/snow), thresholds bindable
    #######################################

    from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
    # Resolve the material CLASS: an explicit MATERIAL_CLASS from the DSL, else resolve the
    # MATERIAL name by path — searching the terrain asset's OWN folder first, then
    # materials/ — so a bespoke material can live next to its terrain. height_scale +
    # the DSL's MATERIAL_PARAMS (e.g. strata_freq derived from the terrace period) flow through.
    mat_cls = self._material_class
    if mat_cls is None:
      from ork.hypergraph.ptex3d.resolve import resolve_material, load_material_class
      mpath   = resolve_material(self._material, extra_dirs=[Path(self._dsl_path).parent])
      mat_cls = load_material_class(mpath)
    print(f"terrain viewer2:   material -> {mat_cls.__name__} {self._material_params or ''}", flush=True)
    # GPU CHUNKED TERRAIN: the ground is drawn by a ComputeDrawable through the material's
    # FWD_SSBO_CUSTOM variant (see project_fwd_ssbo_custom). The terrain DELEGATES the SSBO-pull
    # vertex side to TerrainChunkVertexSource: per square chunk, a 2D-frustum cull picks visible
    # chunks; the VS derives position/normal/uv from heights[] per vertex (the VS IS the gen — no
    # CPU mesh, no MAXV vertex array). Sampling matches _build_terrain_mesh_arrays so follow aligns.
    from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
    self._vsrc = TerrainChunkVertexSource(dim=self._cpu_dim, extent_m=self._extent,
                                          height_m=self._hscale, chunk=self._chunk)
    print(f"terrain viewer2:   gpu-chunk -> {self._vsrc.cps}x{self._vsrc.cps} chunks "
          f"(chunk={self._chunk}), ssbo {self._vsrc.TOTAL/1e6:.1f}MB", flush=True)
    self._mtl_wrap = Ptex3dAsset(dsl_class=mat_cls, height_scale=self._hscale,
                                 vertex_source=self._vsrc, **self._material_params)
    self._mtl_wrap._ctx = ctx
    gmtl = self._mtl_wrap.as_gfx_material   # builds the generated-fxv2 PBRMaterial (FWD_SSBO_CUSTOM baked in)
    self._gmtl = gmtl

    # the MATERIAL owns which baked channels it samples via ctx.tex(name) — e.g.
    # XXX3Mat.SAMPLER_CHANNELS / bind_textures binds FlowMap <- flow_discharge. The viewer
    # stays generic: invoke the hook (if any) and keep the returned textures alive.
    self._aux_tex = []
    if hasattr(mat_cls, "bind_textures"):
      self._aux_tex = mat_cls.bind_textures(gmtl, artifacts, ctx) or []

    #######################################
    # ground drawable — GPU-culled chunked terrain via ComputeDrawable + FWD_SSBO_CUSTOM.
    # One SSBO holds the heightfield (+ cull args + visible-chunk list); reset/cull/finalize
    # compute (run per-VP in onPreRender, with the camera) pick the in-view chunks; the
    # material's SSBO-pull VS generates only those polys, indirect-drawn. The rendered surface
    # == terrain_height() by construction (TerrainChunkVertexSource matches the CPU sampling).
    #######################################

    import numpy as _np
    from orkengine.lev2 import ComputeDrawableData
    vs  = self._vsrc
    fs  = gmtl.freestyle                                  # internal FreestyleMaterial (technique/storage/compute)
    _hfarr = self._cpu_hf                                 # NB: do NOT shadow `hf` (the HeightField asset, used by scatter)
    if _hfarr.ndim == 3:
      _hfarr = _hfarr[..., 0]
    heights = _np.ascontiguousarray(_hfarr, dtype=_np.float32).reshape(-1)   # row-major: heights[cz*DIM+cx]

    FXI = ctx.FXI
    self._terr_ssbo = FXI.createShaderStorageBufferWithLength(vs.TOTAL)
    FXI.copyDataIntoShaderStorageBuffer(heights, self._terr_ssbo, vs.HEIGHTS_OFF)

    sif = fs.storage("sif_ptex_vtx")
    cdd = ComputeDrawableData()
    cdd.material = gmtl                                   # standard findPipeline(RCID,_isSSBOSourced) -> FWD_SSBO_CUSTOM
    cdd.addGraphicsStorage(sif, self._terr_ssbo)          # the vertex-source SSBO for the pull VS
    cdd.setCameraParams(self._terr_ssbo, vs.CAM_OFF)      # C++ writes the VP block each frame (cull frustum)
    for (name, gx, gy, gz) in vs.compute_passes():        # reset -> cull -> finalize
      cdd.addComputePass(fs.computeShader(name), [(sif, self._terr_ssbo)], gx, gy, gz)
    cdd.setIndirect(args=self._terr_ssbo, args_offset=vs.ARGS_OFF, primtype=tokens.TRIANGLES)
    self.groundnode = self.layer_fwd.createDrawableNodeFromData("terrain", cdd)
    self.groundnode.worldTransform.translation = vec3(0, 0, 0)
    self.groundnode.worldTransform.scale = 1

    # SCATTER: render any baked scatter sinks. The viewer stays GENERIC — the ASSET owns the
    # per-type drawable (its scatter_models() hook), the consumer fills the baked matrices.
    # Each instanced SG node sits in world meters, aligned with the (identity-transform) terrain.
    self._scatter_nodes = []
    _dsl_inst = getattr(hf, "_dsl_inst", None)
    _scatters = artifacts.get("scatters") or {}
    if _scatters and _dsl_inst is not None and hasattr(_dsl_inst, "scatter_models"):
      from ork.hypergraph.dflow.terrain import scatter_consumer
      for _sname, _sinfo in _scatters.items():
        _recipes = _dsl_inst.scatter_models(_sname, ctx)   # ASSET-owned mesh+material per type
        if not _recipes:
          continue
        _nodes = scatter_consumer.install(_sinfo["path"], _recipes, self.layer_fwd, ctx, name=_sname)
        self._scatter_nodes.extend(_nodes)
        print(f"terrain viewer2:   scatter {_sname!r}: {_sinfo['count']} instances across "
              f"{len(_nodes)} type-node(s) {_sinfo['types']}", flush=True)
    # HYPERMESH scatter: the asset's scatter_hypermeshes() hook -> instanced hypermesh draws (one per type).
    # KEEP the returned `lives` alive (the GpuMesh owns the vertex/index SSBOs the drawables reference);
    # the `assets` get their onUpdate driven each frame (animated hypermesh types animate, in sync per type).
    self._scatter_lives  = []
    self._scatter_assets = []
    if _scatters and _dsl_inst is not None and hasattr(_dsl_inst, "scatter_hypermeshes"):
      from ork.hypergraph.dflow.terrain import scatter_consumer
      for _sname, _sinfo in _scatters.items():
        _hm = _dsl_inst.scatter_hypermeshes(_sname, ctx)   # ASSET-owned hypermesh+material per type
        if not _hm:
          continue
        _nodes, _lives, _assets = scatter_consumer.install_hypermeshes(_sinfo["path"], _hm, self.layer_fwd, ctx, name=_sname)
        self._scatter_nodes.extend(_nodes)
        self._scatter_lives.extend(_lives)
        self._scatter_assets.extend(_assets)
        print(f"terrain viewer2:   hyperscatter {_sname!r}: {_sinfo['count']} instances across "
              f"{len(_nodes)} hypermesh type-node(s) {_sinfo['types']}", flush=True)

  ################################################
  # update callback (verbatim from the example; terrain_height now hits the field)
  ################################################

  def _onUpdate(self, updinfo):
    if self._cpu_hf is None:          # terrain not baked yet
      return
    # drive ANIMATED scattered hypermesh types: their onUpdate advances plugs/params; the drawable's
    # in-frame onPreRender re-evals the (shared) graph, so all that type's instances animate in sync.
    for _a in getattr(self, "_scatter_assets", ()):
      if hasattr(_a, "onUpdate"):
        _a.onUpdate(updinfo)
    uicam = self.SGC.uicam
    camlut = self.SGC.cameralut
    DT = updinfo.deltatime
    # arrow-key look: yaw -> heading (Y axis), pitch -> elevation (X axis); recompose
    # the orientation each frame (pure fly camera, like ork.terrain.viewer.py).
    if self._yaw_vel != 0.0:
      uicam.heading = uicam.heading * quat.createFromAxisAngle(
          vec3(0, 1, 0), self._yaw_vel * self._yaw_rate * DT)
    if self._pitch_vel != 0.0:
      uicam.elevation = uicam.elevation * quat.createFromAxisAngle(
          vec3(1, 0, 0), self._pitch_vel * self._pitch_rate * DT)
    uicam.orientation = uicam.elevation * uicam.heading
    # WASD fly in the camera-relative XZ plane (flatten the view dir to the ground).
    zdir = uicam.zDir
    zdir = vec3(zdir.x, 0, zdir.z)
    if zdir.length > 1e-6:
      zdir.normalize()
    xdir = zdir.cross(vec3(0, 1, 0))
    vel  = zdir * self._move.y + xdir * self._move.x
    self._offset += vec3(vel.x, 0, vel.z) * (self._speed * DT)
    # terrain-follow: hold cam_above over the sampled ground; smooth toward it.
    target_y = self.terrain_height_smoothed(self._offset.x, self._offset.z) + self.cam_above
    self._cam_y += (target_y - self._cam_y) * 0.1
    self._offset = vec3(self._offset.x, self._cam_y, self._offset.z)
    uicam.positionOffset = self._offset
    uicam.updateMatrices()
    self.SGC.camera.copyFrom(uicam.cameradata)
    self.SGC.scenegraph.updateScene(camlut)

  ################################################
  # UI event handler for WASD (verbatim from the example)
  ################################################

  def _onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc == ord("W"): self._move = vec2(self._move.x,  1); return ui.HandlerResult()
      if kc == ord("S"): self._move = vec2(self._move.x, -1); return ui.HandlerResult()
      if kc == ord("A"): self._move = vec2(-1, self._move.y); return ui.HandlerResult()
      if kc == ord("D"): self._move = vec2( 1, self._move.y); return ui.HandlerResult()
      if kc == 263: self._yaw_vel   = -1.0; return ui.HandlerResult()  # LEFT arrow
      if kc == 262: self._yaw_vel   =  1.0; return ui.HandlerResult()  # RIGHT arrow
      if kc == 265: self._pitch_vel =  1.0; return ui.HandlerResult()  # UP arrow
      if kc == 264: self._pitch_vel = -1.0; return ui.HandlerResult()  # DOWN arrow
    elif uievent.code == tokens.KEY_UP.hashed:
      kc = uievent.keycode
      if kc in (ord("W"), ord("S")): self._move = vec2(self._move.x, 0); return ui.HandlerResult()
      if kc in (ord("A"), ord("D")): self._move = vec2(0, self._move.y); return ui.HandlerResult()
      if kc in (262, 263): self._yaw_vel   = 0.0; return ui.HandlerResult()
      if kc in (264, 265): self._pitch_vel = 0.0; return ui.HandlerResult()
    return ui.HandlerResult()
  
###############################################################################

def main():
  parser = argparse.ArgumentParser(description='GeoClipMap terrain DSL viewer (basic-clone)')
  parser.add_argument('dsl_file', nargs='?', help='terrain DSL bare name or path to a .py')
  parser.add_argument('--class', dest='class_name', default=None, help='explicit HeightField subclass')
  parser.add_argument('--dim', '-d', type=int, default=2048, help='bake grid resolution (W=H); default 2048')
  parser.add_argument('--chunk', '-c', type=int, default=128,
                      help='GPU cull chunk size in cells/side; default 128 (smaller = finer 2D frustum cull)')
  parser.add_argument('-x', '--extent', type=float, default=None,
                      help='world XZ extent in meters (default: EXTENT_M from the DSL class)')
  parser.add_argument('-H', '--height-scale', dest='height_scale', type=float, default=None,
                      help='FINAL physical meters that normalized height 1.0 represents '
                           '(default: HEIGHT_M from the DSL class)')
  parser.add_argument('--param', '-p', action='append', dest='params', default=[],
                      metavar='KEY=VALUE', help='DSL constructor kwarg (repeatable)')
  parser.add_argument('--list', '-l', action='store_true', help='list terrain DSL files and exit')
  parser.add_argument('-f', '--fullscreen', action='store_true', help='Run in fullscreen mode')
  parser.add_argument('-M', '--material', choices=['hmview', 'fxv2'], default=None,
                      help='ground material (default: MATERIAL from the DSL class, else hmview): '
                           'hmview = height/slope ptex3d blend; '
                           'fxv2 = hand-written terrain_viewer.fxv2 (white, mesh-normal PBR)')
  parser.add_argument('-t', '--ssaa', type=int, default=0,
                      help='SSAA supersample multiplier (0 = off; 1(2x2), 2(3x3), 3(4x4) supersample to kill aliasing)')
  args = parser.parse_args()

  if args.list or args.dsl_file is None:
    list_dsl_files()
    sys.exit(0)

  import ast
  def parse_param(spec):
    if "=" not in spec:
      raise ValueError(f"--param expects KEY=VALUE, got {spec!r}")
    key, raw = spec.split("=", 1)
    try:    value = ast.literal_eval(raw)
    except (ValueError, SyntaxError): value = raw
    return key.strip(), value

  try:
    dsl_path = resolve_dsl_file(args.dsl_file)
    dsl_kwargs = dict(parse_param(s) for s in args.params)
    # The terrain DSL owns its PHYSICAL world scale — read EXTENT_M/HEIGHT_M off the
    # class (defaulted on the HeightField base). CLI flags override per run. (The
    # erosion exaggeration lives inside the DSL graph, not here.)
    dsl_cls = load_dsl_class(dsl_path, args.class_name)
  except (FileNotFoundError, ValueError) as e:
    print(f"terrain viewer2: {e}", file=sys.stderr); sys.exit(2)

  extent_m = args.extent       if args.extent       is not None else float(dsl_cls.EXTENT_M)
  height_m = args.height_scale if args.height_scale is not None else float(dsl_cls.HEIGHT_M)
  # the DSL also declares its suggested material + params (the terrain knows how it wants
  # to be shaded — e.g. strata_freq derived from the terrace period). -M overrides the name.
  material = args.material if args.material is not None else getattr(dsl_cls, "MATERIAL", "hmview")
  material_params = dict(getattr(dsl_cls, "MATERIAL_PARAMS", {}) or {})
  material_class  = getattr(dsl_cls, "MATERIAL_CLASS", None)   # explicit Ptex3d subclass, else resolve by name

  name = Path(dsl_path).stem
  app = GeoClipMapApp(dsl_path, args.class_name, name, args.dim, extent_m, height_m,
                      dsl_kwargs=dsl_kwargs, fullscreen=args.fullscreen, material=material,
                      material_params=material_params, material_class=material_class, ssaa=args.ssaa,
                      chunk=args.chunk)
  app.ezapp.mainThreadLoop()


if __name__ == "__main__":
  main()
