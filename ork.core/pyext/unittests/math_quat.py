#!/usr/bin/env python3 

import unittest, math
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np

EPSILON_DIGITS = 5

class TestCoreMathQuatMethods(unittest.TestCase):
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
  def test_quat_concat(self):
    q_rotx = quat(vec3(1,0,0), 3.14159/2)
    q_roty = quat(vec3(0,1,0), 3.14159/2)
    q_rotz = quat(vec3(0,0,1), 3.14159/2)
    q_rot = q_roty*q_rotz
    self.assertAlmostEqual(q_rot.x, 0.5,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.y, 0.5,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.z, 0.5,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.w, 0.5,EPSILON_DIGITS)
    q_rot = q_rotz*q_roty
    handedness = "left" if q_rot.w<0 else "right"
    self.assertTrue(handedness=="right")
    # check normalisation
    #print( "system is %s handed" % handedness )
    self.assertAlmostEqual(q_rot.x, -0.5,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.y, 0.5,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.z, 0.5,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.w, 0.5,EPSILON_DIGITS)
    q_rot = q_rotz*q_roty*q_rotx
    handedness = "left" if q_rot.w<0 else "right"
    self.assertTrue(handedness=="right")
    self.assertAlmostEqual(q_rot.x, 4.33813e-07,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.y, 0.707107,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.z, 4.33813e-07,EPSILON_DIGITS)
    self.assertAlmostEqual(q_rot.w, 0.707107,EPSILON_DIGITS)
    # determine handedness from above results
    handedness = "left" if q_rot.w<0 else "right"
    self.assertTrue(handedness=="right")
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
    
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
