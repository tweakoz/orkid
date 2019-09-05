At one time or another Orkid has been ported to the PS2, NintendoDS, Wii, XBox360, Osx, Linux and Windows.

Unfortunately, with my job and family, I have had only the bandwidth to maintain the Linux and Osx builds.

If you want to minimize any pain, right now I would recommend Linux (Ubuntu 19.04 x86/64). The MacOsx build is only tested with latest Xcode on Mojave..

I do not currently test on Intel gfx chips. If you have an NVidia or AMD/ATI card that should be fine. I also recommend proprietary drivers over the open source ones. Open source is great and all, but I find the open source video drivers are still not up to par with their proprietary counterparts.
[UPDATE] - Since GL3 has been merged to master - Intel 4000 Graphics (Macbook Air 2011) on Osx 10.8 has been confirmed as working.

In general building will require a bunch of dependencies which are not included. There is a script included that automates the downloading, building and installation of these dependencies.

To build on Osx Mojave (10.14)+
==================================
* install homebrew, and with install deps listed in ork.installdeps.ubuntu19.py
* clone it, cd into repo 
* make env (this will setup build environment on your local shell only. just "exit" to unset this environment)
* make prep (copy some deps to the stage folder)
* make (to build orkid itself)
* ork.asset.buildall.py

To build on Ubuntu19.04 x86/64
==================================
* clone it, cd into repo 
* make env (this will setup build environment on your local shell only. just "exit" to unset this environment)
* ork.installdeps.ubuntu19.py
* build and install openvr sdk from https://github.com/ValveSoftware/openvr
* make prep (copy some deps to the stage folder)
* make (to build orkid itself)
* ork.asset.buildall.py

everything will be built/installed into the <repo_root>/stage folder.
the stage/bin and stage/lib paths were added to your environment variables already when you did a 'make env'.

To run on Ubuntu19.04 LTS x86/64
======
* run ork.tool.test.ix.release (from the repo root folder). It is in your path already, so just type ork.[tab tab] and see which orkid executables are present.
* directly load a scene from the commandline with a command like this:
```ork.tool.test.osx.release -edit ork.data/src/example_scenes/particles/vortex1.mox```
* once loaded use ctrl-. to play (since it kinda looks like a play button)
*             use ctrl-, to stop
*             use ` for cycle through cameras
*             use / to toggle editor/HUD on or off
*             use space to toggle compositor on or off


misc
=====
ork.find.py "phrase" - search source folders for a quoted phrase - the quotes are optional for simple single word seaches

the automatic asset pipe is in flux. in the meantime typing "make assets" will build whatever assets are configured to convert for a given branch - see do_assets.py in the repo's root folder.



