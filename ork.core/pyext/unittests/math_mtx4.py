#!/usr/bin/env python3 

import unittest, math
from orkengine.core import vec2, vec3, vec4, quat, mtx4, mtx4
import numpy as np

EPSILON_DIGITS = 5

class TestCoreMathMtx4Methods(unittest.TestCase):
  ########################################
  def test_mtx4_constructor(self):
    m = mtx4()
    c0 = m.getColumn(0)
    c1 = m.getColumn(1)
    c2 = m.getColumn(2)
    c3 = m.getColumn(3)
    
    self.assertEqual(c0.x, 1)
    self.assertEqual(c0.y, 0)
    self.assertEqual(c0.z, 0)
    self.assertEqual(c0.w, 0)
    
    self.assertEqual(c1.x, 0)
    self.assertEqual(c1.y, 1)
    self.assertEqual(c1.z, 0)
    self.assertEqual(c1.w, 0)
    
    self.assertEqual(c2.x, 0)
    self.assertEqual(c2.y, 0)
    self.assertEqual(c2.z, 1)
    self.assertEqual(c2.w, 0)
    
    self.assertEqual(c3.x, 0)
    self.assertEqual(c3.y, 0)
    self.assertEqual(c3.z, 0)
    self.assertEqual(c3.w, 1)

  ########################################
  def test_mtx4_setScale(self):
    m = mtx4()
    m.setScale(2,3,4)
    c0 = m.getColumn(0)
    c1 = m.getColumn(1)
    c2 = m.getColumn(2)
    self.assertEqual(c0.x, 2)
    self.assertEqual(c0.y, 0)
    self.assertEqual(c0.z, 0)
    self.assertEqual(c0.w, 0)
  ########################################
  def test_mtx4_inverse(self):
    m = mtx4()
    m.setScale(2,3,4)
    im = m.inverse
    c0 = im.getColumn(0)
    c1 = im.getColumn(1)
    c2 = im.getColumn(2)
    c3 = im.getColumn(3)
    
    #print(c0)

    self.assertAlmostEqual(c0.x, 0.5)
    self.assertAlmostEqual(c0.y, 0)
    self.assertAlmostEqual(c0.z, 0)
    self.assertAlmostEqual(c0.w, 0)
    
    self.assertAlmostEqual(c1.x, 0)
    self.assertAlmostEqual(c1.y, 1/3)
    self.assertAlmostEqual(c1.z, 0)
    self.assertAlmostEqual(c1.w, 0)
    
    self.assertAlmostEqual(c2.x, 0)
    self.assertAlmostEqual(c2.y, 0)
    self.assertAlmostEqual(c2.z, 0.25)
    self.assertAlmostEqual(c2.w, 0)

    self.assertAlmostEqual(c3.x, 0)
    self.assertAlmostEqual(c3.y, 0)
    self.assertAlmostEqual(c3.z, 0)
    self.assertAlmostEqual(c3.w, 1)

    mim = m*im
    
    c0 = mim.getColumn(0)
    c1 = mim.getColumn(1)
    c2 = mim.getColumn(2)
    c3 = mim.getColumn(3)
    
    self.assertAlmostEqual(c0.x, 1)
    self.assertAlmostEqual(c0.y, 0)
    self.assertAlmostEqual(c0.z, 0)
    self.assertAlmostEqual(c0.w, 0)
    
    self.assertAlmostEqual(c1.x, 0)
    self.assertAlmostEqual(c1.y, 1)
    self.assertAlmostEqual(c1.z, 0)
    self.assertAlmostEqual(c1.w, 0)
    
    self.assertAlmostEqual(c2.x, 0)
    self.assertAlmostEqual(c2.y, 0)
    self.assertAlmostEqual(c2.z, 1)
    self.assertAlmostEqual(c2.w, 0)

    self.assertAlmostEqual(c3.x, 0)
    self.assertAlmostEqual(c3.y, 0)
    self.assertAlmostEqual(c3.z, 0)
    self.assertAlmostEqual(c3.w, 1)

  ########################################
  def test_mtx4_xnormal(self):
    m = mtx4()
    m.setColumn(0, vec4(1,2,3,0))
    m.setColumn(1, vec4(4,5,6,1))
    m.setColumn(2, vec4(7,8,9,2))
    m.setColumn(3, vec4(7,8,9,3))
    xnormal = m.xNormal().normalized()
    ynormal = m.yNormal().normalized()
    znormal = m.zNormal().normalized()
    #print(xnormal)
    self.assertAlmostEqual(xnormal.x, 0.502571,EPSILON_DIGITS)
    self.assertAlmostEqual(xnormal.y, 0.574366,EPSILON_DIGITS)
    self.assertAlmostEqual(xnormal.z, 0.646161,EPSILON_DIGITS)
    #print(ynormal)
    self.assertAlmostEqual(ynormal.x, 0.455842,EPSILON_DIGITS)
    self.assertAlmostEqual(ynormal.y, 0.569803,EPSILON_DIGITS)
    self.assertAlmostEqual(ynormal.z, 0.683764,EPSILON_DIGITS)
    #print(znormal)
    self.assertAlmostEqual(znormal.x, 0.267261,EPSILON_DIGITS)
    self.assertAlmostEqual(znormal.y, 0.534522,EPSILON_DIGITS)
    self.assertAlmostEqual(znormal.z, 0.801784,EPSILON_DIGITS)
    
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
