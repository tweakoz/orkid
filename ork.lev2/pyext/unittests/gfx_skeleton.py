#!/usr/bin/env python3 

import unittest, math, sys, os
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
from orkengine import lev2
import numpy as np

this_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(this_dir)
import _fixture 

EPSILON = 1.0e-5
CHECK_CLOSE = lambda a,b: math.fabs(a-b)<EPSILON

hand_name = "Left"

class TestLev2XgmSkeletonMethods(unittest.TestCase):
  ########################################
  def test_skel_1(self):
    skl = _fixture.instance().skeleton
    self.assertEqual(skl.numJoints, 50)
    self.assertEqual(skl.numBones, 42)
    self.assertEqual(skl.name, "")
  ########################################
  def test_skel_2(self):
    skl = _fixture.instance().skeleton
    j_hand = skl.jointIndex("mixamorig.%sHand"%(hand_name))
    j_forearm = skl.jointIndex("mixamorig.%sForeArm"%(hand_name))
    jnts_thumbs = [ skl.jointIndex("mixamorig.%sHandThumb%d"%(hand_name,i+1)) for i in range(4)]
    jnts_index = [ skl.jointIndex("mixamorig.%sHandIndex%d"%(hand_name,i+1)) for i in range(4)]
    self.assertEqual(j_hand, 12)
    self.assertEqual(j_forearm, 11)
    self.assertEqual(jnts_thumbs, [13,14,15,16])
    self.assertEqual(jnts_index, [17,18,19,20])
  ########################################
  def test_skel_3(self):
    skl = _fixture.instance().skeleton
    j_hand = skl.jointIndex("mixamorig.%sHand"%(hand_name))
    self.assertEqual(skl.jointName(j_hand), "mixamorig.%sHand"%(hand_name))
    self.assertEqual(skl.descendantJointsOf(12), [17, 13, 14, 15, 16, 44, 18, 19, 20, 45])
    self.assertEqual(skl.childJointsOf(12), [17, 13])
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################

if __name__ == '__main__':
    unittest.main()
