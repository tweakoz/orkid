At one time or another Orkid has been ported to the PS2, NintendoDS, Wii, XBox360, Osx, Linux and Windows.

Unfortunately, with my job and family, I have had only the bandwidth to maintain the Linux and Osx builds.

If you want to minimize any pain, right now I would recommend Linux (Ubuntu 19.04 x86/64). The MacOsx build is only tested with Xcode11 on Catalina..

I do not currently test on Intel gfx chips. If you have an NVidia or AMD/ATI card that should be fine. I also recommend proprietary drivers over the open source ones. Open source is great and all, but I find the open source video drivers are still not up to par with their proprietary counterparts.
[UPDATE] - Since GL3 has been merged to master - Intel 4000 Graphics (Macbook Air 2011) on Osx 10.8 has been confirmed as working.

In general building will require a bunch of dependencies which are not included. There is a script included that automates the downloading, building and installation of these dependencies.

To bootstrap on MacOs BigSur (11.0)+
==================================
* install homebrew, and with install deps listed in ork.installdeps.ubuntu19.py
* ```git clone https://github.com/tweakoz/orkid```
* ```cd orkid```
* ```git submodule init```
* ```git submodule update```
* ```git lfs update```
* ```git lfs pull```
* ```./ork.build/bin/obt.osx.installdeps.py``` <- install system deps (requires homebrew already setup)
* ```./ork.build/bin/init_env.py --create .stage``` <- this creates a staging 'container' folder and launches an environment
* ```./build.py --ez``` <- builds deps and orkid (into staging folder)
* ```ork.asset.buildall.py``` <- builds assets (using built orkid executable)
* ```ork.test.buildtestassets.py``` <- build test assets (using built orkid executable)
* ```exit``` <- After an --ez build exit and reload the environment
* ```.stage/.launch_env```
* ```ork.example.lev2.gfx.minimal3D.exe``` <- run a c++ example
* ```./ork.lev2/examples/python/window.py``` <- run a python example

To bootstrap on Ubuntu19.10/Ubuntu20.04 x86/64
==================================
* ```sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 1``` <- set python3 as default
* ```sudo apt install python3-pip python3-yarl``` <- we need python3-pip and a few packages to bootstrap
* ```sudo apt install cmake wget curl git-lfs``` <- we need a few packages to bootstrap
* ```sudo apt install libreadline-dev libxcb-xfixes0-dev``` <- packages not yet added to installdeps script.
* ```git clone http://github.com/tweakoz/orkid```
* ```cd orkid```
* ```git submodule init```
* ```git submodule update```
* ```git lfs update```
* ```git lfs pull```
* ```./ork.build/bin/obt.ix.installdeps.ubuntu19.py``` <- install obt system deps (this will ask for sudo password)
* ```./ork.build/bin/init_env.py --create .stage``` <- this creates a staging folder and launches an environment
* ```ork.installdeps.ubuntu19.py``` <- install orkid system deps (this will ask for sudo password)
* ```.stage/.launch_env``` <- reload the environment (to get updated environment variables)
* ```ork,build.py``` <- build all deps, and orkid (will take a while)
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
* ```ork.doxygen.py``` <- regenerate doxygen docs

* to generate JSON dump of all top level actions (relative to the current staging folder state) that will be taken to perform an orkid build (which will also execute the actions). Actions which execute commandlines will include working folder and environment variable data. The output from this theoretically should be enough for an externally managed process to complete a build *entirely without OBT*. TODO - can we generate the trace without executing ?

```
ork.build.py --obttrace
```

This will generate a dump like the below much abbreviated snippet:

