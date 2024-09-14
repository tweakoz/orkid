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
  camera.cameras_path = utils.SolarisPath(name=name, parent=stage.cameras_path)
  return camera

###############################################################################

def createPointLight(
  stage : utils.SolarisStage,
  input : utils.SolarisObject,
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

def createKarmaRenderSettings(
  stage : utils.SolarisStage,
  name : str,
  camera : utils.SolarisObject,
  input : utils.SolarisObject,
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
  rendersettings.render_path = utils.SolarisPath(name=name, parent=stage.render_path)

  return rendersettings

###############################################################################
# create solaris usd rop node
###############################################################################

def createUsdRopNode(
  stage : utils.SolarisStage,
  name : str,
  input : utils.SolarisObject,
  rendersettings : utils.SolarisObject,
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

  solobj = utils.SolarisObject(parent=stage)
  solobj.node = materialx_subnet
  solobj.prim = None
  solobj.prim_name = None
  solobj.node_name = mat_name
  solobj.materials_path = utils.SolarisPath(name=mat_name, parent=stage.materials_path)

  return solobj


#########################################################
# assign material to a shape node
#########################################################

def assignMaterial(
  stage : utils.SolarisStage,
  name : str,
  shape : utils.SolarisObject,
  material : utils.SolarisObject,
  input : utils.SolarisObject
  ):

  assign_material = stage.createTypedNode(
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
# Create a USD primitive node to instantiate a sphere
#########################################################

def createSphereAndNode(
  stage : utils.SolarisStage,
  prim_name : str,
  node_name : str,
  radius : float = 1.0 ):

  sph_prim = stage.impl.createNode("sphere", prim_name)
  sph_prim.setParms({"radius":radius})
  sph_node = hou.node("/stage/"+prim_name)
  sph_node.setName(node_name)
  sol_obj = utils.SolarisObject(parent=stage)
  sol_obj.node = sph_node
  sol_obj.prim = sph_prim
  sol_obj.prim_name = prim_name
  sol_obj.node_name = node_name
  sol_obj.stage_path = utils.SolarisPath(name=node_name, parent=stage.stage_path)
  sol_obj.prim_path = utils.SolarisPath(name=prim_name, parent=None)
  sol_obj.root_path = utils.SolarisPath(name=node_name, parent=None)

  return sol_obj
