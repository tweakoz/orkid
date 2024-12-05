#pragma once


#include <ork/math/cvector3.h>
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
#include <openvdb/openvdb.h>
#include <openvdb/math/Math.h>

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

///////////////////////////////////////////////////////////////////////////////

struct TestGridCell {
  TestGridCell(float val=0.0f)
      : _level(val) {
    //_writeCount = std::make_shared<counter_t>(0);
  }
  TestGridCell(const TestGridCell& oth)
      : _abc(oth._abc)
      , _rgb(oth._rgb)
      , _level(oth._level) {
    //_writeCount = std::make_shared<counter_t>(0);
  }
  TestGridCell operator-() const {
    TestGridCell rval = *this;
    rval._level = -rval._level;
    return rval;
  }
  TestGridCell operator-(const TestGridCell& rhs) const {
    TestGridCell rval = *this;
    rval._level = this->_level-rhs._level;
    return rval;
  }
  TestGridCell operator+(const TestGridCell& rhs) const {
    TestGridCell rval = *this;
    rval._level = this->_level+rhs._level;
    return rval;
  }
  bool operator==(const TestGridCell& rhs) const {
    return _level == rhs._level;
  }
  bool operator<(const TestGridCell& rhs) const {
    return _level < rhs._level;
  }
  bool operator>(const TestGridCell& rhs) const {
    return _level > rhs._level;
  }
  bool operator<=(const TestGridCell& rhs) const {
    return _level <= rhs._level;
  }
  bool operator>=(const TestGridCell& rhs) const {
    return _level >= rhs._level;
  }

  fvec3 _abc;
  fvec3 _rgb;
  float _level;
  using counter_t = std::atomic<int>;
  using counter_ptr_t = std::shared_ptr<counter_t>;
  //counter_ptr_t _writeCount;
};

inline std::ostream& operator<<(std::ostream& os, const TestGridCell& cell) {
    os << cell._level;
    return os;
}

inline TestGridCell Abs(const TestGridCell& cell) {
  TestGridCell rval = cell;
  rval._level = fabs(rval._level);
  return rval;
}

static constexpr size_t L3_SIZE = 4;
static constexpr size_t L2_SIZE = 2;
static constexpr size_t L1_SIZE = 6;

using vdb_tree_test = openvdb::tree::Tree4<TestGridCell,L1_SIZE, L2_SIZE, L3_SIZE>::Type;
using vdb_grid_test = openvdb::Grid<vdb_tree_test>;
using vdb_grid_test_ptr_t  = std::shared_ptr<vdb_grid_test>;

using vdb_tree_test_leaf_t = vdb_tree_test::LeafNodeType;                                        // L3 (   8^3 [512] voxels )
using vdb_tree_test_int2_t = openvdb::v12_0::tree::InternalNode<vdb_tree_test_leaf_t,L2_SIZE>;   // L2 ( 128^3 [2M]  voxels )
using vdb_tree_test_int1_t = openvdb::v12_0::tree::InternalNode<vdb_tree_test_int2_t,L1_SIZE>;   // L1 (4096^3 [64G] voxels )
using vdb_tree_test_root_t = vdb_tree_test::RootNodeType;                                        // L0 ??? voxels

///////////////////////////////////////////////////////////////////////////////

struct FloatVoxel {
  openvdb::Coord coord;
  float value;
};
using cq_t = MpMcBoundedQueue<FloatVoxel, 4 << 20>;

} // namespace ork::lev2