```
[
  {
    "topenv": {
      "ORKID_WORKSPACE_DIR": "${OWSDIR}",
      "OBT_PYTHON_DECOD_NAME": "python3.9d",
      "OBT_VULKAN_VERSION": "1.2.170.0",
      "OBT_NUM_CORES": "96",
      "PKG_CONFIG_PATH": "${OBT_STAGE}/qt5/lib/pkgconfig:${OBT_STAGE}/lib64/pkgconfig:${OBT_STAGE}/lib/pkgconfig:/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig",
      "PYTHONNOUSERSITE": "TRUE",
      "PREFIX": "${OBT_STAGE}",
      "LITEX_ROOT": "${OBT_STAGE}/builds/litex",
      "OBT_SEARCH_EXTLIST": ".cpp:.c:.cc:.h:.hpp:.inl:.qml:.m:.mm:.py:.txt:.md:.glfx:.ini",
      "OBT_PYLIB": "${OBT_STAGE}/python-3.9.4/lib",
      "PWD": "${TOPEXECDIR}",
      "LOGNAME": "${USER}",
      "OBT_PYTHON_LIB_PATH": "${OBT_STAGE}/python-3.9.4/lib",
      "OBT_PYPKG": "${OBT_STAGE}/pyvenv/lib64/python3.9/site-packages",
      "LITEX_BOARDS": "${OBT_STAGE}/builds/litex/litex-boards/litex_boards",
      "OBT_PYTHON_LIB_FILE": "libpython3.9d.so",
      "OBT_SEARCH_PATH": "${OWSDIR}/obt.project:${OWSDIR}/ork.dox:${OWSDIR}/ork.data:${OWSDIR}/ork.core:${OWSDIR}/ork.lev2:${OWSDIR}/ork.eda:${OWSDIR}/ork.ecs:${OWSDIR}/ork.tool",
      "USERNAME": "${USER}",
      "IM_CONFIG_PHASE": "1",
      "VULKAN_SDK": "${OBT_STAGE}/builds/vulkan/1.2.170.0/x86_64",
      "VIRTUAL_ENV": "${OBT_STAGE}/pyvenv",
      "OBT_PYTHON_LIB_NAME": "libpython3.9d",
      "OBT_BUILDS": "${OBT_STAGE}/builds",
      "OBT_STAGE": "${OBT_STAGE}",
      "OBT_DEP_PATH": "${OWSDIR}/ork.build/deps",
      "ISPC": "${OBT_STAGE}/bin/ispc",
      "PYTHONPATH": "${OWSDIR}/ork.build/scripts:${OBT_STAGE}/lib/python",
      "OBT_PYTHON_HEADER_PATH": "${OBT_STAGE}/python-3.9.4/include/python3.9d",
      "HFS": "/opt/hfs19.0",
      "SHLVL": "3",
      "OBT_VULKAN_ROOT": "${OBT_STAGE}/builds/vulkan/1.2.170.0/x86_64",
      "OBT_PYTHON_DECO_NAME": "python3.9",
      "LD_LIBRARY_PATH": "${OBT_STAGE}/builds/vulkan/1.2.170.0/x86_64/lib:${OBT_STAGE}/python-3.9.4/lib:${OBT_STAGE}/qt5/lib:${OBT_STAGE}/lib64:${OBT_STAGE}/lib:${OBT_STAGE}/pyvenv/lib64/python3.9/site-packages/PySide2:${OBT_STAGE}/pyvenv/lib64/python3.9/site-packages/shiboken2",
      "OBT_ROOT": "${OWSDIR}/ork.build",
      "PATH": "${OBT_STAGE}/pyvenv/bin:${OBT_STAGE}/qt5/bin:${OWSDIR}/obt.project/scripts/../bin:${OBT_STAGE}/bin:${OWSDIR}/ork.build/bin:${HOME}/.sdkman/candidates/sbt/current/bin:${HOME}/.sdkman/candidates/java/current/bin:${HOME}/.cargo/bin:${HOME}/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/opt/Xilinx/Vivado/bin:${OBT_STAGE}/opt/toolchain/m68k-amiga/bin:/opt/Xilinx/Vivado/2020.1/bin:${OBT_STAGE}/opt/toolchain/aarch64-elf/bin:${OBT_STAGE}/builds/litex/litex-boards/litex_boards/targets:${OBT_STAGE}/builds/litex/riscv64-unknown-elf-gcc-8.3.0-2019.08.0-x86_64-linux-ubuntu14/bin:${OBT_STAGE}/builds/vulkan/1.2.170.0/x86_64/bin:/opt/hfs19.0/bin",
      "LUA_PATH": "${OWSDIR}/ork.data/src/scripts/?.lua",
      "OLDPWD": "${OWSDIR}",
      "OBT_PYTHON_PYLIB_PATH": "${OBT_STAGE}/python-3.9.4/lib/python3.9",
      "PKG_CONFIG": "${OBT_STAGE}/bin/pkg-config",
      "_": "${OWSDIR}/obt.project/scripts/../bin/ork.build.py"
    }
  },
  {
    "sysargv": [
      "${OWSDIR}/obt.project/scripts/../bin/ork.build.py",
      "--obttrace"
    ]
  },
  {
    "op": "ork.build.py",
    "subops": [
      {
        "op": "path.chdir(${OBT_STAGE}/orkid)"
      },
      {
        "op": "dep.require(['vulkan', 'openvr', 'rtmidi', 'glm', 'eigen', 'lexertl14', 'parsertl14', 'rapidjson', 'luajit', 'pybind11', 'ispctexc', 'openexr', 'oiio', 'openvdb', 'embree', 'igl', 'glfw', 'assimp', 'easyprof', 'bullet'])",
        "subops": [
          {
            "op": "Provider.provide(vulkan)",
            "subops": [
              {
                "op": "command(cmd.exec)",
                "curwd": "${OBT_STAGE}/orkid",
                "arglist": [
                  "wget",
                  "-O",
                  "${OBT_STAGE}/downloads/vulkansdk-linux-x86_64-1.2.170.0.tar.gz",
                  "https://sdk.lunarg.com/sdk/download/1.2.170.0/linux/vulkansdk-linux-x86_64-1.2.170.0.tar.gz"
                ],
                "os_env": {
                  "PYTHONHOME": "${OBT_STAGE}/python-3.9.4"
                },
                "use_shell": "False"
              }
            ]
          }
        ]
      }
    ]
  }
]
```
