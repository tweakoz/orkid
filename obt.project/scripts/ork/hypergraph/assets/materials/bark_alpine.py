###############################################################################
# BarkAlpine — the ALPINE bark family for the tree SECTION-BAKE path (TREES2
# Track 2: the parked live `Bark` revived as a BAKE-TIME source, extended to the
# frequency ladder). Everything here is evaluated ONCE per section layer by the
# GPU section-bake driver; runtime pays texture lookups only.
#
# OBJECT-SPACE TEXTURING (owner ruling 2026-08-11: "if its the same tree it
# should look the same … it should be textured in object space"): the ladder is
# authored ONCE, in the object space of the shared ~2.34 m prototype skeleton
# (ls_trunk_baked.BROADLEAF_SHAPE). The bake never sees a draw scale; the
# instance transform magnifies bark WITH the tree, so a x45 individual gets
# proportionally bigger furrows than a x15 one — intended, embraced. The old
# world-meter authoring + world_scale/feature_scale compensation knobs are
# REMOVED (they made the same material read ~3 cm in the prototype and ~1.65 m
# in the forest). Numerically this ladder IS the r7-approved read: the ratified
# feature_scale = world_scale/4 ratio collapses to object sizes = old world
# sizes / 4 (the field coordinate was always P_object * 4), so at the forest's
# mean x30 instance the trunk carries ~18 ridges around / ~9 across a face —
# the old-growth reference.
#
# Frequency ladder (all sizes in OBJECT meters on the ~2.34 m prototype; world
# size = object size x instance scale, e.g. x30 mean -> furrow 0.055 -> 1.65 m
# ridge-to-ridge on a ~9.6 m trunk):
#   meso  0.014-0.075 : the species furrow language, height -> normal + cavity
#                   AO. ONE parameter space spans the alpine species:
#                   `plate_mix` crossfades longitudinal ridge/furrow fbm
#                   (larch/oak) into voronoi scale plates (spruce); `lenticel`
#                   adds the smooth-bark horizontal lenticel banding (beech/
#                   young wood).
#   micro ~0.012      : grain fbm -> roughness zoning + fine relief. The
#                   resolvable end is set by the per-scene bake res budget
#                   (object texel = mesh extent / res); the sub-texel end waits
#                   on the hybrid live-detail pass or higher bake res.
#   overlay 0.05-0.15 : moss (lower trunk, north aspect, cavity-seeking) + pale
#                   lichen plates (ridge-top voronoi cells) as albedo+roughness
#                   zoning. Masks: cavity, OBJECT height above base, aspect
#                   (horizontal normal vs north).
#
# Physical-albedo doctrine (owner-ratified): bark albedo value ~0.15-0.35,
# authored below via HSV; NO gain compensation against the scene's lighting
# mood. Roughness ~0.9 base (matte needs >0.75 in this engine).
#
# Species param sets are DATA (BARK_SPECIES) — three alpine languages, no
# hardcoded species zoo. SAME-TREE-SAME-LOOK CONTRACT: scenes consume these
# dicts VERBATIM (no per-scene overrides — an override would both fork the look
# across scenes and bypass bark_content_key's re-keying). Per-species sets on
# distinct asset names re-key the per-variant bake (the owner-ratified
# bake-per-variant shape).
#
# Capture schema = the SectionArrayPBR contract (3 targets, MRT order):
#   SectionAlbedo  xyz albedo                  (w UNFILLED -> coverage 1.0)
#   SectionParams  x=rough y=metal z=ao, w=1   (own target — the OIIO PNG write
#                  path divides RGB by packed alpha, so rough/ao NEVER ride a
#                  base/normal alpha; the TERR2 mrao law verbatim)
#   SectionNormal  tangent-space n*0.5+0.5     (signed->UNORM encode; decoded
#                  *2-1 + tbn re-expansion in the sampler)
###############################################################################
import hashlib

from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_warp, section_normal
from ork.hypergraph.colors import hsv


