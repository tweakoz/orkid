#!/usr/bin/env python3

################################################################################

import sys, math, random, numpy, obt.path, obt.deco
from orkengine.core import *
from orkengine.lev2 import *
lev2appinit()
l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from common.primitives import createParticleData

DECO = obt.deco.Deco()

print(animMaxBones)

anim = XgmAnim("data://tests/chartest/char_idle")
anim_inst = XgmAnimInst(anim)

model = XgmModel("data://tests/chartest/char_mesh")

anim_inst.bindToSkeleton(model.skeleton)

print( "####################################################")
print(anim, model)
print(anim_inst)
print(anim_inst.sampleRate)
print(anim_inst.numFrames)
print(anim_inst.weight)
print(anim_inst.use_temporal_lerp)
print(anim_inst.mask)
print(anim_inst.mask.bytes)
print(anim_inst.poser)

print( "####################################################")
print(model.skeleton)
print("skeleton.name: " + str(model.skeleton.name))
print("skeleton.numJoints: " + str(model.skeleton.numJoints))
print("skeleton.numBones: " + str(model.skeleton.numBones))

for i in range(0,model.skeleton.numJoints):
  print( i, model.skeleton.jointName(i) )
  print( " joint: " + str(model.skeleton.jointMatrix(i)) )
  print( "  bind: " + str(model.skeleton.bindMatrix(i)) )

print( "####################################################")


poser = anim_inst.poser
for i in range(0,animMaxBones):
  binding = poser.poseBinding(i)
  if binding.skeletonIndex!=0xffff and binding.channelIndex!=0xffff:
    print("posebinding %d : skel<%s> channel<%s>" % (i, binding.skeletonIndex, binding.channelIndex) )
for i in range(0,animMaxBones):
  binding = poser.animBinding(i)
  if binding.skeletonIndex!=0xffff and binding.channelIndex!=0xffff:
    print("animbinding %d : skel<%s> channel<%s>" % (i, binding.skeletonIndex, binding.channelIndex) )

bxf = BoneTransformer(model.skeleton)
print(bxf)
lpose = XgmLocalPose(model.skeleton)
lpose.bindPose()

ikc = IkChain(model.skeleton)
ikc.bindToBone("mixamorig.RightArm")
ikc.bindToBone("mixamorig.RightForeArm")
ikc.prepare()
ikc.compute(lpose,vec3(0,0,0))


wpose = XgmWorldPose(model.skeleton)
wpose.fromLocalPose(lpose,mtx4())
