import os
import ork.deco
import ork.env
import ork.path

def setup():
  deco = ork.deco.Deco()
  file_path = os.path.realpath(__file__)
  scripts_dir = ork.path.Path(os.path.dirname(file_path))
  obtprj_dir = scripts_dir/".."
  orkid_dir = (obtprj_dir/"..").resolve()
  orkbin_dir = obtprj_dir/"bin"
  ORK_PROJECT_NAME = "Orkid"
  assert(orkid_dir.exists())
  ork.env.set("ORKID_WORKSPACE_DIR",orkid_dir)
  ork.env.prepend("PATH",orkbin_dir)

  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.core")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.lev2")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.eda")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.ent")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.tool")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.data"/"platform_lev2"/"shaders"/"glfx")
  ork.env.append("LUA_PATH",orkid_dir/"ork.data"/"src"/"scripts"/"?.lua")
  print(deco.yellow("Initialized Orkid Build Enviroment"))

