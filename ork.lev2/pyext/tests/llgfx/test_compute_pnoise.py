#!/usr/bin/env ork.python
"""
Phase-0 gate for the HyperSyn unified procedural substrate.

Proves the canonical sampler-free noise libblock `lib_pnoise` (pnoise.i2)
COMPILES and RUNS inside a COMPUTE shader — both INLINED and IMPORTED from
`orkshader://pnoise.i2` — and that noise(vec3) is sane. This is the single gate
that unblocks the ExprModule -> hfbake/hfdisplacement work.
See .claude/skills/hypersyn/UNIFIED_SUBSTRATE.md §9a.

GPU lifecycle mirrors test_terrain_bake.py: subsystem-mode headless init +
bindGfxToCurrentThread, so inline compute/readback runs on a BOUND context (the
loadingContext lives on its own thread and is the wrong one for inline work).

Outcomes:
  * inline GREEN  -> libblock-in-compute works (expected; mirrors hfdflow_module_pha).
  * import GREEN  -> Phase 1 can SHARE one source via `import "orkshader://pnoise.i2"`.
  * import RED    -> Phase 1 inlines the libblock text instead (still one source). NOT a blocker.
  * inline RED    -> a real dialect problem; investigate (unexpected — noise(vec3) is basic ALU).

Note: GPU-vs-CPU value matching is intentionally NOT asserted — hash()=fract(sin(n)*1e4)
on large n is precision-sensitive, so CPU libm and GPU sin diverge. The meaningful checks
are: compiles, runs, finite + in ~[0,1], and inline==import (same GPU math). Fragment-vs-
compute byte-identity is guaranteed by construction (identical source -> identical SPIR-V).
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import struct
import math
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

tokens = core.CrcStringProxy()

N = 256  # one noise sample per work group

# The sampler-free libblock body — kept identical to pnoise.i2 for the INLINE variant.
LIB_PNOISE = """
libblock lib_pnoise {
  float hash(float n) { return fract(sin(n) * 1e4); }
  float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
  float noise(float x) {
    float i = floor(x); float f = fract(x);
    float u = f * f * (3.0 - 2.0 * f);
    return mix(hash(i), hash(i + 1.0), u);
  }
  float noise(vec2 x) {
    vec2 i = floor(x); vec2 f = fract(x);
    float a = hash(i); float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0)); float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
  }
  float noise(vec3 x) {
    const vec3 step = vec3(110, 241, 171);
    vec3 i = floor(x); vec3 f = fract(x);
    float n = dot(i, step);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash(n + dot(step, vec3(0,0,0))), hash(n + dot(step, vec3(1,0,0))), u.x),
            mix(hash(n + dot(step, vec3(0,1,0))), hash(n + dot(step, vec3(1,1,0))), u.x), u.y),
        mix(mix(hash(n + dot(step, vec3(0,0,1))), hash(n + dot(step, vec3(1,0,1))), u.x),
            mix(hash(n + dot(step, vec3(0,1,1))), hash(n + dot(step, vec3(1,1,1))), u.x), u.y),
        u.z);
  }
}
"""

def shader_text(inline):
  head = "" if inline else 'import "orkshader://pnoise.i2";\n'
  lib  = LIB_PNOISE if inline else ""
  return f"""{head}
