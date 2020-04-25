#!/usr/bin/env python3

import os
from ork import command, log, path

log.marker( "Generating lev2 shiboken bindings")

this_dir = os.path.dirname(os.path.realpath(__file__))
orkid_dir = path.Path(os.environ["ORKID_WORKSPACE_DIR"])
qt_dir = path.Path(os.environ["QTDIR"])
os.chdir(this_dir)

# todo - autodetect and run only when changes occured

env = {
  "CLANG_INSTALL_DIR": "/usr/include/clang/10"
}

output_dir = path.stage()/"orkid"/"shiboken-bindings"/"lev2-qtui"
os.system("rm -rf %s"%output_dir)

command.run([
  "shiboken2",
  #"--silent",
  "--output-directory=%s"%output_dir,
  "-I%s"%this_dir,
  "-I%s"%(qt_dir/"include"),
  "-I%s"%(orkid_dir/"ork.core"/"inc"),
  "-I%s"%(orkid_dir/"ork.lev2"/"inc"),
  "--enable-pyside-extensions",
  "--avoid-protected-hack",
  "--enable-parent-ctor-heuristic",
  "--use-isnull-as-nb_nonzero",
  "--enable-return-value-heuristic",
  "--generator-set",
  "--language-level=c++17",
  "test.h",
  "test.xml"
], env)


#(shiboken) clang_parseTranslationUnit2(0x0, cmd[12]=-nostdinc -isystem/usr/bin/../lib/gcc/x86_64-linux-gnu/9/../../../../include/c++/9 -isystem/usr/bin/../lib/gcc/x86_64-linux-gnu/9/../../../../include/x86_64-linux-gnu/c++/9 -isystem/usr/bin/../lib/gcc/x86_64-linux-gnu/9/../../../../include/c++/9/backward -isystem/usr/local/include -isystem/usr/lib/llvm-10/lib/clang/10.0.0/include -isystem/usr/include/x86_64-linux-gnu -isystem/usr/include -fPIC -Wno-constant-logical-operand -std=c++14 /tmp/nothing_oFglAg.hpp)
