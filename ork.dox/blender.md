To export a rigged character with blender:
==================================
* select both the mesh and full-armature (easiest in object mode)
* start an export as GLTF
* format: GLTF-embedded
* Transform: +y up
* Geom: Apply, UV's, Normals, Tangents, VertexColors, Materials, PNG, no-compression
* Skinning: enabled. do not include all bone influences
* convert to xgm : ork.tool.release --filter ass:xgm --in infile.gltf --out outfile.xgm

To export a pure armature animation with blender:
==================================
* start an export as collada
* Main: y-up, -z forward, apply global orientation
* Arm: Export to SL/OpenSim
* Anim: Include Anims, Samples, xform type: matrix, Keep keyframes, all keyed, include all actions
* convert to xga : ork.tool.release --filter ass:xga --in infile.gltf --out outfile.xga

To export a rigged animation with blender:
==================================
* export as GLTF
* format: GLTF-embedded
* Transform: +y up
* Geom: should not matter (try all off)
* Anim: Limit to Playback, Always Sample, NLA Strips
* Skinning: on, but probably does not matter.
* convert to xga : ork.tool.release --filter ass:xga --in infile.gltf --out outfile.xga
* note: currently bone "Roll" does not work, set to 0