fxconfig fxcfg_default {{}}
storage_interface sif_output (descriptor_set 0) {{
  buffer layout(std430) output_data {{ float values[{N}]; }};
}}
compute_interface iface_compute {{
  storage {{ sif_output }}
  inputs {{ layout(local_size_x = 1, local_size_y = 1, local_size_z = 1); }}
}}
{lib}
compute_shader cs_pnoise : iface_compute : lib_pnoise {{
  int index = int(gl_WorkGroupID.x);
  float t   = float(index) * 0.137;
  values[index] = noise(vec3(t, t * 0.5 + 1.3, t * 0.25 + 2.7));
}}
"""


def run_variant(ctx, inline, label):
  """Compile + dispatch one variant; return the list of N floats, or None on compile failure."""
  print(f"\n--- variant '{label}' (inline={inline}) ---", flush=True)
  fxi = ctx.FXI
  ci = ctx.CI
  try:
    shader = fxi.shaderFromShaderText(f"pnoise_{label}", shader_text(inline))
    cs = fxi.computeShader(shader, "cs_pnoise") if shader is not None else None
  except Exception as e:
    print(f"[{label}] COMPILE FAILED: {e}", flush=True)
    return None
  if shader is None or cs is None:
    print(f"[{label}] COMPILE FAILED (null shader/compute object)", flush=True)
    return None
  ssbo = fxi.createShaderStorageBufferWithLength(N * 4)
  # dispatch like the terrain bake driver (hfdflow.cpp): a dispatch PHASE on the
  # bound context, no beginFrame/endFrame. endDispatchPhase submits AND waits, so
  # the SSBO is readable immediately after.
  ci.beginDispatchPhase()
  ci.bindStorageBuffer(cs, 0, ssbo)
  ci.dispatch(cs, N, 1, 1)
  ci.endDispatchPhase()
  m = fxi.mapStorageBuffer(ssbo, 0, N * 4, tokens.READ_ONLY)
  vals = [struct.unpack('f', m.data[i * 4:i * 4 + 4])[0] for i in range(N)]
  fxi.unmapStorageBuffer(m)
  print(f"[{label}] ran OK; first 5 = {[round(v,4) for v in vals[:5]]}", flush=True)
  return vals


def sane(vals):
  finite = all(math.isfinite(v) for v in vals)
  ranged = all(-0.02 <= v <= 1.02 for v in vals)   # value noise of [0,1] hashes
  return finite, ranged


def main():
  print("=" * 60, flush=True)
  print("Phase-0 gate: lib_pnoise in a COMPUTE shader (inline + import)", flush=True)
  print("=" * 60, flush=True)

  # offscreen GPU lifecycle (mirrors test_terrain_bake.py): subsystem-mode init +
  # bindGfxToCurrentThread so inline compute/readback runs on a BOUND context.
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  # INLINE first (must pass — mirrors hfdflow_module_pha's inline libblock).
  inline_vals = run_variant(ctx, inline=True, label="inline")
  # IMPORT second (the nicety). If shadlang can't import a libblock into a compute
  # interface this may fail to compile (caught) — the inline result above is the gate.
  print("\n(attempting the import variant; the inline result above is the gate)", flush=True)
  import_vals = run_variant(ctx, inline=False, label="import")

  ezapp.mainThreadEnd()

  print("\n" + "=" * 60, flush=True)
  ok = True

  if inline_vals is None:
    print("GATE: inline lib_pnoise FAILED to compile/run in compute  -> RED (unexpected)")
    ok = False
  else:
    fin, rng = sane(inline_vals)
    print(f"inline : compiled+ran=YES finite={fin} in[0,1]={rng}")
    ok = ok and fin and rng

  if import_vals is None:
    print("import : NOT usable in compute -> Phase 1 INLINES the libblock (proven path). Not a blocker.")
  else:
    fin, rng = sane(import_vals)
    print(f"import : compiled+ran=YES finite={fin} in[0,1]={rng}")
    ok = ok and fin and rng
    if inline_vals is not None:
      maxdiff = max(abs(a - b) for a, b in zip(inline_vals, import_vals))
      print(f"inline vs import: max|diff| = {maxdiff:.3g}  (same GPU math -> expect ~0)")
      ok = ok and (maxdiff < 1e-5)
      print("import path WORKS -> Phase 1 can SHARE one source via orkshader://pnoise.i2")

  print("=" * 60)
  print("PHASE-0 GATE:", "GREEN" if ok else "RED")
  print("=" * 60)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
