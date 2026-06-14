################################################################################
# col_vdb.py — metaballs + VdbCollider funnel demo.
#
# Builds a CLOSED triangle mesh of a hollow funnel (truncated cone with
# annular caps), converts it to an OpenVDB signed-distance field via
# `lev2.vdb.meshToLevelSet`, and feeds that grid into a VdbCollider.
# Particles emit above the funnel mouth and fall through under gravity;
# the SDF collider pushes them back whenever they try to penetrate the
# wall material → they slide down the inner conical surface and exit
# through the spout hole at the bottom.
#
# The funnel mesh is closed (top annular cap + outer cone + bottom
# annular cap + inner cone, all stitched). meshToLevelSet requires a
# closed surface — open meshes produce garbage inside/outside.
#
# Tuning:
#   _funnel_top_outer / top_inner : top mouth radii
#   _funnel_bot_outer / bot_inner : bottom spout radii (bot_inner is the
#                                   diameter of the hole particles fall
#                                   through)
#   _funnel_top_y / bot_y         : funnel vertical extent
#   _funnel_voxel                 : SDF grid voxel size (smaller = sharper
#                                   collision but more memory)
#   VCOL.Restitution / Friction   : same as Plane/SphereCollider
#
# Hosted by ork.particle.viewer.py:
#   ork.particle.viewer.py col_vdb
#   ork.particle.viewer.py col_vdb -p base_radius=1.5    # blob radius scale
#   ork.particle.viewer.py col_vdb -p pool_size=2000     # smaller pool
################################################################################

import math

from orkengine.core import vec3, vec4, dataflow, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import particles, Texture, GfxEnv, vdb

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr as E

lev2_pyexdir.addToSysPath()
from lev2utils import shaders

tokens = CrcStringProxy()


