#!/usr/bin/env python3 

import unittest
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np


class TestMathQuatMethods(unittest.TestCase):
  ########################################
  def test_quat_to_np(self):
    q = quat()
    as_np = np.array(q, copy=False)
    self.assertEqual(as_np[0], 1)
    self.assertEqual(as_np[1], 0)
    self.assertEqual(as_np[2], 0)
    self.assertEqual(as_np[3], 0)
  ########################################
  def test_npdouble_to_quat(self):
    q = np.array([1.0,2.0,3.0,4.0])
    as_quat = quat(q)
    self.assertEqual(as_quat.x, 1)
    self.assertEqual(as_quat.y, 2)
    self.assertEqual(as_quat.z, 3)
    self.assertEqual(as_quat.w, 4)
  ########################################
  def test_np_to_quat(self):
    q = np.array([1,2,3,4])
    as_quat = quat(q)
    self.assertEqual(as_quat.x, 1)
    self.assertEqual(as_quat.y, 2)
    self.assertEqual(as_quat.z, 3)
    self.assertEqual(as_quat.w, 4)
    
  ########################################
  #def test_dquat(self):
  #  q = dquat()
  #  as_np = np.array(q, copy=False)
  #  self.assertEqual(as_np[0], 1)
  #  self.assertEqual(as_np[1], 0)
  #  self.assertEqual(as_np[2], 0)
  #  self.assertEqual(as_np[3], 0)
  ########################################
  #def test_npdouble_to_dquat(self):
  #  q = np.array([1.0,2.0,3.0,4.0])
  #  as_quat = dquat(q)
  #  self.assertEqual(as_quat.x, 1)
  #  self.assertEqual(as_quat.y, 2)
  #  self.assertEqual(as_quat.z, 3)
  #  self.assertEqual(as_quat.w, 4)
  ########################################
  #def test_npint_to_dquat(self):
  #  q = np.array([1,2,3,4])
  #  as_quat = dquat(q)
  #  self.assertEqual(as_quat.x, 1)
  #  self.assertEqual(as_quat.y, 2)
  #  self.assertEqual(as_quat.z, 3)
  #  self.assertEqual(as_quat.w, 4)
  ########################################
  
if __name__ == '__main__':
    unittest.main()
