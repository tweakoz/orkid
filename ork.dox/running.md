Currently you must be in an orkid shell environment to run any orkid commands..

To enter shell environment
==========================
cd <orkid_root>
make env

To run on Ubuntu19.04 LTS x86/64
======
* run ```ork.tool.release``` (from the repo root folder). It is in your path already, so just type ork.[tab tab] and see which orkid executables are present.
* directly load a scene from the commandline with a command like this:
```ork.tool.release -edit ork.data/src/example_scenes/particles/vortex1.mox```

Main Viewport Keys
=============
```
  (replace ctrl with cmd on mac)
ctrl-n : new scene
ctrl-o : load scene
ctrl-s : save scene
ctrl-q : quit
ctrl-e : new entity (at spawnpoint)
ctrl-f : toggle fullscreen
ctrl-. : play (since it kinda looks like a play button)
ctrl-, : stop
`      : cycle through cameras
/      : toggle editor/HUD on or off
ctrl-/ : toggle pickbuffer debugger
space  : toggle compositor on or off (only on when playing)
zxc    : left,middle and right mouse button emulation for those with 1 button trackpads
wasd   : left dpad emulation (only when playing)
cursors: right dpad emulation (only when playing)
```
editor camera navigation:
=========================
```
alt-left-mouse-drag : rotate-xy editor camera (on outer circle of screen: rotate-z) 
z-trackpad-move     : rotate-xy editor camera

alt-middle-mouse-drag : pan editor camera
x-trackpad-move       : pan editor camera

mousewheel            : zoom editor camera (best when wheel has intertia, like a mac trackpad)
shift-mousewheel      :  .. but faster

double-click          : set spawnpoint on mouseray/surface intersection
                         center camera on spawnpoint

move   : rotate editor camera 
```

Outliner Keys
=============
```
s      : toggle display of systems
e      : toggle display of entities
g      : toggle display of scene globals
a      : toggle display of archetypes (and again for child component data's)
!      : switch between day and night ui themes
```
