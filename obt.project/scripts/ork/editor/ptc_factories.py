################################################################################
# Particle System Factories for Scene Editor
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, Transform, dataflow
from orkengine.lev2 import particles, ParticlesDrawableData, Texture

tokens = CrcStringProxy()

################################################################################
# Elliptical Particle System (based on ptc_elliptical.py)
################################################################################

def createEllipticalSystem(scenegraph, layer, name):
  """Create an elliptical attractor particle system."""
  graphdata = dataflow.GraphData.createShared()

  # Instantiate modules
  pool = graphdata.create("POOL", particles.Pool)
  emitter = graphdata.create("EMIT", particles.EllipticalEmitter)
  glob = graphdata.create("GLOB", particles.Globals)
  turb = graphdata.create("TURB", particles.Turbulence)
  ellip = graphdata.create("ELIP", particles.EllipticalAttractor)
  grav = graphdata.create("GRAV", particles.Gravity)
  sprites = graphdata.create("SPRT", particles.SpriteRenderer)

  # Connect modules
  graphdata.connect(emitter.inputs.pool, pool.outputs.pool)
  graphdata.connect(turb.inputs.pool, emitter.outputs.pool)
  graphdata.connect(ellip.inputs.pool, turb.outputs.pool)
  graphdata.connect(grav.inputs.pool, ellip.outputs.pool)
  graphdata.connect(sprites.inputs.pool, grav.outputs.pool)

  # Configure
  pool.pool_size = 5000

  emitter.inputs.LifeSpan = 1
  emitter.inputs.EmissionRate = 250
  emitter.inputs.EmissionVelocity = 0.1
  emitter.inputs.MinU = 0
  emitter.inputs.MaxU = 1
  emitter.inputs.MinV = 0
  emitter.inputs.MaxV = 1

  grav.inputs.G = 0.01
  grav.inputs.Mass = 1
  grav.inputs.OthMass = 1
  grav.inputs.MinDistance = 1
  grav.inputs.Center = vec3(0, 0, 0)

  ellip.inputs.Inertia = 1/100.0
  ellip.inputs.P1 = vec3(0, 1, 0)
  ellip.inputs.P2 = vec3(0, -1, 0.1)
  ellip.inputs.Dampening = 0.999

  turb.inputs.Amount = vec3(1, 1, 1)

  # Material
  material = particles.GradientMaterial.createShared()
  material.blending = tokens.ADDITIVE
  material.depthtest = tokens.LEQUALS
  material.gradient.setColorStops({
    0.0: vec4(1, 1, 1, 1),
    1.0: vec4(1, 1, 1, 1)
  })

  sprites.material = material
  sprites.inputs.Size = 0.1

  # Create drawable and node
  drawable_data = ParticlesDrawableData()
  drawable_data.graphdata = graphdata
  drawable_data.emitterIntensity = 0.0  # disabled for now
  drawable_data.emitterRadius = 0.0

  ptc_drawable = drawable_data.createSGDrawable(scenegraph)
  sg_node = layer.createDrawableNode(name, ptc_drawable)
  sg_node.sortkey = 1

  xform = Transform()
  xform.translation = vec3(0, 2, 0)
  sg_node.worldTransform = xform

  # Store metadata
  sg_node.user.particle_preset = "elliptical"
  sg_node.user.graphdata = graphdata
  sg_node.user.drawable_data = drawable_data

  return sg_node

################################################################################
# Sprite Particle System (based on _ptc_harness.py)
################################################################################

