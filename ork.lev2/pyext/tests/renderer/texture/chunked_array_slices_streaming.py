#!/usr/bin/env ork.python

################################################################################
# Streaming chunked-upload: 4-slice TextureArray with each slice owning its
# own animation, computed in a dedicated Python thread with numpy,
# double-buffered, and streamed from the loader thread via
# txi.streamTextureRegion() each loader iteration.
#
# Slice patterns (each animates at a different rate):
#   slice 0 — animated checkerboard (cell phase scrolls diagonally)
#   slice 1 — concentric circles (radii pulse, slow rotation)
#   slice 2 — diagonal stripes @ +45° scrolling fast
#   slice 3 — diagonal stripes @ -30° scrolling slow
#
# Rendering: a quad samples sampler2DArray with a time-varying slice index
# `t = mod(time * cycle_rate, 4.0)`, blending slice floor(t) with slice
# (floor(t)+1)%4 by frac(t). So the visible content rotates through all
# four slices smoothly while each slice itself keeps animating.
#
# Setup flow:
#   onGpuInit  (render thread) — reserve + seed initial frames + finalize
#                                 the TextureArray, build pipeline, start
#                                 the producer threads. One-shot.
#   onLoaderUpdate (loader thread) — each loader iteration, per slice:
#                                 check dirty flag; if set, take lock,
#                                 read front buffer, txi.streamTextureRegion().
#   onLoaderInit / onLoaderExit — just lifecycle prints; demonstrate the
#                                 hooks fire. The actual setup runs on the
#                                 render thread to avoid a cross-thread
#                                 ordering dependency.
#
# Threading model:
#   4 daemon producer threads (one per slice) compute frames into back
#   buffers and atomically swap with front buffers under per-slice locks,
#   setting dirty=True. The loader thread reads front buffers and uploads.
################################################################################

import math, signal, sys, threading, time
import numpy as np
from orkengine.core import vec3, vec4, mtx4, CrcStringProxy
from orkengine import lev2
from orkengine.lev2 import lev2exdir

sys.path.append(lev2exdir().as_string + "/python")
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

DIM        = 4096
NUM_SLICES = 4
NUM_MIPS   = int(math.log2(DIM)) + 1   # DIM must be a power of 2

# Pattern features were originally tuned at 256² — scale them with DIM so
# the visual density / speed match regardless of resolution.
SCALE = DIM / 256.0

################################################################################
# Mip-chain helpers. Caller provides every mip level via uploadTextureRegion;
# the chunked-streaming backend takes one (mip, layer) per call, so we
# pre-generate the chain in numpy.
################################################################################

def make_mip_chain(dim):
  """Allocate a list of (W,H,4) uint8 arrays for mip 0..log2(dim)."""
  chain = []
  cur = dim
  while cur >= 1:
    chain.append(np.zeros((cur, cur, 4), dtype=np.uint8))
    cur //= 2
  return chain

