Currently you must be in an orkid shell environment to run any orkid commands..

To enter shell environment
==========================
cd <orkid_root>
make env

To run on Ubuntu19.04 LTS x86/64
======
* run ```ork.tool.test.ix.release``` (from the repo root folder). It is in your path already, so just type ork.[tab tab] and see which orkid executables are present.
* directly load a scene from the commandline with a command like this:
```ork.tool.test.osx.release -edit ork.data/src/example_scenes/particles/vortex1.mox```

General Keys
=============
```
  (replace ctrl with cmd on mac)
ctrl-. : play (since it kinda looks like a play button)
ctrl-, : stop
`      : cycle through cameras
/      : toggle editor/HUD on or off
ctrl-/ : toggle pickbuffer debugger
space  : toggle compositor on or off
zxc    : left,middle and right mouse button emulation for those with 1 button trackpads
```

Outliner Keys
=============
```
ctrl-n : new scene
ctrl-e : new entity
g      : toggle display of scene globals
s      : toggle display of systems
e      : toggle display of entities
a      : toggle display of archetypes (and again for child component data's)
```
