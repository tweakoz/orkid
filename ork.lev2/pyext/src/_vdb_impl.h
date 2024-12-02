#pragma once

#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/util/NullInterrupter.h>
#include <openvdb_ax/compiler/Logger.h>
#include <openvdb_ax/compiler/VolumeExecutable.h>
#include <openvdb_ax/compiler/Compiler.h>
#include <openvdb_ax/compiler/CustomData.h>
#include <openvdb/tools/Merge.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/Interpolation.h>

#include <ork/kernel/concurrent_queue.h>

namespace ork::lev2 {

using vdb_basegrid_t      = openvdb::GridBase;
using vdb_basegrid_ptr_t  = std::shared_ptr<vdb_basegrid_t>;
using vdb_floatgrid_t     = openvdb::FloatGrid;
using vdb_floatgrid_ptr_t = std::shared_ptr<vdb_floatgrid_t>;
using vdb_vec3grid_t      = openvdb::Vec3SGrid;
using vdb_vec3grid_ptr_t  = std::shared_ptr<vdb_vec3grid_t>;

using vdb_volume_exec_t     = openvdb::ax::VolumeExecutable;
using vdb_volume_exec_ptr_t = std::shared_ptr<vdb_volume_exec_t>;
using vdb_custom_data_t     = openvdb::ax::CustomData;
using vdb_custom_data_ptr_t = std::shared_ptr<vdb_custom_data_t>;
using vdb_transform_t       = openvdb::math::Transform;
using vdb_transform_ptr_t   = std::shared_ptr<vdb_transform_t>;

struct FloatVoxel {
  openvdb::Coord coord;
  float value;
};
using cq_t = MpMcBoundedQueue<FloatVoxel, 4 << 20>;

}