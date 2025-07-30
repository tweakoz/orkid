from obt import path as obt_path
import os 


def _root():
	return obt_path.Path(os.environ["ORKID_WORKSPACE_DIR"])

def __getattr__(name):
  if name == "root":
  	return _root()
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
  return None
