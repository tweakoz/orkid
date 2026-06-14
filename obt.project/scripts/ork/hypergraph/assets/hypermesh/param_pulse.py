###############################################################################
# ParamPulse — E.6/2.12 MaterialParamSink demo: a graph node drives a material
# UBO param BY NAME. The chain is a STATIC icosphere + material_param(); the
# asset's onUpdate pokes ONLY the sink's float plug (no geometry recompute) and
# the drain applies it to the material per-frame via the live-rebind contract
# (GfxMaterial::bindParam -> stamp -> every cached pipeline re-overlays).
# Expected: the sphere's emissive PULSES warm orange ~0.5 Hz while the mesh
# itself never re-evaluates.
#
#   ./ork.hypermesh.viewer.py param_pulse
###############################################################################
import math
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.ptex3d import Ptex3d


class PulseMat(Ptex3d):
  """`glow_gain` is a runtime ctx.param (a ublk_ptex_params member, rebindable
  with zero recompile) — the asset's MaterialParamSink drives it per-frame."""
  def __init__(self, ctx, *, roughness=0.35, albedo=None, metallic=None):
    glow = ctx.param("glow_gain", 0.0)
    self.surface(albedo=vec3(0.16, 0.17, 0.20) if albedo is None else albedo,
                 metallic=0.0 if metallic is None else metallic,
                 roughness=roughness,
                 emissive=vec3(1.0, 0.42, 0.10) * glow * 6.0)


class ParamPulse(Hypermesh):
  MATERIAL_CLASS = PulseMat

  def __init__(self):
    super().__init__()
    n = self.icosphere(radius=1.6, subdivisions=3)
    self._sink = self.material_param(n, name="glow_gain", value=0.0)
    self.output(n)

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    self._sink.inputs.value = 0.5 + 0.5 * math.sin(t * 3.0)
