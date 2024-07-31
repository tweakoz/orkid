#!/usr/bin/env python3 

import unittest, math, sys, os
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
from orkengine import lev2
import numpy as np

this_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(this_dir)
import _fixture 

EPSILON_DIGITS = 6

hand_name = "Left"

class TestLev2CameraData(unittest.TestCase):
  ########################################
  def test_cam_1(self):
    self.assertAlmostEqual(1.0,1.00000001,EPSILON_DIGITS)

if __name__ == '__main__':
    unittest.main()
