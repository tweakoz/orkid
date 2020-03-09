#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import numpy, time
from orkcore import *
from orklev2 import *
from PIL import Image

fpmtx = mtx4.perspective(45,1,0.1,3)
fvmtx = mtx4.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
frus = Frustum()
frus.set(fvmtx,fpmtx)

qsubmesh = meshutil.SubMesh()
qsubmesh.addQuad(frus.nearCorner(3), # near
                 frus.nearCorner(2),
                 frus.nearCorner(1),
                 frus.nearCorner(0),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,0.5,1.0,1))
qsubmesh.addQuad(frus.farCorner(0), # far
                 frus.farCorner(1),
                 frus.farCorner(2),
                 frus.farCorner(3),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,0.5,0.0,1))
qsubmesh.addQuad(frus.nearCorner(1), # top
                 frus.farCorner(1),
                 frus.farCorner(0),
                 frus.nearCorner(0),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,1.0,0.5,1))
qsubmesh.addQuad(frus.nearCorner(3), # bottom
                 frus.farCorner(3),
                 frus.farCorner(2),
                 frus.nearCorner(2),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,0.0,0.5,1))
qsubmesh.addQuad(frus.nearCorner(0), # left
                 frus.farCorner(0),
                 frus.farCorner(3),
                 frus.nearCorner(3),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.0,0.5,0.5,1))
qsubmesh.addQuad(frus.nearCorner(2), # right
                 frus.farCorner(2),
                 frus.farCorner(1),
                 frus.nearCorner(1),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(1.0,0.5,0.5,1))
tsubmesh = meshutil.SubMesh()
meshutil.triangulate(qsubmesh,tsubmesh)

tsubmesh.writeObj("submesh.obj")
