import hou 
import typing
from obt import path

###############################################################################

def setAnimGlobals( fps=30, start=1, end=1, active=1 ):
  hou.setFps(fps=fps,
             modify_frame_count=True,
             preserve_keyframes=False,
             preserve_frame_start=False)
  hou.hscript("tset `(%d-1)/$FPS` `%d/$FPS`" % (start,end))
  hou.setFrame(active)

###############################################################################

class SolarisPath:

  def __init__(self, name=None, parent=None):
    self.node_name = name
    self.parent = parent

  # operator /
  def __truediv__(self, other):
    return SolarisPath(name=other, parent=self)
    
  def __str__(self):
    return "%s:%s:%s"%(self.node_name, self.parent,self.fqname)

  @property
  def fqname(self):
    if self.parent:
      return self.parent.fqname + "/" + self.node_name
    elif self.node_name!=None:
      return "/"+self.node_name
    else:
      return ""

###############################################################################

class SolarisObject:
  def __init__(self, node_name=None, parent=None):
    self.parent = parent
    self.prim_name = None
    self.node_name = None
    self.prim = None 
    self.node = None
    self.path = SolarisPath(name=node_name, parent=parent)

###############################################################################

  def _findInputNamed(self, node, name):
    for i in range(node.inputs()):
      if node.input(i).name() == name:
        return node.input(i)
    return None

###############################################################################

  def assignAsOutputNode( self ):
    self.node.setDisplayFlag(True)

###############################################################################

class SolarisStage:
  
  #########################################################

  def __init__(self):
    #super().__init__(node_name="stage", parent=None)
    impl = hou.node("/stage")
    if not impl:
      impl = hou.node("/obj").createNode("lopnet", "stage")
    self.impl = impl
    self.material_lib = self.impl.createNode("materiallibrary", "my_material_lib")
    self.materials_path = SolarisPath(name="materials",parent=None)
    self.cameras_path = SolarisPath(name="cameras",parent=None)
    self.render_path = SolarisPath(name="Render",parent=None)
    self.stage_path = SolarisPath(name="stage", parent=None)  

  #########################################################
  # create generic node
  #########################################################

  def createTypedNode(self,
                      clazz = SolarisObject,
                      parent=None, 
                      typ="subnet",
                      name="generic_node",
                      params=None,
                      inputs=None,
                      displayFlag=None):
    #####################
    sol_obj = clazz(parent=self)
    sol_obj.node = self.impl.createNode(typ, name)
    sol_obj.node_name = name
    sol_obj.prim = None
    sol_obj.prim_name = None
    sol_obj.stage_path = SolarisPath(name=name, parent=self.stage_path)
    #####################
    if params is not None:
      conv_dict = {}
      for key in params:
        val = params[key]
        if type(val) == SolarisPath:
          val = val.fqname
        conv_dict[key] = val
      sol_obj.node.setParms(conv_dict)
    #####################
    if inputs is not None:
      if type(inputs) == list:
        for i in range(len(inputs)):
          inp = inputs[i]
          if issubclass(type(inp), SolarisObject):
            inp = inp.node
          sol_obj.node.setInput(i, inp)
    #####################
    if displayFlag is not None:
      sol_obj.node.setDisplayFlag(displayFlag)
    #####################
    return sol_obj


###############################################################################

class SolarisCamera(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="camera", parent=parent)

###############################################################################

class SolarisGeoNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="camera", parent=parent)

###############################################################################

class SolarisLightNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="camera", parent=parent)

###############################################################################

class SolarisMaterialNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="camera", parent=parent)

###############################################################################

class SolarisRenderSettingsNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="camera", parent=parent)

