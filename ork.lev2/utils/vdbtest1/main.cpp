#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/util/NullInterrupter.h>
#include <random>
#include <vector>
#include <iostream>
#include <cmath>

// Populate the given grid with a narrow-band level set representation of a sphere.
// The width of the narrow band is determined by the grid's background value.
// (Example code only; use tools::createSphereSDF() in production.)
template <class GridType> void makeSphere(GridType& grid, float radius, const openvdb::Vec3f& c) {
  using ValueT = typename GridType::ValueType;
  // Distance value for the constant region exterior to the narrow band
  const ValueT outside = grid.background();
  // Distance value for the constant region interior to the narrow band
  // (by convention, the signed distance is negative in the interior of
  // a level set)
  const ValueT inside = -outside;
  // Use the background value as the width in voxels of the narrow band.
  // (The narrow band is centered on the surface of the sphere, which
  // has distance 0.)
  int padding = int(openvdb::math::RoundUp(openvdb::math::Abs(outside)));
  // The bounding box of the narrow band is 2*dim voxels on a side.
  int dim = int(radius + padding);
  // Get a voxel accessor.
  typename GridType::Accessor accessor = grid.getAccessor();
  // Compute the signed distance from the surface of the sphere of each
  // voxel within the bounding box and insert the value into the grid
  // if it is smaller in magnitude than the background value.
  openvdb::Coord ijk;
  int &i = ijk[0], &j = ijk[1], &k = ijk[2];
  for (i = c[0] - dim; i < c[0] + dim; ++i) {
    const float x2 = openvdb::math::Pow2(i - c[0]);
    for (j = c[1] - dim; j < c[1] + dim; ++j) {
      const float x2y2 = openvdb::math::Pow2(j - c[1]) + x2;
      for (k = c[2] - dim; k < c[2] + dim; ++k) {
        // The distance from the sphere surface in voxels
        const float dist = openvdb::math::Sqrt(x2y2 + openvdb::math::Pow2(k - c[2])) - radius;
        // Convert the floating-point distance to the grid's value type.
        ValueT val = ValueT(dist);
        // Only insert distances that are smaller in magnitude than
        // the background value.
        if (val < inside || outside < val)
          continue;
        // Set the distance for voxel (i,j,k).
        accessor.setValue(ijk, val);
      }
    }
  }
  // Propagate the outside/inside sign information from the narrow band
  // throughout the grid.
  openvdb::tools::signedFloodFill(grid.tree());
}

int main(int argc, char** argv) {

  openvdb::initialize();
  // Create a FloatGrid and populate it with a narrow-band
  // signed distance field of a sphere.
  // set the point density to 0.5
  openvdb::FloatGrid::Ptr grid = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(
      /*radius=*/50.0,
      /*center=*/openvdb::Vec3f(1.5, 2, 3),
      /*voxelSize=*/0.5,
      /*width=*/1.1);

  // Associate some metadata with the grid.
  grid->insertMeta("radius", openvdb::FloatMetadata(50.0));
  // Name the grid "LevelSetSphere".
  grid->setName("LevelSetSphere");
  // Create a VDB file object and write out the grid.
  auto pth = ork::file::Path::stage_dir() / "mygrids.vdb";
  openvdb::io::File(pth.c_str()).write({grid});
}