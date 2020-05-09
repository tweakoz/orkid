# Orkid Library Topology
###### Orkid is divided into several shared libraries:
---
## ork.core
  - Reflection system
  - General OS utilities
  - File IO
  - Performance minded containers
  - Math (LA/trig/audio/etc)
  - Operations Queue (higher level concurrency, inspired by Apple's GCD)
  - Threading / low-level concurrency
  - DataBlocks / DataCache - cache output of processed data.
  - Asset managers with on the fly (and cached) conversion
  - Includes python bindings
  - Notable dependencies (License):
    + Boost (Boost, permissive)
    + Python3 (PSF, permissive)
    + PyBind11 (Custom, permissive)


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
     + MIDI/OSC
     + TUIO


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
      - PickBuffer Rendering Node
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
     + construct and edit meshes live in-engine, finalize and cache for rendering
     + python bindings allow interoperability with numpy style 3D mesh frameworks


  - Includes python bindings

  - Notable dependencies (License):
    + Ork.Core (Boost, permissive)
    + OpenGL (Permissive)
    + PortAudio (Custom, permissive)
    + PortMIDI (Apache2.0, permissive)
    + TUIO (LGPL, permissive)
    + OpenVR (BSD3, Permissive)
    + Vulkan 
    + MoltenVK (Apache2.0, Permissive)
    + IGL - geometry/mesh processing library (MPL2, permissive)
      - CGAL (GPL, or LGPL3 - permissive if using LGPL3)
    + ISPC (BSD, permissive)
    + ISPCTextureCompressor (MIT, permissive)
    + Nvida Texture Tools (MIT, permissive)
    + OpenImageIO (BSD3, Permissive)
    + Python3 (PSF, permissive)
    + PyBind11 (Custom, permissive)
    + Assimp (BSD3, Permissive) 
    + QT5 (LGPL3)
    + PyQT5 (GPL, or commercial - your choice)
    + Pyside2 (LGPL3)

##### python3 module: orkengine.lev2

---

## ork.ent
archetype / entity / component / scene system. Lets you load a pregenerated 'scene' document and run it as a simulation. Contains a collection of commonly useful components, including:

  - Lev2 Compositor system
  - Lev2 Rigid/Skinned 3D model components / archetypes
  - Animation controllers component with animation blending support
  - Lev2 Particle controller component and archetype
  - Bullet physics object component
  - Bullet "world" system
  - Lev2 Audio effect playback component with dataflow based modulation support
  - Lev2 Audio stream playback component
  - Lev2 Audio manager system
  - Notable dependencies (License):
      + Ork.Core (Boost, permissive)
      + Ork.Lev2 (Boost, permissive)
      + BulletPhysics (zlib, permissive)

##### python3 module: orkengine.ecs (planned)

---

* ork.tool :
  - Scene / Archetype / Component / Object editor.
  - Asset conversion filters.
  - Notable dependencies:
      + Ork.Core (Boost, permissive)
      + Ork.Lev2 (Boost, permissive)
      + Ork.Ent (Boost, permissive)
      + Qt5 (LGPL3)
      + Python3 (PSF, permissive)

##### python3 module: orkengine.tool (planned)

---

* Visual Topology :

![vizorg](OrkidEngineLayout.png)
