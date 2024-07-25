from orkengine import lev2 

class Fixture:
        
  def __init__(self):
    self.ezapp = lev2.lev2appinit()
    self.ctx = lev2.GfxEnv.loadingContext()
    self.ctx.makeCurrent()
    lev2.lev2apppoll() # process opq
    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    lev2.lev2apppoll() # process opq
    self.modelinst = lev2.XgmModelInst(self.model)
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose 
    self.worldpose = self.modelinst.worldpose
    self.skeleton = self.model.skeleton
    self.anim = lev2.XgmAnim("data://tests/chartest/char_testanim1")
    self.animinst = lev2.XgmAnimInst(self.anim)  
    self.animinst.bindToSkeleton(self.skeleton)

def instance():
  if not hasattr(instance, "fixture"):
    instance.fixture = Fixture()
  return instance.fixture
