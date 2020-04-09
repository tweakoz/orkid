At one time or another Orkid has been ported to the PS2, NintendoDS, Wii, XBox360, Osx, Linux and Windows.

Unfortunately, with my job and family, I have had only the bandwidth to maintain the Linux and Osx builds.

If you want to minimize any pain, right now I would recommend Linux (Ubuntu 19.04 x86/64). The MacOsx build is only tested with Xcode11 on Catalina..

I do not currently test on Intel gfx chips. If you have an NVidia or AMD/ATI card that should be fine. I also recommend proprietary drivers over the open source ones. Open source is great and all, but I find the open source video drivers are still not up to par with their proprietary counterparts.
[UPDATE] - Since GL3 has been merged to master - Intel 4000 Graphics (Macbook Air 2011) on Osx 10.8 has been confirmed as working.

In general building will require a bunch of dependencies which are not included. There is a script included that automates the downloading, building and installation of these dependencies.

To build on Osx Catalina (10.15)+
==================================
* install homebrew, and with install deps listed in ork.installdeps.ubuntu19.py
* ```git clone https://github.com/tweakoz/orkid```
* ```cd orkid```
* ```git submodule init```
* ```git submodule update```
* ```obt.osx.installdeps.py``` <- install system deps (requires homebrew already setup)
* ```./ork.build/bin/init_env.py --create .stage``` <- this creates a staging 'container' folder and launches an environment
* ```./build.py --ez``` <- builds deps and orkid (into staging folder)
* ```ork.asset.buildall.py``` <- builds assets (using built orkid executable)
* ```ork.test.buildtestassets.py``` <- build test assets (using built orkid executable)
* ```exit``` <- After an --ez build exit and reload the environment
* ```.stage/.launch_env```
* ```ork.example.lev2.gfx.minimal3D.exe``` <- run a c++ example
* ```./ork.lev2/examples/python/window.py``` <- run a python example

To build on Ubuntu19.10/Ubuntu20.04 x86/64
==================================
* ```update-alternatives --install /usr/bin/python python /usr/bin/python3 1``` <- set python3 as default
* ```sudo apt install python3-pip python3-yarl``` <- we need python3-pip and a few packages to bootstrap
* ```sudo apt install cmake wget curl git-lfs``` <- we need a few packages to bootstrap
* ```sudo apt install libreadline-dev libxcb-xfixes0-dev``` <- packages not yet added to installdeps script.
* ```git clone http://github.com/tweakoz/orkid```
* ```cd orkid```
* ```git submodule init```
* ```git submodule update```
* ```./ork.build/bin/obt.ix.installdeps.ubuntu19.py``` <- install obt system deps
* ```./ork.build/bin/init_env.py --create .stage``` <- this creates a staging folder and launches an environment
* ```ork.installdeps.ubuntu19.py``` <- install orkid system deps
* ```./build.py --ez``` <- builds deps and orkid (into staging folder)
* ```ork.asset.buildall.py``` <- builds assets (using built orkid executable)
* ```ork.test.buildtestassets.py``` <- build test assets (using built orkid executable)
* ```exit``` <- After an --ez build exit environment
* ```.stage/.launch_env``` <- reload the environment (to get updated environment variables)
* ```ork.example.lev2.gfx.minimal3D.exe``` <- run a c++ example
* ```./ork.lev2/examples/python/window.py``` <- run a python example

everything will be built and installed into the staging folder.
the ```<stage>/bin``` and ```<stage>/lib``` paths were added to your environment variables already when you launched the environment'.

Build issues, notes for later fixes
==================================
There are a few bugs in the build process from a new working copy.
* ```qt5 environment not initialized properly before qt5 built. just exit the environment session and re-enter it after qt5 built.```
* ```ork.tuio not installed properly. This is an issue in the ork.tuio/CMakeLists.txt - to fix:```
     ```cd <staging>/orkid/ork.tuio; make install .```
     ```more specifically - ./build.py does do a multi-project install. Apparently in nested cmake projects, installs are deferred until all subprojects are built, as opposed to when the individual subprojects are finished building. The probably fix for this is to just make ork.tuio an external dependency```
     
misc
=====
* ```<staging_folder>/.launch_env``` <- relaunch previously made environment container.
* ```obt.find.py "phrase"``` - search source folders for a quoted phrase - the quotes are optional for simple single word seaches
* ```obt.replace.py "findphrase" "replace"``` - search source folders for a quoted phrase - the quotes are optional for simple single word seaches
* ```ork.asset.buildall.py``` <- rebuild all assets
* ```ork.doxygen.py``` <- regenerate doxygen docs


the automatic asset pipe is in flux. more on this later...
