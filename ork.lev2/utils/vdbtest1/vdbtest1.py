#!/usr/bin/env ork.python

import math
from obt import path as obt_path 
import openvdb as vdb

# A grid comprises a sparse tree representation of voxel data,
# user-supplied metadata and a voxel space to world space transform,
# which defaults to the identity transform.
# A FloatGrid stores one single-precision floating point value per voxel.
# Other grid types include BoolGrid and Vec3SGrid.  The module-level
# attribute pyopenvdb.GridTypes gives the complete list.
cube = vdb.FloatGrid()
cube.fill(min=(100, 100, 100), max=(199, 199, 199), value=1.0)
 
# Name the grid "cube".
cube.name = 'cube'

radius = 50.0 
desired_num_points = 10000
voxel_size = radius / math.cbrt(desired_num_points);

# Populate another FloatGrid with a sparse, narrow-band level set
# representation of a sphere with radius 50 voxels, located at
# (1.5, 2, 3) in index space.
sphere = vdb.createLevelSetSphere(radius=radius, center=(1.5, 2, 3), voxelSize=voxel_size, halfWidth = 3.0)
 
# Associate some metadata with the grid.
sphere['radius'] = radius

# Associate a scaling transform with the grid that sets the voxel size
# to 0.5 units in world space.
sphere.transform = vdb.createLinearTransform(voxelSize=0.5)
 
# Name the grid "sphere".
sphere.name = 'sphere'

outside = sphere.background
width = 2.0 * outside

# Visit and update all of the grid's active values, which correspond to
# voxels in the narrow band.
for iter in sphere.iterOnValues():
  dist = iter.value
  iter.value = (outside - dist) / width
 
# Visit all of the grid's inactive tile and voxel values and update
# the values that correspond to the interior region.
for iter in sphere.iterOffValues():
  if iter.value < 0.0:
    iter.value = 1.0
    iter.active = False

# Set exterior voxels to 0.
sphere.background = 0.0

sphere.gridClass = vdb.GridClass.FOG_VOLUME
# Write both grids to a VDB file.

vdb_out_path = obt_path.stage()/'mygrids.vdb'
vdb.write(str(vdb_out_path), grids=[cube, sphere])
print(f"wrote vdb {vdb_out_path}") 
