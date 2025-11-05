from obt import path as obt_path
import os 


def _root():
	return obt_path.Path(os.environ["ORKID_WORKSPACE_DIR"])

def __getattr__(name):
  if name == "root":
  	return _root()
  if name == "project":
  	return _root()/"obt.project"
  if name == "scripts":
  	return _root()/"obt.project"/"scripts"
  elif name == "data":
  	return _root()/"ork.data"
  elif name == "lev2":
  	return _root()/"ork.lev2"
  elif name == "lev2_pylib":
  	return _root()/"ork.lev2"/"examples"/"python"
  elif name == "render_tests":
  	return _root()/"ork.lev2"/"pyext"/"tests"/"renderer"
  elif name == "py_examples":
  	return _root()/"ork.lev2"/"examples"/"python"
  elif name == "py_lev2utils":
  	return _root()/"ork.lev2"/"examples"/"python"/"lev2utils"
  elif name == "pyvenv":
  	return obt_path.Path(os.environ["OBT_PYTHONHOME"])
  elif name == "assetcache":
  	return obt_path.stage()/"assetcache"
  elif name == "cdntest":
  	return obt_path.stage()/"cdntest"
  elif name == "singularity_data":
  	return obt_path.stage()/"share"/"singularity"
  elif name == "effect_textures":
  	return _root()/"ork.data"/"src"/"effect_textures"
  # iOS Device subspace
  elif name == "ios_subspace":
  	return obt_path.stage()/"subspaces"/"ios"
  elif name == "ios_builds":
  	return obt_path.stage()/"subspaces"/"ios"/"builds"
  elif name == "ios_include":
  	return obt_path.stage()/"subspaces"/"ios"/"include"
  elif name == "ios_lib":
  	return obt_path.stage()/"subspaces"/"ios"/"lib"
  # iOS Simulator subspace
  elif name == "iossim_subspace":
  	return obt_path.stage()/"subspaces"/"iossim"
  elif name == "iossim_builds":
  	return obt_path.stage()/"subspaces"/"iossim"/"builds"
  elif name == "iossim_include":
  	return obt_path.stage()/"subspaces"/"iossim"/"include"
  elif name == "iossim_lib":
  	return obt_path.stage()/"subspaces"/"iossim"/"lib"
  return None
