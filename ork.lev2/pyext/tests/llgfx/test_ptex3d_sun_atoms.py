#!/usr/bin/env ork.python
###############################################################################
# ptex3d LIVE-SUN atom gate (jul26). ctx.sun_dir / ctx.has_sun / ctx.sun_color /
# ctx.sun_intensity expose the engine's per-frame directional sun (the ublk_sun block
# written by the forward prologue, fwdnode_impl_sub.cpp _update_sun_cascades) to any
# ptex3d surface expression. The seam is OPT-IN by libblock inheritance, so this gate
# pins the properties a material author depends on:
#   s1  a sun-using surface emits the ublk_sun inheritance on lib_ptex_surface and
#       reads the block members SWIZZLED (xyz = travel direction, w = has_sun).
#   s1c the COLOR/INTENSITY members trigger the same opt-in ON THEIR OWN — a material
#       that weights radiometry by the policy-crossfaded intensity (the sun->moon
#       handoff fix) need not mention sun_dir at all.
#   s2  a material that never names the sun generates text with NO ublk_sun
#       anywhere — the seam is purely additive, so every existing material's
#       cached shader (and content-addressed bake) stays exactly where it was.
#   s3  a compute BAKE rejects every sun atom loudly (a bake is a function of
#       position only; there is no frame, hence no sun).
#   s4  the ExprIR round-trip renders the atoms back to their ctx spellings (an
#       unregistered atom would make editor re-eval of any sun expression fail).
# Pure codegen — no GPU, no app init, no scene.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine import core   # core before lev2 (and before any ptex3d import)

from ork.hypergraph.ptex3d.dsl import SurfaceCtx, P, emit_surface, emit_compute_field
from ork.hypergraph.ptex3d import exprir_surface
from ork.hypergraph.ptex3d.fxv2_template import generate_surface_fxv2

ctx = SurfaceCtx()


def s1_optin_emission():
    """sun body -> ublk_sun inherited + both members read."""
    lit = P.max(P.dot(ctx.N, P.normalize(-ctx.sun_dir)), 0.0) * ctx.has_sun
    body, _libs, inherits, imports, params, _s, _as = emit_surface({"albedo": P.vec3(lit)})
    text = generate_surface_fxv2(body, lib_inherits=inherits, extra_imports=imports,
                                 params=params)
    lib_line = [l for l in text.split("\n") if l.startswith("libblock lib_ptex_surface")]
    ok = (len(lib_line) == 1 and lib_line[0].endswith(": ublk_sun {")
          and "(-sun_dir.xyz)" in text and "sun_dir.w" in text)
    print("    s1 libblock: %s" % (lib_line[0] if lib_line else "<missing>"), flush=True)
    return ok


def s1c_color_optin():
    """the color/intensity members alone trigger the block (the handoff-fix path)."""
    w = P.clamp(ctx.sun_intensity * 0.25, 0.0, 1.0) * ctx.has_sun
    body, _libs, inherits, imports, params, _s, _as = emit_surface(
        {"albedo": ctx.sun_color * w})
    text = generate_surface_fxv2(body, lib_inherits=inherits, extra_imports=imports,
                                 params=params)
    lib_line = [l for l in text.split("\n") if l.startswith("libblock lib_ptex_surface")]
    ok = (len(lib_line) == 1 and lib_line[0].endswith(": ublk_sun {")
          and "sun_color.xyz" in text and "sun_color.w" in text)
    print("    s1c libblock: %s" % (lib_line[0] if lib_line else "<missing>"), flush=True)
    return ok


def s2_additive():
    """no sun in the expression -> no ublk_sun in the generated text."""
    body, _libs, inherits, imports, params, _s, _as = emit_surface(
        {"albedo": P.vec3(P.dot(ctx.N, ctx.N_object))})
    text = generate_surface_fxv2(body, lib_inherits=inherits, extra_imports=imports,
                                 params=params)
    return ("ublk_sun" not in text and "sun_dir" not in text
            and "sun_color" not in text)


def s3_bake_rejects():
    """the bake gate refuses the frame-dependent atoms."""
    results = []
    for node in (P.length(ctx.sun_dir), ctx.has_sun,
                 P.length(ctx.sun_color), ctx.sun_intensity):
        try:
            emit_compute_field(node)
            results.append(False)
        except TypeError as e:
            results.append("no bake form" in str(e))
    return all(results)


def s4_exprir_roundtrip():
    """captured IR renders back to the ctx spellings (editor re-eval)."""
    rendered = [exprir_surface.render(exprir_surface.capture(n))
                for n in (ctx.sun_dir, ctx.has_sun, ctx.sun_color, ctx.sun_intensity)]
    print("    s4 render: %s" % rendered, flush=True)
    return rendered == ["ctx.sun_dir", "ctx.has_sun",
                        "ctx.sun_color", "ctx.sun_intensity"]


def main():
    results = {
        "s1_optin_emission":  s1_optin_emission(),
        "s1c_color_optin":    s1c_color_optin(),
        "s2_additive":        s2_additive(),
        "s3_bake_rejects":    s3_bake_rejects(),
        "s4_exprir_roundtrip": s4_exprir_roundtrip(),
    }
    ok = all(results.values())
    print("\n=== ptex3d sun-atom gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    for k, v in results.items():
        print("    %s: %s" % (k, "ok" if v else "FAIL"), flush=True)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
