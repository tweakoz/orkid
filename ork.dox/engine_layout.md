# Orkid Library Topology
###### Orkid is divided into several shared libraries:
---
## ork.core
  - Reflection system
  - General OS utilities
  - File IO
  - Performance minded containers
  - Math (LA/trig/audio/etc)
  - Operations Queue (higher level concurrency)
  - Threading / low-level concurrency
  - DataBlocks / DataCache - cache output of processed data.
  - Asset managers with on the fly (and cached) conversion
  - Includes python bindings
  - Notable dependencies:
    + Boost
    + Python3/PyBind11


###### python3 module: orkengine.core
---
## ork.lev2
  - Platform a/v/input drivers.
   - Graphics
    + OpenGL
   - Audio
    + PortAudio
   - Input
    + KB/mouse
    + OpenVR HMD/Controller tracking


  - Lower level graphics renderables
    + DrawBuffers - MultiBuffered threaded renderer
    + Frustum culler
    + RenderQueue (with coarse depth and state sorting)
    + Rigid/Skinned models with variable partitioning support
      - Loads GLTF2/GLB
        + embedded textures
        + Metallic Rougness PBR
      - Loads DAE, OBJ and probably other assimp formats
      - Instancing Support via instance textures
    + Custom callback/lambda based renderables
    + Animation controllers with masking and blending
    + Dataflow graph based particle systems
    + PTEX: dataflow graph based procedural texturing system
      - you can put a PTEX on a light projector! (if you can afford the gpu time)


  - Compositor (aka Framegraph)
    + Overridable compositing techniques
    + Included Node Based Technique
      - Render, Post and Output Nodes
      - Screen, RtGroup and OpenVR Output Nodes
      - 2OP and PTEX Post Nodes
      - Forward Rendering Node
      - Clustered/Deferred PBR Rendering node
        + Metallic, Roughness workflow
        + PointLights, SpotLights (w/textured cookies)
        + Cpu Light Processor
        + NvidiaMeshShader Light Processor (WIP)
        + Mono/Stereo support


  - Lower level audio
    + Spatialized audio effect system
    + Audio stream playback controllers
    + Singularity - Software synthesizer modeled on pro audio synthesizers
      - Loads Kurzweil KRZ files
      - Loads Casio CZ101 sysex files
      - Loads Yamaha TX81Z sysex files
      - Loads Soundfont SF2 files


  - Higher level Scenegraph.
    + SceneNode based wrappers of the low level renderables
    + SceneNode based wrappers of the low level audibles (planned)


  - Mesh Processing
     + construct and edit meshes live in-engine, finalize for rendering


  - Includes python bindings

  - Notable dependencies
    + Ork.Core
    + OpenGL (osx/iX)
    + QT5. (osx/iX)
    + PortAudio (osx/iX)
    + OpenVR
    + Vulkan/MoltenVK
    + IGL (geometry/mesh processing library)
    + OpenImageIO (image io library)
    + Python3/PyBind11
    + Assimp

##### python3 module: orkengine.lev2

---

## ork.ent
archetype / entity / component / scene system. Lets you load a pregenerated 'scene' document and run it as a simulation. Contains a collection of commonly useful components, including:

  - Fixed Function and Node based frame compositor
  - Rigid/Skinned 3D model components / archetypes
  - Animation controllers component with animation blending support
  - Particle controller component and archetype
  - Bullet physics object component
  - Bullet "world" system
  - Audio effect playback component with dataflow based modulation support
  - Audio stream playback component
  - Audio manager system
  - Notable dependencies:
      + Ork.Core,Ork.Lev2
      + BulletPhysics

##### python3 module: orkengine.ecs (planned)

---

* ork.tool :
  - Scene / Archetype / Component / Object editor.
  - Asset conversion filters.
  - Notable dependencies:
      + Ork.Core
      + Ork.Lev2
      + Ork.Ent
      + Qt5
      + Python3

##### python3 module: orkengine.tool (planned)

---

* Visual Topology :

![vizorg](OrkidEngineLayout.svg)
