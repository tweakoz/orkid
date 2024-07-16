import os, sys, threading, time, random, math, code
from pathlib import Path
from obt import path as obt_path

from IPython import embed
from IPython.terminal.embed import InteractiveShellEmbed
from traitlets.config import Config

from orkengine.core import vec3, quat, CrcStringProxy
import orkengine.lev2 as lev2
import orkengine.ecs as ecs
from ork import path as ork_path
sys.path.append(str(ork_path.lev2_pylib)) # add parent dir to path
from lev2utils import cameras

this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(obt_path.orkid()/"ork.lev2"/"examples"/"python")) # add parent dir to path
from ork import path as ork_path
sys.path.append(str(ork_path.lev2_pylib)) # add parent dir to path
tokens = CrcStringProxy()
################################################################################
if False:
  RENDERING_MODEL = "ForwardPBR"
  LAYERNAME = "std_forward"
  all_layers = [LAYERNAME,"depth_prepass"]
else:
  RENDERING_MODEL = "DeferredPBR"
  LAYERNAME = "std_deferred"
  all_layers = [LAYERNAME,"depth_prepass"]

BALLS_NODE_NAME = "balls"
NUM_BALLS = 2500
MAX_BALLS = 5000
SPAWN_RATE = 0.1
SSAO_NUM_SAMPLES = 96
################################################################################

