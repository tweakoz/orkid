###############################################################################
# ork.hypergraph.ptex3d.compute_template — assemble a COMPUTE shader from an
# emitted SurfNode body (emit_compute_field) for the terrain bake (the unified
# procedural substrate: self.hfbake / hfmask / hfdisplacement).
#
# Counterpart to fxv2_template.py (the FRAGMENT shell). The text carries
# %DIMU%/%DIMSQ%/%EXTENT_M%/%HEIGHT_M% placeholders the C++ ExprModule fills per
# bake (BakeEnv, PHYSICAL scale). The shell defines the atoms the body references:
#   opos / wpos : world position of the texel (origin-centered XZ; texel-CENTER)
#   onrm        : object normal (up; a heightfield generator has no surface yet)
#   footprint   : texel size extent_m/dim  -> ctx.footprint (the bake AA metric)
#
# Noise: the canonical sampler-free `lib_pnoise` (pnoise.i2) is imported and the
# shader inherits it, so `noise(...)` resolves to the SAME math as the fragment's
# lib_mmnoise (byte-identical -> bake/shade coincide). See UNIFIED_SUBSTRATE.md §4.
###############################################################################


def build_field_shader(lines, final, libsrcs=(), inherits=(), n_inputs=0):
  """Full compute shader text (with %placeholders%) computing one scalar field.
     lines/final come from emit_compute_field; libsrcs/inherits are its deps.
     n_inputs: image inputs to declare (In0..); input 0 is the current height
     (sets ctx.P_object.y = in0*height_m), the rest reachable as ctx.input(k)."""
  use_noise = ("lib_mmnoise" in inherits) or ("lib_pnoise" in inherits)
  imports = 'import "orkshader://pnoise.i2";\n' if use_noise else ""

  # Helper sources (e.g. _ptex_fbm) call noise(), so they must see lib_pnoise. Wrap
  # them in a libblock that inherits it; the compute_shader inherits that. With no
  # libsrcs we inherit lib_pnoise directly (noise() is then in scope in the body);
  # with neither noise nor libsrcs the shader needs no libblock at all.
  #
  # Struct-returning ops (e.g. P.voronoi -> ptex_voro_t) need their struct TYPE in
  # scope. The fragment template (fxv2_template) defines it in `typeblock types_ptex`
  # which lib_ptex_surface inherits; mirror that here when a libsrc references the
  # struct, and inherit it into lib_expr — the compute_shader then sees it transitively
  # (exactly as the fragment shader gets SurfaceOut via the surface libblock).
  typeblock, types_inherit = "", ""
  if any("ptex_voro_t" in s for s in libsrcs):
    typeblock = ("typeblock types_expr {\n"
                 "  struct ptex_voro_t { float f1; float edge; float fwedge; float cellA; float cellB; };\n"
                 "}\n")
    types_inherit = " : types_expr"
  if libsrcs:
    base = types_inherit + (" : lib_pnoise" if use_noise else "")
    libblock = typeblock + "libblock lib_expr%s {\n%s\n}\n" % (base, "\n".join(libsrcs))
    inherit = " : lib_expr"
  else:
    libblock = ""
    inherit = " : lib_pnoise" if use_noise else ""

  # input image fields (In0..In{n-1}): one storage_interface each, listed in the
  # compute_interface AFTER sif_out (so the bind slots are out=0, in0=1, in1=2, ...).
  # %% escapes the % for the %-format below; survives as %DIMSQ% for the C++ sub.
  in_decls = "".join(
      "storage_interface sif_in%d (descriptor_set 0) { buffer layout(std430) ib%d { float in%ddata[%%DIMSQ%%]; }; }\n"
      % (k, k, k) for k in range(n_inputs))
  in_list  = "".join(" sif_in%d" % k for k in range(n_inputs))
  in_reads = "".join("  float in%d = in%ddata[i];\n" % (k, k) for k in range(n_inputs))
  # CONVENTION: input 0 is the current HEIGHT (normalized). Set the surface elevation
  # to in0 * height_m (PHYSICAL) so ctx.P_object.y / ctx.P.y match what a fragment
  # material sees -> the SAME strata(ctx) shades AND displaces (the unification).
  ypos = "in0data[i] * float(%HEIGHT_M%)" if n_inputs >= 1 else "0.0"

  # NB: concatenate (don't %-format) — the literal "%DIMU%" would be misread as a spec.
  write = "odata[i] = " + final + ";"
  body = "\n  ".join(list(lines) + [write])

  return f"""{imports}fxconfig fxcfg_default {{}}
storage_interface sif_out (descriptor_set 0) {{
  buffer layout(std430) ob {{ float odata[%DIMSQ%]; }};
}}
{in_decls}compute_interface iface {{
  storage {{ sif_out{in_list} }}
  inputs {{ layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); }}
}}
{libblock}compute_shader cs_expr : iface{inherit} {{
  if (gl_GlobalInvocationID.x >= %DIMU% || gl_GlobalInvocationID.y >= %DIMU%) {{ return; }}
  uint xi = gl_GlobalInvocationID.x;
  uint yi = gl_GlobalInvocationID.y;
  uint i  = yi * %DIMU% + xi;
{in_reads}  vec2 uv = (vec2(float(xi), float(yi)) + 0.5) / float(%DIMU%);   // texel-CENTER
  vec2 xz = (uv - 0.5) * float(%EXTENT_M%);                        // world XZ, origin-centered
  float __y = {ypos};                  // physical elevation from input-0 height (0 = generator)
  vec3 opos = vec3(xz.x, __y, xz.y);   // ctx.P_object — Y matches the fragment surface (strata aligns)
  vec3 wpos = opos;                    // ctx.P
  vec3 onrm = vec3(0.0, 1.0, 0.0);     // ctx.N_object
  float footprint = float(%EXTENT_M%) / float(%DIMU%);            // ctx.footprint = texel size
  float height_m  = float(%HEIGHT_M%);  // ctx.height_m
  float extent_m  = float(%EXTENT_M%);  // ctx.extent_m
  {body}
}}
"""
