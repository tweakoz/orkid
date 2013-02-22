#Orkid Library Topology

---

Orkid is divided into several shared libraries:

---
* ork.core 
	- Reflection kernel
	- Performance minded containers
	- Math (LA/trig/audio/etc)
	- File IO
	- Threading/concurrency
	- Asset managers
	- General OS utilities.
	- Notable dependencies: 
		+ Intel TBB
		+ Boost

---

* ork.lev2 : 
	- Platform a/v/input drivers.
		- Graphics
			+ OpenGL
			+ D3D9c
		- Audio
			+ FMOD (wii & pc)
			+ XAudio2
		- Input
			+ KB/mouse
			+ XInput
	- low level renderables and audibles.
		+ MultiBuffered threaded renderer
		+ Frustum culler
		+ RenderQueue (with coarse depth and state sorting)
		+ Rigid/Skinned models with variable partitioning support
		+ Custom callback based renderables (soon lambda based)

		+ Animation controllers with masking and blending
		+ Fixed Function and Modular particle systems
		+ Spatialized audio effect system
		+ Audio stream playback controllers.
	- Notable dependencies: 
		+ OpenGL (win32/iX)
		+ QT5. (win32/iX)
		+ D3D9c (win32)
		+ FMod (win32/wii/iX)

---

* ork.bullet273 : bullet physics engine 

---

* ork.ent : archetype / entity / component / scene system. Lets you load a pregenerated 'scene' document and run it as a simulation. Contains a collection of commonly useful components, including:
	- Rigid/Skinned 3D model components / archetypes
	- Animation controllers component with animation blending support
	- Particle controller component and archetype
	- Bullet physics object component
	- Bullet sector-mesh collidable component (for use in antigrav racing type games. Includes interpolated/localized gravitational direction support. 
	- Racing-line component (for racer AI use)
	- Bullet "world" manager scene-component
	- Audio effect playback component with dataflow based modulation support
	- Audio stream playback component
	- Audio manager scene-component
	 
---

* ork.tool :
	- Scene / Archetype / Component / Object editor.
	- Asset conversion filters.
	- Distributed Lightmapper (WIP)

---

* tozkit 
	- Production rendering utilities.
 		+ Eventually one will be able to take a frame from the realtime renderer and re-render using a production renderer such as Renderman / Houdini mantra, etc...
 		+ Notable dependencies: 
 			* loki
 			* blitz
 			* hdf5
 			* OpenEXR
 			* OpenShaderLanguage
 			* OpenImageIO
 			* Alembic
 			* Cortex-Vfx
 			* 3Delight
 		+ All of the above dependencies are automagically downloaded and built/installed via included scripts