def _object_bump_normal(ctx, height, *, strength):
  """RES-INDEPENDENT capture-space bump: tangent slope per OBJECT METER, not per
  atlas texel. The shared world_bump_normal uses raw dFdx/dFdy — per-PIXEL
  slopes — so a BAKED normal's strength scales with 1/bake_res AND with each
  chart's texel density: the same bark baked at 1024 (forest budget) carried
  ~4x the tilt of the 4096 prototype bake — normals slammed sideways (observed:
  rainbow atlas at 1024 vs blue-dominant at 4096), lighting them as black
  facets with hot specular = the owner's "black shiny metal" chunks, and the
  two scenes' coats diverging (same-tree-same-look violation). Normalizing each
  screen derivative by the object-space footprint of the same screen step makes
  the encoded slope dh/d(object m) — identical at every bake res and chart
  density. `strength` is now DIMENSIONLESS slope gain (1.0 = true geometric
  slope of the authored heights)."""
  du = P.length(P.dFdx(ctx.P_object))            # object meters per atlas step (u)
  dv = P.length(P.dFdy(ctx.P_object))            # object meters per atlas step (v)
  hu = P.dFdx(height) / P.max(du, 1.0e-6)
  hv = P.dFdy(height) / P.max(dv, 1.0e-6)
  ts = P.normalize(P.vec3(hu * -strength, hv * -strength, 1.0))
  return P.func("({0} * {1})", [ctx.tbn, ts], rtype="vec3")


# THE one surviving look knob (owner ruling): a single shared ladder dilation —
# wavelengths AND relief amplitudes together, so slopes are preserved. Both
# scenes read it from HERE (module data folds into bark_content_key, so edits
# re-bake everywhere identically). NEVER per-scene, NEVER derived from scatter
# scale. 1.0 = the authored object-space ladder below.
LADDER_SCALE = 1.0


###############################################################################
# species parameter sets — DATA, not classes. Pass as **BARK_SPECIES[name] to
# BarkAlpineStored, VERBATIM (see the same-tree-same-look contract above).
###############################################################################
BARK_SPECIES = {
    # deep longitudinal furrows (larch / oak language)
    "furrow": dict(
        plate_mix      = 0.0,
        furrow_m       = 0.055,    # ridge-to-ridge around the trunk (object m)
        furrow_depth_m = 0.010,
        lenticel       = 0.0,
        ridge_color    = hsv(27.0, 0.30, 0.35),
        furrow_color   = hsv(19.0, 0.45, 0.11)),
    # scale plates, per-plate tonal drift (spruce language)
    "plate": dict(
        plate_mix      = 1.0,
        plate_m        = 0.014,
        plate_depth_m  = 0.0035,
        lenticel       = 0.0,
        ridge_color    = hsv(18.0, 0.34, 0.26),
        furrow_color   = hsv(14.0, 0.45, 0.13)),
    # smooth + lenticel banding, silvery-gray (beech / young upper wood)
    "smooth": dict(
        plate_mix      = 0.0,
        furrow_m       = 0.075,
        furrow_depth_m = 0.0015,
        furrow_contrast= 0.55,
        lenticel       = 0.55,
        rough_base     = 0.80,
        rough_var      = 0.10,
        moss_amt       = 0.15,     # barely any moss up in the crown wood
        lichen_amt     = 0.45,
        ridge_color    = hsv(45.0, 0.10, 0.34),
        furrow_color   = hsv(40.0, 0.16, 0.22)),
}


def bark_content_key():
  """CACHE-KEY SHIM, shared by every scene that bakes this family: the C++
  section-bake content key (sectionBakeContentKey, hmdflow_render.cpp) hashes
  the graph + the material asset NAMES — not the materials' shader content or
  params. Folding a hash of THIS module's bytes into the bake-asset names makes
  any look edit re-key the bake automatically. Under the object-space ruling
  ALL bark material data lives in this one module (species dicts, defaults,
  LADDER_SCALE; scenes pass BARK_SPECIES entries verbatim), so the module hash
  IS the material identity — and every scene prints the SAME key, which is the
  same-bake-everywhere gauge. Retire when the engine key learns material
  identity (seam filed)."""
  with open(__file__, "rb") as f:
    return hashlib.sha1(f.read()).hexdigest()[:8]