class ColVdbSystem(ParticleSystem):

  # PBR2 Phase 2 — lobe kwargs accepted via **lobe_kwargs and forwarded
  # to the PBRMaterial via snake_case setters. Same kwarg names as
  # self.asset.PbrMaterial. Lets col_vdb particles be glass / amber /
  # iridescent without bespoke shader work.
  _LOBE_KWARGS = (
    "has_transmission", "transmission_factor",
    "has_ior", "ior",
    "has_volume", "volume_thickness_factor",
    "has_diffuse_transmission", "diffuse_transmission_factor",
    "has_specular", "specular_factor",
    "has_clearcoat", "clearcoat_factor",
    "has_sheen", "sheen_factor",
    "has_iridescence", "iridescence_factor",
    "sheen_color", "specular_color",
    "attenuation_color", "diffuse_transmission_color",
    "clearcoat_roughness", "sheen_roughness", "attenuation_distance",
  )

  def __init__(self, *,
               collision_sdf=None,
               base_radius=1.0,
               pool_size=10000,
               color=vec4(1,1,1,1),
               metallic=0.0,
               roughness=0.0,
               follow_entity="",
               emitter_entity="",
               **lobe_kwargs):
    """Parameterizable Tier-2 particle system.

    Kwargs:
      collision_sdf  — pre-built openvdb FloatGrid; if None, the funnel is
                    built eagerly here. Tier 3 Scene composite supplies
                    a shared SDF asset (M2); Tier 1/2 standalone use
                    builds it on construction.
      base_radius — scalar applied to the renderer's Radius plug. Lets
                    `ork.particle.viewer.py col_vdb -p base_radius=1.5`
                    visibly scale the metaball footprint.
      pool_size   — particle pool capacity. Larger = denser blobs but
                    higher per-frame splat cost.
      follow_entity — name of an ECS-published entity whose live world
                    transform the collider should track (e.g. "saddle0"
                    when the saddle entity is the first published under
                    publish_xf="saddle"). Empty (default) → world-space
                    sampling, matching Tier 1/2 standalone behavior.
      emitter_entity — name of an ECS-published entity whose live
                    transform shifts the emit position. When non-empty,
                    the emitter Offset becomes (default_offset +
                    Expr.entity(emitter_entity).pos) so moving the
                    entity in ecsedit moves the spawn point. Empty
                    (default) → static world-space offset.
    """
    super().__init__()

    ctx = GfxEnv.ref.loadingContext()
    self.material = shaders.createPbrMaterialWithColor(
        ctx=ctx,
        color=color,
        metallic=metallic,
        roughness=roughness)

    # SERIALIZABLE material recipe — the live self.material above is a runtime GPU
    # object and silently drops on a graph round-trip (invisible blobs); this
    # reflected PbrMaterialGenData carries the SAME fields (the kwargs ctor applies
    # the same implicit has_<lobe> enables as the loop below) and re-materializes
    # at first render post-deserialize. Both are handed to the renderer.
    from orkengine.lev2 import PbrMaterialGenData
    self.material_gen = PbrMaterialGenData(
        base_color=color, metallic=metallic, roughness=roughness, **lobe_kwargs)

    # PBR2 Phase 2 — forward any lobe kwargs onto the live PBRMaterial.
    # Implicit-enable matches PbrMaterial wrapper: passing a factor or
    # color for a lobe sets its has_<lobe> flag true unless explicitly
    # overridden in lobe_kwargs.
    _implicit_triggers = {
      "has_transmission":          ("transmission_factor",),
      "has_ior":                   ("ior",),
      "has_volume":                ("volume_thickness_factor",
                                    "attenuation_color",
                                    "attenuation_distance"),
      "has_diffuse_transmission":  ("diffuse_transmission_factor",
                                    "diffuse_transmission_color"),
      "has_specular":              ("specular_factor", "specular_color"),
      "has_clearcoat":             ("clearcoat_factor",),
      "has_sheen":                 ("sheen_factor", "sheen_color"),
      "has_iridescence":           ("iridescence_factor",),
    }
    for k, v in lobe_kwargs.items():
      if k not in self._LOBE_KWARGS:
        raise TypeError(f"ColVdbSystem: unknown kwarg {k!r}")
      setattr(self.material, k, v)
    for has_flag, triggers in _implicit_triggers.items():
      if has_flag in lobe_kwargs:
        continue
      if any(t in lobe_kwargs for t in triggers):
        setattr(self.material, has_flag, True)

    self.material.freestyle.rasterstate.depthtest = tokens.LESS

    self._collision_sdf = collision_sdf

    self.ptc_pool = P.PoolData(size=pool_size, name="POOL")

    # Emit offset / direction:
    #   - Standalone (no emitter_entity): static local-frame vec3s.
    #   - ECS host w/ emitter_entity: apply the host entity's full SRT
    #     to the local emit offset (transformPoint) and rotation+scale
    #     to the down-vector (transformDir). Moving the host translates
    #     the spawn point; rotating it tips the emit direction; scaling
    #     stretches the ring radius proportionally with the host scale.
    base_radius_ring = 2.5
    base_velocity    = 0.5
    if emitter_entity:
      # Full host SRT mapped into the emitter:
      #   transformPoint(local) → world position (translation + rotation
      #     + scale all applied; local=(0,0,0) collapses to entity pos).
      #   transformDir(local)   → world direction (rotation + scale; no
      #     translation). Re-normalized inside computePosDir, so scale
      #     doesn't affect the down-vector by itself.
      #   scaleU (scalar)       → uniform scale; multiplied into the ring
      #     radius and emission velocity so a 2× host stretches the ring
      #     2× and spawns particles 2× faster outward.
      emit_offset  = E.entity(emitter_entity).transformPoint(vec3(0, 0, 0))
      emit_dir     = E.entity(emitter_entity).transformDir(vec3(0, -1, 0))
      emit_tangent = E.entity(emitter_entity).transformDir(vec3(1,  0, 0))
      emit_radius  = base_radius_ring# * E.entity(emitter_entity).scaleU
      emit_vel     = base_velocity    #* E.entity(emitter_entity).scaleU
    else:
      emit_offset  = vec3(0, 5, 0)
      emit_dir     = vec3(0, -1, 0)
      emit_tangent = vec3(1,  0, 0)
      emit_radius  = base_radius_ring
      emit_vel     = base_velocity

    self.emitter = P.RingEmitter(self.ptc_pool, name="EMIT",
                                 LifeSpan=3.0,
                                 EmissionRate=300,
                                 EmissionVelocity=emit_vel,
                                 EmissionRadius=emit_radius,
                                 DispersionAngle=0.0,
                                 EmitterSpinRate=9.0,
                                 Direction=emit_dir,
                                 Tangent=emit_tangent,
                                 Offset=emit_offset)

    # Uniform downward force — same effect as the previous point-gravity
    # at (0,-20,0) with G=3 / Mass=0.1 / OthMass=0.1, but position-
    # independent (every particle gets the same Δv per tick). Magnitude
    # tuned to match the look of the previous point-gravity setup.
    self.gravity = P.DirectionalForce(self.emitter,
                                      name="GRAV",
                                      Direction=vec3(0, -1, 0),
                                      Magnitude=5.0)

    self.turb = P.Turbulence(self.gravity,
                             name="TURB",
                             Amount=vec3(0.4, 0.2, 0.4)*8.0)

    # The funnel itself — VdbCollider sampling the SDF built above.
    # Particles entering the funnel mouth slide down the inner cone and
    # exit through the spout hole at the bottom.
    self.funnel = P.VdbCollider(self.turb,
                                name="VCOL",
                                sdf_grid=collision_sdf,
                                follow_entity=follow_entity,
                                Restitution=0.0,
                                Friction=0.01)


    self._size_curve = dataflow.floatxf.multicurve().multicurve
    self._size_curve.splitSegment(0)
    self._size_curve.splitSegment(0)
    self._size_curve.setPoint(0, 0.00, 0.0)
    self._size_curve.setPoint(1, 0.10, 0.5)
    self._size_curve.setPoint(2, 0.75, 0.74)
    self._size_curve.setPoint(3, 1.00, 0.0)

    self.blobs = P.VdbLevelSetRenderer(self.funnel, name="BLOBS",
                                       material=self.material,
                                       material_gen=self.material_gen,
                                       voxel_size=0.1,
                                       kernel=tokens.WYVILL,
                                       Radius=base_radius,
                                       Strength=1.0,
                                       IsoLevel=0.5,
                                       Size=E.curve(E.ptc.unit_age,self._size_curve))
    self.render(self.blobs)
