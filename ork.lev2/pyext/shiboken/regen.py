#!/usr/bin/env python3

import os
from ork import command, log, path

log.marker( "Generating lev2 shiboken bindings")

this_dir = os.path.dirname(os.path.realpath(__file__))
orkid_dir = path.Path(os.environ["ORKID_WORKSPACE_DIR"])
qt_dir = path.Path(os.environ["QTDIR"])
os.chdir(this_dir)

# todo - autodetect and run only when changes occured

command.run([
  "shiboken2",
  #"--silent",
  "--output-directory=%s"%(path.stage()/"orkid"/"shiboken-bindings"/"lev2-qtui"),
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
])
