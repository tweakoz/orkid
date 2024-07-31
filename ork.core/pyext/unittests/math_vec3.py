#!/usr/bin/env python3 

import unittest, math
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np

EPSILON_DIGITS = 5

class TestCoreMathVec3Methods(unittest.TestCase):
  ########################################
  def test_vec3_to_np(self):
    v = vec3(1,2,3)
    as_np = np.array(v, copy=False)
    self.assertEqual(as_np[0], 1)
    self.assertEqual(as_np[1], 2)
    self.assertEqual(as_np[2], 3)
  ########################################
  def test_npdouble_to_vec3(self):
    v = np.array([1.0,2.0,3.0])
    as_vec3 = vec3(v)
    self.assertEqual(as_vec3.x, 1)
    self.assertEqual(as_vec3.y, 2)
    self.assertEqual(as_vec3.z, 3)
  ########################################
  def test_np_to_vec3(self):
    v = np.array([1,2,3])
    as_vec3 = vec3(v)
    self.assertEqual(as_vec3.x, 1)
    self.assertEqual(as_vec3.y, 2)
    self.assertEqual(as_vec3.z, 3)
  ########################################
  def test_vec3_add(self):
    v1 = vec3(1,2,3)
    v2 = vec3(4,5,6)
    v3 = v1+v2
    self.assertEqual(v3.x, 5)
    self.assertEqual(v3.y, 7)
    self.assertEqual(v3.z, 9)
  ########################################
  def test_vec3_sub(self):
    v1 = vec3(1,2,3)
    v2 = vec3(4,5,6)
    v3 = v1-v2
    self.assertEqual(v3.x, -3)
    self.assertEqual(v3.y, -3)
    self.assertEqual(v3.z, -3)
  ########################################
  def test_vec3_mul(self):
    v1 = vec3(1,2,3)
    v2 = vec3(4,5,6)
    v3 = v1*v2
    self.assertEqual(v3.x, 4)
    self.assertEqual(v3.y, 10)
    self.assertEqual(v3.z, 18)
  ########################################
  def test_vec3_mul_scalar(self):
    v1 = vec3(1,2,3)
    v3 = v1*3
    self.assertEqual(v3.x, 3)
    self.assertEqual(v3.y, 6)
    self.assertEqual(v3.z, 9)
  ########################################
  def test_vec3_fraction(self):
    v1 = vec3(1,2,3)
    v3 = v1*0.5
    self.assertEqual(v3.x, 0.5)
    self.assertEqual(v3.y, 1)
    self.assertEqual(v3.z, 1.5)
  ########################################
  def test_vec3_floor(self):
    v1 = vec3(1.5,2.5,3.5)
    v3 = v1.floor
    self.assertEqual(v3.x, 1)
    self.assertEqual(v3.y, 2)
    self.assertEqual(v3.z, 3)
  ########################################
  def test_vec3_normalize(self):
    v1 = vec3(3,4,5)
    v1.normalize()
    self.assertAlmostEqual(v1.x, 0.4242640687119285)
    self.assertAlmostEqual(v1.y, 0.565685424949238)
    self.assertAlmostEqual(v1.z, 0.7071067811865475)
  ########################################
  def test_vec3_fraction(self):
    v1 = vec3(1,2,3)
    v3 = v1*0.5
    self.assertEqual(v3.x, 0.5)
    self.assertEqual(v3.y, 1)
    self.assertEqual(v3.z, 1.5)
  ########################################
  def test_vec3_floor(self):
    v1 = vec3(1.5,2.5,3.5)
    v3 = v1.floor
    self.assertEqual(v3.x, 1)
    self.assertEqual(v3.y, 2)
    self.assertEqual(v3.z, 3)
  ########################################
  def test_vec3_ceil(self):
    v1 = vec3(1.5,2.5,3.5)
    v3 = v1.ceil
    self.assertEqual(v3.x, 2)
    self.assertEqual(v3.y, 3)
    self.assertEqual(v3.z, 4)
  ########################################
  def test_vec3_x(self):
    v1 = vec3(1,2,3)
    self.assertEqual(v1.x, 1)  
  ########################################
  def test_vec3_y(self):
    v1 = vec3(1,2,3)
    self.assertEqual(v1.y, 2)
  ########################################
  def test_vec3_z(self):
    v1 = vec3(1,2,3)
    self.assertEqual(v1.z, 3)
  ########################################
  def test_vec3_xy(self):
    v1 = vec3(1,2,3)
    v2 = v1.xy
    self.assertEqual(v2.x, 1)
    self.assertEqual(v2.y, 2)
  ########################################
  def test_vec3_xz(self):
    v1 = vec3(1,2,3)
    v2 = v1.xz
    self.assertEqual(v2.x, 1)
    self.assertEqual(v2.y, 3)
  ########################################
  def test_vec3_yz(self):
    v1 = vec3(1,2,3)
    v2 = v1.yz
    self.assertEqual(v2.x, 2)
    self.assertEqual(v2.y, 3)
  ########################################
  def test_vec3_zyx(self):
    v1 = vec3(1,2,3)
    v2 = v1.zyx
    self.assertEqual(v2.x, 3)
    self.assertEqual(v2.y, 2)
    self.assertEqual(v2.z, 1)
  ########################################
  def test_vec3_as_list(self):
    v1 = vec3(1,2,3)
    as_list = v1.as_list
    self.assertEqual(as_list[0], 1)
    self.assertEqual(as_list[1], 2)
    self.assertEqual(as_list[2], 3)
  ########################################
  def test_vec3_length(self):
    v1 = vec3(3,4,5)
    self.assertAlmostEqual(v1.length, 7.0710678118654755)
  ########################################
  def test_vec3_lengthSquared(self):
    v1 = vec3(3,4,5)
    self.assertEqual(v1.lengthSquared, 50)    
  ########################################
  def test_vec3_angle(self):
    v1 = vec3(1,0,0)
    v2 = vec3(0,1,0)
    a = v1.angle(v2)
    self.assertAlmostEqual(a,math.pi/2)
  ########################################
  def test_vec3_orientedAngle(self):
    v1 = vec3(1,0,0)
    v2 = vec3(0,1,0)
    ref_axis = vec3(0,0,1)
    a = v1.orientedAngle(v2, ref_axis)
    self.assertAlmostEqual(a,math.pi/2)
    b = v2.orientedAngle(v1, ref_axis)
    self.assertAlmostEqual(b,-math.pi/2)   
  ########################################
  def test_vec3_dot(self):
    v1 = vec3(1,2,3)
    v2 = vec3(4,5,6)
    d = v1.dot(v2)
    self.assertEqual(d, 32)
  ########################################
  def test_vec3_cross(self):
    v1 = vec3(1,2,3)
    v2 = vec3(4,5,6)
    v3 = v1.cross(v2)
    self.assertEqual(v3.x, -3)
    self.assertEqual(v3.y, 6)
    self.assertEqual(v3.z, -3)
  ########################################
  def test_vec3_triple(self):
    up = vec3(0,1,0)
    v1 = vec3(1,2,3).normalized()
    v2 = v1.cross(up)
    v3 = v1.cross(v2)
    
    # check that they are orthogonal
    v1dv2 = v1.dot(v2)
    v1dv3 = v1.dot(v3)
    v2dv3 = v2.dot(v3)
    self.assertAlmostEqual(v3.x, 0.1428571343421936,EPSILON_DIGITS)
    self.assertAlmostEqual(v3.y, -0.7142856121063232 ,EPSILON_DIGITS)
    self.assertAlmostEqual(v3.z, 0.4285714030265808,EPSILON_DIGITS)
    self.assertAlmostEqual(v3.x, 0.1428571343421936,EPSILON_DIGITS)
    self.assertAlmostEqual(v1dv2, 0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1dv3, 0,EPSILON_DIGITS)
    self.assertAlmostEqual(v2dv3, 0,EPSILON_DIGITS)
  ########################################
  def vec3_test_lerp(self):
    v1 = vec3(1,2,3)
    v2 = vec3(4,5,6)
    v3 = v1.lerp(v2, 0.5)
    self.assertEqual(v3.x, 2.5)
    self.assertEqual(v3.y, 3.5)
    self.assertEqual(v3.z, 4.5)
  ########################################
  def vec3_test_serp(self):
    v1 = vec3(1,0,0)
    v2 = vec3(0,1,0)
    v3 = v1.serp(v2, 0.5)
    self.assertAlmostEqual(v3.x, 0.7071067811865475)
  ########################################
  def vec3_test_reflect(self):
    v1 = vec3(1,1,1)
    v2 = vec3(0,1,0)
    v3 = v1.reflect(v2)
    self.assertEqual(v3.x, 1)
    self.assertEqual(v3.y, -1)
    self.assertEqual(v3.z, 1)
  ########################################
  def test_vec3_saturated(self):
    v1 = vec3(1,2,3)
    v2 = v1.saturated
    self.assertEqual(v2.x, 1)
    self.assertEqual(v2.y, 1)
    self.assertEqual(v2.z, 1)
  ########################################
  def test_vec3_clamped(self):
    v1 = vec3(-10,0.5,10)
    v2 = v1.clamped(-1,1)
    self.assertEqual(v2.x, -1)
    self.assertEqual(v2.y, 0.5)
    self.assertEqual(v2.z, 1)
  ########################################
  def test_vec3_rotx(self):
    v1 = vec3(0,2,0)
    v1.rotx(math.pi/2)
    self.assertAlmostEqual(v1.x,0)
    self.assertAlmostEqual(v1.y,0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.z,2)
  ########################################
  def test_vec3_roty(self):
    v1 = vec3(2,0,0)
    v1.roty(math.pi/2)
    self.assertAlmostEqual(v1.x,0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.y,0)
    self.assertAlmostEqual(v1.z,2)
  ########################################
  def test_vec3_rotz(self):
    v1 = vec3(2,0,0)
    v1.rotz(math.pi/2)
    self.assertAlmostEqual(v1.x,0,EPSILON_DIGITS)
    self.assertAlmostEqual(v1.y,2)
    self.assertAlmostEqual(v1.z,0)
  ########################################
  def test_vec3_hsv2rgb(self):
    hsv = vec3(0,1,1)
    rgb = hsv.hsv2rgb()
    self.assertAlmostEqual(rgb.x,1)
    self.assertAlmostEqual(rgb.y,0)
    self.assertAlmostEqual(rgb.z,0)
  ########################################
  def test_vec3_set(self):
    v1 = vec3(1,2,3)
    v1.set(vec3(4,5,6))
    self.assertEqual(v1.x, 4)
    self.assertEqual(v1.y, 5)
    self.assertEqual(v1.z, 6)
  ########################################
  ########################################
  
    
  ########################################
  #def test_dvec3(self):
  #  v = dvec3(1,2,3)
  #  as_np = np.array(v, copy=False)
  #  self.assertEqual(as_np[0], 1)
  #  self.assertEqual(as_np[1], 2)
  #  self.assertEqual(as_np[2], 3)
  ########################################
  #def test_npdouble_to_dvec3(self):
  #  v = np.array([1.0,2.0,3.0])
  #  as_vec3 = dvec3(v)
  #  self.assertEqual(as_vec3.x, 1)
  #  self.assertEqual(as_vec3.y, 2)
  #  self.assertEqual(as_vec3.z, 3)
  ########################################
  #def test_npint_to_dvec3(self):
  #  v = np.array([1,2,3])
  #  as_vec3 = dvec3(v)
  #  self.assertEqual(as_vec3.x, 1)
  #  self.assertEqual(as_vec3.y, 2)
  #  self.assertEqual(as_vec3.z, 3)
  ########################################
  
if __name__ == '__main__':
    unittest.main()
