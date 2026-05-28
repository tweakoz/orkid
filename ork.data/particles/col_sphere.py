################################################################################
# col_sphere.py — metaballs + PlaneCollider + SphereCollider demo.
#
# Clone of col_planar.py with an additional SphereCollider at world
# origin. Particles fall, hit the floor, AND get deflected by a solid
# obstacle sphere in the path of the gravity well. Useful for tuning
# SphereCollider's Center/Radius/Restitution/Friction against a busy
# flow of particles.
#
# Tuning knobs (additional over col_planar.py):
#   OBST.Center      — sphere world position (vec3); default at origin
#   OBST.Radius      — obstacle radius (float); start ~1
#   OBST.Restitution — 0=stick, 0.5=half-bounce, 1=elastic, >1 jumpy
#   OBST.Friction    — 0=slide, 1=full tangential stop
#
# Hosted by ork.particle.viewer.py:
#   ork.particle.viewer.py col_sphere
################################################################################

from orkengine.core import vec3, vec4, dataflow, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import particles, Texture, GfxEnv

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr as E

lev2_pyexdir.addToSysPath()
from lev2utils import shaders

tokens = CrcStringProxy()


class ColSphereSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    ctx = GfxEnv.ref.loadingContext()
    self.material = shaders.createPbrMaterialWithColor(
        ctx=ctx,
        color=vec4(0.35, 0.30, 0.15, 1.0),
        metallic=0.0,
        roughness=0.75)

    self.material.freestyle.rasterstate.depthtest = tokens.LESS

    self.ptc_pool = P.PoolData(size=10000, name="POOL")

    self.emitter = P.RingEmitter(self.ptc_pool, name="EMIT",
                                 LifeSpan=4.0,
                                 EmissionRate=200,
                                 EmissionVelocity=1.25,
                                 EmissionRadius=0.7,
                                 DispersionAngle=0.1,
                                 EmitterSpinRate=9.7,
                                 Direction=vec3(0, -1, 0),
                                 Offset=vec3(0, 4, 0))

    self.gravity = P.Gravity(self.emitter,
                             name="GRAV",
                             Center=vec3(0, -10, 0),
                             G=2.5,
                             Mass=0.1,
                             OthMass=0.1,
                             MinDistance=1.0)

    self.turb = P.Turbulence(self.gravity,
                             name="TURB",
                             Amount=vec3(0.8, 0.4, 0.8)*1.0)

    # Solid obstacle sphere sitting in the path between the emitter
    # ring above and the floor below. Particles flowing toward the
    # gravity well at origin slide / bounce off it. Defaults give a
    # half-bouncy / lightly-tacky stone; crank Restitution for ricochet.
    self.obstacle = P.SphereCollider(self.turb,
                                     name="OBST",
                                     Center=vec3(0, -3, 0),
                                     Radius=5.0,
                                     Restitution=0.1,
                                     Friction=0.0)


    self._size_curve = dataflow.floatxf.multicurve().multicurve
    self._size_curve.splitSegment(0)
    self._size_curve.splitSegment(0)
    self._size_curve.setPoint(0, 0.00, 0.0)
    self._size_curve.setPoint(1, 0.10, 1.0)
    self._size_curve.setPoint(2, 0.75, 1.0)
    self._size_curve.setPoint(3, 1.0, 0.0)

    self.blobs = P.VdbLevelSetRenderer(self.obstacle, name="BLOBS",
                                       material=self.material,
                                       voxel_size=0.100,
                                       kernel=tokens.WYVILL,
                                       Radius=1.0,
                                       Strength=1.0,
                                       IsoLevel=1.0,
                                       Size=E.curve(E.ptc.unit_age,self._size_curve))
    self.render(self.blobs)
