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

anim = XgmAnim("data://tests/chartest/char_idle")
anim_inst = XgmAnimInst(anim)
print(anim)
print(anim_inst)
print(anim_inst.sampleRate)
print(anim_inst.numFrames)
print(anim_inst.weight)
print(anim_inst.use_temporal_lerp)
print(anim_inst.mask)
print(anim_inst.mask.bytes)