def _bark_surface(self, ctx, *,
                  # ── meso: furrow / plate language (OBJECT meters) ──
                  furrow_m        = 0.055,
                  furrow_depth_m  = 0.009,
                  furrow_squash   = 0.24,    # y-squash -> furrows run UP the trunk
                  furrow_warp     = 0.25,    # domain warp: furrows wander (0.45 leaned diagonal)
                  furrow_contrast = 2.4,     # furrow vs ridge sharpness
                  plate_mix       = 0.0,     # 0 = furrows .. 1 = voronoi scale plates
                  plate_m         = 0.0125,
                  plate_depth_m   = 0.003,
                  plate_round     = 0.35,    # crack->interior transition width
                  plate_tint      = 0.22,    # per-plate value drift
                  # ── lenticels (smooth-bark horizontal dashes) ──
                  lenticel        = 0.0,
                  lenticel_x_m    = 0.025,   # dash length around the trunk
                  lenticel_y_m    = 0.003,   # dash thickness up the trunk
                  # ── micro: grain + roughness zoning ──
                  grain_m         = 0.0125,
                  grain           = 0.22,    # micro shade modulation
                  micro_depth_m   = 0.001,
                  micro_rough     = 0.12,
                  # ── colors (physical bark albedo: value ~0.15-0.35) ──
                  ridge_color     = hsv(27.0, 0.30, 0.35),
                  furrow_color    = hsv(19.0, 0.45, 0.11),
                  # ── response ──
                  rough_base      = 0.88,
                  rough_var       = 0.14,    # rougher down in the cavities
                  ao_floor        = 0.42,    # cavity AO depth
                  bump_gain       = 3.5,     # DIMENSIONLESS slope gain (see _object_bump_normal:
                                             # slope per object meter, res-independent; 1.0 = true
                                             # geometric slope, 3.5 ~ the r7 exaggerated relief)
                  # ── overlays ──
                  moss_amt        = 0.85,
                  moss_hi_m       = 0.40,    # moss fades out by this OBJECT height above base
                                             # (~17% of the 2.34 m prototype = ~12 m on a x30 tree)
                  moss_patch_m    = 0.15,
                  moss_color      = hsv(105.0, 0.55, 0.22),
                  north           = (0.0, 0.0, -1.0),   # world aspect the moss prefers
                  lichen_amt      = 0.6,
                  lichen_m        = 0.055,
                  lichen_cov      = 0.18,    # fraction of lichen cells occupied
                  lichen_color    = hsv(90.0, 0.10, 0.55),
                  # ── bake-time constants (fbm loop budgets) ──
                  meso_oct        = 4,
                  micro_oct       = 4,
                  aa              = 1.0):
  """Shared field builder: returns (albedo, rough, ao, world_normal). Every scalar
  above rides ctx.param (A8 — live plugs, re-bindable; the bake stamps them via
  _bound_params so baked == authored)."""
  ld    = ctx.param("ladder_scale",    float(LADDER_SCALE))
  fm    = ctx.param("furrow_m",        float(furrow_m))
  fd    = ctx.param("furrow_depth_m",  float(furrow_depth_m))
  fsq   = ctx.param("furrow_squash",   float(furrow_squash))
  fwp   = ctx.param("furrow_warp",     float(furrow_warp))
  fct   = ctx.param("furrow_contrast", float(furrow_contrast))
  pmix  = ctx.param("plate_mix",       float(plate_mix))
  plm   = ctx.param("plate_m",         float(plate_m))
  pld   = ctx.param("plate_depth_m",   float(plate_depth_m))
  plr   = ctx.param("plate_round",     float(plate_round))
  plt   = ctx.param("plate_tint",      float(plate_tint))
  len_a = ctx.param("lenticel",        float(lenticel))
  lenx  = ctx.param("lenticel_x_m",    float(lenticel_x_m))
  leny  = ctx.param("lenticel_y_m",    float(lenticel_y_m))
  grm   = ctx.param("grain_m",         float(grain_m))
  grn   = ctx.param("grain",           float(grain))
  mid   = ctx.param("micro_depth_m",   float(micro_depth_m))
  mrg   = ctx.param("micro_rough",     float(micro_rough))
  c_rg  = ctx.param("ridge_color",     ridge_color)
  c_fu  = ctx.param("furrow_color",    furrow_color)
  rgh0  = ctx.param("rough_base",      float(rough_base))
  rghv  = ctx.param("rough_var",       float(rough_var))
  aofl  = ctx.param("ao_floor",        float(ao_floor))
  bgn   = ctx.param("bump_gain",       float(bump_gain))
  mos_a = ctx.param("moss_amt",        float(moss_amt))
  mos_h = ctx.param("moss_hi_m",       float(moss_hi_m))
  mos_p = ctx.param("moss_patch_m",    float(moss_patch_m))
  c_mos = ctx.param("moss_color",      moss_color)
  nrth  = ctx.param("north",           tuple(north))
  lic_a = ctx.param("lichen_amt",      float(lichen_amt))
  lic_m = ctx.param("lichen_m",        float(lichen_m))
  lic_c = ctx.param("lichen_cov",      float(lichen_cov))
  c_lic = ctx.param("lichen_color",    lichen_color)

  # OBJECT-space feature coordinate — the bake's ONLY spatial input. No draw
  # scale anywhere in this material: instance transforms magnify bark with the
  # tree (the owner-ruled object-space contract).
  qf = ctx.P_object * (1.0 / ld)

  # ── meso: longitudinal furrows — POSITION-ONLY vertical squash (fork-seamless
  #    by construction; owner requirement "seamless with the branch segments").
  #    The FULL history, so nobody re-treads it:
  #    * tangent-frame axis (tbn bitangent): follows every tube, but a PER-
  #      SURFACE frame jumps where parent/child tubes interpenetrate — the field
  #      TEARS exactly at the crotch (owner-caught).
  #    * spatially-varying growth field (vertical->radial blend): a varying axis
  #      projected against the GLOBAL position degenerates (a radial field's
  #      position component along itself IS the whole vector) — the fbm domain
  #      shears into structureless mottle (measured, atlas r4096 dc19c430).
  #      Radial/cone constructions misbehave near the radiant point = the crotch.
  #    * position-only vertical squash (THIS): one continuous field everywhere,
  #      so both surfaces at a fork sample identical values — truly seamless. On
  #      38-deg sympodial branches the furrows run 38 deg off the member axis (a
  #      spiral-grain read); the 0.055 object spacing is on the order of the
  #      branch diameter, so the residual misalignment reads as grain slope,
  #      not ring-barking. TRUE per-member alignment needs generating data the
  #      fragment lacks: either collar geometry at forks (audit-flagged) or a
  #      sweep-emitted per-vertex axis/arc-length channel (engine seam, filed).──
  pf    = P.vec3(qf.x, qf.y * fsq, qf.z) * (1.0 / fm)
  pf    = domain_warp(pf, fwp, 1.0, 3)
  fr    = P.fbm_aa(pf, meso_oct, aa=aa)
  ridge = P.clamp((fr - 0.5) * fct + 0.5, 0.0, 1.0)          # 0 furrow .. 1 ridge

  # ── meso: voronoi scale plates ──
  vor   = P.voronoi(qf * (1.0 / plm))
  plate = P.smoothstep(0.0, plr, vor.fwedge)                 # 0 crack .. 1 interior
                                                             # (fwedge = width-corrected border dist)
  cellr = P.fract(P.sin(vor.cell * 127.1) * 43758.547)       # per-plate hash

  meso  = P.mix(ridge, plate, pmix)                          # 0 cavity .. 1 top
  h_m   = P.mix(ridge * fd, plate * pld, pmix)               # meso height, object m
  cav   = 1.0 - meso

  # ── lenticels: horizontal dashes (recessed, darkened) ──
  lf   = P.fbm_aa(P.vec3(qf.x * (1.0 / lenx), qf.y * (1.0 / leny), qf.z * (1.0 / lenx)), 3, aa=aa)
  lent = P.smoothstep(0.60, 0.72, lf) * len_a

  # ── micro: grain field -> relief + shade + roughness zoning ──
  g   = P.fbm_aa(qf * (1.0 / grm), micro_oct, aa=aa)
  h_m = h_m + (g - 0.5) * mid - lent * 0.001

  # ── base albedo ──
  # OWNER MUTE (restored aug11 — I re-enabled these four albedo overlay mixes
  # during the object-space re-author, misreading the mute as dead-cache-era
  # debris; at forest scale the lichen/moss blobs become ~1.6 m pale discs that
  # read as foreign chunks on the bark. The mute is DELIBERATE: overlays keep
  # their roughness/height contributions, their ALBEDO stays out until re-tuned
  # for object-space scale and re-ratified.)
  shade = P.saturate(meso + (g - 0.5) * grn)
  alb   = P.mix(c_fu, c_rg, shade)
  #alb   = alb * (1.0 + (cellr - 0.5) * plt * pmix)           # per-plate drift (plate species)
  #alb   = alb * (1.0 - 0.35 * lent)

  rough = P.saturate(rgh0 + cav * rghv + (g - 0.5) * mrg - lent * 0.05)
  ao    = P.mix(1.0, aofl, cav)

  # ── overlay: moss (lower trunk, cavity-seeking, north/up aspect). GATED, not
  #    multiplied: chaining soft masks starved the round-2 bake to 0.01% green —
  #    each gate saturates toward 1 inside its region so the interior of a moss
  #    patch reads as FULL moss, the gates only carve its extent. ──
  hgt   = ctx.P_object.y                                     # OBJECT height above the base
  band  = 1.0 - P.smoothstep(mos_h * 0.35, mos_h, hgt)
  nh    = P.normalize(P.vec3(ctx.N.x, 0.02, ctx.N.z))        # horizontal aspect dir
  asp   = P.saturate(P.dot(nh, P.normalize(nrth)))           # north-facing 0..1
  up    = P.saturate(ctx.N.y)                                # bough tops
  aspg  = P.smoothstep(0.25, 0.55, asp * 0.8 + up * 0.6)     # aspect gate
  pfbm  = P.fbm_aa(qf * (1.0 / mos_p), 3, aa=aa)
  patch = P.smoothstep(0.48, 0.56, pfbm + 0.10 * cav)        # patch gate (cavity-biased;
                                                             # ADDITIVE bias — a multiplicative
                                                             # 0.75x scale held the fbm under the
                                                             # gate: 0.11% green measured)
  moss  = P.saturate(mos_a * band * aspg * patch)
  #alb   = P.mix(alb, c_mos * (0.75 + 0.5 * g), moss)         # (owner mute — see above)
  rough = P.mix(rough, 0.95, moss)
  h_m   = h_m + moss * 0.0015

  # ── overlay: pale lichen plates (ridge tops, sparse voronoi cells) ──
  lv    = P.voronoi(qf * (1.0 / lic_m))
  lr    = P.fract(P.sin(lv.cell * 311.7) * 14371.323)
  blob  = (1.0 - P.smoothstep(0.30, 0.55, lv.f1)) * P.step(1.0 - lic_c, lr)
  lich  = P.saturate(lic_a * blob * (0.30 + 0.70 * meso))
  #alb   = P.mix(alb, c_lic * (0.85 + 0.3 * g), lich)         # (owner mute — see above)
  rough = P.mix(rough, 0.80, lich)
  h_m   = h_m + lich * 0.0005

  # ── height -> shading normal (object-normalized capture-space derivative —
  #    res/chart-density independent, see _object_bump_normal above).
  #    ladder_scale dilates amplitude WITH wavelength -> slopes preserved. ──
  n_w = _object_bump_normal(ctx, h_m * ld, strength=bgn)
  return alb, rough, ao, n_w


