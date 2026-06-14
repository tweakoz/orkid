#!/usr/bin/env ork.python
###############################################################################
# mtl_showcase.py — 10×10 PBR material showcase.
#
# 100 distinct materials exercising the full glTF KHR-extension lobe stack
# plus PBR2 in-flight features. Each row is one rendering theme; each
# column within a row is a variation. Walk-around camera; spheres are
# 0.6m radius on a 1.6m XZ grid.
#
#   row 0 (back)  Metals             — metallic=1, varying albedo / roughness
#   row 1         Plastics / paint   — dielectric, varying gloss
#   row 2         Skin tones         — subsurface, varying tint
#   row 3         Gems / glass       — transmission + ior + volume
#   row 4         Cloth / sheen      — sheen lobe, fabric-like
#   row 5         Wood / stone       — natural materials, dielectric
#   row 6         Iridescence        — thin-film interference
#   row 7         Clearcoat          — car-paint family
#   row 8         Diffuse transmission — wax / milk / leaf / paper
#   row 9 (front) Showcase combos    — multi-lobe stacked materials
#
# All names — vec3 / vec4 / hsv / wavelength / colortemp / mix / colors /
# PbrMaterial / IcoSphere / Transform — are bare-name accessible inside
# Scene subclass methods via Scene.__init_subclass__'s wrapped __globals__;
# no helper imports needed.
#
# Run:
#   ork.scene.viewer.py mtl_showcase
###############################################################################

from orkengine.core import lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene

lev2_pyexdir.addToSysPath()


class ShowcaseScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset             = "ForwardPBR",
      skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
      SkyboxIntensity    = 1.0,
      DiffuseIntensity   = 1.0,
      SpecularIntensity  = 1.0,
      AmbientLight       = vec3(0.04))

    # Direct icosphere mesh — subdivisions=4 → 5120 triangles, 2562 vertices.
    # Per-vertex outward-pointing normals (analytic sphere), so no marching-
    # cubes / voxel-quantization bumpiness like the previous VDB pipeline.
    cols, rows = 10, 10
    dx, dz     = 1.6, 1.6
    x0         = -(cols - 1) * dx / 2.0
    z0         = -(rows - 1) * dz / 2.0

    def place(name, col, row, **mat_kwargs):
      mat = self.asset.PbrMaterial(f"mat_{name}", **mat_kwargs)
      drw = self.asset.IcoSphere(
        f"drw_{name}", radius=0.6, subdivisions=4, material=mat)
      self.entity(
        f"ent_{name}",
        transform={"translation": vec3(x0 + col * dx, 0, z0 + row * dz)},
        components=[SG.component(nodes={"n": {"drawable": drw}})])

    ##########################################################################
    # Row 0 — METALS. metallic=1, sorted by roughness across the full range
    # (0.05 → 0.90). Mirror-finish chrome on the far left through brushed,
    # hammered, sandblasted, and cast surfaces on the right. Roughness span
    # tests how the BRDF specular tail behaves from sharp mirror to nearly
    # Lambertian; the rough cells should look matte-but-still-metallic
    # (no diffuse term — only blurred env reflection).
    ##########################################################################
    metals = [
      ("chrome",          hsv(210, 0.02, 0.88), 0.05, 0.0),
      ("gold",            hsv( 45, 0.85, 1.00), 0.15, 0.0),
      ("gold_brushed",    hsv( 45, 0.80, 0.95), 0.70, 0.0),
      ("gold_antik",      hsv( 38, 0.75, 0.78), 0.80, 0.0),
      ("gold_antik_cc",   hsv( 38, 0.75, 0.78), 0.80, 1.0), 
      ("copper",          hsv( 20, 0.65, 0.85), 0.60, 0.0),
      ("brass",           hsv( 45, 0.55, 0.85), 0.60, 0.0),
      ("steel",           hsv(220, 0.25, 0.50), 0.70, 0.0),
      ("pewter",          hsv( 40, 0.05, 0.55), 0.70, 0.0),
      ("cast_iron",       hsv(220, 0.03, 0.60), 0.90, 0.0),
    ]
    for i, (name, alb, rough, cc_factor) in enumerate(metals):
      place(name, col=i, row=0,
            base_color=alb, metallic=1.0, roughness=rough, clearcoat_factor = cc_factor)

    ##########################################################################
    # Row 1 — PLASTICS / PAINT. Dielectric, varying roughness from glossy
    # (carlike) → matte (ABS). Pure base BRDF only.
    ##########################################################################
    plastics = [
      ("matte_black",     colors.black,         0.95),
      ("matte_charcoal",  colors.charcoal,      0.80),
      ("rubber_red",      colors.red * 0.55,    0.85),
      ("pastel_pink",     hsv(330, 0.30, 1.0),  0.40),
      ("vinyl_orange",    hsv( 20, 0.90, 1.0),  0.35),
      ("neon_green",      hsv(120, 0.90, 1.0),  0.20),
      ("gloss_blue",      colors.blue * 0.55,   0.15),
      ("gloss_red",       colors.red,           0.12),
      ("pearl_white",     colors.cream,         0.18),
      ("polycarb_clear",  colors.lightgray,     0.06),
    ]
    for i, (name, col, rough) in enumerate(plastics):
      place(name, col=i, row=1,
            base_color=col, metallic=0.0, roughness=rough)

    ##########################################################################
    # Row 2 — SKIN. Subsurface scattering on dielectric base. Six real skin
    # tones (light→dark) + four fantasy (alien, vampire, demon, undead).
    ##########################################################################
    skin_radius = vec3(0.50, 0.20, 0.10)
    skins = [
      ("skin_pale",     colors.skin1, hsv( 20, 0.45, 1.00), 0.85, 0.6),
      ("skin_warm",     colors.skin2, hsv( 15, 0.55, 1.00), 0.85, 0.7),
      ("skin_olive",    colors.skin3, hsv( 18, 0.55, 0.90), 0.80, 0.8),
      ("skin_tan",      colors.skin4, hsv( 12, 0.65, 0.75), 0.78, 0.9),
      ("skin_deep",     colors.skin5, hsv( 10, 0.65, 0.55), 0.75, 1.0),
      ("skin_dark",     colors.skin6, hsv(  8, 0.65, 0.35), 0.75, 1.0),
      ("alien_green",   hsv(110, 0.45, 0.55), hsv(120, 0.85, 0.55), 0.65, 0.9),
      ("vampire_pale",  hsv(  0, 0.05, 0.95), hsv(330, 0.60, 0.85), 0.72, 1.0),
      ("demon_red",     hsv(  0, 0.55, 0.55), hsv(  0, 0.95, 0.85), 0.72, 1.0),
      ("undead_gray",   hsv(110, 0.10, 0.55), hsv(110, 0.30, 0.65), 0.85, 1.0),
    ]
    for i, (name, base, sss_color, rough, factor) in enumerate(skins):
      place(name, col=i, row=2,
            base_color        = base,
            metallic          = 0.0,
            roughness         = rough,
            subsurface_color  = sss_color,
            subsurface_radius = skin_radius,
            subsurface_factor = factor)

    ##########################################################################
    # Row 3 — GEMS / GLASS. Transmission + ior + volume (Beer-Lambert).
    # The last three vary transmission_roughness for frosted-glass looks.
    ##########################################################################
    # Each of the five "white-ish" transmissive entries below is tuned for a
    # distinct visual signature so they don't all read as plain glass:
    #   diamond       — IOR 2.42 (highest), neutral, big refractive distortion
    #   clear_glass   — IOR 1.50 reference, perfectly neutral, no frost
    #   water         — vivid cyan body via saturated attenuation_color,
    #                    short atten_distance for strong Beer-Lambert tint
    #   ice           — light cyan body + medium frost (trans_rough 0.25)
    #   frosted_glass — neutral white, heavy frost (trans_rough 0.65) —
    #                    indistinct refraction, clearly "milky"
    gems = [
      # (name, base_color, transmission_factor, ior, attenuation_color, atten_dist, t_rough)
      ("ruby",          colors.ruby,          0.20, 1.77, colors.ruby,          0.5,  0.0 ),
      ("sapphire",      colors.sapphire,      0.20, 1.77, colors.sapphire,      0.5,  0.0 ),
      ("emerald",       hsv(140, 0.85, 0.45), 0.25, 1.58, hsv(140, 0.80, 0.50), 0.6,  0.0 ),
      ("diamond",       colors.white,         0.95, 2.42, colors.white,         3.0,  0.0 ),
      ("amber",         hsv( 38, 0.75, 0.65), 0.55, 1.55, hsv( 32, 0.75, 0.55), 1.0,  0.0 ),
      ("clear_glass",   colors.white,         1.00, 1.50, colors.white,         5.0,  0.0 ),
      ("water",         hsv(190, 0.25, 1.0 ), 0.90, 1.33, hsv(190, 0.85, 0.50), 0.6,  0.0 ),
      ("ice",           hsv(200, 0.18, 1.0 ), 0.75, 1.31, hsv(200, 0.55, 0.70), 0.8,  0.25),
      ("frosted_glass", colors.white,         0.85, 1.50, colors.white,         4.0,  0.65),
      ("jade_stone",    colors.jade,          0.15, 1.55, colors.jade,          0.8,  0.30),
    ]
    for i, (name, base, t_factor, ior_v, atten, atten_d, t_rough) in enumerate(gems):
      kwargs = dict(
        base_color              = base,
        metallic                = 0.0,
        roughness               = 0.10,
        transmission_factor     = t_factor,
        ior                     = ior_v,
        attenuation_color       = atten,
        attenuation_distance    = atten_d,
        # Required for Beer-Lambert to actually run — has_volume is gated
        # on kwargs.contains("volume_thickness_factor"). Without this set,
        # attenuation_color/distance are silently ignored.
        volume_thickness_factor = 1.0,
      )
      if t_rough > 0.0:
        kwargs["transmission_roughness"] = t_rough
      place(name, col=i, row=3, **kwargs)

    ##########################################################################
    # Row 4 — CLOTH / SHEEN. Charlie sheen lobe over rough dielectric base.
    # roughness 0.7-1.0 + sheen_factor + sheen_color produces fabric look.
    ##########################################################################
    cloth = [
      ("velvet_red",      colors.ruby * 0.5,     0.95, hsv(  0, 0.55, 1.0), 1.00),
      ("velvet_blue",     colors.sapphire * 0.5, 0.95, hsv(220, 0.55, 1.0), 1.00),
      ("velvet_green",    colors.jade * 0.6,     0.95, hsv(140, 0.55, 1.0), 0.95),
      ("velvet_purple",   colors.indigo * 0.65,  0.95, hsv(280, 0.55, 1.0), 0.95),
      ("suede_tan",       colors.tan,            0.85, colors.cream,        0.65),
      ("silk_gold",       colors.gold * 0.7,     0.80, hsv( 45, 0.50, 1.0), 0.75),
      ("satin_silver",    hsv( 50, 0.00, 0.85),  0.80, colors.white,        0.55),
      ("wool_gray",       colors.lightgray,      0.95, colors.white,        0.40),
      ("denim",           hsv(220, 0.55, 0.35),  0.85, hsv(220, 0.20, 0.85),0.50),
      ("polyester_white", colors.white * 0.92,   0.55, colors.cream,        0.35),
    ]
    for i, (name, base, rough, sheen_col, sheen_fac) in enumerate(cloth):
      place(name, col=i, row=4,
            base_color      = base,
            metallic        = 0.0,
            roughness       = rough,
            sheen_color     = sheen_col,
            sheen_factor    = sheen_fac,
            sheen_roughness = 0.30)

    ##########################################################################
    # Row 5 — WOOD / STONE. Natural dielectric materials. Roughness ranges
    # from polished marble to weathered slate.
    ##########################################################################
    natural = [
      ("oak_light",     colors.oak1,            0.75),
      ("oak_dark",      colors.oak2,            0.75),
      ("mahogany",      colors.mahogany,        0.75),
      ("birch",         colors.birch,           0.75),
      ("walnut",        colors.chocolate,       0.75),
      ("marble_white",  colors.marble,          0.70),
      ("granite",       colors.granite,         0.70),
      ("slate",         colors.slate,           0.65),
      ("sandstone",     hsv( 35, 0.45, 0.78),   0.85),
      ("limestone",     hsv( 45, 0.10, 0.82),   0.75),
    ]
    for i, (name, base, rough) in enumerate(natural):
      place(name, col=i, row=5,
            base_color = base, metallic = 0.0, roughness = rough)

    ##########################################################################
    # Row 6 — VARIETY + KHR_materials_specular. Four iridescent picks (down
    # from ten — the lobe doesn't need ten variations to show off), plus
    # showcase jade / marble / rubber / car-paint, plus the previously
    # missing specular F0 boost and tint demos.
    ##########################################################################

    # cols 0-1: KHR_materials_specular — F0 override (boost) + tint
    # Specular intensifies the dielectric reflection; specular_color tints it.
    place("spec_F0_boost", col=0, row=6,
      base_color      = colors.skyblue1,
      metallic        = 0.0,
      roughness       = 0.25,
      specular_factor = 1.0,
      specular_color  = colors.white)

    place("spec_warm_tint", col=1, row=6,
      base_color      = colors.cream,
      metallic        = 0.0,
      roughness       = 0.30,
      specular_factor = 1.0,
      specular_color  = colortemp(2400))         # warm-tinted dielectric reflection

    # col 2: soap_bubble — thin-film over air. Mostly transparent with very
    # subtle refraction (effective IOR ≈ 1.02 through a thin film), so
    # iridescence is the dominant visible signature.
    place("soap_bubble", col=2, row=6,
      base_color              = colors.white * 0.0,
      metallic                = 0.0,
      roughness               = 0.10,
      transmission_factor     = 0.90,
      ior                     = 1.01,
      attenuation_color       = colors.white,
      attenuation_distance    = 5.0,
      volume_thickness_factor = 1.0,
      iridescence_factor      = 0.2)

    # cols 3-5: remaining iridescent picks (opaque substrates)
    iridescent = [
      ("peacock",     hsv(200, 0.60, 0.45), 0.85, 0.2),
      ("oil_slick",   colors.charcoal,      0.30, 0.3),
      ("pearl",       colors.cream,         0.75, 0.4),
    ]
    for i, (name, base, rough, ir_factor) in enumerate(iridescent):
      place(name, col=3 + i, row=6,
            base_color         = base,
            metallic           = 0.0,
            roughness          = rough,
            iridescence_factor = ir_factor)

    # col 6: polished jade — subsurface + clearcoat showcase
    place("jade_polished", col=6, row=6,
      base_color          = colors.jade,
      metallic            = 0.0,
      roughness           = 0.70,
      subsurface_color    = mix(colors.jade, colors.leaf2, 0.3),
      subsurface_radius   = vec3(0.30, 0.55, 0.20),
      subsurface_factor   = 0.8,
      clearcoat_factor    = 0.3,
      clearcoat_roughness = 0.05)

    # col 7: polished marble — subtle subsurface + light clearcoat
    place("marble_polished", col=7, row=6,
      base_color          = colors.marble*0.85,
      metallic            = 0.0,
      roughness           = 0.85,
      subsurface_color    = colors.marble,
      subsurface_radius   = vec3(0.20, 0.20, 0.20)*.15,
      subsurface_factor   = 0.65,
      clearcoat_factor    = 0.6,
      clearcoat_roughness = 0.08)

    # col 8: matte black rubber — pure rough dielectric, no extras
    place("rubber_black", col=8, row=6,
      base_color = colors.charcoal*1.5,
      metallic   = 0.0,
      roughness  = 0.8)

    # col 9: ultra-shiny car paint — clearcoat over saturated ruby
    place("car_paint_red_glossy", col=9, row=6,
      base_color          = colors.ruby * 1.5,
      metallic            = 1.0,
      roughness           = 0.95,
      clearcoat_factor    = 1.5,
      clearcoat_roughness = 0.0,
      iridescence_factor=0.35)

    ##########################################################################
    # Row 7 — CLEARCOAT. Second specular lobe over a colored base. Car-paint
    # family — bright base + tight clearcoat = candy/glossy looks.
    ##########################################################################
    clearcoat = [
      ("car_red",        colors.red * 0.8,           0.45, 0.05),
      ("car_black",      colors.black,               0.55, 0.05),
      ("car_white",      colors.white * 0.92,        0.35, 0.05),
      ("car_blue",       colors.sapphire * 0.7,      0.45, 0.05),
      ("racing_yellow",  hsv( 50, 0.95, 1.0) * 0.85, 0.40, 0.08),
      ("candy_purple",   colors.purple * 0.75,       0.30, 0.05),
      ("matte_red_cc",   colors.red * 0.55,          0.95, 0.20),
      ("flake_silver",   colors.silver,              0.30, 0.10),
      ("anodized_gold",  colors.gold * 0.75,         0.25, 0.05),
      ("pearl_pink",     hsv(330, 0.30, 0.95),       0.30, 0.05),
    ]
    for i, (name, base, rough, cc_rough) in enumerate(clearcoat):
      place(name, col=i, row=7,
            base_color          = base,
            metallic            = 0.0,
            roughness           = rough,
            clearcoat_factor    = 1.0,
            clearcoat_roughness = cc_rough)

    ##########################################################################
    # Row 8 — DIFFUSE TRANSMISSION. Backlit translucent surfaces — wax / milk
    # / paper / thin-leaf. Combines with subsurface for milky-glass look.
    ##########################################################################
    translucent = [
      ("wax_white",      colors.cream,             0.65, colors.cream,           0.55),
      ("wax_warm",       hsv( 38, 0.40, 0.95),     0.65, hsv( 30, 0.55, 1.00),   0.55),
      ("milk",           colors.white * 0.95,      0.50, colors.cream,           0.40),
      ("rice_paper",     colors.cream * 0.95,      0.75, hsv( 40, 0.20, 1.00),   0.50),
      ("leaf_spring",    colors.leaf1 * 0.85,      0.55, hsv( 95, 0.85, 1.00),   0.60),
      ("leaf_autumn",    colors.autumn,            0.65, hsv( 25, 0.90, 1.00),   0.65),
      ("lampshade",      hsv( 38, 0.20, 0.95),     0.55, hsv( 35, 0.50, 1.00),   0.70),
      ("frosted_white",  colors.white * 0.90,      0.85, colors.white,           0.45),
      ("thin_marble",    colors.marble,            0.25, hsv( 50, 0.15, 1.00),   0.35),
      ("curtain",        hsv(  0, 0.10, 0.92),     0.75, hsv( 25, 0.30, 1.00),   0.55),
    ]
    for i, (name, base, rough, dt_color, dt_factor) in enumerate(translucent):
      place(name, col=i, row=8,
            base_color                  = base,
            metallic                    = 0.0,
            roughness                   = rough,
            diffuse_transmission_color  = dt_color,
            diffuse_transmission_factor = dt_factor)

    ##########################################################################
    # Row 9 — SHOWCASE COMBOS. Multi-lobe stacks — the recipes that really
    # exercise the lobe-composition path. Names hint at the lobe set.
    ##########################################################################

    # polished jade — subsurface + transmission + clearcoat
    place("polished_jade", col=0, row=9,
      base_color             = colors.jade,
      metallic               = 0.0,
      roughness              = 0.55,
      subsurface_color       = mix(colors.jade, colors.leaf2, 0.3),
      subsurface_radius      = vec3(0.20, 0.55, 0.10),
      subsurface_factor      = 0.8,
      transmission_factor    = 0.20,
      transmission_roughness = 0.20,
      ior                    = 1.55,
      attenuation_color      = colors.jade,
      attenuation_distance   = 0.8,
      clearcoat_factor       = 1.0,
      clearcoat_roughness    = 0.05)

    # oily chrome — metal + iridescence
    place("oily_chrome", col=1, row=9,
      base_color         = colors.silver,
      metallic           = 1.0,
      roughness          = 0.10,
      iridescence_factor = 1.0)

    # wet skin — subsurface + clearcoat (skin lit + glossy film)
    place("wet_skin", col=2, row=9,
      base_color          = colors.skin3,
      metallic            = 0.0,
      roughness           = 0.45,
      subsurface_color    = hsv(15, 0.55, 1.0),
      subsurface_radius   = vec3(0.40, 0.15, 0.08),
      subsurface_factor   = 0.9,
      clearcoat_factor    = 0.6,
      clearcoat_roughness = 0.10)

    # frosted ruby — transmission_roughness + subsurface red
    place("frosted_ruby", col=3, row=9,
      base_color             = colors.ruby,
      metallic               = 0.0,
      roughness              = 0.35,
      transmission_factor    = 0.45,
      transmission_roughness = 0.55,
      ior                    = 1.77,
      attenuation_color      = colors.ruby,
      attenuation_distance   = 0.4,
      subsurface_color       = colors.ruby,
      subsurface_radius      = vec3(0.40, 0.10, 0.05),
      subsurface_factor      = 0.4)

    # holo velvet — sheen + iridescence
    place("holo_velvet", col=4, row=9,
      base_color         = colors.indigo * 0.6,
      metallic           = 0.0,
      roughness          = 0.95,
      sheen_color        = colors.white,
      sheen_factor       = 0.9,
      sheen_roughness    = 0.30,
      iridescence_factor = 0.85)

    # pearlescent paint — clearcoat + iridescence
    place("pearl_paint", col=5, row=9,
      base_color          = colors.cream,
      metallic            = 0.0,
      roughness           = 0.30,
      clearcoat_factor    = 1.0,
      clearcoat_roughness = 0.05,
      iridescence_factor  = 0.75)

    # glowing crystal — high-value base + transmission (no real emissive lobe yet)
    place("glowing_crystal", col=6, row=9,
      base_color           = wavelength(540) * 1.5,   # super-bright green
      metallic             = 0.0,
      roughness            = 0.05,
      transmission_factor  = 0.85,
      ior                  = 1.55,
      attenuation_color    = colors.leaf1,
      attenuation_distance = 1.5)

    # volcanic glass — dark transmission + heavy attenuation
    place("obsidian", col=7, row=9,
      base_color           = colors.black,
      metallic             = 0.0,
      roughness            = 0.05,
      transmission_factor  = 0.75,
      ior                  = 1.49,
      attenuation_color    = colors.charcoal,
      attenuation_distance = 0.3,
      clearcoat_factor     = 1.0,
      clearcoat_roughness  = 0.02)

    # candy apple — clearcoat over deep red + iridescence flake hint
    place("candy_apple", col=8, row=9,
      base_color          = colors.ruby * 0.7,
      metallic            = 0.0,
      roughness           = 0.30,
      clearcoat_factor    = 1.0,
      clearcoat_roughness = 0.03,
      iridescence_factor  = 0.20)

    # frosted candle wax — diffuse transmission + subsurface warm + sheen
    place("frosted_wax", col=9, row=9,
      base_color                  = colors.cream,
      metallic                    = 0.0,
      roughness                   = 0.65,
      diffuse_transmission_color  = colortemp(2400),
      diffuse_transmission_factor = 0.55,
      subsurface_color            = colortemp(3200),
      subsurface_radius           = vec3(0.50, 0.30, 0.18),
      subsurface_factor           = 0.7,
      sheen_color                 = colors.cream,
      sheen_factor                = 0.25,
      sheen_roughness             = 0.40)
