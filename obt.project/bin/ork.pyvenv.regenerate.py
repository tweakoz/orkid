#!/usr/bin/env python3

from obt import command, path, dep, host

PYTHON = dep.instance("python")
PYBUILDDIR = PYTHON.build_dest/f"Python-{PYTHON.version}"/".build"

###################
# remove old pyvenv
#  since the purpose of this script is to regenerate it cleanly
####################

command.system(["rm", "-rf", path.stage()/"pyvenv"],do_log=True)

###################
# reinstall python
#   luckily its already built
#   presumably we did not change the python implementation
#   so we just need to re-install it
#   it has already been configured to install into the pyvenv
####################

command.system(["make", "install"], working_dir=PYBUILDDIR,do_log=True)

######################
# rebuild orkid
#  against new copy of python
#######################

command.system(["rm", "-rf", path.builds()/"orkid"/".build"],do_log=True)
command.system(["ork.build.py"],do_log=True)

######################
# MacOs: since we built some deps
#        for good measure, fixup the dylib installname / rpath's
######################

if host.IsOsx:
  command.system(["obt.osx.macho.fixup.libs.py", "--all"],do_log=True)
  command.system(["obt.osx.macho.fixup.libs.py", "--all"],do_log=True)
