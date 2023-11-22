#
import bpy


def export_character_mesh(blend_path, glb_path):
  ###############################################################  
  # Load the Blender file
  ###############################################################  

  bpy.ops.wm.open_mainfile(filepath=str(blend_path))

  ###############################################################  
  # Create a new collection for visible objects
  ###############################################################  

  visible_collection = bpy.data.collections.new("ExportObjects")
  bpy.context.scene.collection.children.link(visible_collection)

  ###############################################################  
  # Move visible objects to the new collection
  ###############################################################  
  visited = set()  # Keep track of objects that have been visited
  for obj in bpy.context.view_layer.objects:
    visible_mesh = (obj.visible_get() and obj.type == 'MESH')  # Check if the object is visible and is a mesh
    print("VISIBLE: ", visible_mesh)
    if visible_mesh and obj.name not in visited:
      visible_collection.objects.link(obj)  # Link object to the new collection
      if obj.name in bpy.context.scene.collection.objects:
        print(bpy.context.scene.collection.objects)
        print(obj,obj.name)
        bpy.context.scene.collection.objects.unlink(obj)  # Unlink object from the original collection
        visited.add(obj.name)  # Add object to the set of visited objects

  ###############################################################  
  # Set export options
  ###############################################################  

  export_options = {
    "export_format": "GLB", 
    #"export_selected": True, # Export only objects visible in the current view layer
    "export_apply": True,  
    "export_materials": "EXPORT",
    "export_colors": True,
    "export_animations": False,
    "export_def_bones": False, # Export deformation bones only
    "export_skins": True, # Export skinning weights
    "export_all_influences": False, # limit to 4 bone weighting
    "export_morph": False, # Export shape keys ?
    "export_lights": False,
    "export_cameras": False,
    #"export_texture_transform": False,
    "export_yup": True,
  }

  ###############################################################  
  # Export to glTF
  ###############################################################  

  bpy.ops.export_scene.gltf(filepath=str(glb_path), **export_options, use_selection=True)
  bpy.ops.wm.quit_blender()