def createSpriteSystem(scenegraph, layer, name):
  """Create a sprite-based particle system with nozzle/ring emitters."""
  graphdata = dataflow.GraphData.createShared()

  # Instantiate modules
  pool = graphdata.create("POOL", particles.Pool)
  emitn = graphdata.create("EMITN", particles.NozzleEmitter)
  emitr = graphdata.create("EMITR", particles.RingEmitter)
  glob = graphdata.create("GLOB", particles.Globals)
  grav = graphdata.create("GRAV", particles.Gravity)
  turb = graphdata.create("TURB", particles.Turbulence)
  vort = graphdata.create("VORT", particles.Vortex)
  sprites = graphdata.create("SPRI", particles.SpriteRenderer)

  # Connect modules
  graphdata.connect(emitn.inputs.pool, pool.outputs.pool)
  graphdata.connect(emitr.inputs.pool, emitn.outputs.pool)
  graphdata.connect(grav.inputs.pool, emitr.outputs.pool)
  graphdata.connect(turb.inputs.pool, grav.outputs.pool)
  graphdata.connect(vort.inputs.pool, turb.outputs.pool)
  graphdata.connect(sprites.inputs.pool, vort.outputs.pool)

  # Configure
  pool.pool_size = 4096

  emitn.inputs.LifeSpan = 5
  emitn.inputs.EmissionRate = 100
  emitn.inputs.EmissionVelocity = 2
  emitn.inputs.DispersionAngle = 30
  emitn.inputs.Offset = vec3(0, 0, 0)

  emitr.inputs.LifeSpan = 5
  emitr.inputs.EmissionRate = 100
  emitr.inputs.EmissionRadius = 1
  emitr.inputs.EmitterSpinRate = 1
  emitr.inputs.EmissionVelocity = 1
  emitr.inputs.DispersionAngle = 30
  emitr.inputs.Offset = vec3(0, 0, 0)

  grav.inputs.G = 1
  grav.inputs.Mass = 1
  grav.inputs.OthMass = 1
  grav.inputs.MinDistance = 1
  grav.inputs.Center = vec3(0, 0, 0)

  turb.inputs.Amount = vec3(2, 2, 2)

  vort.inputs.VortexStrength = 0.5
  vort.inputs.OutwardStrength = 0.5
  vort.inputs.Falloff = 1.0

  # Material
  material = particles.GradientMaterial.createShared()
  material.blending = tokens.OFF
  material.depthtest = tokens.LEQUALS
  material.gradient.setColorStops({
    0.0: vec4(1, 0.8, 0.2, 1),
    0.5: vec4(1, 0.3, 0.1, 1),
    1.0: vec4(0.2, 0.1, 0.1, 0)
  })

  sprites.material = material
  sprites.inputs.Size = 0.015
  sprites.inputs.GradientIntensity = 1

  # Create drawable and node
  drawable_data = ParticlesDrawableData()
  drawable_data.graphdata = graphdata
  drawable_data.emitterIntensity = 0.0  # disabled for now
  drawable_data.emitterRadius = 0.0

  ptc_drawable = drawable_data.createSGDrawable(scenegraph)
  sg_node = layer.createDrawableNode(name, ptc_drawable)
  sg_node.sortkey = 1

  xform = Transform()
  xform.translation = vec3(0, 2, 0)
  sg_node.worldTransform = xform

  # Store metadata
  sg_node.user.particle_preset = "sprite"
  sg_node.user.graphdata = graphdata
  sg_node.user.drawable_data = drawable_data

  return sg_node

################################################################################
# Streak Particle System
################################################################################

def createStreakSystem(scenegraph, layer, name):
  """Create a streak-based particle system."""
  graphdata = dataflow.GraphData.createShared()

  # Instantiate modules
  pool = graphdata.create("POOL", particles.Pool)
  emitn = graphdata.create("EMITN", particles.NozzleEmitter)
  glob = graphdata.create("GLOB", particles.Globals)
  grav = graphdata.create("GRAV", particles.Gravity)
  turb = graphdata.create("TURB", particles.Turbulence)
  streaks = graphdata.create("STRK", particles.StreakRenderer)

  # Connect modules
  graphdata.connect(emitn.inputs.pool, pool.outputs.pool)
  graphdata.connect(grav.inputs.pool, emitn.outputs.pool)
  graphdata.connect(turb.inputs.pool, grav.outputs.pool)
  graphdata.connect(streaks.inputs.pool, turb.outputs.pool)

  # Configure
  pool.pool_size = 4096

  emitn.inputs.LifeSpan = 3
  emitn.inputs.EmissionRate = 200
  emitn.inputs.EmissionVelocity = 3
  emitn.inputs.DispersionAngle = 45
  emitn.inputs.Offset = vec3(0, 0, 0)

  grav.inputs.G = 0.5
  grav.inputs.Mass = 1
  grav.inputs.OthMass = 1
  grav.inputs.MinDistance = 1
  grav.inputs.Center = vec3(0, -10, 0)

  turb.inputs.Amount = vec3(1, 1, 1)

  # Material
  material = particles.GradientMaterial.createShared()
  material.blending = tokens.OFF
  material.depthtest = tokens.LEQUALS
  material.gradient.setColorStops({
    0.0: vec4(0.5, 0.8, 1, 1),
    0.5: vec4(0.2, 0.4, 1, 1),
    1.0: vec4(0.1, 0.1, 0.5, 0)
  })

  streaks.material = material
  streaks.inputs.Length = 0.15
  streaks.inputs.Width = 0.02

  # Create drawable and node
  drawable_data = ParticlesDrawableData()
  drawable_data.graphdata = graphdata
  drawable_data.emitterIntensity = 0.0  # disabled for now
  drawable_data.emitterRadius = 0.0

  ptc_drawable = drawable_data.createSGDrawable(scenegraph)
  sg_node = layer.createDrawableNode(name, ptc_drawable)
  sg_node.sortkey = 1

  xform = Transform()
  xform.translation = vec3(0, 2, 0)
  sg_node.worldTransform = xform

  # Store metadata
  sg_node.user.particle_preset = "streak"
  sg_node.user.graphdata = graphdata
  sg_node.user.drawable_data = drawable_data

  return sg_node

################################################################################
# Preset Registry
################################################################################

PARTICLE_PRESETS = {
  "elliptical": createEllipticalSystem,
  "sprite": createSpriteSystem,
  "streak": createStreakSystem,
}
