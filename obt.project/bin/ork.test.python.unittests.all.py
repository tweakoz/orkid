#!/usr/bin/env python3 
###############################################################################
import unittest, sys
from obt import path as obt_path 
from obt.deco import Deco
import importlib.util
deco = Deco()
from orkengine.core import disableLogging

###############################################################################
orkdir = obt_path.orkid()
disableLogging()
###############################################################################
def register_tests(path_dict):
  for k,v in path_dict.items():
    name = k
    spec = importlib.util.spec_from_file_location(name, v)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    # add module's unittest.TestCase classes to globals
    for k,v in module.__dict__.items():
      if isinstance(v,type) and issubclass(v,unittest.TestCase):
        class_name = v.__name__
        num_tests = len([x for x in dir(v) if x.startswith("test")])
        count_str = deco.rgbstr(255,255,0,str(num_tests))
        class_name = deco.rgbstr(255,128,0,class_name)
        print("%s tests from class: %s" % (count_str,class_name))
        globals()[k] = v
      pass
def print_summary(path_dict):
  for k,v in path_dict.items():
    name = k
    spec = importlib.util.spec_from_file_location(name, v)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    # add module's unittest.TestCase classes to globals
    for k,v in module.__dict__.items():
      if isinstance(v,type) and issubclass(v,unittest.TestCase):
        class_name = v.__name__
        num_tests = len([x for x in dir(v) if x.startswith("test")])
        count_str = deco.rgbstr(255,255,0,str(num_tests))
        class_name = deco.rgbstr(255,128,0,class_name)
        print("%s tests from class: %s" % (count_str,class_name))
      pass
###############################################################################
orkcore_tests_dir = orkdir / "ork.core"/"pyext"/"unittests"
core_tests = {
    "math_vec2": orkcore_tests_dir/"math_vec2.py",
    "math_vec3": orkcore_tests_dir/"math_vec3.py",
    "math_vec4": orkcore_tests_dir/"math_vec4.py",
    "math_quat": orkcore_tests_dir/"math_quat.py",
    "math_mtx3": orkcore_tests_dir/"math_mtx3.py",
    "math_mtx4": orkcore_tests_dir/"math_mtx4.py",
}
###############################################################################
orklev2_tests_dir = orkdir / "ork.lev2"/"pyext"/"unittests"
lev2_tests = {
    "gfx_skeleton": orklev2_tests_dir/"gfx_skeleton.py",
    "gfx_pose": orklev2_tests_dir/"gfx_pose.py",
}
###############################################################################
orkecs_tests_dir = orkdir / "ork.ecs"/"pyext"/"unittests"
ecs_tests = {
    "ecs": orkecs_tests_dir/"ecs.py",
}
###############################################################################
register_tests(core_tests)
register_tests(lev2_tests)
#register_tests(ecs_tests)
###############################################################################

if __name__ == '__main__':
    unittest.main()
