create an ssh key on development computer
associate ssh key with your github account. Build automation will assume you can clone via ssh.
If you chose you choose to have an ssh password, maybe set up an ssh authentication agent, otherwise you will have to babysit and continually type passwords during builds.
make sure email and username set for github

# INITIAL SYSTEM-WIDE SETUP (Macos)

* update to sequoia 15.4.3 (or whatever is latest)
* update to xcode 16.3 (or whatever is latest)
* remove homebrew ```sudo rm -rf /opt/homebrew```
* reinstall homebrew:
    ```/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"```
* if you dont want to delete and reinstall homebrew, you probably should at least update it.
* ```brew install python3```
* probably want to use bash instead of zsh
* prepend /opt/homebrew/bin to $PATH - so homebrew's python takes precedence (Apple tends to ship old Pythons).
* ```xcodebuild -runFirstLaunch``` # agree to Eula I think...

# INITIAL SYSTEM-WIDE SETUP (Ubuntu 24.04/x64)

* update to latest Ubuntu 24.04
* probably want to use bash instead of zsh
* ```sudo apt install python3-venv, python3-pip```
  
# VENV SETUP 

* ensure base python environment is clean, any packages installed systemwide or userwide
    may interfere with venv's that inherit. pep-668. If you rely on a customized python environment
    for other work, that is fine - just quarantine it into it's own launch script. OBT/Orkid will itself
    be quarantined and should not interfere.
* ensure using python3.13 (from homebrew)
* ```python3 -m venv ~/.venv```
* ```source ~/.venv/bin/activate```

# OBT SETUP

* ```pip3 install ork.build``` # installs OBT into venv
* ```obt.versions.py``` : ensure obt version 0.0.303
* ***(MacOs)*** ```obt.osx.installdeps.py``` # installs homebrew scoped deps
or
* ***(Ubuntu 24.04/x64)*** ```obt.ix.installdeps.ubuntu_x86_64.py``` #  installs apt scoped deps

# STAGING ENV / FOLDER SETUP

* ```obt.env.create.py --stagedir ~/.staging-xxx --wipe --pipeline``` # wipe means remove old staging folder, if exists
* ```~/.staging-xxx/obt-launch-env``` # launch OBT shell / environment, type exit if you wish to leave

# BUILD ORKID

* ```obt.dep.pipeline.py orkid``` # download and (attempt to) build orkid (builds orkid's deps first)
* ```ork.build.py``` # incremental build (only works after orkid was already built and environment relaunched - as $PATHS must be updated)

* Building with profiling: uncomment #define BUILD_WITH_EASY_PROFILER in profiling.inl

# Test ORKID

* ```ork.cache.prime.py``` # preload asset cache
* ```ork.app.testrunner.py``` # run orkid's visual test suite
* standard camera controls - mac trackpad (or mouse): 

   Maya inspired arrangement - with the Z,X,C keys standing in as L,M,R mouse buttons.

   hold z key + move trackpad: rotate camera

   hold x key + move trackpad: pan camera

   hold c key + move trackpad: dolly camera

   2 finger scroll: zoom camera

   Command-Q to quit

# Debug ORKID

* debug an example (in c++ debugger) 
```ork.debug.xcode.py ./ork.lev2/pyext/tests/renderer/lighting/probe.py```
* if profiling with instruments, just attach to python process

```
