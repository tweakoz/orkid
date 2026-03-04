################################################################################
# FPS Game Logic
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, random
from orkengine.core import vec3, vec4, quat, Transform, CrcStringProxy
from orkengine import lev2, ecs
from orkengine.lev2 import RigidPrimitive, RigidPrimitiveDrawableData, PBRMaterial, Image
from ork import path as ork_path
import trimesh
from lev2utils.submeshes import trimeshToSubmesh

tokens = CrcStringProxy()

################################################################################
# Constants
################################################################################

GROUP_PLAYER = 1
GROUP_BALL = 2
GROUP_ENV = 4
GROUP_ALL = GROUP_PLAYER | GROUP_BALL | GROUP_ENV
NUM_BALLS = 1000
SIMRATE = 60
BALLS_NODE_NAME = "balls-instancing-node"
OBJ_PATH = ork_path.data/"tests"/"environ"/"envtest5.obj"
WALK_FORCE = 5e2
SCALE = vec3(5, -2, 5)
OFFSET = vec3(0, 0, 0)
FWD_LAYERS = ["depth_prepass", "std_forward"]

################################################################################

class FpsGame:

  def __init__(self, runtime, envmap="arena"):
    self.runtime = runtime
    self.envmap = envmap
    self.player_transform = None
    self.spawncounter = 0
    self.playerforce = None
    self.playerforce_rot = 0.0
    self.player_physics_componentdata = None
    self.ball_state = {}
    self.sys_phys = None
    self.sys_sg = None
    self.room_SGCOMP = None
    self.room_material = None

  ##############################################################################
  # Scene Population
  ##############################################################################

  def populate_scene(self):
    """Add FPS systems, archetypes, and spawners to runtime.scene_data."""
    sd = self.runtime.scene_data

    # SceneGraph system
    systemdata_SG = sd.declareSystem("SceneGraphSystem")
    systemdata_SG.declareLayer("std_forward")
    systemdata_SG.declareLayer("depth_prepass")
    systemdata_SG.declareParams({
      "SkyboxTexPathStr": self.envmap,
      "SkyboxIntensity": float(1),
      "SpecularIntensity": float(0.5),
      "DiffuseIntensity": float(2),
      "AmbientLight": vec3(0),
      "DepthFogDistance": float(10000),
      "preset": "ForwardPBR",
    })

    # Bullet physics system
    systemdata_phys = sd.declareSystem("BulletSystem")
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = SIMRATE
    systemdata_phys.debug = False
    systemdata_phys.linGravity = vec3(0, -9.8 * 3, 0)

    # Ball instancing drawable
    drawable = lev2.InstancedModelDrawableData("data://tests/pbr_calib.glb")
    drawable.resize(NUM_BALLS)
    systemdata_SG.declareNodeOnLayer(
      name=BALLS_NODE_NAME,
      drawable=drawable,
      layers=FWD_LAYERS)

    # Archetypes
    self._createBallData(sd)
    self._createEnvironmentData(sd)
    self._createPlayerData(sd)
    self._createProjectileData(sd)

  ##############################################################################

  def _createPlayerData(self, sd):
    arch_player = sd.declareArchetype("PlayerArchetype")
    c_scenegraph = arch_player.declareComponent("SceneGraphComponent")
    c_physics = arch_player.declareComponent("BulletObjectComponent")

    capsule = ecs.BulletShapeCapsuleData()
    capsule.radius = 1.0
    capsule.extent = 3.0

    c_physics.mass = 10.0
    c_physics.friction = 0.1
    c_physics.restitution = 0.01
    c_physics.angularDamping = 0.5
    c_physics.linearDamping = 0.5
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.angularFactor = vec3(0, 1, 0)
    c_physics.shape = capsule
    c_physics.groupAssign = GROUP_PLAYER
    c_physics.groupCollidesWith = GROUP_BALL | GROUP_ENV

    self.ball_state = {}

    def incrBallState(id):
      state = self.ball_state.get(id, -1) + 1
      self.ball_state[id] = state
      return state

    def onCollision(table):
      ea = table[tokens.entityA]
      if ea.spawner.archetype == arch_player:
        gb = table[tokens.groupB]
        if gb == GROUP_BALL:
          erB = table[tokens.entrefB]
          sgcomp = self.runtime.controller.findComponent(erB, "SceneGraphComponent")
          state = incrBallState(erB.id)
          hsv = vec3()
          hsv.x = float(state) * 0.1
          hsv.y = 1.0
          hsv.z = 0.5
          rgb = hsv.hsv2rgb()
          self.runtime.controller.componentNotify(sgcomp, tokens.ChangeModColor, vec4(rgb, 1))

    c_physics.onCollision(onCollision)

    self.playerforce = ecs.DirectionalForceData()
    c_physics.declareForce("playerforce", self.playerforce)
    self.playerforce_rot = 0.0

    player_spawner = sd.declareSpawner("player_spawner")
    player_spawner.archetype = arch_player
    player_spawner.autospawn = True
    player_spawner.transform.translation = vec3(0, 5, -35)
    player_spawner.transform.orientation = quat(vec3(1, 0, 0), math.pi * 0.5)

    def onSpawn(table):
      entity = table[tokens.entity]
      self.player_transform = entity.transform

    player_spawner.onSpawn(onSpawn)
    self.player_physics_componentdata = c_physics

  ##############################################################################

  def _createBallData(self, sd):
    arch_ball = sd.declareArchetype("BallArchetype")
    c_scenegraph = arch_ball.declareComponent("SceneGraphComponent")
    c_physics = arch_ball.declareComponent("BulletObjectComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 1.0

    c_physics.mass = 1.0
    c_physics.friction = 0.3
    c_physics.restitution = 0.45
    c_physics.angularDamping = 0.01
    c_physics.linearDamping = 0.01
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = sphere
    c_physics.groupAssign = GROUP_BALL
    c_physics.groupCollidesWith = GROUP_ALL

    nid = lev2.scenegraph.NodeInstanceData(BALLS_NODE_NAME)
    c_physics.declareNodeInstance(nid)
    c_scenegraph.declareNodeInstance(nid)

    ball_spawner = sd.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False

  ##############################################################################

  def _createProjectileData(self, sd):
    arch_proj = sd.declareArchetype("ProjectileArchetype")
    c_scenegraph = arch_proj.declareComponent("SceneGraphComponent")
    c_physics = arch_proj.declareComponent("BulletObjectComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 0.5

    c_physics.mass = 5.0
    c_physics.friction = 0.3
    c_physics.restitution = 0.45
    c_physics.angularDamping = 0.01
    c_physics.linearDamping = 0.01
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = sphere
    c_physics.groupAssign = GROUP_BALL
    c_physics.groupCollidesWith = GROUP_ALL

    ball_drawable = lev2.ModelDrawableData("data://tests/pbr_calib.glb")
    c_scenegraph.declareNodeOnLayer(
      name="projnode",
      drawable=ball_drawable,
      layers=FWD_LAYERS,
      modcolor=vec4(0, 0, 1, 0))

    proj_spawner = sd.declareSpawner("proj_spawner")
    proj_spawner.archetype = arch_proj
    proj_spawner.transform.scale = 0.5
    proj_spawner.autospawn = False

  ##############################################################################

  def _createEnvironmentData(self, sd):
    arch_room = sd.declareArchetype("RoomArchetype")
    c_scenegraph = arch_room.declareComponent("SceneGraphComponent")
    c_physics = arch_room.declareComponent("BulletObjectComponent")

    shape = ecs.BulletShapeMeshData()
    shape.meshpath = str(OBJ_PATH)
    shape.scale = SCALE
    shape.translation = OFFSET

    c_physics.mass = 0.0
    c_physics.allowSleeping = True
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = shape
    c_physics.groupAssign = GROUP_ENV
    c_physics.groupCollidesWith = GROUP_ALL

    self.room_SGCOMP = c_scenegraph

    env_spawner = sd.declareSpawner("env_spawner")
    env_spawner.archetype = arch_room
    env_spawner.autospawn = True
    env_spawner.transform.translation = vec3(0, -10, 0)
    env_spawner.transform.scale = 1.0

  ##############################################################################
  # GPU Init (needs GPU context for mesh/material)
  ##############################################################################

  def gpu_init(self, ctx):
    """Load room mesh and create material. Call from _onGpuInit."""
    tmesh = trimesh.load(str(OBJ_PATH))
    submesh = trimeshToSubmesh(tmesh)
    submesh = submesh.withSmoothedNormalsAndBinormals(0.125)

    rprimdata = RigidPrimitiveDrawableData()
    rprimdata.primitive = RigidPrimitive(submesh, ctx)

    material = PBRMaterial()
    material.name = "ConcreteMaterial"
    material.shaderpath = "orkshader://concrete"

    color_img = Image.createFromFile("src://effect_textures/white.dds")
    normal_img = Image.createFromFile("src://effect_textures/default_normal.dds")
    mtlruf_img = Image.createFromFile("src://effect_textures/white.dds")
    material.assignImages(
      ctx, color=color_img, normal=normal_img,
      mtlruf=mtlruf_img, doConform=True)
    material.metallicFactor = 0.0
    material.roughnessFactor = 1.0
    material.doubleSided = False
    material.addBasicStateLambda()
    material.addLightingLambda()
    material.gpuInit(ctx)

    self.room_material = material
    rprimdata.material = material

    room_mesh_transform = Transform()
    room_mesh_transform.nonUniformScale = SCALE
    room_mesh_transform.translation = OFFSET

    self.room_SGCOMP.declareNodeOnLayer(
      name="envnode",
      drawable=rprimdata,
      layers=FWD_LAYERS,
      transform=room_mesh_transform)

  ##############################################################################
  # Post-start: find simulation systems
  ##############################################################################

  def post_start(self):
    """Find systems after simulation starts."""
    self.sys_phys = self.runtime.controller.findSystem("BulletSystem")
    self.sys_sg = self.runtime.controller.findSystem("SceneGraphSystem")
    self.runtime.controller.systemNotify(self.sys_sg, tokens.ResizeFromMainSurface, True)

  ##############################################################################
  # Per-frame update
  ##############################################################################

  def update(self, ezapp):
    """Ball spawning, camera tracking player, tick simulation."""
    controller = self.runtime.controller
    if controller is None:
      return

    # spawn balls
    prob = random.randint(0, 100)
    if prob < 2 and self.spawncounter < NUM_BALLS:
      i = random.randint(-175, 175)
      j = random.randint(-175, 175)
      self.spawncounter += 1
      SAD = ecs.SpawnAnonDynamic("ball_spawner")
      SAD.overridexf.orientation = quat(vec3(0, 1, 0), 0)
      SAD.overridexf.scale = 1.0
      SAD.overridexf.translation = vec3(i, 25, j)
      controller.spawnEntity(SAD)

    # camera follows player
    PXF = self.player_transform
    if PXF is not None:
      UIC = self.runtime.uicam.cameradata
      ROT = self.playerforce_rot
      DIR = (UIC.target - UIC.eye).normalized
      MOTION_DIR = vec3(DIR.x, DIR.y, DIR.z)
      MOTION_DIR.roty(ROT)
      self.playerforce.direction = MOTION_DIR

      if ezapp.shouldUpdateThrottleOnGPU:
        EYE = PXF.translation + OFFSET
        TGT = EYE + DIR
        UP = vec3(0, 1, 0)
        controller.systemNotify(self.sys_sg, tokens.UpdateCamera, {
          tokens.eye: EYE,
          tokens.tgt: TGT,
          tokens.up: UP,
          tokens.near: 0.2,
          tokens.far: 1000.0,
          tokens.fovy: UIC.fovy
        })

    controller.updateSimulation()

  ##############################################################################
  # Event handling
  # NOTE: routed via sgv.camera_evhandler (widget tree path),
  #       NOT via _onUiEvent (which bypasses LayoutGroup::doRouteUiEvent
  #       and breaks profiler toggle).
  ##############################################################################

  def handle_camera_event(self, uievent):
    """Handle FPS input (WASD/space/fire). Delegates camera to runtime."""

    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc == ord(" "):
        self.playerImpulse(vec3(0, 500, 0))
        return lev2.ui.HandlerResult()
      elif kc == ord("W"):
        self.playerforce.magnitude = WALK_FORCE
        self.playerforce_rot = 0.0
        return lev2.ui.HandlerResult()
      elif kc == ord("A"):
        self.playerforce.magnitude = WALK_FORCE
        self.playerforce_rot = math.pi * 1.5
        return lev2.ui.HandlerResult()
      elif kc == ord("S"):
        self.playerforce.magnitude = WALK_FORCE
        self.playerforce_rot = math.pi
        return lev2.ui.HandlerResult()
      elif kc == ord("D"):
        self.playerforce.magnitude = WALK_FORCE
        self.playerforce_rot = math.pi * 0.5
        return lev2.ui.HandlerResult()
      elif kc == ord("F"):
        self.fireProjectile()
        return lev2.ui.HandlerResult()

    elif uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode in [ord("W"), ord("A"), ord("S"), ord("D")]:
        self.playerforce.magnitude = 0.0
        return lev2.ui.HandlerResult()

    return self.runtime.handle_camera_event(uievent)

  ##############################################################################

  def playerImpulse(self, impulse):
    self.runtime.controller.systemNotify(
      self.sys_phys,
      tokens.IMPULSE_ON_COMPONENT_DATA,
      {
        tokens.component: self.player_physics_componentdata,
        tokens.impulse: impulse
      })

  ##############################################################################

  def fireProjectile(self):
    PXF = self.player_transform
    if PXF is not None:
      EYE = PXF.translation + OFFSET
      UIC = self.runtime.uicam.cameradata
      DIR = (UIC.target - UIC.eye).normalized

      SAD = ecs.SpawnAnonDynamic("proj_spawner")
      SAD.overridexf.orientation = quat(vec3(0, 1, 0), 0)
      SAD.overridexf.scale = 1.0
      SAD.overridexf.translation = EYE + DIR * 5.0
      ent = self.runtime.controller.spawnEntity(SAD)
      c_physics = self.runtime.controller.findComponent(ent, "EcsBulletObjectComponent")

      controller = self.runtime.controller
      sys_phys = self.sys_phys

      def LAUNCH_OP():
        controller.systemNotify(
          sys_phys,
          tokens.IMPULSE_ON_COMPONENT,
          {
            tokens.component: c_physics,
            tokens.impulse: DIR * 350.0
          })

      controller.realtimeDelayedOperation(0.1, LAUNCH_OP)

  ##############################################################################

  def reset(self):
    """Reset game state for restart."""
    self.spawncounter = 0
    self.player_transform = None
    self.ball_state = {}
