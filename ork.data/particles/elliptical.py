################################################################################
# elliptical.py — HyperSyn particles DSL: elliptical attractor + streaks.
#
# Equivalent to renderer/particles/ptc_elliptical3.py (the original imperative
# form) but authored as a pure DSL class. Host with:
#
#   ork.particles.player.py ork.lev2/pyext/tests/hypersyn/particles/elliptical.py
#
# This file contains zero engine boilerplate — no app, no scene, no camera, no
# drawable plumbing. Just the ParticleSystem subclass. The player provides
# everything else.
#
# All time-varying inputs are bound declaratively via Expr (see __init__'s
# trailing block). No onUpdate body — the per-frame math runs as a floatxf
# chain in C++ (sourced from Globals.RelTime, lazy-added by the lowerer).
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

tokens = CrcStringProxy()


class EllipticalSystem(ParticleSystem):
  """Elliptical attractor with vortex+turbulence, streak-rendered through a
  gradient material. Time-driven emission window + turbulence amplitude — all
  bound declaratively, no per-frame Python."""

  def __init__(self):
    super().__init__()

    # ---- graph: pool → emitter → turb → vortex → ellipse → gravity → streaks ----

    self.ptc_pool = P.PoolData(size=50000, name="POOL")

    # Static kwargs for the emitter; MinV/MaxV are bound time-driven below.
    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=1.0,
                                       DispersionAngle=180,
                                       LifeSpan=2.0,
                                       Scalar=3,
                                       EmissionRate=10000,
                                       MinU=0,
                                       MaxU=1,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # turbulence.Amount is time-driven (bound below); no kwarg here.
    self.turbulence = P.Turbulence(self.emitter, name="TURB")

    self.vortex = P.Vortex(self.turbulence, name="VORT",
                           VortexStrength=-5,
                           OutwardStrength=-1,
                           Falloff=0.001)

    self.elliptical = P.EllipticalAttractor(self.vortex, name="SPHR",
                                            Scalar=1,
                                            Power=0.01,
                                            Inertia=111,
                                            Dampening=0.999,
                                            P1=vec3(0, 0, 0),
                                            P2=vec3(0, 0, 0))

    self.gravity = P.Gravity(self.elliptical, name="GRAV",
                             Center=vec3(0, 1, 0),
                             G=0,
                             Mass=1,
                             OthMass=1,
                             MinDistance=10)

    # ---- material (look) ----

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1
    self.material.gradient.setColorStops({
      0.0: vec4(1, 1, 1, 1),
      0.4: vec4(1, 0, 1, 1),
      0.5: vec4(.2, .4, 1, 1),
      1.0: vec4(0, 0, 0, 1),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    # ---- renderer (chain terminus + render() sink for the player) ----

    self.streaks = P.StreakRenderer(self.gravity, name="STRK",
                                    material=self.material,
                                    Length=0.15,
                                    Width=0.015)
    self.render(self.streaks)

    ############################################################################
    # Time-driven inputs — bound declaratively. Equivalent to the original
    # imperative onUpdate body:
    #
    #   T = updinfo.absolutetime * 0.25
    #   emitter.MinV    = 0.6 + sin(T * 4) * 0.2     # period 2π
    #   emitter.MaxV    = 0.4 - sin(T * 4) * 0.2     # period 2π
    #   turbulence.Amount = vec3(sin(T) * 20)        # period 8π
    #
    # The lowerer compiles these into a single Globals.RelTime source +
    # per-plug floatxf chains (and a Vec3Combine module for Amount). The
    # per-frame math runs entirely in C++; no Python tick.
    ############################################################################

    e = Expr

    self.emitter.MinV      = 0.6 + e.sin(e.time) * 0.2
    self.emitter.MaxV      = 0.4 - e.sin(e.time) * 0.2
    self.turbulence.Amount = e.vec3(e.sin(e.time * 0.25) * 20)
