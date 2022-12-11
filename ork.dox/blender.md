Blender Version Tested: 2.81a

To export a rigid mesh with blender:
==================================
* select both the mesh and full-armature (easiest in object mode)
* start an export as GLTF
* format: GLTF-embedded (glb extension)
* Transform: +y up
* Geometry: 
  + Mesh:
    - Apply Modifiers: OFF
    - UV's: ON
    - Normals: ON
    - Tangents: ON
    - VertexColors: OFF
    - Loose Edges: OFF
    - Loose Points: OFF
  + Material: 
    - USE Principled BSDF Shaders with Metallic Roughness Workflow
    - Materials: Export
    - Images: Automatic
    - PBR Extensions:
      1. Export original PBR Specular: OFF
    - Compression: OFF
* Animation: all off

To export a rigged character with blender:
==================================
* select both the mesh and full-armature (easiest in object mode)
* start an export as GLTF
* format: GLTF-embedded (glb extension)
* Transform: +y up
* Geometry: 
  + Mesh:
    - Apply Modifiers: OFF
    - UV's: ON
    - Normals: ON
    - Tangents: ON
    - VertexColors: OFF
    - Loose Edges: OFF
    - Loose Points: OFF
  + Material: 
    - USE Principled BSDF Shaders with Metallic Roughness Workflow
    - Materials: Export
    - Images: Automatic
    - PBR Extensions:
      1. Export original PBR Specular: OFF
    - Compression: OFF
* Animation: all off, except Skinning
  + Include all bone influences=ON (make sure no more than 4!)
  + Export Deformation Bones Only=ON

To export a pure armature animation with blender (tested with blender 3.4):
==================================
* start an export as GLTF
* format: GLTF-embedded (gltf extension)
* Include: (all off)
* Transform: y-up
* Geometry: all off
* Animation: Use Current Frame=On, Shape Keys=Off, Skinning=Off, Animation=On [Limit to Playback Range=On,Always Sample=On, SampleRate=1, Export All Armature Actions=Off] 

To export a rigged animation with blender (tested with blender 3.4):
==================================
* use quaternion mode for bones - axis/ang has interpolation issues
* start an export as GLTF
* format: GLTF-embedded (gltf extension)
* Include: (all off)
* Transform: y-up
* Geometry: all off
* Animation: Use Current Frame=Off, Shape Keys=Off, Skinning=Off, Animation=On [Limit to Playback Range=On,Always Sample=On, SampleRate=1, Export All Armature Actions=Off] 

regarding apply modifiers:
This can mess up normals when exporting glTF
see: https://github.com/KhronosGroup/glTF-Blender-IO/issues/799
