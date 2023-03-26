#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import numpy
from orkengine.core import *
from orkengine.lev2 import *

def FrustumQuads():
    fpmtx = mtx4.perspective(45,1,0.1,3)
    fvmtx = mtx4.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
    frus = Frustum()
    frus.set(fvmtx,fpmtx)
    qsubmesh = meshutil.SubMesh()
    qsubmesh.addQuad(frus.nearCorner(3), # near
                     frus.nearCorner(2),
                     frus.nearCorner(1),
                     frus.nearCorner(0))
    qsubmesh.addQuad(frus.farCorner(0), # far
                     frus.farCorner(1),
                     frus.farCorner(2),
                     frus.farCorner(3))
    qsubmesh.addQuad(frus.nearCorner(1), # top
                     frus.farCorner(1),
                     frus.farCorner(0),
                     frus.nearCorner(0))
    qsubmesh.addQuad(frus.nearCorner(3), # bottom
                     frus.farCorner(3),
                     frus.farCorner(2),
                     frus.nearCorner(2))
    qsubmesh.addQuad(frus.nearCorner(0), # left
                     frus.farCorner(0),
                     frus.farCorner(3),
                     frus.nearCorner(3))
    qsubmesh.addQuad(frus.nearCorner(2), # right
                     frus.farCorner(2),
                     frus.farCorner(1),
                     frus.nearCorner(1))
    return qsubmesh

#qsubmesh.addQuad(vec3(-3,0,-3), # cut
#                 vec3(+3,0,-3),
#                 vec3(+3,0,+3),
#                 vec3(-3,0,+3))
