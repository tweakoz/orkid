###############################################################################
# Orkid Media Engine
# Copyright 2010-2022, Michael T. Mayers
# email: michael@tweakoz.com
###############################################################################
# orkid as project initialization

import os
import obt.deco
import obt.env
import obt.path

def setup():

  this_dir = obt.path.directoryOfInvokingModule(__file__)

  ##############################################
  # compute paths
  ##############################################

  deco = obt.deco.Deco()
  file_path = os.path.realpath(__file__)
  scripts_dir = obt.path.Path(os.path.dirname(file_path))
  obtprj_dir = scripts_dir/".."
  orkid_dir = (obtprj_dir/"..").resolve()
  orkbin_dir = obtprj_dir/"bin"
  ORK_PROJECT_NAME = "Orkid"
  assert(orkid_dir.exists())

  ##############################################
  # mark ORKID as project
  ##############################################

  obt.env.set("ORKID_WORKSPACE_DIR",orkid_dir)
  obt.env.set("ORKID_IS_MAIN_PROJECT","1")

  ##############################################
  # add orkid scripts to enviromment PATH
  ##############################################

  obt.env.prepend("PATH",orkbin_dir)

  obt.env.append("PYTHONPATH",this_dir)

  ##############################################
  # add search paths
  #  for obt.find.py
  ##############################################

  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"obt.project")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.dox")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.data")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.core")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.lev2")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.eda")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.ecs")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.tool")
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.tuio")
  obt.env.append("LUA_PATH",orkid_dir/"ork.data"/"src"/"scripts"/"?.lua")

  ##############################################
  # add search extensions
  #  for obt.find.py
  ##############################################

  obt.env.append("OBT_SEARCH_EXTLIST", ".cpp:.c:.cc:.h:.hpp:.inl")
  obt.env.append("OBT_SEARCH_EXTLIST", ".qml:.m:.mm:.py")
  obt.env.append("OBT_SEARCH_EXTLIST", ".txt:.md:.glfx:.ini")

  ##############################################

def extend_bashrc():
  return ["ork.goto.orkid() { cd ${ORKID_WORKSPACE_DIR}; };\n"] \
       + ["ork.goto.data_root() { cd ${ORKID_WORKSPACE_DIR}/ork.data; };\n"] \
       + ["ork.goto.data_src() { cd ${ORKID_WORKSPACE_DIR}/ork.data/src; };\n"] \
       + ["ork.goto.data_test() { cd ${ORKID_WORKSPACE_DIR}/ork.data/src; };\n"] \
       + ["ork.goto.data_lev2() { cd ${ORKID_WORKSPACE_DIR}/ork.data/platform_lev2; };\n"]
