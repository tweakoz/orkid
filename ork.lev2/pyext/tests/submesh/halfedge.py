#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, sys, time
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
################################################################################

smv = meshutil.SubMesh()
smv.makeVertex(position=dvec3(-1,-1,-1))
smv.makeVertex(position=dvec3(+1,-1,-1))
smv.makeVertex(position=dvec3(-1,+1,-1))
smv.makeVertex(position=dvec3(+1,+1,-1))
smv.makeVertex(position=dvec3(-1,-1,+1))
smv.makeVertex(position=dvec3(+1,-1,+1))
smv.makeVertex(position=dvec3(-1,+1,+1))
smv.makeVertex(position=dvec3(+1,+1,+1))

smc = smv.convexHull(0)
polys = smc.polys
vertices = smc.vertices

print(smc)
for i,v in enumerate(vertices):
  print("v%d: %s"%(i,v.position))
for i,p in enumerate(polys):
  edges = smc.edgesForPoly(p)
  estr = ""
  for e in edges:
    verts = e.vertices
    estr += "[%s->%s] " % (verts[0].poolindex,verts[1].poolindex)
  print("p%d: %s : %s"%(i,p,estr))


for i,p in enumerate(polys):
  edges = smc.edgesForPoly(p)
  for e in edges:
    out = str(e)+"\n"
    out += " poly : " + str(e.polygon) + "\n"
    out += " twin : " + str(e.twin) + "\n"
    out += " twinpoly : " + str(e.twin.polygon) + "\n"
    print(out)
