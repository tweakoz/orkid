#!/usr/bin/env python3 

import unittest, math
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np

EPSILON = 1.0e-5
CHECK_CLOSE = lambda a,b: math.fabs(a-b)<EPSILON

class TestCoreMathMtx3Methods(unittest.TestCase):
  ########################################
  def test_mtx3_constructor(self):
    m = mtx3()
    c0 = m.getColumn(0)
    c1 = m.getColumn(1)
    c2 = m.getColumn(2)
    
    self.assertEqual(c0.x, 1)
    self.assertEqual(c0.y, 0)
    self.assertEqual(c0.z, 0)
    
    self.assertEqual(c1.x, 0)
    self.assertEqual(c1.y, 1)
    self.assertEqual(c1.z, 0)
    
    self.assertEqual(c2.x, 0)
    self.assertEqual(c2.y, 0)
    self.assertEqual(c2.z, 1)
    
  ########################################
  def test_mtx3_setScale(self):
    m = mtx3()
    m.setScale(2,3,4)
    c0 = m.getColumn(0)
    c1 = m.getColumn(1)
    c2 = m.getColumn(2)
    self.assertEqual(c0.x, 2)
    self.assertEqual(c0.y, 0)
    self.assertEqual(c0.z, 0)
  ########################################
  def test_mtx3_inverse(self):
    m = mtx3()
    m.setScale(2,3,4)
    im = m.inverse
    c0 = im.getColumn(0)
    c1 = im.getColumn(1)
    c2 = im.getColumn(2)
    
    #print(c0)

    self.assertEqual(CHECK_CLOSE(c0.x, 0.5), True)
    self.assertEqual(CHECK_CLOSE(c0.y, 0), True)
    self.assertEqual(CHECK_CLOSE(c0.z, 0), True)
    
    self.assertEqual(CHECK_CLOSE(c1.x, 0), True)
    self.assertEqual(CHECK_CLOSE(c1.y, 1/3), True)
    self.assertEqual(CHECK_CLOSE(c1.z, 0), True)
    
    self.assertEqual(CHECK_CLOSE(c2.x, 0), True)
    self.assertEqual(CHECK_CLOSE(c2.y, 0), True)
    self.assertEqual(CHECK_CLOSE(c2.z, 0.25), True)
    
    mim = m*im
    
    c0 = mim.getColumn(0)
    c1 = mim.getColumn(1)
    c2 = mim.getColumn(2)
    
    self.assertEqual(CHECK_CLOSE(c0.x, 1), True)
    self.assertEqual(CHECK_CLOSE(c0.y, 0), True)
    self.assertEqual(CHECK_CLOSE(c0.z, 0), True)
    
    self.assertEqual(CHECK_CLOSE(c1.x, 0), True)
    self.assertEqual(CHECK_CLOSE(c1.y, 1), True)
    self.assertEqual(CHECK_CLOSE(c1.z, 0), True)
    
    self.assertEqual(CHECK_CLOSE(c2.x, 0), True)
    self.assertEqual(CHECK_CLOSE(c2.y, 0), True)
    self.assertEqual(CHECK_CLOSE(c2.z, 1), True)
  ########################################
  def test_mtx3_xnormal(self):
    m = mtx3()
    m.setColumn(0, vec3(1,2,3))
    m.setColumn(1, vec3(4,5,6))
    m.setColumn(2, vec3(7,8,9))
    xnormal = m.xNormal().normalized
    ynormal = m.yNormal().normalized
    znormal = m.zNormal().normalized
    #print(xnormal)
    self.assertEqual(CHECK_CLOSE(xnormal.x, 0.502571), True)
    self.assertEqual(CHECK_CLOSE(xnormal.y, 0.574366), True)
    self.assertEqual(CHECK_CLOSE(xnormal.z, 0.646161), True)
    #print(ynormal)
    self.assertEqual(CHECK_CLOSE(ynormal.x, 0.455842), True)
    self.assertEqual(CHECK_CLOSE(ynormal.y, 0.569803), True)
    self.assertEqual(CHECK_CLOSE(ynormal.z, 0.683764), True)
    #print(znormal)
    self.assertEqual(CHECK_CLOSE(znormal.x, 0.267261), True)
    self.assertEqual(CHECK_CLOSE(znormal.y, 0.534522), True)
    self.assertEqual(CHECK_CLOSE(znormal.z, 0.801784), True)
    
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

if __name__ == '__main__':
    unittest.main()
