###############################################################################
# ren_pueblo_bake.py — ISOLATED pueblo section-bake scene (regression cover for
# the stored-mode black-face defect, hm.section.v3).
#
# ONE pueblo room-block (PuebloStepped or PuebloRow, sectioned) drawn ONCE with
# the SectionArrayPBR stored sampler; materials={gid: capture material} is the
# per-gid BAKE MAP (adobe wall/roof, timber door/trim) — the SAME wiring scn_swest
# uses for its stored buildings, stripped of terrain + scatter + Bullet so the
# section-bake path (unwrap -> per-gid atlas bake -> stored sample) is the only
# thing under test. The pueblo's many small gid0 charts on a gutter-heavy atlas
# EXPOSE the bake-vs-sample v-orientation mismatch that filled parapet caps /
# setback risers / base-flare faces BLACK before the fix.
#
#   PUEBLO_VARIANT=stepped|row  PUEBLO_MODE=stored|proc  SWEST_BAKE_RES=512
#   PB_NOSKY=1  -> disable the skybox DRAW (IBL kept) so the building isolates
#                  against black for a clean missing-albedo measurement
#   ork.scene.tojson.py -i ren_pueblo_bake -o /tmp/pb.ecs
#   ork.ecs.player.exe /tmp/pb.ecs --offscreen -S /tmp/pb.png -F 40 \
#       --camdist 20 --camheight 6
###############################################################################

import os

from orkengine.core import vec3, CrcStringProxy, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.bld_pueblo_row import PuebloRow
from ork.hypergraph.assets.hypermesh.bld_pueblo_stepped import PuebloStepped
from ork.hypergraph.assets.materials.adobe import Adobe, AdobeStored
from ork.hypergraph.assets.materials.timber import Timber, TimberStored
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArrayPBR

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()

VARIANT  = os.environ.get("PUEBLO_VARIANT", "stepped")
BAKE_RES = int(os.environ.get("SWEST_BAKE_RES", "512"))
MODE     = os.environ.get("PUEBLO_MODE", "stored")   # stored | proc
_CLS     = {"stepped": PuebloStepped, "row": PuebloRow}[VARIANT]


class PuebloBakeScene(Scene):

  def __init__(self):
    super().__init__()

    nosky  = os.environ.get("PB_NOSKY", "0") == "1"
    sky_kw = {"enable_skybox": False} if nosky else {}
    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.2,
        DiffuseIntensity  = 1.5,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        msaa              = 2,
        ssaa              = 0,
        **sky_kw)

    self.system_data("HypermeshSystem")

    # swest sun (owner-tuned angle/tint) so sunlit faces receive strong DIRECT light —
    # black-on-a-sunlit-face then isolates a COVERAGE hole from any shadow darkening.
    self.sun(elevation = 45.0,
             azimuth   = 285.0,
             color     = vec3(1.0, 0.82, 0.60),
             intensity = 1.5,
             shadow_map_size = 2048,
             shadow_caster = False)

    stored    = (MODE == "stored")
    AdobeCls  = AdobeStored  if stored else Adobe
    TimberCls = TimberStored if stored else Timber

    def _mat(name, cls, **kw):
      return self.asset.Ptex3d(name,
                               dsl_class     = cls,
                               vertex_source = GpuMeshRenderSource(),
                               **kw)

    wall = _mat("pb_adobe", AdobeCls, instance_variation=0.14)
    roof = _mat("pb_adobe_roof", AdobeCls,
                albedo_lo=vec3(0.42, 0.315, 0.20),
                albedo_hi=vec3(0.55, 0.43, 0.285),
                grain_m=0.18, instance_variation=0.12)
    door = _mat("pb_door", TimberCls,
                early_color=vec3(0.24, 0.17, 0.11),
                late_color=vec3(0.13, 0.095, 0.07),
                silver=0.12, roughness=0.90)
    trim = _mat("pb_timber", TimberCls, instance_variation=0.18)

    mesh = self.asset.Hypermesh(
        "bld_pueblo_" + VARIANT,
        dsl_class = _CLS,
        **({"sectioned": True} if stored else {}))

    if stored:
      sampler = _mat("pb_sampler", SectionArrayPBR, instance_variation=0.14)
      dd = mesh.drawable_data(
          material     = sampler,
          materials    = {0: wall, 2: door, 3: roof, 4: trim},
          section_bake = True,
          bake_res     = BAKE_RES)
    else:
      dd = mesh.drawable_data(
          material  = wall,
          materials = {2: door, 3: roof, 4: trim})

    self.entity(
        "pueblo0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = dd,
            layername = "std_forward",
            nodename  = "pueblo0")])


__all__ = ["PuebloBakeScene"]
