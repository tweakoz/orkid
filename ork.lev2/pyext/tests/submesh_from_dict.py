#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *

manual_submesh = meshutil.SubMesh.createFromDict({
    "vertices": [
        {
          "p": vec3(0, 0, 0),      # position
          "n": vec3(0, 0, 1),      # normal
          "c0": vec4(1, 0, 0, 1),  # color 0
          "uv0": vec2(0, 0),       # uv 0
          "b0":  vec3(0, 0, 0),    # binormal 0
          "t0":  vec3(0, 0, 0),    # tangent 0
        },
        {
          "p": vec3(1, 0, 0), 
          "n": vec3(0, 0, 1),
          "c0": vec4(1, 0, 0, 1), 
          "uv0": vec2(1, 0),
          "b0":  vec3(0, 0, 0),
          "t0":  vec3(0, 0, 0),
        },
        {
          "p": vec3(1, 1, 0),
          "n": vec3(0, 0, 1),
          "c0": vec4(1, 0, 0, 1),
          "uv0": vec2(1, 1),
          "b0":  vec3(0, 0, 0),
          "t0":  vec3(0, 0, 0),
        },
        {
          "p": vec3(0, 1, 0), 
          "n": vec3(0, 0, 1),
          "c0": vec4(1, 0, 0, 1),
          "uv0": vec2(0, 1),
          "b0":  vec3(0, 0, 0),
          "t0":  vec3(0, 0, 0),
        },
    ],
    "faces": [
        [0, 1, 2],
        [0, 2, 3]
    ]
})

print(manual_submesh)
print(manual_submesh.vertices)
print(manual_submesh.polys)
print(manual_submesh.edges)
