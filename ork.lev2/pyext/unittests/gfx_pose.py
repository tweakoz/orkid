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

class TestLev2XgmLocalPoseMethods(unittest.TestCase):
  ########################################
  def test_pose_1(self):
    # fetch vars for convenience
    skl = _fixture.instance().skeleton
    lpose = _fixture.instance().localpose
    anim = _fixture.instance().anim
    num_anim_channels = anim.numJointChannels
    animinst = _fixture.instance().animinst
    num_frames = animinst.numFrames
    root_index = skl.rootNodeIndex
    num_joints = skl.numJoints
    root_descendants = skl.descendantJointsOf(root_index)
    print(skl.jointName(root_index))
    print(root_descendants)

    print("num_anim_channels",num_anim_channels)
    print("num_frames",num_frames)

    bind_matrices = skl.bindMatrices 
    inv_bind_matrices = skl.inverseBindMatrices

    for i in range(num_frames):
      # set the animation playback frame
      animinst.currentFrame = i
      # apply the animation to the pose
      lpose.identityPose()
      animinst.applyToPose(lpose)
      # blend the pose from the 0 or more animations that were applied..
      lpose.blendPoses()
      # compute the concatenated matrices
      lpose.concatenate()
      # grab copies of matrix arrays
      local_matrices = lpose.localMatrices.as_list 
      concat_matrices = lpose.concatMatrices.as_list 
      bindrela_matrices = lpose.bindRelativeMatrices.as_list 
      # all matrix lists should have the same length
      assert(len(local_matrices)==len(concat_matrices))
      assert(len(local_matrices)==len(bindrela_matrices))
      #############################
      for j in range(num_joints):
        lmtx = local_matrices[j]
        cmtx = concat_matrices[j]
        brmtx = bindrela_matrices[j]
        print("joint",j)
        print("local mtx")
        print(lmtx)
        print("concat mtx")
        print(cmtx)
        print("bindrela mtx")
        print(brmtx)
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################
  ########################################

if __name__ == '__main__':
    unittest.main()
