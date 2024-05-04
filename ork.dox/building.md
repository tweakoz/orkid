At one time or another Orkid has been ported to the PS2, NintendoDS, Wii, XBox360, MacOs, Linux and Windows.

Unfortunately, with my job and family, I have had only the bandwidth to maintain the Linux and MacOs builds.

If you want to minimize any pain, right now I would recommend Linux (Ubuntu 22.04 x86/64). The MacOs build is only tested with Xcode on Ventura (Both Intel And Apple Silicon)..



In general building will require a bunch of dependencies which are not included. There is a script included that automates the downloading, building and installation of these dependencies.

To bootstrap on MacOs Sonoma (14.0)+
==================================
* install / update [homebrew](http://brew.sh)
* install / update XCode via AppStore.
* Install Ork Build Tools (OBT)
  * see [OBT installation docs](https://github.com/tweakoz/ork.build/blob/develop/README.md)
* If you have not done so already, create an ork.build (OBT) *staging* environment/container.
  * ```obt.create.env.py --stagedir ~/.staging-xxx --wipe```
  * This can take a bit, it will be building a container scoped python and a few other deps which are required for consistent OBT operation.
* Launch the staging environment
  * ```~/.staging-xxx/.launch_env```
  * ~/.staging-xxx/bin will now be in your $PATH
* Check which deps will be built for orkid
  * ```obt.dep.info.py orkid```
* Build orkid (clean)
  * ```obt.dep.build.py orkid --force --wipe```
* Build orkid (incremental)
  * ```obt.dep.build.py orkid --incremental```
* Pre-convert some assets
  * ```ork.asset.process.py```
* Run a c++ example
  * ```ork.example.lev2.gfx.minimal3D.exe```
* Run a python example
  * ```${ORKID_LEV2_EXAMPLES_DIR}/python/scenegraph/minimal.py```
* The Orkid Source Tree will be at ${ORKID_WORKSPACE_DIR}
* Create an XCode Project if that is more your style (or just for easier debugging).
  * first build via the standard commandline method, so that all deps are built.
  * ${ORKID_WORKSPACE_DIR}/obt.project/bin/ork.build.py --xcode
  * ```open ${ORKID_WORKSPACE_DIR}/.build/orkid.xcworkspace```
* Leave The OBT environment
  * ```exit``` 


To bootstrap on Ubuntu22.04 x86/64
==================================
* update your machine
  * ```sudo apt update```
  * ```sudo apt dist-upgrade```
* set python3 as default
  * ```sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 1``` 
* Install Ork Build Tools (OBT)
  * see [OBT installation docs](https://github.com/tweakoz/ork.build/blob/develop/README.md)
* Create an ork.build (OBT) *staging* environment/container.
  * ```obt.create.env.py --stagedir ~/.staging-xxx --wipe```
  * This can take a bit, it will be building a container scoped python and a few other deps which are required for consistent OBT operation.
* Launch the staging environment
  * ```~/.staging-xxx/.launch_env```
  * ~/.staging-xxx/bin will now be in your $PATH
  * ~/.staging-xxx/lib will now be in your $LD_LIBRARY_PATH
* Check which deps will be built for orkid
  * ```obt.dep.info.py orkid```
* Build orkid (clean)
  * ```obt.dep.build.py orkid --force --wipe```
* Build orkid (incremental)
  * ```obt.dep.build.py orkid --incremental```
* Pre-convert some assets
  * ```ork.asset.process.py```
* Run a c++ example
  * ```ork.example.lev2.gfx.minimal3D.exe```
* Run a python example
  * ```${ORKID_LEV2_EXAMPLES_DIR}/python/scenegraph/minimal.py``` 
* The Orkid Source Tree will be at ${ORKID_WORKSPACE_DIR}
* Leave The OBT environment/shell (This will returrn you to your parent shell)
  * ```exit``` 
     
Miscellaneous OBT Commands
==================================
* relaunch a previously created environment container
  * ```~/.staging-xxx/.launch_env```
* list dependencies known by OBT
  * ```obt.dep.list.py```
* list info about a specific dependency
  * ```obt.dep.info.py <depname>```
* list build status of a specific dependency
  * ```obt.dep.status.py <depname>```
* search source folders for a quoted phrase - the quotes are optional for simple single word seaches
  * ```obt.dep.find.py --dep <depname> "phrase"```
* search source folders for a quoted phrase - the quotes are optional for simple single word seaches
  * ```obt.find.py "phrase"``` - 
* search source folders for a quoted phrase - the quotes are optional for simple single word seaches
  * ```obt.replace.py "findphrase" "replace"```
* regenerate doxygen docs
  * ```ork.doxygen.py```

Build Debugging
==================================
* to generate JSON dump of all top level actions (relative to the current staging folder state) that will be taken to perform an orkid build (which will also execute the actions). Actions which execute commandlines will include working folder and environment variable data. The output from this theoretically should be enough for an externally managed process to complete a build *entirely without OBT*. TODO - can we generate the trace without executing ?
  * ```ork.build.py --obttrace```

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
