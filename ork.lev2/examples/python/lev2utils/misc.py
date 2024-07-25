import math, random
from orkengine.core import vec3, quat

################################################################################

class modelinst(object):

  def __init__(self,model,layer, index):

    super().__init__()
    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.pos = vec3(random.uniform(-25.0,25),
                    random.uniform(1,3),
                    random.uniform(-25.0,25))
    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normalized
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)
    self.scale = random.uniform(0.5,0.7)
    self.sgnode.worldTransform.translation = self.pos 
    self.sgnode.worldTransform.scale = self.scale

  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    self.sgnode.worldTransform.orientation = self.rot 