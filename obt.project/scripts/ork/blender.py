#
from obt import path, host

if host.IsLinux:
  blender_dir = path.Path("/opt/blender401")
  executable = blender_dir/"blender"
else:
  blender_dir = path.Path("/Applications/Blender.app/Contents/MacOS")
  executable = blender_dir/"Blender"

def export_character_mesh(blend_path, glb_path):
  import bpy
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
    #"loglevel": 4,
    #"export_selected": True, # Export only objects visible in the current view layer
    ##################################################
    "use_mesh_edges": False,
    "use_mesh_vertices": False,
    "use_selection": False,
    "use_visible": True,
    "use_renderable": True,
    ##################################################
    "export_apply": False,  
    #"export_texture_transform": False,
    "export_yup": True,
    ##################################################
    "export_lights": False,
    "export_cameras": False,
    ##################################################
    "export_colors": False, # compatibility only
    "export_texcoords": True,
    "export_normals": True,
    "export_tangents": True,
    #"export_gn_mesh": False, # expirimental
    "export_attributes": True, # 'Export Attributes (when starting with underscore)',
    ##################################################
    "export_image_format": "AUTO",
    "export_jpeg_quality": 100,
    "export_keep_originals": False,
    "export_original_specular": False,
    "export_materials": "EXPORT",
    #"export_unused_images": True,
    #"export_unused_textures": True,
    ##################################################
    "export_skins": True, # Export skinning weights
    "export_influence_nb": 4, # Number of bones per vertex
    "export_all_influences": False,
    ##################################################
    #"export_action_filter": False,
    #"export_hierarchy_full_collections": False,
  }
  export_options2 = {
    "export_animations": False, 
    "export_frame_range": False,
    "export_frame_step": 1, 
    "export_force_sampling": True,
    "export_animation_mode": "ACTIONS", # Export actions (actives and on NLA tracks) as separate animations
    "export_nla_strips": False,
    "export_nla_strips_merged_animation_name": "Animation",
    "export_def_bones": False,
    "export_hierarchy_flatten_bones": False,
    #"export_hierarchy_flatten_objs": False,
    #"export_armature_object_remove": False,
    "export_optimize_animation_size": True,
    "export_optimize_animation_keep_anim_armature": True,
    "export_optimize_animation_keep_anim_object": False,
    "export_negative_frame": "SLIDE", # Slide animation to start at frame 0'
    "export_anim_slide_to_zero": False,
    "export_bake_animation": False,
    "export_anim_single_armature": True, # 'Export all Armature Actions',
    "export_reset_pose_bones": True, # 'Reset pose bones between actions'
    "export_current_frame": False, # 'Use Current Frame as Object Rest Transformations'
    "export_rest_position_armature": True, # 'Use Rest Position Armature',
    "export_anim_scene_split_object": True, # Split Animation by Object'
    ##################################################
    "export_morph": False, # Export shape keys ?
    "export_morph_normal": False, 
    "export_morph_tangent": False, 
    "export_morph_animation": False, 
    "export_morph_reset_sk_data": False, 
    ##################################################
    "export_try_sparse_sk": False,
    "export_try_omit_sparse_sk": False,
    ##################################################
    "export_gpu_instances": False,
  }

  ###############################################################  
  # Export to glTF
  ###############################################################  

  bpy.ops.export_scene.gltf(filepath=str(glb_path), **export_options)
  bpy.ops.wm.quit_blender()
