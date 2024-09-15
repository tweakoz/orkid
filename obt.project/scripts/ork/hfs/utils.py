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
    self.node_name = None
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

class SolarisStage(SolarisObject):
  
  #########################################################

  def __init__(self):
    #super().__init__(node_name="stage", parent=None)
    node = hou.node("/stage")
    if not node:
      node = hou.node("/obj").createNode("lopnet", "stage")
    self.node = node

    self.cameras_path = SolarisPath(name="cameras",parent=None)
    self.render_path = SolarisPath(name="Render",parent=None)
    self.stage_path = SolarisPath(name="stage", parent=None)  

###############################################################################

class SolarisSubnetNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="subnet", parent=parent)

###############################################################################

class SolarisMergeNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="merge", parent=parent)
    self.subitems = []

###############################################################################

class SolarisCamera(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="camera", parent=parent)

###############################################################################

class SolarisGeoNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="geo", parent=parent)

###############################################################################

class SolarisLightNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="light", parent=parent)

###############################################################################

class SolarisMaterialNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="material", parent=parent)

###############################################################################

class SolarisRenderSettingsNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="rendersettings", parent=parent)

###############################################################################

class SolarisMaterialsLibraryNode(SolarisObject):
  def __init__(self, parent=None):
    super().__init__(node_name="materials", parent=parent)
    self.materials_path = SolarisPath(name="materials",parent=None)
    self.node = parent.node.createNode("materiallibrary", "my_material_lib")

###############################################################################

def createTypedNode(parent : SolarisObject,
                    clazz = SolarisObject,
                    typ : str = "subnet",
                    name : str = "generic_node",
                    params : dict = None,
                    inputs : list = None,
                    displayFlag : bool = None ):
  #####################
  sol_obj = clazz(parent=parent)
  sol_obj.node = parent.node.createNode(typ, name)
  sol_obj.node_name = name
  sol_obj.stage_path = SolarisPath(name=name, parent=parent.stage_path)
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

def createHqueueRenderOut( 
  usd_render_node : SolarisRenderSettingsNode,
  hq_server : str,
  hq_hfs_linux : str,
  shared_file : str,
  this_name : str ):

  out = hou.node("/out")
  fetch_rop = out.createNode("fetch", "fetch_rop")
  fetch_rop.setParms({"source": usd_render_node.stage_path.fqname})

  # Configure HQueue render settings
  # Set HQueue rendering to process the Solaris LOP network

  hqueue_render = out.createNode("hq_render", "hqueue_render")
  hqueue_render.setParms({
    "hq_useuniversalhfs": False,
    "hq_hip_action": "use_target_hip",
    "hq_server": hq_server,  # Current HIP file path
    "hq_hfs_linux": hq_hfs_linux,  # Current HIP file path
    "hq_hip": str(shared_file),  # Current HIP file path
    "hq_job_name": "OBT_"+this_name,
  })
  hqueue_render.setInput(0, fetch_rop)

  out.layoutChildren()
  
  return hqueue_render
  