import os, sys, threading, time, random
from pathlib import Path
from obt import path as obt_path

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
LAYERNAME = "std_deferred"
NUM_BALLS = 3
SPAWN_RATE = 0.1
################################################################################

class MYCONTROLLER:

  def __init__(self,ezapp):
    self.ezapp = ezapp
    self.run_state = 0
    cameras.setupUiCamera( app=self, eye = vec3(50), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))
    self.ecsInit()
    self.ecsLaunch()

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
    systemdata_SG.declareParams({
      "SkyboxIntensity": float(2.0),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
    })

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

    self.sys_phys = self.controller.findSystem("PythonSystem")
    self.sys_sg = self.controller.findSystem("SceneGraphSystem")###
    self.sys_python = self.controller.findSystem("PythonSystem")###

    print(self.sys_sg)

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
    
  ##############################################

  def onUpdateExit(self):
    self.controller.uninstallRenderCallbackOnEzApp(self.ezapp)
    self.controller.stopSimulation()
    self.run_state = 2
    #self.run_thread.join()
    self.run_thread = None
    self.controller.terminateSimulation()

  ##############################################

  def onGpuExit(self,ctx):
    self.controller.gpuExit(ctx)
    self.controller.uninstallUpdateCallbackOnEzApp(self.ezapp)

  ##############################################

  def createBallData(self):

    arch_ball = self.ecsscene.declareArchetype("BallArchetype")
    c_scenegraph = arch_ball.declareComponent("SceneGraphComponent")
    #c_python = arch_ball.declareComponent("PythonComponent")

    drawable = lev2.ModelDrawableData("data://tests/pbr_calib.glb")
    c_scenegraph.declareNodeOnLayer( name="ballnode",drawable=drawable,layer=LAYERNAME)

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False  
    ball_spawner.transform.scale = 1.0
    self.ball_spawner = ball_spawner
    
  ##############################################

  def run_loop(self):
    self.run_state = 0
    time.sleep(1)
    print("run_loop begin")

    SAD = ecs.SpawnAnonDynamic("ball_spawner")
    SAD.overridexf.orientation = quat(vec3(0,1,0),0)
    SAD.overridexf.scale = 1.0

    all_entities = dict()

    self.run_state = 1
    while(self.run_state==1):

      time.sleep(0.1)

      prob = random.randint(0,100)

      if prob < 50:

        i = random.randint(-15,15)
        j = random.randint(-15,15)
        k = random.randint(-15,15)
        pos = vec3(i,j,k)

        SAD.overridexf.translation = pos
        e = self.controller.spawnEntity(SAD)
        all_entities[e.id] = e

        count = len(all_entities)
        print("spawned ent<%d> @ %s (count: %d)" % (e.id,pos,count))

    print("run_loop done")
    self.run_state=3
    