
![logo](https://github.com/tweakoz/orkid/blob/develop/ork.data/dox/doxylogo.png "OrkidLogo")


### Build Status

![Build Status(Macos)](https://github.com/tweakoz/orkid/actions/workflows/build_macos.yml/badge.svg)

![Build Status(Linux)](https://github.com/tweakoz/orkid/actions/workflows/build_linux.yml/badge.svg)

### Description

Orkid is a C++23 flexible media presentation engine. By media, we mean games, realtime 2d/3d graphics, and/or audio. Orkid itself is licensed with the permissive MIT license, that said Orkid does have dependencies on other libraries which will have different licenses and it is left up to the user to remain compliant with them. Orkid, being fairly modular can be run with less dependencies and reduced functionality to simplify license creep.

### History

Early Orkid was used for a a few games by Santa Cruz Games. It was used as both a game engine and tooling infrastructure.
 * [Godzilla Unleashed for Nintendo DS (2007)](https://www.youtube.com/watch?v=832QifjseQ4)
 * [Tomb Raider Underworld for Nintendo DS (2008)](https://www.youtube.com/watch?v=PP5SJN-HIFE)
 * [Igor The Game for the Wii and PC (2008)](https://www.youtube.com/watch?v=lEsus2UA9lg)
 * [Spongebob Gravjet Racing for Wii/Pc/XBox360 (2009)](https://www.youtube.com/watch?v=8X90xEPwyVE)
 
 
Software Development Information
========
Installation via PyPi

```pip3 install orkid``` # install it (macos seqoia on apple silicon or linux-x86_64)

```ork.shell --command ork.data.fetch.py``` # run a command in it (without polluting base shell)

```ork.shell --command ork.cache.prime.py``` # run a command in it (without polluting base shell)

```ork.shell``` # interactive orkid shell

```ork.shell --dev``` # interactive orkid development shell


[How to build](ork.dox/building.md)

[GLSLFX shader format docs](ork.dox/gfx/glslfx.md)

[EngineLibraryTopology](ork.dox/engine_layout.md)
   * [Ork.Core.Reflection](ork.dox/core/reflection.md)
   * [Ork.Core.Datablocks](ork.dox/core/datablocks.md)
   * [Ork.Core.AssetCatalog](ork.dox/core/asset_catalog_tdd.md)
   * [Ork.Core.ThreadingPolicy](ork.dox/core/threading-policy.md)
   * [Ork.Core.TaskGraph](ork.dox/core/taskgraph_tdd.md)
   * [Ork.Core.Parser](ork.dox/core/parser.md)
   * [Ork.Core.Logging](ork.dox/core/logging.md)
   * [Ork.Lev2.Graphics](ork.dox/gfx/lev2-graphics.md)
   * [Ork.Lev2.Graphics.GlFx](ork.dox/gfx/glslfx.md)
   * [Ork.Lev2.Audio](ork.dox/aud/singularity.md)
   * [Ork.ECS](ork.dox/ecs/ecs.md)

[Doxygen - code documentation](https://www.orkid-engine.dev:4430/doxygen_html/index.html)

Artist Information
========

[Blender pipeline notes](ork.dox/gfx/blender.md)

Example Content (most of it old, updates coming soon.)
========

<a href="https://media.githubusercontent.com/media/tweakoz/orkid/develop/ork.data/misc/screenshot_pbr.png"><img src="https://github.com/tweakoz/orkid/blob/develop/ork.data/misc/th_screenshot_pbr.png" title="Github Logo"></a>
<a href="https://media.githubusercontent.com/media/tweakoz/orkid/develop/ork.data/misc/shadowedptexspotlight.png"><img src="https://github.com/tweakoz/orkid/blob/develop/ork.data/misc/th_shadowedptexspotlight.png" title="Github Logo"></a>

![Particles](http://tweakoz.com/resources/images/th_sshot_psys.jpg)

![ProceduralTextures](http://tweakoz.com/resources/images/th_sshot_proctex.jpg)

![TerrainGen](http://tweakoz.com/resources/images/th_terrain03.jpg)

Other (video) examples of content:

[Python FPS](https://www.youtube.com/watch?v=v56eGoGkSCI)

[OpenVDB Integration 1](https://www.youtube.com/watch?v=cbLzJVn6V0U)

[OpenVDB Integration : Sculpting](https://www.youtube.com/watch?v=6YPNi88nvTI)

[SSAO (wip)](https://www.youtube.com/watch?v=K0LPqbTjxK4)

[Python poser app](https://www.youtube.com/watch?v=-lBU_JCkZLI)

[PBR ShaderBalls](https://www.youtube.com/watch?v=LtlVotV_9vg_)

[SceneGraph-Picking](https://youtu.be/d39JF4ApsVw)

[Deferred-PBR-SpotLightProjectors-1](https://www.youtube.com/watch?v=AwhDNZPhcBk)

[Deferred-PBR-PointLightProjectors-1](https://www.youtube.com/watch?v=ffEbkF9l4yw)

[Deferred-PBR-PointLightProjectors-2](https://www.youtube.com/watch?v=Xem37Nfp-d8)

[MoonDiver](https://youtu.be/2zNd4k_6I6s)

[Compositor](https://www.youtube.com/watch?v=zLBhF8WCDgQ)

[NodeCompositor](https://www.youtube.com/watch?v=AGMazbsbJYE)

[Particles-Vortex&TurbulenceNodes](https://youtu.be/Qr8QK6ns0Tk)

[Paritcles-SphereColliderNode](https://www.youtube.com/watch?v=yb9a6k4VeaU)

[Particles-MultiSystem](https://www.youtube.com/watch?v=o7uJFNQc3Go)

[Particles-ChainOfSystems](https://youtu.be/sQTiz0Ooo6I)

[ParticlesAndCompositor](https://www.youtube.com/watch?v=qmULG3ZOoS0)

[ProceduralTexture](https://www.youtube.com/watch?v=FdAfxQjR3AQ)

[Prodigy2-GameDemo](http://tweakoz.com/resources/videos/p2d.mp4)

[OrkidVR-POC](https://www.youtube.com/watch?v=6tOPVw8T_sU)

[Singularity-Live Sequencer](https://www.youtube.com/watch?v=5fouh_9CwZE)

[Singularity-Soundfield Processing](https://www.youtube.com/watch?v=vxivKOwVijI)

[Singularity-NewAudioSynth](https://www.youtube.com/watch?v=irBaba13quQ)

[Singularity-ModulatorHUD](https://www.youtube.com/watch?v=1PEXp9-6eRA)

