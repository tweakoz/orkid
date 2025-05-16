#!/usr/bin/env python3

from obt import command, path, dep, host

PYTHON = dep.instance("python")
PYBUILDDIR = PYTHON.build_dest/f"Python-{PYTHON.version}"/".build"
PYSITDIR = PYTHON.site_packages_dir/'torch'

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

###################
# reinstall pytorch
####################

command.system(["obt.dep.build.py", "pytorch", "--force", "--wipe"],do_log=True)

####################
# recopy pytorch libs into $stage/lib/
#  so other libs in lib/ can find them
# todo: this might only be needed for macOS (think its rpath related)
######################

command.system(["cp", "-r", f"{PYSITDIR/'lib'}/*", f"{path.libs()}/"],do_log=True)

######################
# MacOs: libc10 requires codesigning on macOS
#         and the previous codesign invocation gets out of date 
#         when reinstalling pytorch
######################

if host.IsOsx:
  command.system(["codesign", "--force", "--deep", "--sign", "-", f"{path.libs()/'libc10.dylib'}"],do_log=True)

######################
# rebuild orkid
#  against new copy of python
#  and new copy of pytorch
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
