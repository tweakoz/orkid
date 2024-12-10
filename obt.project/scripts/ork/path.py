from obt import path as obt_path
import os 


def _root():
	return obt_path.Path(os.environ["ORKID_WORKSPACE_DIR"])

def __getattr__(name):
  if name == "root":
  	return _root()
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
  return None
