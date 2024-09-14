import hou 
import typing
from obt import path

INHERIT_PARM_EXPRESSION = '''n = hou.pwd()
n_hasFlag = n.isMaterialFlagSet()
i = n.evalParm('inherit_ctrl')
r = 'none'
if i == 1 or (n_hasFlag and i == 2):
    r = 'inherit'
return r'''


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
  # Create a USD primitive node to instantiate a sphere
  #########################################################

  def createSphereAndNode(self, 
                          prim_name="sphere_prim",
                          node_name="sphere_node",
                          radius=1.0):
    sph_prim = self.impl.createNode("sphere", prim_name)
    sph_prim.setParms({"radius":radius})
    sph_node = hou.node("/stage/"+prim_name)
    sph_node.setName(node_name)
    sol_obj = SolarisObject(parent=self)
    sol_obj.node = sph_node
    sol_obj.prim = sph_prim
    sol_obj.prim_name = prim_name
    sol_obj.node_name = node_name
    sol_obj.stage_path = SolarisPath(name=node_name, parent=self.stage_path)
    sol_obj.prim_path = SolarisPath(name=prim_name, parent=None)
    sol_obj.root_path = SolarisPath(name=node_name, parent=None)

    return sol_obj

  #########################################################
  # create generic node
  #########################################################

  def createTypedNode(self,
                      parent=None, 
                      typ="subnet",
                      name="generic_node",
                      params=None,
                      inputs=None,
                      displayFlag=None):
    #####################
    sol_obj = SolarisObject(parent=self)
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
          if type(inp) == SolarisObject:
            inp = inp.node
          sol_obj.node.setInput(i, inp)
    #####################
    if displayFlag is not None:
      sol_obj.node.setDisplayFlag(displayFlag)
    #####################
    return sol_obj

  #########################################################
  # assign material to a shape node
  #########################################################

  def assignMaterial(
    self,
    name = None,
    shape=None,
    material=None,
    input=None):

    assign_material = self.createTypedNode(
      typ="assignmaterial",
      name=name,
      params={
        "primpattern1": shape.root_path,
        "matspecpath1": material.materials_path,
      },
      inputs = [input]
    )
    
    return assign_material

  #########################################################
  # create usd materialx node
  #########################################################

  def createMaterialX(self, 
                      mat_name="materialx1",
                      baseColor=(0.5,0.5,0.5),
                      subsurface=0.0,
                      subsurfaceColor=(1,1,1),
                      subsurfaceRadius=(1,1,1),
                      subsurfaceScale=1.0
                      ):
                          
    materialx_subnet = self.material_lib.createNode("subnet", mat_name)
    materialx_subnet.setMaterialFlag(True)    
    #materialx_subnet.setInput(0, sphere_node)
    parameters = materialx_subnet.parmTemplateGroup()

    newParm_hidingFolder = hou.FolderParmTemplate("mtlxBuilder","MaterialX Builder",folder_type=hou.folderType.Collapsible)
    control_parm_pt = hou.IntParmTemplate('inherit_ctrl','Inherit from Class', 
                        num_components=1, default_value=(2,), 
                        menu_items=(['0','1','2']),
                        menu_labels=(['Never','Always','Material Flag']))

    newParam_tabMenu = hou.StringParmTemplate("tabmenumask", "Tab Menu Mask", 1, default_value=["MaterialX parameter constant collect null genericshader subnet subnetconnector suboutput subinput"])

    class_path_pt = hou.properties.parmTemplate('vopui', 'shader_referencetype')
    class_path_pt.setLabel('Class Arc')
    class_path_pt.setDefaultExpressionLanguage((hou.scriptLanguage.Python,))
    class_path_pt.setDefaultExpression((INHERIT_PARM_EXPRESSION,))   

    ref_type_pt = hou.properties.parmTemplate('vopui', 'shader_baseprimpath')
    ref_type_pt.setDefaultValue(['/__class_mtl__/`$OS`'])
    ref_type_pt.setLabel('Class Prim Path')               

    newParam_rctxname = hou.StringParmTemplate("shader_rendercontextname", "Render Context Name", 1, default_value=["mtlx"])

    newParm_hidingFolder.addParmTemplate(control_parm_pt)  
    newParm_hidingFolder.addParmTemplate(class_path_pt)    
    newParm_hidingFolder.addParmTemplate(ref_type_pt)      

    # add divider

    newParm_hidingFolder.addParmTemplate(hou.SeparatorParmTemplate("divider1"))
           
    newParm_hidingFolder.addParmTemplate(newParam_tabMenu)
    newParm_hidingFolder.addParmTemplate(newParam_rctxname)

    parameters.append(newParm_hidingFolder)
    materialx_subnet.setParmTemplateGroup(parameters)

    #######################
    # delete materialx_subnet.subinput1 and materialx_subnet.suboutput1

    materialx_subnet.node("subinput1").destroy()
    materialx_subnet.node("suboutput1").destroy()

    #######################
    # create materialx surface input connector (surface_output)
    #  set its type to surface
    #  set its connector type to output
    #######################

    surface_output = materialx_subnet.createNode("subnetconnector", "surface_output")
    surface_output.setParms({"parmtype": "surface", 
                             "connectorkind": "output",
                             "parmname": "surface",
                             "parmlabel": "surface",})

    #######################
    # create materialx surface input connector (surface_output)
    #  set its type to surface
    #  set its connector type to output
    #######################

    displacement_output = materialx_subnet.createNode("subnetconnector", "displacement_output")
    displacement_output.setParms({"parmtype": "displacement", 
                                  "connectorkind": "output",
                                  "parmname": "displacement",
                                  "parmlabel": "displacement",})

    #######################
    # create mtlxstandard_surface shader
    # connect it's out to the surface_output
    #######################

    surfaceshader = materialx_subnet.createNode("mtlxstandard_surface", mat_name+"_surface")
    surface_output.setInput(0, surfaceshader, 0)

    dispshader = materialx_subnet.createNode("mtlxdisplacement", mat_name+"_displacement")
    displacement_output.setInput(0, dispshader, 0)

    #######################
    # set surface shader base color
    #######################

    surfaceshader.setParms({
      "base_color": baseColor,
      "subsurface": subsurface,
      "subsurface_color": subsurfaceColor,
      "subsurface_radius": subsurfaceRadius,
      "subsurface_scale": subsurfaceScale,
    })

    #######################
    # layout all on the materialx_subnet
    #######################

    materialx_subnet.layoutChildren()

    #######################

    solobj = SolarisObject(parent=self)
    solobj.node = materialx_subnet
    solobj.prim = None
    solobj.prim_name = None
    solobj.node_name = mat_name
    solobj.materials_path = SolarisPath(name=mat_name, parent=self.materials_path)

    return solobj

  #########################################################
  # create utilities
  #########################################################
  
  def createCamera(self,**kwargs):
    return SolarisCamera(stage=self,**kwargs)
  def createPointLight(self,**kwargs):
    return SolarisPointLight(stage=self,**kwargs)
  def createKarmaRenderSettings(self,**kwargs):
    return SolarisKarmaRenderSettings(stage=self,**kwargs)
  def createUsdRopNode(self,**kwargs):
    return SolarisUsdRopNode(stage=self,**kwargs)

