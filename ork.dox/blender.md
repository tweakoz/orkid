To export a rigged character with blender:
==================================
* select both the mesh and full-armature (easiest in object mode)
* start an export as GLTF
* format: GLTF-embedded
* +y up
* Geom: Apply, UV's, Normals, Tangents, VertexColors, Materials, PNG, no-compression
* Skinning: enabled. do not include all bone influences

To export a rigged animation with blender:
==================================
* start an export as collada
* Main: y-up, -z forward, apply global orientation
* Arm: Export to SL/OpenSim
* Anim: Include Anims, Samples, xform type: matrix, Keep keyframes, all keyed, include all actions
