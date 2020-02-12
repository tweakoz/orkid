#Orkid Library Topology

---

Orkid is divided into several shared libraries:

---
* ork.core (python3 module: orkcore)
	- Reflection kernel
	- Performance minded containers
	- Math (LA/trig/audio/etc)
	- File IO
	- Threading/concurrency
	- Asset managers
	- General OS utilities.
	- Notable dependencies: 
		+ Boost
		+ Python3/PyBind11

---

* ork.lev2 :  (python3 module: orklev2)
	- Platform a/v/input drivers.
		- Graphics
			+ OpenGL
		- Audio
			+ PortAudio
		- Input
			+ KB/mouse
			+ OpenVR
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
	- Notable dependencies
	        + Ork.Core
		+ OpenGL (osx/iX)
		+ QT5. (osx/iX)
		+ PortAudio (osx/iX)
		+ OpenVR (iX)
		+ Python3/PyBind11
		+ Vulkan/MoltenVK
---

* ork.ent : archetype / entity / component / scene system. Lets you load a pregenerated 'scene' document and run it as a simulation. Contains a collection of commonly useful components, including:
	- Fixed Function and Node based frame compositor
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
	- Notable dependencies:
	    + Ork.Core,Ork.Lev2
	    + BulletPhysics
	 
---

* ork.tool :
	- Scene / Archetype / Component / Object editor.
	- Mesh Processing
	- Asset conversion filters.
	- Distributed Lightmapper (WIP)
	- Notable dependencies:
	    + Ork.Core,Ork.Lev2, Ork.Ent