def _downsample_2x2(src, dst):
  """2x2 box filter from src (2H, 2W, C) into dst (H, W, C), uint8."""
  H, W, C = dst.shape
  np.copyto(
      dst,
      (src.astype(np.uint16).reshape(H, 2, W, 2, C).sum(axis=(1, 3)) // 4)
          .astype(np.uint8))

def gen_mip_chain(chain, start_mip=1):
  """In-place: chain[0] holds the full image; fill chain[start_mip..] via
     2x2 box filter. ~33% extra compute relative to mip 0 (when start_mip=1)."""
  for k in range(start_mip, len(chain)):
    _downsample_2x2(chain[k-1], chain[k])

# Cycle through all NUM_SLICES every CYCLE_PERIOD seconds.
CYCLE_PERIOD = 4.0

# Per-pattern animation rate multiplier. 1/9 = 9x slower than default.
ANIM_SPEED = 1.0 / 9.0

################################################################################
# Shader — blends two adjacent slices by frac(time) so the visible texture
# smoothly transitions while each underlying slice is itself animating.
################################################################################

STREAM_SHADER = """
fxconfig fxcfg_default { glsl_version = "330"; }

sampler_set samplers(descriptor_set 0) {
  sampler2DArray ColorMap;
}

uniform_set ublock_vtx {
  mat4 mvp;
}

uniform_block ublock_frg (descriptor_set 0) {
  vec4 modcolor;
  float SliceIndex;  // continuous; floor()/fract() pick blend pair
  float NumSlices;
}

vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv;
  }
}

fragment_interface iface_frg : ublock_frg {
  inputs {
    vec2 frg_uv;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}

vertex_shader vs_x : iface_vtx {
  gl_Position = mvp * pos;
  frg_uv = uv0;
}

fragment_shader fs_x : iface_frg : samplers {
  // Composite ALL slices unconditionally (average) so that a single slice
  // flashing black or stale is visible as a brightness/color shift in the
  // sum, rather than being hidden by the slice not being sampled. Makes
  // the streaming bug observable per-slice.
  vec3 sum = vec3(0.0);
  sum += texture(ColorMap, vec3(frg_uv, 0.0)).rgb;
  sum += texture(ColorMap, vec3(frg_uv, 1.0)).rgb;
  sum += texture(ColorMap, vec3(frg_uv, 2.0)).rgb;
  sum += texture(ColorMap, vec3(frg_uv, 3.0)).rgb;
  out_clr = vec4(sum * 0.5 * modcolor.rgb, 1.0);
}

state_block sb_default : default {
  DepthTest = LEQUALS;
  DepthMask = ON;
}

technique tek_x {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_x;
    fragment_shader = fs_x;
    state_block     = sb_default;
  }
}
"""

################################################################################
# Rolling-window stats reporter. Each tick records one event; every
# REPORT_PERIOD seconds the accumulator prints a one-line summary and
# resets. Thread-safe so multiple producers + loader + render can all
# share an instance per role.
################################################################################

REPORT_PERIOD = 2.0

class Stats:
  def __init__(self, label):
    self.label = label
    self._lock = threading.Lock()
    self._count = 0
    self._total_us = 0
    self._min_us = float("inf")
    self._max_us = 0
    self._last_print = time.monotonic()

  def tick(self, duration_us):
    with self._lock:
      self._count += 1
      self._total_us += duration_us
      if duration_us < self._min_us: self._min_us = duration_us
      if duration_us > self._max_us: self._max_us = duration_us
      now = time.monotonic()
      elapsed = now - self._last_print
      if elapsed >= REPORT_PERIOD:
        rate = self._count / elapsed
        avg = self._total_us / self._count if self._count else 0
        print(f"[{self.label}] {rate:6.1f}/s  "
              f"avg={avg/1000.0:6.2f}ms  "
              f"min={self._min_us/1000.0:6.2f}ms  "
              f"max={self._max_us/1000.0:6.2f}ms  "
              f"n={self._count}")
        self._count = 0
        self._total_us = 0
        self._min_us = float("inf")
        self._max_us = 0
        self._last_print = now

################################################################################
# Slice producers — each owns a back buffer + front buffer + lock + dirty
# flag. Compute thread fills back, swaps under lock, sets dirty=True.
# Loader thread reads front under lock when dirty, clears flag, uploads.
################################################################################

PRODUCER_WORKERS = 4  # horizontal strips per producer

class SliceProducer:
  def __init__(self, slice_index, dim, pattern_fn, fps):
    self.slice_index = slice_index
    self.dim = dim
    self.pattern_fn = pattern_fn
    self.frame_period = 1.0 / fps
    # Pre-allocate FULL MIP CHAINS for back and front so the hot loop has
    # no allocator pressure. chain[0] is mip 0 (dim x dim); chain[k] is
    # dim/2^k x dim/2^k. Workers write mip 0 strips; coordinator generates
    # mips 1..N-1 via box filter.
    self._back_chain  = make_mip_chain(dim)
    self._front_chain = make_mip_chain(dim)
    # `_back` / `_front` aliases for mip 0 — keep so the strip workers
    # can index by row without learning about chains.
    self._back  = self._back_chain[0]
    self._front = self._front_chain[0]
    self._lock  = threading.Lock()
    self._dirty = False
    self._stop  = False
    self._t0    = time.monotonic()
    self._frame_index = 0
    self._stats = Stats(f"prod{slice_index}")
    # Phi & n snapshotted by the coordinator at the start of each frame
    # so all PRODUCER_WORKERS workers compute against identical values.
    self._cur_phi = 0.0
    self._cur_n   = 0
    # Strip workers: split the image into PRODUCER_WORKERS horizontal
    # strips; each worker writes its strip into self._back. Barriers
    # gate go / done; numpy releases the GIL on its hot ops so the
    # workers actually run in parallel.
    assert dim % PRODUCER_WORKERS == 0
    self._strip_h = dim // PRODUCER_WORKERS
    # Deepest mip whose strip-height is still >= 1 row per worker. Workers
    # can compute their share of mips 0..L; coordinator handles the small
    # remaining mips (L+1..N-1) since they don't divide N ways.
    self._worker_deepest_mip = int(math.log2(self._strip_h))
    self._start_barrier = threading.Barrier(PRODUCER_WORKERS + 1)
    self._done_barrier  = threading.Barrier(PRODUCER_WORKERS + 1)
    self._workers = [
        threading.Thread(
            target=self._worker, args=(q,),
            name=f"slice-{slice_index}-w{q}", daemon=True)
        for q in range(PRODUCER_WORKERS)
    ]
    self._thread = threading.Thread(
        target=self._run, name=f"slice-{slice_index}", daemon=True)

  def start(self):
    for w in self._workers:
      w.start()
    self._thread.start()

  def stop(self):
    self._stop = True
    # Wake any worker currently waiting on a barrier.
    try: self._start_barrier.abort()
    except threading.BrokenBarrierError: pass
    try: self._done_barrier.abort()
    except threading.BrokenBarrierError: pass

  def _worker(self, q_idx):
    y0 = q_idx * self._strip_h
    while True:
      try:
        self._start_barrier.wait()
      except threading.BrokenBarrierError:
        return
      if self._stop:
        return
      # 1. Mip 0 strip via the producer's pattern function.
      strip0 = self._back_chain[0][y0 : y0 + self._strip_h]
      self.pattern_fn(strip0, self._cur_phi, self._cur_n, y0, 0)
      # 2. This worker's strip share of mips 1..L (L = deepest mip that
      #    splits cleanly across PRODUCER_WORKERS). Each worker reads
      #    only ITS OWN rows of mip k-1 (which it just wrote) and writes
      #    only ITS OWN rows of mip k. No cross-worker reads, no race.
      for k in range(1, self._worker_deepest_mip + 1):
        strip_h_k = self._strip_h >> k
        y0_k      = y0 >> k
        src = self._back_chain[k-1][y0_k*2 : y0_k*2 + strip_h_k*2]
        dst = self._back_chain[k]    [y0_k   : y0_k   + strip_h_k    ]
        _downsample_2x2(src, dst)
      try:
        self._done_barrier.wait()
      except threading.BrokenBarrierError:
        return

  def _run(self):
    while not self._stop:
      phi = (time.monotonic() - self._t0) * ANIM_SPEED
      # Snapshot phi/n for the workers — they all read these values
      # so the strips are coherent. Set BEFORE the start barrier.
      self._cur_phi = phi
      self._cur_n   = self._frame_index

      t0 = time.monotonic()
      try:
        self._start_barrier.wait()  # release workers (compute mip 0 strips)
        self._done_barrier.wait()   # wait for all strips to finish
      except threading.BrokenBarrierError:
        return
      # Workers already filled mips 0..L in parallel. Coordinator does
      # the few remaining tiny mips (L+1..N-1, e.g. 2x2 and 1x1 at 4K).
      gen_mip_chain(self._back_chain,
                    start_mip=self._worker_deepest_mip + 1)
      compute_us = (time.monotonic() - t0) * 1e6

      with self._lock:
        # Swap whole chains (cheap reference swap; numpy arrays inside
        # are not copied).
        self._back_chain,  self._front_chain  = self._front_chain,  self._back_chain
        self._back,        self._front        = self._back_chain[0], self._front_chain[0]
        self._dirty = True
      self._frame_index += 1
      self._stats.tick(compute_us)
      time.sleep(self.frame_period)

  def take_frame(self):
    """Loader-thread side. Returns (bytes, True) if a new frame is ready
       since last call, else (None, False). Cheap: copies only when dirty."""
    with self._lock:
      if not self._dirty:
        return (None, False)
      self._dirty = False
      # Copy out front buffer bytes so the upload's staging copy doesn't
      # race the producer the next time it swaps in.
      data = self._front.tobytes()
    return (data, True)

  def latest_bytes(self):
    """Always returns a snapshot of the producer's latest front buffer
       (mip 0 only), regardless of dirty state."""
    with self._lock:
      self._dirty = False
      return self._front.tobytes()

  def latest_chain_bytes(self):
    """Always returns a list of bytes snapshots, one per mip level, from
       the producer's latest front mip chain. Used when the loader needs
       to re-upload every (slice, mip) every cycle."""
    with self._lock:
      self._dirty = False
      return [m.tobytes() for m in self._front_chain]

################################################################################
# Pattern functions — write into `out` (HxWx4 uint8 numpy view).
# All allocate scratch via numpy broadcasting; per-frame cost dominates.
################################################################################

def pattern_checkerboard(out, phi, n, y_off, x_off):
  H, W, _ = out.shape
  cell = int(16 * SCALE)
  # Wrap the scroll offset at the FULL visual period (2 * cell), not at
  # `cell`. Wrapping at `cell` flips the checker parity each wrap.
  period = 2 * cell
  ox = int((phi * 60 * SCALE) % period)
  oy = int((phi * 40 * SCALE) % period)
  y = (np.arange(y_off, y_off + H) + oy)[:, None] // cell
  x = (np.arange(x_off, x_off + W) + ox)[None, :] // cell
  on = ((x ^ y) & 1).astype(np.uint8)
  r = np.where(on, 255, 0).astype(np.uint8)
  out[..., 0] = r
  out[..., 1] = 0
  out[..., 2] = 0
  out[..., 3] = 255

def pattern_concentric(out, phi, n, y_off, x_off):
  H, W, _ = out.shape
  # Center is in global coords: DIM/2.
  yy = (np.arange(y_off, y_off + H) - DIM / 2)[:, None]
  xx = (np.arange(x_off, x_off + W) - DIM / 2)[None, :]
  r = np.sqrt(xx * xx + yy * yy)
  # High-contrast rings: hard step on a radial phase.
  band = (7.0 + 2.0 * math.sin(phi * 0.9)) * SCALE   # ~7px per ring at 256²
  phase = (r / band) - phi * 2.0
  ring = (np.floor(phase).astype(np.int32) & 1).astype(np.uint8)
  bw = np.where(ring, 255, 0).astype(np.uint8)
  out[..., 0] = bw
  out[..., 1] = bw
  out[..., 2] = bw
  out[..., 3] = 255

def _diag_stripes_n(out, n, angle_deg, px_per_publish, color_a, color_b, y_off, x_off):
  """Frame-counter driven: each producer publish advances the stripe by
     exactly `px_per_publish` pixels along the stripe normal."""
  H, W, _ = out.shape
  rad = math.radians(angle_deg)
  cs, sn = math.cos(rad), math.sin(rad)
  yy = np.arange(y_off, y_off + H)[:, None].astype(np.float32)
  xx = np.arange(x_off, x_off + W)[None, :].astype(np.float32)
  proj = xx * cs + yy * sn + float(n * px_per_publish)
  stripe = ((proj // (14.0 * SCALE)).astype(np.int32) & 1).astype(np.uint8)
  out[..., 0] = np.where(stripe, color_a[0], color_b[0]).astype(np.uint8)
  out[..., 1] = np.where(stripe, color_a[1], color_b[1]).astype(np.uint8)
  out[..., 2] = np.where(stripe, color_a[2], color_b[2]).astype(np.uint8)
  out[..., 3] = 255

def pattern_diag_a(out, phi, n, y_off, x_off):
  # pure green stripes (G on/off), +45°, 1 px per publish
  _diag_stripes_n(out, n, angle_deg=45.0, px_per_publish=1,
                  color_a=(0, 255, 0), color_b=(0, 0, 0),
                  y_off=y_off, x_off=x_off)

def pattern_diag_b(out, phi, n, y_off, x_off):
  # pure blue stripes (B on/off), -30°, 1 px per publish
  _diag_stripes_n(out, n, angle_deg=-30.0, px_per_publish=1,
                  color_a=(0, 0, 255), color_b=(0, 0, 0),
                  y_off=y_off, x_off=x_off)

################################################################################
# Quad mesh helper (identical to chunked_array_slices.py).
################################################################################

def buildQuadMesh():
  vertices = np.array([
    [-1.0, -1.0, 0.0],
    [ 1.0, -1.0, 0.0],
    [ 1.0,  1.0, 0.0],
    [-1.0,  1.0, 0.0],
  ], dtype=np.float32)
  uvs = np.array([
    [0.0, 1.0],
    [1.0, 1.0],
    [1.0, 0.0],
    [0.0, 0.0],
  ], dtype=np.float32)
  faces = [4, 0, 1, 2, 3]
  return vertices, uvs, faces

################################################################################

class App:
  def __init__(self):
    # IMPORTANT: initialize all attributes that loader-thread callbacks
    # might touch BEFORE calling OrkEzApp.create(self). create() registers
    # the Python hooks (onLoaderInit/Update/Exit, onGpu*) and the loader
    # thread is already running by that point — onLoaderUpdate can fire
    # on the next loader iteration, racing the rest of __init__.
    self.time = 0.0
    self.tex_array = None
    self._setup_done = False
    self.producers = [
        SliceProducer(0, DIM, pattern_checkerboard, fps=60),
        SliceProducer(1, DIM, pattern_concentric,   fps=45),
        SliceProducer(2, DIM, pattern_diag_a,       fps=90),
        SliceProducer(3, DIM, pattern_diag_b,       fps=30),
    ]
    # Instrumentation. loader_iter = all onLoaderUpdate calls;
    # loader_cycle = those where any producer was dirty and we did
    # an actual upload+finalize batch. render = one tick per render frame.
    self._loader_iter_stats  = Stats("loader_iter")
    self._loader_cycle_stats = Stats("loader_cycle")
    self._render_stats       = Stats("render_frame")
    self._last_render_t      = time.monotonic()
    # Now safe to publish self to the engine.
    self.ezapp = lev2.OrkEzApp.create(self, height=720, width=960)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 0, 3), tgt=vec3(0, 0, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())

  ############################################################################
  # Loader-thread hooks. Setup happens on render thread (onGpuInit);
  # here we only stream incremental updates.
  ############################################################################

  def onLoaderInit(self, ctx):
    print("[onLoaderInit] loader thread alive")

  def onLoaderUpdate(self, ctx):
    """Fires every loader iteration. When ANY producer has new data, we
       upload ALL four slices to the back image using each producer's
       latest published front buffer, then commit via finalizeUpload.

       Why upload all four: the streaming backend uses a strict double
       buffer with no in-CB front-to-back copy (the copy would race
       in-flight render frames sampling the current front). So each
       cycle's back image gets only the slices we write — anything not
       written shows whatever was in the back from the prior swap, which
       is *two cycles* stale. By re-uploading every slice from each
       producer's latest publication every cycle, the back is always
       fully populated and the visible texture is always coherent."""
    iter_t0 = time.monotonic()
    if not self._setup_done:
      self._loader_iter_stats.tick((time.monotonic() - iter_t0) * 1e6)
      return
    any_dirty = any(p._dirty for p in self.producers)
    if not any_dirty:
      self._loader_iter_stats.tick((time.monotonic() - iter_t0) * 1e6)
      return
    cycle_t0 = time.monotonic()
    txi = ctx.TXI
    for p in self.producers:
      chain = p.latest_chain_bytes()
      for k, data in enumerate(chain):
        mw = max(1, DIM >> k)
        mh = max(1, DIM >> k)
        txi.uploadTextureRegion(
            tex=self.tex_array.tex,
            mip_level=k, array_layer=p.slice_index,
            offset_x=0, offset_y=0, offset_z=0,
            extent_w=mw, extent_h=mh, extent_d=1,
            data=data)
    txi.finalizeUpload(tex=self.tex_array.tex)
    cycle_us = (time.monotonic() - cycle_t0) * 1e6
    self._loader_cycle_stats.tick(cycle_us)
    self._loader_iter_stats.tick((time.monotonic() - iter_t0) * 1e6)

  def onLoaderExit(self, ctx):
    print("[onLoaderExit] stopping producers")
    for p in self.producers:
      p.stop()

  ############################################################################
  # Render-thread hooks
  ############################################################################

  def onGpuInit(self, ctx):
    self.ctx = ctx
    createSceneGraph(app=self)
    txi = ctx.TXI

    # Reserve a STREAMING + MIPMAPPED TextureArray. The chunked backend
    # allocates two GPU images each with NUM_MIPS levels; the caller
    # uploads every (mip, slice) per cycle; finalizeUpload swaps front/back.
    self.tex_array = lev2.TextureArray(w=DIM, h=DIM, slices=NUM_SLICES,
                                       fmt=tokens.RGBA8, mipmapped=True)
    self.tex_array.streaming = True
    txi.reserveTextureArray(tarr=self.tex_array,
                            w=DIM, h=DIM,
                            num_slices=NUM_SLICES, num_mips=NUM_MIPS,
                            fmt=tokens.RGBA8)
    # Seed the first cycle: write mip 0 (full image at phi=0) into each
    # producer's front chain, generate the rest of the chain, then upload
    # every (mip, slice) of the back.
    for p in self.producers:
      p.pattern_fn(p._front_chain[0], 0.0, 0, 0, 0)
      gen_mip_chain(p._front_chain)
      for k, mip in enumerate(p._front_chain):
        mw = max(1, DIM >> k)
        mh = max(1, DIM >> k)
        txi.uploadTextureRegion(
            tex=self.tex_array.tex,
            mip_level=k, array_layer=p.slice_index,
            offset_x=0, offset_y=0, offset_z=0,
            extent_w=mw, extent_h=mh, extent_d=1,
            data=mip.tobytes())

    # Commit. On GPU completion, back becomes front and _img_sampling is
    # set; streaming uploads on the loader thread can begin safely.
    def _onFinalized():
      self._setup_done = True
      print("[finalize] first cycle complete; streaming enabled")
    txi.finalizeUpload(tex=self.tex_array.tex, on_complete=_onFinalized)

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "chunked_array_streaming", STREAM_SHADER)
    mtl.rasterstate.culltest  = tokens.OFF
    mtl.rasterstate.depthtest = tokens.LEQUALS

    permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = mtl.shader.technique("tek_x")

    self.pipeline = mtl.fxcache.findPipeline(permu)
    self.pipeline.bindParam(mtl.param("mvp"),       tokens.RCFD_Camera_MVP_Mono)
    self.pipeline.bindParam(mtl.param("modcolor"),  vec4(1, 1, 1, 1))
    self.pipeline.bindParam(mtl.param("ColorMap"),  self.tex_array)
    self.pipeline.bindParam(mtl.param("NumSlices"), float(NUM_SLICES))
    self.slice_param = mtl.param("SliceIndex")
    self.pipeline.bindParam(self.slice_param, float(0.0))
    self.pipeline.sharedMaterial = mtl

    verts, uvs, faces = buildQuadMesh()
    self.mesh = lev2.MicroMesh.fromVertAndFaceLists(verts, faces)
    self.mesh.updateUVs(uvs)
    self.mesh.computeNormals()
    self.prim = lev2.RigidPrimitive()
    self.prim.updateWithMicroMesh(self.mesh, ctx, tokens.TRIANGLES)
    self.node = self.prim.createNode("quad", self.layer1, self.pipeline)
    self.node.worldTransform.scale = 1.0

    self.scene.lightingmanager.gpuInit(ctx)

    # Start producer threads. Streaming uploads on the loader thread are
    # gated on _setup_done, which finalize's on_complete callback flips
    # only after the GPU has actually transitioned the array layout.
    for p in self.producers:
      p.start()

  def onGpuUpdate(self, ctx):
    # Continuous slice index — fragment shader does floor/fract to pick
    # the two slices to blend.
    t = (self.time / CYCLE_PERIOD) * NUM_SLICES
    self.pipeline.bindParam(self.slice_param, float(t))
    # Render-frame timing: time between successive onGpuUpdate calls.
    now = time.monotonic()
    self._render_stats.tick((now - self._last_render_t) * 1e6)
    self._last_render_t = now

  def onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  def onUpdate(self, updinfo):
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut)


App().ezapp.mainThreadLoop()
