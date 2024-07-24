from orkengine import lev2 

class Fixture:
        
  def __init__(self):
    self.ezapp = lev2.lev2appinit()
    self.ctx = lev2.GfxEnv.loadingContext()
    self.ctx.makeCurrent()
    lev2.lev2apppoll() # process opq
    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    lev2.lev2apppoll() # process opq

def instance():
  if not hasattr(instance, "fixture"):
    instance.fixture = Fixture()
  return instance.fixture