###############################################################################

def SolarisCamera(
  stage : SolarisStage,
  input : SolarisObject,
  name : str = "camera",
  translation : tuple = (0,0,0),
  lookat : tuple = (0,0,0),
  aperatureH : float = 35.0,
  aspectRatio : float = 1.777 ):

  camera = stage.createTypedNode( 
    typ="camera", 
    name=name, 
    params={ 
      "t": translation,
      "lookatposition": lookat,
      "horizontalAperture": aperatureH,
      #"verticalAperture": aper_v,
      "aspectratiox": aspectRatio,
      "aspectratioy": 1.0,
    },
    inputs=[input]
  )
  camera.cameras_path = SolarisPath(name=name, parent=stage.cameras_path)
  return camera

###############################################################################

def SolarisPointLight(
  stage : SolarisStage,
  input : SolarisObject,
  name : str = "point_light",
  translation : tuple = (0,0,0),
  intensity : float = 1.0 ):

  light = stage.createTypedNode(
    typ="light::2.0", 
    name=name,
    params={ 
      "t": translation,
      "lighttype": "point",
      "xn__inputsintensity_i0a": intensity
    },
    inputs=[input]
  )

  return light
        
###############################################################################

def SolarisKarmaRenderSettings(
  stage : SolarisStage,
  name : str,
  camera : SolarisObject,
  input : SolarisObject,
  resolutionx : int = 1280,
  samplesperpixel : int = 9 ):

  rendersettings = stage.createTypedNode(
    typ="karmarenderproperties", 
    name=name,
    params={
      "camera": camera.cameras_path,
      "resolutionx": resolutionx,
      "samplesperpixel": samplesperpixel,
    },
    inputs=[input]
  )
  rendersettings.render_path = SolarisPath(name=name, parent=stage.render_path)

  return rendersettings

###############################################################################
# create solaris usd rop node
###############################################################################

def SolarisUsdRopNode(
  stage : SolarisStage,
  name : str,
  input : SolarisObject,
  rendersettings : SolarisObject,
  renderer : str = "BRAY_HdKarma", # (BRAY_HdKarma,BRAY_HdKarmaXPU)
  start_frame : int = 1,
  end_frame : int = 1 ):

  ROP = stage.createTypedNode(
    typ="usdrender_rop", 
    name="render1",
    params= {
      "renderer": renderer,
      "rendersettings": rendersettings.render_path,
      "trange": "on",  # frame range
      "f1": start_frame,  # frame start
      "f2": end_frame,  # frame end
      "f3": 1,  # frame increment
    },
    inputs=[input]
  )
  return ROP
