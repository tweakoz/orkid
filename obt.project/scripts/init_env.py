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
import obt.host

OBT_DYLD_FALLBACK_LIBRARY_PATH = str(obt.path.libs())
DYLD_LIBRARY_PATH = str(obt.path.libs())
ENV_DYLD_LIBRARY_PATH = ""
ENV_OBT_DYLD_FALLBACK_LIBRARY_PATH = os.environ.get("OBT_DYLD_FALLBACK_LIBRARY_PATH", "")
ORKID_SETUP_VULKAN_FN = "export ORKID_GRAPHICS_API=VULKAN; "
ORKID_SETUP_VULKAN_FN += f"export DYLD_LIBRARY_PATH={ENV_DYLD_LIBRARY_PATH}{DYLD_LIBRARY_PATH}; "
ORKID_SETUP_VULKAN_FN += f"export DYLD_FALLBACK_LIBRARY_PATH={ENV_OBT_DYLD_FALLBACK_LIBRARY_PATH}{OBT_DYLD_FALLBACK_LIBRARY_PATH}; "

if "OBT_NONDEV" not in os.environ:
  print(ORKID_SETUP_VULKAN_FN)
# todo figure out how to get DYLD_* to survive bash stack push
#   alternatively, get rid of vulkan's need for DYLD_* in the first place

ORKID_SETUP_OPENGL_FN = "export ORKID_GRAPHICS_API=OPENGL; "
ORKID_SETUP_OPENGL_FN += f"unset DYLD_LIBRARY_PATH; "
ORKID_SETUP_OPENGL_FN += f"unset DYLD_FALLBACK_LIBRARY_PATH; "

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
  obt.env.append("ORKID_ASSET_MANIFEST_DIRS",orkid_dir/"ork.data"/"asset_manifests")

  ##############################################
  # macOS/MoltenVK: Metal argument buffers ON. Without AB, Metal caps samplers
  # at 16 per fragment stage and generated forward fragments (terrain
  # FWD_SSBO_CUSTOM + impostors + IBL + cookies + sun cascades) exceed it ->
  # MSL compile error -> pipeline create VK_ERROR_INITIALIZATION_FAILED.
  # MUST be shell env: MoltenVK snapshots config at dylib load, before any
  # engine code runs (in-process setenv is too late).
  ##############################################

  if obt.host.IsOsx:
    obt.env.set("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS","1")
  

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
  obt.env.append("OBT_SEARCH_PATH",orkid_dir/"ork.ftxui")
  obt.env.append("LUA_PATH",orkid_dir/"ork.data"/"src"/"scripts"/"?.lua")

  ##############################################
  # add search extensions
  #  for obt.find.py
  ##############################################

  obt.env.append("OBT_SEARCH_EXTLIST", ".cpp:.c:.cc:.h:.hpp:.inl")
  obt.env.append("OBT_SEARCH_EXTLIST", ".qml:.m:.mm:.py:.swift")
  obt.env.append("OBT_SEARCH_EXTLIST", ".txt:.md:.fxv2:.ini")

  ##############################################

def extend_bashrc():
  return ["ork.goto.root() { cd ${ORKID_WORKSPACE_DIR}; };\n"] \
       + ["ork.goto.orkid() { cd ${ORKID_WORKSPACE_DIR}; };\n"] \
       + ["ork.goto.data() { cd ${ORKID_WORKSPACE_DIR}/ork.data; };\n"] \
       + ["ork.goto.asset_cache() { cd ${OBT_STAGE}/assetcache; };\n"] \
       + ["ork.goto.data_src() { cd ${ORKID_WORKSPACE_DIR}/ork.data/src; };\n"] \
       + ["ork.goto.data_test() { cd ${ORKID_WORKSPACE_DIR}/ork.data/src; };\n"] \
       + ["ork.goto.data_lev2() { cd ${ORKID_WORKSPACE_DIR}/ork.data/platform_lev2; };\n"]
       
