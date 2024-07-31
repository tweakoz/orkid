#!/usr/bin/env python3 

import unittest, math
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np

EPSILON_DIGITS = 6

class TestCoreMathVec4Methods(unittest.TestCase):
  ########################################
  def test_vec4_to_np(self):
    v = vec4(1,2,3,4)
    as_np = np.array(v, copy=False)
    self.assertEqual(as_np[0], 1)
    self.assertEqual(as_np[1], 2)
    self.assertEqual(as_np[2], 3)
    self.assertEqual(as_np[3], 4)
  ########################################
  def test_npdouble_to_vec4(self):
    v = np.array([1.0,2.0,3.0,4.0])
    as_vec4 = vec4(v)
    self.assertEqual(as_vec4.x, 1)
    self.assertEqual(as_vec4.y, 2)
    self.assertEqual(as_vec4.z, 3)
    self.assertEqual(as_vec4.w, 4)
  ########################################
  def test_np_to_vec4(self):
    v = np.array([1,2,3,4])
    as_vec4 = vec4(v)
    self.assertEqual(as_vec4.x, 1)
    self.assertEqual(as_vec4.y, 2)
    self.assertEqual(as_vec4.z, 3)
    self.assertEqual(as_vec4.w, 4)
  ########################################
  # all of same tests as vec3
  ########################################
  def test_vec4_add(self):
    v1 = vec4(1,2,3,4)
    v2 = vec4(5,6,7,8)
    v3 = v1+v2
    self.assertEqual(v3.x, 6)
    self.assertEqual(v3.y, 8)
    self.assertEqual(v3.z, 10)
    self.assertEqual(v3.w, 12)
  ########################################
  def test_vec4_sub(self):
    v1 = vec4(1,2,3,4)
    v2 = vec4(5,6,7,8)
    v3 = v1-v2
    self.assertEqual(v3.x, -4)
    self.assertEqual(v3.y, -4)
    self.assertEqual(v3.z, -4)
    self.assertEqual(v3.w, -4)
  ########################################
  def test_vec4_mul(self):
    v1 = vec4(1,2,3,4)
    v2 = vec4(5,6,7,8)
    v3 = v1*v2
    self.assertEqual(v3.x, 5)
    self.assertEqual(v3.y, 12)
    self.assertEqual(v3.z, 21)
    self.assertEqual(v3.w, 32)
  ########################################
  def test_vec4_mul_scalar(self):
    v1 = vec4(1,2,3,4)
    v3 = v1*3
    self.assertEqual(v3.x, 3)
    self.assertEqual(v3.y, 6)
    self.assertEqual(v3.z, 9)
    self.assertEqual(v3.w, 12)
  ########################################
  def test_vec4_fraction(self):
    v1 = vec4(1.3,2.3,3.3,4.3)
    v3 = v1.fract
    self.assertAlmostEqual(v3.x, 0.3)
    self.assertAlmostEqual(v3.y, 0.3)
    self.assertAlmostEqual(v3.z, 0.3)
    self.assertAlmostEqual(v3.w, 0.3,EPSILON_DIGITS)
  ########################################
  def test_vec4_floor(self):
    v1 = vec4(1.5,2.5,3.5,4.5)
    v3 = v1.floor
    self.assertEqual(v3.x, 1)
    self.assertEqual(v3.y, 2)
    self.assertEqual(v3.z, 3)
    self.assertEqual(v3.w, 4)
  ########################################
  def test_vec4_ceil(self):
    v1 = vec4(1.5,2.5,3.5,4.5)
    v3 = v1.ceil
    self.assertEqual(v3.x, 2)
    self.assertEqual(v3.y, 3)
    self.assertEqual(v3.z, 4)
    self.assertEqual(v3.w, 5)
  ########################################
  def test_vec4_normalize(self):
    v1 = vec4(3,4,5,1)
    v1.normalize()
    self.assertAlmostEqual(v1.x, 0.4242640687119285)
    self.assertAlmostEqual(v1.y, 0.565685424949238)
    self.assertAlmostEqual(v1.z, 0.7071067811865475)
    self.assertAlmostEqual(v1.w, 1.0)
  ########################################
  def test_vec4_length(self):
    v1 = vec4(3,4,5,1)
    l = v1.length
    self.assertAlmostEqual(l, 7.0710678118654755)
  ########################################
  def test_vec4_lengthSquared(self):
    v1 = vec4(3,4,5,1)
    l = v1.magSquared
    self.assertAlmostEqual(l, 50)
  ########################################
  def test_vec4_x(self):
    v1 = vec4(3,4,5,1)
    self.assertEqual(v1.x, 3)
  ########################################
  def test_vec4_y(self):
    v1 = vec4(3,4,5,1)
    self.assertEqual(v1.y, 4)
  ########################################
  def test_vec4_z(self):
    v1 = vec4(3,4,5,1)
    self.assertEqual(v1.z, 5)
  ########################################
  def test_vec4_w(self):
    v1 = vec4(3,4,5,1)
    self.assertEqual(v1.w, 1)
  ########################################
  def test_vec4_xyz(self):
    v1 = vec4(3,4,5,1)
    v2 = v1.xyz
    self.assertEqual(v2.x, 3)
    self.assertEqual(v2.y, 4)
    self.assertEqual(v2.z, 5)
  ########################################
  # rgba-normalized to uint32_t rgba (big endian)
  ########################################
  def test_vec4_rgbaU32(self):
    rgba = vec4(0.1,0.2,0.3,0.5) 
    u32 = rgba.rgbaU32
    self.assertEqual(u32, 0x19334c7f) 
  ########################################
  def test_vec4_argbU32(self):
    argb = vec4(0.1,0.2,0.3,0.5) 
    u32 = argb.argbU32
    self.assertEqual(u32, 0x7f19334c)
  ########################################
  def test_vec4_abgrU32(self):
    abgr = vec4(0.1,0.2,0.3,0.5) 
    u32 = abgr.abgrU32
    self.assertEqual(u32, 0x7f4c3319)
  ########################################
  def test_vec4_perspectiveDivided(self):
    v1 = vec4(3,4,5,17.7)
    v2 = v1.perspectiveDivided
    self.assertAlmostEqual(v2.x, 3/17.7)
    self.assertAlmostEqual(v2.y, 4/17.7)
    self.assertAlmostEqual(v2.z, 5/17.7)
  ########################################
  def test_vec4_dot(self):
    v1 = vec4(1,2,3,1)
    v2 = vec4(4,5,6,1)
    d = v1.dot(v2)
    self.assertEqual(d, 32)
  ########################################
  def test_vec4_cross(self):
    v1 = vec4(1,2,3,1)
    v2 = vec4(4,5,6,1)
    v3 = v1.cross(v2)
    self.assertEqual(v3.x, -3)
    self.assertEqual(v3.y, 6)
    self.assertEqual(v3.z, -3)
  ########################################
  def vec4_test_lerp(self):
    v1 = vec4(1,2,3,1)
    v2 = vec4(4,5,6,1)
    v3 = v1.lerp(v2, 0.5)
    self.assertEqual(v3.x, 2.5)
    self.assertEqual(v3.y, 3.5)
    self.assertEqual(v3.z, 4.5)
    self.assertEqual(v3.w, 1)
  ########################################
  def vec4_test_serp(self):
    v1 = vec4(1,0,0,0)
    v2 = vec4(0,1,0,0)
    v3 = v1.serp(v2, 0.5)
    self.assertAlmostEqual(v3.x, 0.7071067811865475)
  ########################################
  def vec4_test_reflect(self):
    v1 = vec4(1,1,1,1)
    v2 = vec4(0,1,0,0)
    v3 = v1.reflect(v2)
    self.assertEqual(v3.x, 1)
    self.assertEqual(v3.y, -1)
    self.assertEqual(v3.z, 1)
    self.assertEqual(v3.w, 1)
  ########################################
  def test_vec4_saturated(self):
    v1 = vec4(1,2,3,4)
    v2 = v1.saturated
    self.assertEqual(v2.x, 1)
    self.assertEqual(v2.y, 1)
    self.assertEqual(v2.z, 1)
    self.assertEqual(v2.w, 1)
  ########################################
  def test_vec4_normalized(self):
    v1 = vec4(3,4,5,1).normalized
    self.assertAlmostEqual(v1.length,1)
  ########################################
  def test_vec4_rotx(self):
    v1 = vec4(0,2,0,0)
    v1.rotx(math.pi/2)
    self.assertAlmostEqual(v1.x,0)
    self.assertAlmostEqual(v1.y,0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.z,2,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.w,0)
  ########################################
  def test_vec4_roty(self):
    v1 = vec4(2,0,0,0)
    v1.roty(math.pi/2)
    self.assertAlmostEqual(v1.x,0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.y,0)
    self.assertAlmostEqual(v1.z,2)
    self.assertAlmostEqual(v1.w,0)
  ########################################
  def test_vec4_rotz(self):
    v1 = vec4(2,0,0,0)
    v1.rotz(math.pi/2)
    self.assertAlmostEqual(v1.x,0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.y,2)
    self.assertAlmostEqual(v1.z,0)
    self.assertAlmostEqual(v1.w,0)
  ########################################



  ########################################
  ########################################
  ########################################
  #def test_dvec4(self):
  #  v = dvec4(1,2,3,4)
  #  as_np = np.array(v, copy=False)
  #  self.assertEqual(as_np[0], 1)
  #  self.assertEqual(as_np[1], 2)
  #  self.assertEqual(as_np[2], 3)
  #  self.assertEqual(as_np[3], 4)
  ########################################
  #def test_npdouble_to_dvec4(self):
  #  v = np.array([1.0,2.0,3.0,4.0])
  #  as_vec4 = dvec4(v)
  #  self.assertEqual(as_vec4.x, 1)
  #  self.assertEqual(as_vec4.y, 2)
  #  self.assertEqual(as_vec4.z, 3)
  #  self.assertEqual(as_vec4.w, 4)
  ########################################
  #def test_npint_to_dvec4(self):
  #  v = np.array([1,2,3,4])
  #  as_vec4 = dvec4(v)
  #  self.assertEqual(as_vec4.x, 1)
  #  self.assertEqual(as_vec4.y, 2)
  #  self.assertEqual(as_vec4.z, 3)
  #  self.assertEqual(as_vec4.w, 4)
  ########################################
      
if __name__ == '__main__':
    unittest.main()
    