class MYCONTROLLER:

  def __init__(self,main_app):
    self.main_app = main_app
    self.ezapp = main_app.ezapp
    self.run_state = 0
    cameras.setupUiCamera( app=self, eye = vec3(50), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))
    self.ecsInit()
    self.ecsLaunch()
    self.all_entities = dict()

  ##############################################

  def ecsInit(self):

    ####################
    # create ECS scene
    ####################

    self.ecsscene = ecs.SceneData()

    ##############################################
    # setup python scripting system
    ##############################################

    systemdata_python = self.ecsscene.declareSystem("PythonSystem")
    systemdata_python.systemUpdateScript = str(this_dir/"_system.py")

    ####################
    # create scenegraph
    ####################

    systemdata_SG = self.ecsscene.declareSystem("SceneGraphSystem")
    systemdata_SG.declareLayer(LAYERNAME)
    systemdata_SG.declareLayer("depth_prepass")
    systemdata_SG.declareParams({
      "preset": RENDERING_MODEL,
      "SkyboxIntensity": float(3.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.0),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
      "SSAONumSamples": SSAO_NUM_SAMPLES,
      "SSAONumSteps": 3,
      "SSAOBias": -1e-5,
      "SSAORadius": 4.0*25.4/1000, # 2 inches
      "SSAOWeight": 1.0,
      "SSAOPower": 2.0,
    })

    drawable = lev2.InstancedModelDrawableData("data://tests/pbr_calib_lopoly.glb")
    drawable.resize(MAX_BALLS)
    systemdata_SG.declareNodeOnLayer( name=BALLS_NODE_NAME,
                                      drawable=drawable,
                                      layers = all_layers)

    self.instance_drawable = drawable
    ####################
    # create archetype/entity data
    ####################

    self.createBallData()
    
  ##############################################

  def ecsLaunch(self):

    ##################
    # create / bind controller
    ##################

    self.controller = ecs.Controller()
    self.controller.bindScene(self.ecsscene)
    self.ezapp.vars.controller = self.controller

    ##################
    # install rendercallback on ezapp
    #  This is so the ezapp will render the ecs scene from C++,
    #   without the need for the c++ to call into python on the render thread
    ##################

    self.controller.installRenderCallbackOnEzApp(self.ezapp)
    self.controller.installUpdateCallbackOnEzApp(self.ezapp)

    ##################
    # launch simulation
    ##################
        
    #self.controller.beginWriteTrace(str(obt_path.temp()/"ecstrace.json"));
    self.controller.createSimulation()
    self.controller.startSimulation()

    ##################
    # retrieve simulation systems
    ##################

    self.sys_sg = self.controller.findSystem("SceneGraphSystem")###
    self.sys_python = self.controller.findSystem("PythonSystem")###

    #print(self.sys_sg)

    ##################
    # init systems
    ##################

    self.controller.systemNotify( self.sys_sg,tokens.ResizeFromMainSurface,True)
    self.spawncounter = 0

    ##################
    # start run loop on thread
    ##################
    
    self.run_thread = threading.Thread(target=self.run_loop)
    self.run_thread.start()

    if self.main_app.interactive:
      def start_repl():
        time.sleep(2)
        repl_banner = "CONTROLLER REPL. Type exit() to exit the REPL."
        local_vars = globals().copy()
        local_vars['self'] = self
        #code.interact(banner=repl_banner, local=local_vars)
        c = Config()
        c.TerminalInteractiveShell.colors = 'Linux'  # Options: 'NoColor', 'Linux', 'LightBG'
        ipshell = InteractiveShellEmbed(config=c, banner1="IPython REPL. Type exit() to exit the REPL.", user_ns=local_vars)
        ipshell()
        #embed(config=c, banner1="IPython REPL. Type exit() to exit the REPL.", user_ns=local_vars)

      repl_thread = threading.Thread(target=start_repl)
      repl_thread.start()
    
  ##############################################

  def onUpdateExit(self):
    self.controller.uninstallUpdateCallbackOnEzApp(self.ezapp)
    self.controller.stopSimulation()
    self.run_state = 2
    self.run_thread.join
    self.run_thread = None
    self.controller.terminateSimulation()

  ##############################################

  def disableSSAO(self):
      self.controller.systemNotify( self.sys_sg,
                                    tokens.UpdatePbrCommon,{
                                      tokens.SSAONumSamples: 0
                                    }
                                  )

  def enableSSAO(self):
      self.controller.systemNotify( self.sys_sg,
                                    tokens.UpdatePbrCommon,{
                                      tokens.SSAONumSamples: SSAO_NUM_SAMPLES
                                    }
                                  )

  ##############################################

  def onGpuExit(self,ctx):
    self.controller.gpuExit(ctx)
    self.controller.uninstallRenderCallbackOnEzApp(self.ezapp)

  ##############################################

  def createBallData(self):

    arch_ball = self.ecsscene.declareArchetype("BallArchetype")
    c_scenegraph = arch_ball.declareComponent("SceneGraphComponent")
    c_python = arch_ball.declareComponent("PythonComponent")


    #drawable = lev2.ModelDrawableData("data://tests/pbr_calib.glb")
    #c_scenegraph.declareNodeOnLayer( name="ballnode",
    #                                 drawable=drawable,
    #                                 layer=LAYERNAME)


    nid = lev2.scenegraph.NodeInstanceData(BALLS_NODE_NAME)
    c_python.declareNodeInstance(nid)
    c_scenegraph.declareNodeInstance(nid)

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False  
    ball_spawner.transform.scale = 1.0
    self.ball_spawner = ball_spawner

    self.SAD = ecs.SpawnAnonDynamic("ball_spawner")
    self.SAD.overridexf.orientation = quat(vec3(0,1,0),0)
    self.SAD.overridexf.scale = 1.0

  ##############################################

  def userSpawn(self,pos,scale):
    self.SAD.overridexf.translation = pos
    self.SAD.overridexf.scale = scale
    e = self.controller.spawnEntity(self.SAD)
    #self.SAD.overridexf.scale = 1.0
    self.all_entities[e.id] = e

  def userSpawnMany(self, pos, scale, the_range, count):
    for i in range(count):
      x = random.uniform(-the_range,the_range)
      y = random.uniform(-the_range,the_range)
      z = random.uniform(-the_range,the_range)
      self.userSpawn(vec3(x,y,z)+pos,scale)
    
  def printCamera(self):
      self.controller.systemNotify( self.sys_python,
                                    tokens.PRINT_CAMERA,{})

  def help(self):
    print("try self.printCamera()")
    print("try self.userSpawn(pos,scale)")
    print("try self.userSpawnMany(pos,scale,range,count)")

  ##############################################
  def reload(self):
    self.controller.systemNotify( self.sys_python,
                                  tokens.RELOAD,{})
  ##############################################

  def run_loop(self):

    self.run_state = 0 # signal that we are not yet running



    self.run_state = 1        # signal that we are running
    phase = 0.0
    while(self.run_state==1): # run loop

      time.sleep(0.001)

      prob = random.randint(0,100)

      if prob < 50:

        count = len(self.all_entities)

        ##########################################
        # dont let too many balls accumulate
        # grab a random entity from all_entities
        ##########################################

        if False and count >= NUM_BALLS:

          index = random.choice(list(all_entities.keys()))
          if all_entities[index] is None:
              continue

          ent = self.all_entities[index]
          
          ##########################################
          # despawn it
          ##########################################

          del self.all_entities[ent.id]
          self.controller.despawnEntity(ent)

        count = len(self.all_entities)
        
        if count < NUM_BALLS:

          i = random.randint(-100,100)
          j = random.randint(-100,100)
          k = random.randint(-100,100)
          pos = vec3(i,j,k)

          self.SAD.overridexf.translation = pos
          e = self.controller.spawnEntity(self.SAD)
          self.all_entities[e.id] = e

        ##########################################
        # set new target for 1 entity
        ##########################################

        index = random.choice(list(self.all_entities.keys()))
        ent = self.all_entities[index]
        #print("send SET_TARGET eid: %d"%ent.id)
        i = random.randint(-30,30)
        j = random.randint(-30,30)
        k = random.randint(-30,30)
        pos = vec3(i,j,k)
        
        self.controller.systemNotify( self.sys_python,
                                      tokens.SET_TARGET,{
                                        tokens.ent: ent,
                                        tokens.pos: pos
                                      })
        #c = ent.getComponent("PythonComponent")
        #print(count)
      if False:      
        phase += 0.001
        tgt = vec3(0,0,0)
        eye = vec3(math.sin(phase),0,-math.cos(phase))*30.0

        self.controller.systemNotify( self.sys_sg,
                                      tokens.UpdateCamera,{
                                      tokens.eye: eye,
                                      tokens.tgt: tgt,
                                      tokens.up: vec3(0,1,0),
                                      tokens.near: 0.1,
                                      tokens.far: 1000.0,
                                      tokens.fovy: 90.0*(3.14159/180.0),
        })

    self.run_state=3 # signal that we are done
    