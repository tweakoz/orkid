###############################################################################
# Orkid Media Engine
# Copyright 2010-2022, Michael T. Mayers
# email: michael@tweakoz.com
###############################################################################
# orkid as project initialization

import os
import ork.deco
import ork.env
import ork.path

def setup():

  ##############################################
  # compute paths
  ##############################################

  deco = ork.deco.Deco()
  file_path = os.path.realpath(__file__)
  scripts_dir = ork.path.Path(os.path.dirname(file_path))
  obtprj_dir = scripts_dir/".."
  orkid_dir = (obtprj_dir/"..").resolve()
  orkbin_dir = obtprj_dir/"bin"
  ORK_PROJECT_NAME = "Orkid"
  assert(orkid_dir.exists())

  ##############################################
  # mark ORKID as project
  ##############################################

  ork.env.set("ORKID_WORKSPACE_DIR",orkid_dir)
  ork.env.set("ORKID_IS_MAIN_PROJECT","1")

  ##############################################
  # add orkid scripts to enviromment PATH
  ##############################################

  ork.env.prepend("PATH",orkbin_dir)

  ##############################################
  # add search paths
  #  for obt.find.py
  ##############################################

  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"obt.project")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.dox")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.data")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.core")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.lev2")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.eda")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.ecs")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.tool")
  ork.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.tuio")
  ork.env.append("LUA_PATH",orkid_dir/"ork.data"/"src"/"scripts"/"?.lua")

  ##############################################
  # add search extensions
  #  for obt.find.py
  ##############################################

  ork.env.append("OBT_SEARCH_EXTLIST", ".cpp:.c:.cc:.h:.hpp:.inl")
  ork.env.append("OBT_SEARCH_EXTLIST", ".qml:.m:.mm:.py")
  ork.env.append("OBT_SEARCH_EXTLIST", ".txt:.md:.glfx:.ini")

  ##############################################

  print(deco.yellow("Initialized Orkid Build Enviroment"))

