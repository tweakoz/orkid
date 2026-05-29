#!/usr/bin/env ork.python
###############################################################################
# mtl_colors.py — author-helper color acid test.
#
# 6×6 grid of PBR spheres. Each row demonstrates one variation of the
# color authoring vocabulary at ork.hypergraph.colors:
#
#   row 0:  hsv() hue sweep        (6 hues, full saturation/value)
#   row 1:  hsv() value sweep      (hue 30°, value 0.15 → 1.0)
#   row 2:  wavelength()            (visible spectrum, 400 → 700 nm)
#   row 3:  colortemp()             (black-body 1500K → 10000K)
#   row 4:  palette samples         (named colors.* picks)
#   row 5:  arithmetic + mix()      (scalar dim, brighten, modulate,
#                                    50/50 mix, weighted mix, invert)
#
# Camera looks down -Y at a ground plane of spheres in XZ. Row 0 is at
# -Z (back), row 5 at +Z (front). Column 0 is at -X, column 5 at +X.
#
# Bare names (vec3 / hsv / wavelength / colortemp / mix / colors /
# PbrMaterial / SphereSdf / VdbGridToDrawable / Transform) resolve via
# Scene.__init_subclass__'s wrapped __globals__ — no imports needed.
#
# Run:
#   ork.scene.viewer.py mtl_colors
###############################################################################

from orkengine.core import lev2_pyexdir
from ork.hypergraph.ecs.scene import Scene

lev2_pyexdir.addToSysPath()


class ColorsScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset             = "ForwardPBR",
      skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
      SkyboxIntensity    = 1.0,
      DiffuseIntensity   = 1.0,
      SpecularIntensity  = 1.0,
      AmbientLight       = vec3(0.05))

    sphere_sdf = self.asset.SphereSdf("sphere_sdf", radius=0.8)

    cols, rows = 6, 6
    dx, dz     = 2.2, 2.2
    x0         = -(cols - 1) * dx / 2.0
    z0         = -(rows - 1) * dz / 2.0

    def place(name, color, col, row):
      mat = self.asset.PbrMaterial(
        f"mat_{name}",
        base_color = color,
        metallic   = 0.0,
        roughness  = 0.35)
      drw = self.asset.VdbGridToDrawable(
        f"drw_{name}",
        grid=sphere_sdf, material=mat, iso=0.0)
      self.entity(
        f"ent_{name}",
        transform={"translation": vec3(x0 + col * dx, 0, z0 + row * dz)},
        components=[SG.component(nodes={"n": {"drawable": drw}})])

    ##########################################################################
    # Row 0 — hsv() hue sweep. Same saturation/value, hue marching 0 → 300°
    # so we get red, yellow, green, cyan, blue, magenta.
    ##########################################################################
    for i in range(cols):
      place(f"hsv_hue_{i*60}", hsv(i * 60.0, 1.0, 1.0), col=i, row=0)

    ##########################################################################
    # Row 1 — hsv() value sweep. Fixed warm hue (30° / orange) and high
    # saturation, value ramps 0.15 → 1.0 (so we see brightness shading at
    # one stable color identity).
    ##########################################################################
    for i in range(cols):
      v = 0.15 + (i / (cols - 1)) * 0.85
      place(f"hsv_val_{int(v*100)}", hsv(30, 0.8, v), col=i, row=1)

    ##########################################################################
    # Row 2 — wavelength(). Visible spectrum 400 nm (violet) to 700 nm (red).
    # Dan Bruton's piecewise approximation + edge intensity falloff.
    ##########################################################################
    for i in range(cols):
      nm = 400 + i * 60      # 400 460 520 580 640 700
      place(f"wl_{nm}nm", wavelength(nm), col=i, row=2)

    ##########################################################################
    # Row 3 — colortemp(). Six black-body temperatures spanning candle (warm
    # amber) through D65 (near-white) to overcast (cool blue).
    ##########################################################################
    for i, k in enumerate([1500, 2400, 3200, 5500, 6500, 10000]):
      place(f"ct_{k}K", colortemp(k), col=i, row=3)

    ##########################################################################
    # Row 4 — named palette. Six picks spanning the categorical palette so
    # named-color authoring is exercised end-to-end (gendata → material).
    ##########################################################################
    palette_picks = [
      colors.ruby, colors.gold, colors.jade,
      colors.sapphire, colors.oak1, colors.skyblue2,
    ]
    for i, c in enumerate(palette_picks):
      place(f"pal_{i}", c, col=i, row=4)

    ##########################################################################
    # Row 5 — arithmetic + mix(). Each ball exercises a different operator
    # path so we visually confirm color algebra produces the right RGB:
    #   col 0:  jade dimmed                 colors.jade * 0.5
    #   col 1:  skin brightened             colors.skin2 + 0.1
    #   col 2:  modulate (vec4 product)     wavelength(550) * colortemp(2400)
    #   col 3:  alpha-correct 50/50 mix     mix(colors.ruby, colors.sapphire)
    #   col 4:  weighted mix                mix(colors.jade, colors.gold, 0.3)
    #   col 5:  channel invert              1 - colors.skyblue2
    ##########################################################################
    arithmetic = [
      colors.jade * 0.5,
      colors.skin2 + 0.1,
      wavelength(550) * colortemp(2400),
      mix(colors.ruby, colors.sapphire),
      mix(colors.jade, colors.gold, 0.3),
      1 - colors.skyblue2,
    ]
    for i, c in enumerate(arithmetic):
      place(f"arith_{i}", c, col=i, row=5)