###############################################################################
# BarkAlpineLive — the same ladder evaluated PER-PIXEL at draw (owner mode,
# aug11: procedural bark, no bake in the loop). No section bake, no atlas, no
# cache key: every bark_alpine.py edit is on screen next run with nothing to
# re-key. This is the live-proctex cost profile the aug09 campaign removed
# SCENE-WIDE for perf — here it runs on trunk/branch gids only; measure before
# widening. As an ordinary surface material its impostor capture is the
# world-space one, so far billboards bake real bark (unlike the stored path,
# whose section-capture technique baked UV charts — the mirror-chunk bug).
###############################################################################
class BarkAlpineLive(Ptex3d):
  def __init__(self, ctx, *, instance_variation=0.0, **kw):
    alb, rough, ao, n_w = _bark_surface(self, ctx, **kw)
    # per-instance albedo variation, same convention as Solid: instanced
    # hypermeshes carry seed01 in ctx.Cd.y; v scales albedo (1-v .. 1+v).
    if instance_variation > 0.0:
      v = float(instance_variation)
      alb = alb * ((1.0 - v) + (2.0 * v) * ctx.Cd.y)
    self.surface(albedo=alb, roughness=rough, ao=ao, normal=n_w)


###############################################################################
# BarkAlpineStored — the CAPTURE material for the per-gid bake map
# (drawable_data(section_bake=True, materials={gid: this})). Standalone-valid:
# its surface_stored() reconstructs from its own captures.
###############################################################################
class BarkAlpineStored(Ptex3d):
  PTEX_MODE    = "stored"
  PTEX_CAPTURE = True
  ALBEDO = "SectionAlbedo"
  PARAMS = "SectionParams"
  NORMAL = "SectionNormal"

  def __init__(self, ctx, **kw):
    alb, rough, ao, n_w = _bark_surface(self, ctx, **kw)
    self.capture(self.ALBEDO, alb)                             # w unfilled -> coverage 1.0
    self.capture(self.PARAMS, P.vec3(rough, 0.0, ao))          # x=rough y=metal z=ao (own target)
    self.capture(self.NORMAL, section_normal(ctx, n_w))        # signed->UNORM tangent store
    # reconstruction (drawn only when this asset is used standalone; under the
    # bake map the sampler below is the drawn material)
    s_alb = ctx.texArray(self.ALBEDO, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz
    s_par = ctx.texArray(self.PARAMS, uv=ctx.uv, layer=ctx.layer, default=0.0)
    s_nts = ctx.texArray(self.NORMAL, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz * 2.0 - 1.0
    s_nw  = P.normalize(P.func("({0} * {1})", [ctx.tbn, s_nts], rtype="vec3"))
    self.surface_stored(albedo=s_alb, roughness=s_par.x, metallic=s_par.y, ao=s_par.z, normal=s_nw)


###############################################################################
# BarkSectionPBR — the drawn STORED SAMPLER (drawable_data material=). Samples
# the three baked arrays at ctx.layer; DECODES the tangent-space normal via the
# fragment tbn (this is what makes the baked furrows light — SectionArrayPBR's
# schema, plus the normal lane).
###############################################################################
class BarkSectionPBR(Ptex3d):
  ALBEDO = "SectionAlbedo"
  PARAMS = "SectionParams"
  NORMAL = "SectionNormal"
  PTEX_MODE    = "stored"
  PTEX_CAPTURE = True

  def __init__(self, ctx, *, roughness=0.85, **kw):
    # fallback capture = the target schema + never-black content for UNBOUND gids
    self.capture(self.ALBEDO, P.vec3(0.4, 0.35, 0.3))
    self.capture(self.PARAMS, P.vec3(float(roughness), 0.0, 1.0))
    self.capture(self.NORMAL, section_normal(ctx, ctx.N))
    alb = ctx.texArray(self.ALBEDO, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz
    par = ctx.texArray(self.PARAMS, uv=ctx.uv, layer=ctx.layer, default=0.0)
    nts = ctx.texArray(self.NORMAL, uv=ctx.uv, layer=ctx.layer, default=0.5).xyz * 2.0 - 1.0
    n_w = P.normalize(P.func("({0} * {1})", [ctx.tbn, nts], rtype="vec3"))
    self.surface_stored(albedo=alb, roughness=par.x, metallic=par.y, ao=par.z, normal=n_w)


__all__ = ["BarkAlpineStored", "BarkSectionPBR", "BARK_SPECIES",
           "LADDER_SCALE", "bark_content_key"]
