#!/usr/bin/env ork.python

import time, os, math
from obt import path as obt_path
from orkengine.core import vec2,vec3,vec4,CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import vdb as ork_vdb
from orkengine.lev2 import OrkEzApp, RefreshFastest, ui, PBRMaterial, FxPipelinePermutation
from orkengine.lev2 import primitives, RigidPrimitive, meshutil, MicroMesh, Image

CENTER = vec3(0,0,0)
RADIUS1 = 5.0 
VOXEL_SIZE = .1
HALF_WIDTH = 3.0/VOXEL_SIZE

time1 = time.time()
for i in range(100):
  spherea = ork_vdb.FloatGrid.createLevelSetSphere( "a",         # element name
                                                    RADIUS1,     # world units
                                                    CENTER,      # world units 
                                                    VOXEL_SIZE,  # world units
                                                    HALF_WIDTH)  # voxel units

time2 = time.time()

for i in range(100):
  sphereb = ork_vdb.FloatGrid.createLevelSetSphere( "a", 
                                                    0.25, 
                                                    CENTER+vec3(RADIUS1,0,0), 
                                                    0.1, 
                                                    1.01 )

time3 = time.time()

for i in range(100):
  spherec = spherea.csgDifference(sphereb)

time4 = time.time()

for i in range(400):
  x = math.sin(math.pi*2.0*i/400.0)*RADIUS1
  z = math.cos(math.pi*2.0*i/400.0)*RADIUS1
  spherex = ork_vdb.FloatGrid.createLevelSetSphere( "a", 
                                                    0.25, 
                                                    CENTER+vec3(x,0,z), 
                                                    0.1, 
                                                    1.01 )
  spherec = spherec.csgDifference(spherex)

time5 = time.time()

print(spherea.xform)
print("Time to create sphere-a: ", (time2 - time1)/100.0 )
print("Time to create sphere-b: ", (time3 - time2)/100.0 )      
print("Time to create sphere-c: ", (time4 - time3)/100.0 )
print("Time to create sphere-d: ", (time5 - time4)/400.0 )

home = obt_path.Path(os.environ["HOME"])
spherec.saveToVDB(home/"spherec8.vdb")
      