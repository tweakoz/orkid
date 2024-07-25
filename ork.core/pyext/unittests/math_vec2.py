#!/usr/bin/env python3 

import unittest, math
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np

EPSILON = 1.0e-5
CHECK_CLOSE = lambda a,b: math.fabs(a-b)<EPSILON

class TestCoreMathVec2Methods(unittest.TestCase):
  ########################################
  def test_vec2(self):
    v = vec2(1,2)
    as_np = np.array(v, copy=False)
    self.assertEqual(as_np[0], 1)
    self.assertEqual(as_np[1], 2)
  ########################################
  def test_npdouble_to_vec2(self):
    v = np.array([1.0,2.0])
    as_vec2 = vec2(v)
    self.assertEqual(as_vec2.x, 1)
    self.assertEqual(as_vec2.y, 2)
  ########################################
  def test_npint_to_vec2(self):
    v = np.array([1,2])
    as_vec2 = vec2(v)
    self.assertEqual(as_vec2.x, 1)
    self.assertEqual(as_vec2.y, 2)
  ########################################
  def test_vec2_add(self):
    v1 = vec2(1,2)
    v2 = vec2(3,4)
    v3 = v1+v2
    self.assertEqual(v3.x, 4)
    self.assertEqual(v3.y, 6)
  ########################################
  def test_vec2_sub(self):
    v1 = vec2(1,2)
    v2 = vec2(3,4)
    v3 = v1-v2
    self.assertEqual(v3.x, -2)
    self.assertEqual(v3.y, -2)
  ########################################
  def test_vec2_mul(self):
    v1 = vec2(1,2)
    v2 = vec2(3,4)
    v3 = v1*v2
    self.assertEqual(v3.x, 3)
    self.assertEqual(v3.y, 8)
  ########################################
  def test_vec2_mul_scalar(self):
    v1 = vec2(1,2)
    v3 = v1*3
    self.assertEqual(v3.x, 3)
    self.assertEqual(v3.y, 6)
  ########################################
  def test_vec2_fraction(self):
    v1 = vec2(1,2)
    v3 = v1*0.5
    self.assertEqual(v3.x, 0.5)
    self.assertEqual(v3.y, 1)
  ########################################
  def test_vec2_floor(self):
    v1 = vec2(1.5,2.5)
    v3 = v1.floor
    self.assertEqual(v3.x, 1)
    self.assertEqual(v3.y, 2)
  ########################################
  def test_vec2_ceil(self):
    v1 = vec2(1.5,2.5)
    v3 = v1.ceil
    self.assertEqual(v3.x, 2)
    self.assertEqual(v3.y, 3)
  ########################################
  def test_vec2_length(self):
    v1 = vec2(3,4)
    l = v1.length
    self.assertEqual(l, 5)
  ########################################
  def test_vec2_lengthSquared(self):
    v1 = vec2(3,4)
    l = v1.lengthSquared
    self.assertEqual(l, 25)
  ########################################
  def test_vec2_yx(self):
    v1 = vec2(3,4)
    v2 = v1.yx
    self.assertEqual(v2.x, 4)
    self.assertEqual(v2.y, 3)
  ########################################
  def test_vec2_x(self):
    v1 = vec2(3,4)
    self.assertEqual(v1.x, 3)
  ########################################
  def test_vec2_y(self):
    v1 = vec2(3,4)
    self.assertEqual(v1.y, 4)
  ########################################
  def test_vec_dot(self):
    v1 = vec2(1,2)
    v2 = vec2(3,4)
    d = v1.dot(v2)
    self.assertEqual(d, 11)
  ########################################
  def test_vec2_angle(self):
    v1 = vec2(1,0)
    v2 = vec2(0,1)
    a = v1.angle(v2) # radians of 90 degrees
    self.assertTrue(CHECK_CLOSE(a,math.pi/2))
  ########################################
  def test_vec2_orientedAngle(self):
    v1 = vec2(1,0)
    v2 = vec2(0,1)
    a = v1.orientedAngle(v2)
    self.assertTrue(CHECK_CLOSE(a,math.pi/2))
    b = v2.orientedAngle(v1)
    self.assertTrue(CHECK_CLOSE(b,-math.pi/2))
  ########################################
  def test_vec2_normalize(self):
    v1 = vec2(3,4).normalized
    self.assertTrue(CHECK_CLOSE(v1.length,1))    
  ########################################
  def test_vec2_mag(self):
    v1 = vec2(3,4)
    m = v1.mag()
    self.assertEqual(m, 5)
  ########################################
  def test_vec2_magSquared(self):
    v1 = vec2(3,4)
    m = v1.magSquared()
    self.assertEqual(m, 25)
  ########################################
  def test_vec2_lerp(self):
    v1 = vec2(1,2)
    v2 = vec2(3,4)
    v3 = vec2()
    v3.lerp(v1,v2,0.5)
    self.assertEqual(v3.x, 2)
    self.assertEqual(v3.y, 3)
  ########################################
  def test_vec2_serp(self):
    v1 = vec2(1,2)
    v2 = vec2(3,4)
    v3 = vec2(4,5)
    v4 = vec2(6,7)
    result = vec2()
    result.serp(v1,v2,v3,v4,0.5,0.5)
    # va = lerp(v1,v2,0.5) == (2,3)
    # vb = lerp(v3,v4,0.5) == (5,6)
    # result = lerp(va,vb,0.5) == (3.5,4.5)
    self.assertEqual(result.x, 3.5)
    self.assertEqual(result.y, 4.5)
  ########################################
  
  
  
  
  
  
  ########################################
  #def test_dvec2(self):
  #  v = dvec2(1,2)
  #  as_np = np.array(v, copy=False)
  #  self.assertEqual(as_np[0], 1)
  #  self.assertEqual(as_np[1], 2)
  ########################################
  #def test_npdouble_to_dvec2(self):
  #  v = np.array([1.0,2.0])
  #  as_vec2 = dvec2(v)
  #  self.assertEqual(as_vec2.x, 1)
  #  self.assertEqual(as_vec2.y, 2)
  ########################################
  #def test_npint_to_dvec2(self):
  #  v = np.array([1,2])
  #  as_vec2 = dvec2(v)
  #  self.assertEqual(as_vec2.x, 1)
  #  self.assertEqual(as_vec2.y, 2)
  ########################################

if __name__ == '__main__':
    unittest.main()
