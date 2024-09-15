import hou
from ork.hfs import utils 

INHERIT_PARM_EXPRESSION = '''n = hou.pwd()
n_hasFlag = n.isMaterialFlagSet()
i = n.evalParm('inherit_ctrl')
r = 'none'
if i == 1 or (n_hasFlag and i == 2):
    r = 'inherit'
return r'''

###############################################################################

def createCamera(
  stage : utils.SolarisStage,
  input : utils.SolarisObject,
  name : str = "camera",
  translation : tuple = (0,0,0),
  lookat : tuple = (0,0,0),
  aperatureH : float = 35.0,
  aspectRatio : float = 1.777 ):

  camera = utils.createTypedNode( 
    parent=stage,
    clazz = utils.SolarisCamera,                                 
    typ="camera", 
    name=name, 
    params={ 
      "t": translation,
      "lookatenable": True,
      "lookatposition": lookat,
      "horizontalAperture": aperatureH,
      #"verticalAperture": aper_v,
      "aspectratiox": aspectRatio,
      "aspectratioy": 1.0,
    },
    inputs=[input]
  )
  camera.cameras_path = utils.SolarisPath(name=name, parent=stage.cameras_path)
  return camera

###############################################################################

def createPointLight(
  stage : utils.SolarisStage,
  input : utils.SolarisObject,
  name : str = "point_light",
  translation : tuple = (0,0,0),
  intensity : float = 1.0 ):

  light = utils.createTypedNode(
    parent=stage,
    clazz=utils.SolarisLightNode,
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

def createKarmaRenderSettings(
  stage : utils.SolarisStage,
  name : str,
  camera : utils.SolarisCamera,
  input : utils.SolarisObject,
  resolutionx : int = 1280,
  samplesperpixel : int = 9,
  secondary_minsamples : int = 1,
  secondary_maxsamples : int = 1,
  sssquality : int = 1,
  tonemap : str = "off",
  denoiser : str = "off" ):
  

  rendersettings = utils.createTypedNode(
    parent=stage,
    clazz=utils.SolarisRenderSettingsNode,
    typ="karmarenderproperties", 
    name=name,
    params={
      "camera": camera.cameras_path,
      "resolutionx": resolutionx,
      "samplesperpixel": samplesperpixel,
      "varianceaa_minsamples" : secondary_minsamples,
      "varianceaa_maxsamples" : secondary_maxsamples,
      "sssquality" : sssquality,
      "denoiser": denoiser,
      "tonemap": tonemap,
    },
    inputs=[input]
  )
  rendersettings.render_path = utils.SolarisPath(name=name, parent=stage.render_path)

  return rendersettings

###############################################################################
# create solaris usd rop node
###############################################################################

def createUsdRopNode(
  stage : utils.SolarisStage,
  name : str,
  input : utils.SolarisObject,
  rendersettings : utils.SolarisRenderSettingsNode,
  renderer : str = "BRAY_HdKarma", # (BRAY_HdKarma,BRAY_HdKarmaXPU)
  start_frame : int = 1,
  end_frame : int = 1 ):

  ROP = utils.createTypedNode(
    parent=stage,
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

###############################################################################
# create solaris MaterialX Builder Node
###############################################################################

def createMaterialXNode(
  stage : utils.SolarisStage, 
  mat_name : str,
  baseColor : tuple = (0.5,0.5,0.5),
  subsurface : float = 0.0,
  subsurfaceColor : tuple = (0,0,0),
  subsurfaceRadius : tuple = (1,1,1),
  subsurfaceScale : float = 1.0
  ):
                        
  materialx_subnet = stage.material_lib.createNode("subnet", mat_name)
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

  solobj = utils.SolarisMaterialNode(parent=stage)
  solobj.node = materialx_subnet
  solobj.node_name = mat_name
  solobj.materials_path = utils.SolarisPath(name=mat_name, parent=stage.materials_path)

  return solobj

#########################################################
# assign material to a shape node
#########################################################

def assignMaterial(
  stage : utils.SolarisStage,
  name : str,
  shapes : list,
  material : utils.SolarisMaterialNode,
  input : utils.SolarisObject
  ):

  prims = ""
  
  for item in shapes:
    prims += item.root_path.fqname + " "

  assign_material = utils.createTypedNode(
    parent = stage,
    typ="assignmaterial",
    name=name,
    params={
      "primpattern1": prims,
      "matspecpath1": material.materials_path,
    },
    inputs = [input]
  )
  
  return assign_material

#########################################################
# Create a USD primitive node to instantiate a sphere
#########################################################

def createSphereAndNode(
  parent : utils.SolarisObject,
  node_name : str,
  translation : tuple = (0,0,0),
  input : utils.SolarisObject = None,
  outputs : list = None,
  radius : float = 1.0,
):

  stage_path = utils.SolarisPath(name=node_name, parent=parent.stage_path)
  root_path = utils.SolarisPath(name=node_name, parent=None)

  sph_node = parent.node.createNode("sphere", node_name)
  sph_node.setParms({
    "radius":radius,
    "t":translation
  })

  sol_obj = utils.SolarisGeoNode(parent=parent)
  sol_obj.node = sph_node
  sol_obj.node_name = node_name
  sol_obj.stage_path = stage_path
  sol_obj.root_path = root_path

  if outputs is not None:
    for output_item in outputs:
      node = output_item[0]
      index = output_item[1]
      node.setInput(index, sph_node, 0)

  return sol_obj

#########################################################
# Create a USD primitive node to instantiate a sphere
#########################################################

def createSubnet(
  parent : utils.SolarisObject,
  node_name : str,
  input : utils.SolarisObject = None,
  outputs : list = None ):

  stage_path = utils.SolarisPath(name=node_name, parent=parent.stage_path)
  root_path = utils.SolarisPath(name=node_name, parent=None)

  subnet = parent.node.createNode("subnet", node_name)

  sol_obj = utils.SolarisGeoNode(parent=parent)
  sol_obj.node = subnet
  sol_obj.node_name = node_name
  sol_obj.stage_path = stage_path
  sol_obj.root_path = root_path

  if outputs is not None:
    for output_item in outputs:
      node = output_item[0]
      index = output_item[1]
      node.setInput(index, subnet, 0)

  ##############################################
  # add "disconnectAll" method to SolarisGeoNode
  ##############################################

  def disconnectAll(self):
    child_nodes = self.node.children()
    for child in child_nodes:
      child.setInput(0, None)
    

  sol_obj.disconnectAll = disconnectAll.__get__(sol_obj)

  ##############################################
  # add setOutput method to SolarisGeoNode
  ##############################################

  def setOutput(self, subnode):
    child_node = self.node.node("output0")
    child_node.setInput(0, subnode.node, 0)
    
  sol_obj.setOutput = setOutput.__get__(sol_obj)
  
  ##############################################

  return sol_obj

#########################################################
# Create a USD primitive node to instantiate a sphere
#########################################################

def createMergeNode(
  parent : utils.SolarisObject,
  node_name : str,
  input : utils.SolarisObject = None,
  outputs : list = None,
  mergeItems : list = [] ):
  
  stage_path = utils.SolarisPath(name=node_name, parent=parent.stage_path)
  root_path = utils.SolarisPath(name=node_name, parent=None)
  
  merge_node = parent.node.createNode("merge", node_name)
  
  for item in mergeItems:
    merge_node.setNextInput(item.node)
    
  sol_obj = utils.SolarisGeoNode(parent=parent)
  sol_obj.node = merge_node
  sol_obj.node_name = node_name
  sol_obj.stage_path = stage_path
  sol_obj.root_path = root_path
  
  if outputs is not None:
    for output_item in outputs:
      node = output_item[0]
      index = output_item[1]
      node.setInput(index, merge_node, 0)  
      
  return sol_obj

#########################################################
# Create a curve
#########################################################

def setTransformKeyframe(
  object : utils.SolarisObject,
  translation : tuple = (0,0,0),
  frame : int = 1 ):

  parm = object.node.parm("tx")
  keyframe = hou.Keyframe()
  keyframe.setFrame(frame)
  keyframe.setValue(translation[0])
  parm.setKeyframe(keyframe)  

  parm = object.node.parm("ty")
  keyframe = hou.Keyframe()
  keyframe.setFrame(frame)
  keyframe.setValue(translation[1])
  parm.setKeyframe(keyframe)  

  parm = object.node.parm("tz")
  keyframe = hou.Keyframe()
  keyframe.setFrame(frame)
  keyframe.setValue(translation[2])
  parm.setKeyframe(keyframe)  

GENMOVIE = '''  
import subprocess
def generate_movie_with_mplay(directory, frame_pattern):
    """
    directory: the folder where frames are stored
    frame_pattern: the pattern of frame filenames (e.g., 'frame.%04d.exr')
    """
    mplay_cmd = [
        'mplay', '-r', '24',  # '-r' sets frame rate
        os.path.join(directory, frame_pattern)
    ]
    
    # Run the mplay command
    subprocess.run(mplay_cmd)

# Example usage
generate_movie_with_mplay("/path/to/frames", "frame.%04d.exr")
'''