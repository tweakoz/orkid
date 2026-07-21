#!/usr/bin/env python3
###############################################################################
# Phase-2 gate: shader DSL/compile failures raise CATCHABLE python exceptions
#  (not process crashes), and the interpreter survives to compile good shaders.
#
#   A) malformed DSL         -> ork::ParseError        -> catchable (line/col)
#   B) type-broken VTG frag  -> ork::ShaderCompileError -> catchable
#                               (worker-thread JIT: >1 stage; 'no matching
#                                overloaded function' shaderc diagnostic carried
#                                back across the worker boundary)
#   C) good VTG shader still compiles after A + B (host survived)
###############################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

BROKEN_PARSE = "uniform_set us { mat4 mat4 mvp; }\n"  # two type keywords: parse error

def _vtg(frag_body):
  return f"""
fxconfig fxcfg_default {{ }}
uniform_set ublock {{ mat4 mvp; }}
state_block sb_default : default {{ }}
vertex_interface viface : ublock {{
  inputs {{ vec4 position : POSITION; }}
  outputs {{ vec4 frg_clr; }}
}}
fragment_interface fiface {{
  inputs {{ vec4 frg_clr; }}
  outputs {{ layout(location = 0) vec4 out_clr; }}
}}
vertex_shader vs : viface {{
  gl_Position = mvp * position;
  frg_clr = vec4(1.0);
}}
fragment_shader fs : fiface {{
  {frag_body}
}}
technique tek {{
  fxconfig = fxcfg_default;
  vf_pass = {{ vs, fs, sb_default }}
}}
"""

# B: dot(vec2,vec3) -> glslang 'no matching overloaded function'; parses fine.
BROKEN_COMPILE = _vtg("float bad = dot(vec2(1.0), vec3(1.0));\n  out_clr = vec4(bad);")
GOOD_VTG       = _vtg("float ok = dot(vec3(1.0), vec3(1.0));\n  out_clr = vec4(ok);")


def expect_raises(fxi, name, text, want_substr=None):
  try:
    fxi.shaderFromShaderText(name, text)
  except Exception as e:  # noqa: catch anything the binding surfaces
    msg = str(e)
    ok_sub = (want_substr is None) or (want_substr in msg)
    print(f"[{name}] RAISED {type(e).__name__} (want_substr={want_substr!r} present={ok_sub})")
    print(f"[{name}]   msg head: {msg[:200].strip()}")
    return ok_sub
  print(f"[{name}] ERROR: expected an exception, none raised")
  return False


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  fxi = ctx.FXI

  results = {}

  # A) malformed DSL -> catchable ParseError
  results['A_parse'] = expect_raises(fxi, "A_parse_error", BROKEN_PARSE)

  # B) type-broken fragment -> catchable ShaderCompileError with shaderc text
  results['B_compile'] = expect_raises(
      fxi, "B_compile_error", BROKEN_COMPILE, want_substr="no matching overloaded function")

  # C) good shader still compiles AFTER the two failures (interpreter survived)
  good_ok = False
  try:
    sh = fxi.shaderFromShaderText("C_good", GOOD_VTG)
    good_ok = sh is not None
    print(f"[C_good] compiled shader (survived) -> {good_ok}")
  except Exception as e:
    print(f"[C_good] UNEXPECTED failure on good shader: {e}")

  results['C_good'] = good_ok

  ezapp.mainThreadEnd()

  print("=" * 60)
  print("RESULTS:", results)
  all_ok = all(results.values())
  print("GATE:", "PASS" if all_ok else "FAIL")
  sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
  main()
