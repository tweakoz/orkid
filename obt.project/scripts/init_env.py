import os
import ork.deco
import ork.env
import ork.path

def setup():
  file_path = os.path.realpath(__file__)
  scripts_dir = ork.path.Path(os.path.dirname(file_path))
  obtprj_dir = scripts_dir/".."
  orkid_dir = (obtprj_dir/"..").resolve()
  orkbin_dir = obtprj_dir/"bin"
  print(orkid_dir)
  ORK_PROJECT_NAME = "Orkid"
  assert(orkid_dir.exists())
  ork.env.set("ORKID_WORKSPACE_DIR",orkid_dir)
  ork.env.prepend("PATH",orkbin_dir)
