////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include "_vdb_impl.h"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

template <typename T> struct VoxelMap {

  VoxelMap(int width, int height, int depth)
      : _width(width)
      , _height(height)
      , _depth(depth) {
    _data.resize(width * height * depth);
  }

  void pset(int x, int y, int z, T value) {
    OrkAssert(x >= 0 && x < _width);
    OrkAssert(y >= 0 && y < _height);
    OrkAssert(z >= 0 && z < _depth);
    _data[x + y * _width + z * _width * _height] = value;
  }

  int _width  = 0;
  int _height = 0;
  int _depth  = 0;
  std::vector<T> _data;
};

using vmapf_t      = VoxelMap<float>;
using vmapf_ptr_t  = std::shared_ptr<vmapf_t>;
using vmapv3_t     = VoxelMap<fvec3>;
using vmapv3_ptr_t = std::shared_ptr<vmapv3_t>;

void pyinit_gfx_openvdb(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb = module_lev2.def_submodule("vdb", "OrkidOpenVDBBridge");
  auto ax   = ovdb.def_submodule("ax", "OrkidOpenVDBAxBridge");

  auto grid_type = py::class_<vdb_basegrid_t, vdb_basegrid_ptr_t>(ovdb, "BaseGrid")
                       .def("activeVoxelCount", [](vdb_basegrid_ptr_t grid) -> uint64_t { return grid->activeVoxelCount(); });
  type_codec->registerStdCodec<vdb_basegrid_ptr_t>(grid_type);
  /////////////////////////////////////////////////////////////////////////////////
  using coord_t = openvdb::Coord;
  auto coord_type = py::class_<coord_t>(ovdb, "Coord").def(py::init<int, int, int>())
  .def_property("x", [](const coord_t& coord) -> int { return coord.x(); }, [](coord_t& coord, int val) { coord.setX(val); })
  .def_property("y", [](const coord_t& coord) -> int { return coord.y(); }, [](coord_t& coord, int val) { coord.setY(val); })
  .def_property("z", [](const coord_t& coord) -> int { return coord.z(); }, [](coord_t& coord, int val) { coord.setZ(val); })
  .def( "__repr__", [](const coord_t& coord) -> std::string {
    std::ostringstream oss;
    oss << "Coord(" << coord.x() << "," << coord.y() << "," << coord.z() << ")";
    return oss.str();
  });
  type_codec->registerStdCodec<coord_t>(coord_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto vmapf_type = py::class_<vmapf_t, vmapf_ptr_t>(ovdb, "VoxelMapF").def(py::init<int, int, int>()).def("pset", &vmapf_t::pset);
  auto vmapv3_type =
      py::class_<vmapv3_t, vmapv3_ptr_t>(ovdb, "VoxelMapV3").def(py::init<int, int, int>()).def("pset", &vmapv3_t::pset);
  /////////////////////////////////////////////////////////////////////////////////
  struct citer_proxy {
    openvdb::FloatGrid::ValueOnCIter iter;
    citer_proxy(openvdb::FloatGrid::ValueOnCIter iter)
        : iter(iter) {
    }
    std::pair<openvdb::Coord, float> next() {
      if (iter) {
        auto coord = iter.getCoord();
        auto value = *iter;
        ++iter;
        return std::make_pair(coord, value);
      } else {
        throw py::stop_iteration();
      }
    }
  };
  using citer_proxy_ptr = std::shared_ptr<citer_proxy>;
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_fgrid_iter_type = py::class_<citer_proxy, citer_proxy_ptr>(ovdb, "FloatGridIter")
                                  .def("__iter__", [](citer_proxy_ptr iter) -> citer_proxy_ptr { return iter; })
                                  .def("__next__", [](citer_proxy_ptr iter) -> citer_proxy_ptr { //
                                    auto next = std::make_shared<citer_proxy>(iter->iter);
                                    return next;
                                  });
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::Transform
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_xform_type = py::class_<vdb_transform_t, vdb_transform_ptr_t>(ovdb, "Transform")
                             .def_static(
                                 "create",
                                 [](float scale) -> vdb_transform_ptr_t {
                                   py::gil_scoped_release release;
                                   auto xform = std::make_shared<openvdb::math::Transform>();
                                   xform->postScale(scale);
                                   return xform;
                                 })
                             .def("__repr__", [](vdb_transform_ptr_t xform) -> std::string {
                               std::ostringstream oss;
                               bool uni_scale  = xform->hasUniformScale();
                               bool is_linear  = xform->isLinear();
                               auto voxel_size = xform->voxelSize();
                               // double determinant = xform->determinant();
                               oss << "Transform:" << std::endl;
                               oss << "  UniformScale: " << uni_scale << std::endl;
                               oss << "  IsLinear: " << is_linear << std::endl;
                               oss << "  VoxelSize: " << voxel_size << std::endl;
                               // oss << "  Determinant: " << determinant << std::endl;
                               return oss.str();
                             });
  type_codec->registerStdCodec<vdb_transform_ptr_t>(ovdb_xform_type);
  /////////////////////////////////////////////////////////////////////////////////
  using int2_ptr_t = ork::python::unmanaged_ptr<vdb_tree_test_int2_t>;
  using int2_const_ptr_t = ork::python::unmanaged_ptr<const vdb_tree_test_int2_t>;
  auto ovdb_test_grid_int2_type = py::class_<int2_ptr_t>(ovdb, "TestGridInt2Node")
    .def_property_readonly("origin", [](int2_ptr_t node) -> coord_t { return node->origin(); })
    .def_property_readonly("version", [](int2_ptr_t node) -> int { return node->getVersion(); })
    .def_property_readonly("hash", [](int2_ptr_t node) -> uint64_t { return node->hash(); }) //
    .def( "__repr__", [](int2_ptr_t node) -> std::string {
      std::ostringstream oss;
      auto origin = node->origin();
      oss << "TGINT2(" << origin.x() << "," << origin.y() << "," << origin.z() << ")";
      return oss.str();
    });        
  type_codec->registerStdCodec<int2_ptr_t>(ovdb_test_grid_int2_type);
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::FloatGrid is already bound by nanobind in OpenVdb
  //  but we probably need it here also for lev2 gfx access
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_test_grid_type =
      py::class_<vdb_grid_test, vdb_basegrid_t, vdb_grid_test_ptr_t>(ovdb, "TestGrid")
          ///////////////////////////////////////////////////////
          .def_property_readonly_static("leaf_dim", [](py::object /* clazz */) -> size_t { return vdb_tree_test_leaf_t::DIM; })
          .def_property_readonly_static("int1_dim", [](py::object /* clazz */) -> size_t { return vdb_tree_test_int1_t::DIM; })
          .def_property_readonly_static("int2_dim", [](py::object /* clazz */) -> size_t { return vdb_tree_test_int2_t::DIM; })
          .def_property_readonly_static("leaf_max_voxels", [](py::object /* clazz */) -> size_t { return vdb_tree_test_leaf_t::NUM_VALUES; })
          .def_property_readonly_static("int1_max_voxels", [](py::object /* clazz */) -> size_t { return vdb_tree_test_int1_t::NUM_VOXELS; })
          .def_property_readonly_static("int2_max_voxels", [](py::object /* clazz */) -> size_t { return vdb_tree_test_int2_t::NUM_VOXELS; })
          ///////////////////////////////////////////////////////
          .def_static(
              "create",
              [](std::string name, vdb_transform_ptr_t xform, float background_level ) -> vdb_grid_test_ptr_t {
                py::gil_scoped_release release;
                TestGridCell background;
                background._level = background_level;
                auto grid = std::make_shared<vdb_grid_test>(background);
                grid->setName(name);
                grid->setTransform(xform);
                grid->setGridClass(openvdb::GRID_LEVEL_SET);
                return grid;
              })
          ///////////////////////////////////////////////////////
          .def_property_readonly(
              "clone", [](vdb_grid_test_ptr_t grid) -> vdb_grid_test_ptr_t { return std::make_shared<vdb_grid_test>(*grid); })
          ///////////////////////////////////////////////////////
          .def_property_readonly(
              "nonLeafCount", [](vdb_grid_test_ptr_t grid) -> size_t { return grid->tree().nonLeafCount(); })
          .def_property_readonly(
              "leafCount", [](vdb_grid_test_ptr_t grid) -> size_t { return grid->tree().leafCount(); })
          ///////////////////////////////////////////////////////
          .def( "worldToIndex", [](vdb_grid_test_ptr_t grid, fvec3 wpos) -> coord_t {
            py::gil_scoped_release release;
            auto& xform = grid->transform();
            auto ipos = xform.worldToIndex(openvdb::Vec3f(wpos.x, wpos.y, wpos.z));
            auto as_coord = coord_t(ipos.x(), ipos.y(), ipos.z());
            return as_coord;
          })
          ///////////////////////////////////////////////////////
          .def( "indexToWorld", [](vdb_grid_test_ptr_t grid, coord_t ipos) -> fvec3 {
            py::gil_scoped_release release;
            auto& xform = grid->transform();
            auto wpos = xform.indexToWorld(openvdb::Vec3f(ipos.x(), ipos.y(), ipos.z()));
            auto as_fvec3 = fvec3(wpos.x(), wpos.y(), wpos.z());
            return as_fvec3;
          })
          ///////////////////////////////////////////////////////
          .def(
              "fill",
              [](vdb_grid_test_ptr_t grid, fvec3 center, float radius, float value) {
                py::gil_scoped_release release;
                openvdb::CoordBBox bbox;
                fvec3 bbmin = center - fvec3(radius);
                fvec3 bbmax = center + fvec3(radius);
                bbox.expand(openvdb::Coord(bbmin.x, bbmin.y, bbmin.z));
                bbox.expand(openvdb::Coord(bbmax.x, bbmax.y, bbmax.z));
                grid->fill(bbox, TestGridCell(value));
              })
          ///////////////////////////////////////////////////////
          .def(
              "setVoxel",
              [](vdb_grid_test_ptr_t grid, fvec3 coord, float value) {
                py::gil_scoped_release release;
                auto coord_w = openvdb::Vec3f(coord.x, coord.y, coord.z);
                auto coord_i = grid->worldToIndex(coord_w);
                auto coord_ii = openvdb::Coord(coord_i.x(), coord_i.y(), coord_i.z());
                auto tgc = grid->tree().getValue(coord_ii);
                tgc._level = value;
                tgc._rgb = fvec3(value);
                //tgc._writeCount->fetch_add(1);
                grid->tree().setValue(coord_ii, tgc);
              })
          ///////////////////////////////////////////////////////
          .def(
              "accumVoxel",
              [](vdb_grid_test_ptr_t grid, fvec3 coord, float value) {
                py::gil_scoped_release release;
                auto coord_w = openvdb::Vec3f(coord.x, coord.y, coord.z);
                auto coord_i = grid->worldToIndex(coord_w);
                auto coord_ii = openvdb::Coord(coord_i.x(), coord_i.y(), coord_i.z());
                auto tgc = grid->tree().getValue(coord_ii);
                tgc._level += value;
                tgc._rgb = fvec3(value);
                //tgc._writeCount->fetch_add(1);
                grid->tree().setValue(coord_ii, tgc);
              })
          ///////////////////////////////////////////////////////
          .def(
              "accumVoxelRGB",
              [](vdb_grid_test_ptr_t grid, fvec3 coord, fvec3 value) {
                py::gil_scoped_release release;
                auto coord_w = openvdb::Vec3f(coord.x, coord.y, coord.z);
                auto coord_i = grid->worldToIndex(coord_w);
                auto coord_ii = openvdb::Coord(coord_i.x(), coord_i.y(), coord_i.z());
                auto tgc = grid->tree().getValue(coord_ii);
                tgc._level = 1.0;
                tgc._rgb += value;
                /*int icount = tgc._writeCount->fetch_add(1);
                if((icount%16)==15){
                  printf("accumVoxelRGB<%d> _rgb<%g %g %g>\n", icount, tgc._rgb.x, tgc._rgb.y, tgc._rgb.z);
                }*/
                grid->tree().setValue(coord_ii, tgc);
              })
          ///////////////////////////////////////////////////////
          .def(
              "tileStats",
              [](vdb_grid_test_ptr_t grid) {
                auto& tree = grid->tree();
                size_t num_l0_tiles = 0;
                size_t num_l1_tiles = 0;
                size_t num_l2_tiles = 0;
                size_t num_l3_tiles = 0;
                for (auto iter = tree.beginNode(); iter; ++iter) {
                  switch (iter.getDepth()) { //
                    case 0: { //
                      vdb_tree_test_root_t* node = nullptr;
                      iter.getNode(node);
                      if (node) { //
                        num_l0_tiles++;
                      };
                      break; 
                    }
                    case 1: { //
                      vdb_tree_test_int1_t* node = nullptr; 
                      iter.getNode(node); 
                      if (node) { //
                        num_l1_tiles++;
                      }; 
                      break; 
                    }
                    case 2: { //
                      vdb_tree_test_int2_t* node = nullptr; 
                      iter.getNode(node); 
                      if (node) { //
                        num_l2_tiles++;
                      }; 
                      break; 
                    }
                    case 3: { //
                      vdb_tree_test_leaf_t* node = nullptr; 
                      iter.getNode(node); 
                      if (node) {
                        num_l3_tiles++;
                      }; 
                      break; 
                    }
                  }
                }
                printf("num_l0_tiles<%zu> num_l1_tiles<%zu> num_l2_tiles<%zu> num_l3_tiles<%zu>\n", num_l0_tiles, num_l1_tiles, num_l2_tiles, num_l3_tiles);                  
              })
          ///////////////////////////////////////////////////////
          .def_property_readonly(
              "int2nodes",
              [](vdb_grid_test_ptr_t grid) -> py::list {
                py::list int2nodes;
                auto& tree = grid->tree();
                for (auto iter = tree.beginNode(); iter; ++iter) {
                  switch (iter.getDepth()) { //
                    case 2: { //
                      vdb_tree_test_int2_t* node = nullptr; 
                      iter.getNode(node); 
                      if (node) { //
                        int2nodes.append(int2_ptr_t(node));
                      }; 
                      break; 
                    }
                    default:
                      break;
                  }
                }
                return int2nodes;
              })
          ///////////////////////////////////////////////////////
          .def(
              "csgDifference",
              [](vdb_grid_test_ptr_t grid, vdb_grid_test_ptr_t other) -> vdb_grid_test_ptr_t {
                py::gil_scoped_release release;
                auto diff = openvdb::tools::csgDifferenceCopy(*grid, *other);
                return diff;
              });
  type_codec->registerStdCodec<vdb_grid_test_ptr_t>(ovdb_test_grid_type);
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::FloatGrid is already bound by nanobind in OpenVdb
  //  but we probably need it here also for lev2 gfx access
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_fgrid_type =
      py::class_<vdb_floatgrid_t, vdb_basegrid_t, vdb_floatgrid_ptr_t>(ovdb, "FloatGrid")
          .def_static(
              "create",
              [](std::string name, vdb_transform_ptr_t xform, float background) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto grid = std::make_shared<openvdb::FloatGrid>(background);
                grid->setName(name);
                grid->setTransform(xform);
                grid->setGridClass(openvdb::GRID_LEVEL_SET);
                return grid;
              })
          .def_static(
              "createLevelSetSphere",
              [](std::string name, float radius, fvec3 center, float vxlsize, float hwidth) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto grid = openvdb::tools::createLevelSetSphere<vdb_floatgrid_t>(
                    radius, openvdb::Vec3f(center.x, center.y, center.z), vxlsize, hwidth);
                grid->setName(name);
                // createLevelSetSphere (float radius, const openvdb::Vec3f &center, float voxelSize, float
                // halfWidth=float(LEVEL_SET_HALF_WIDTH), InterruptT *interrupt=nullptr, bool threaded=true)
                return grid;
              })
          ///////////////////////////////////////////////////////
          .def(
              "fill",
              [](vdb_floatgrid_ptr_t grid, fvec3 center, float radius, float value) {
                py::gil_scoped_release release;
                openvdb::CoordBBox bbox;
                auto coord_va = openvdb::Vec3f(center.x - radius, center.y - radius, center.z - radius);
                auto coord_vb = openvdb::Vec3f(center.x + radius, center.y + radius, center.z + radius);
                auto coord_ia = grid->worldToIndex(coord_va);
                auto coord_ib = grid->worldToIndex(coord_vb);
                bbox.expand(openvdb::Coord(coord_va.x(), coord_va.y(), coord_va.z()));
                bbox.expand(openvdb::Coord(coord_vb.x(), coord_vb.y(), coord_vb.z()));
                grid->fill(bbox, value);
              })
          ///////////////////////////////////////////////////////
          .def(
              "blitWithBrush",
              [](vdb_floatgrid_ptr_t grid, fvec3 center, vmapf_ptr_t vmap) {
                py::gil_scoped_release release;
                int width   = vmap->_width;
                int height  = vmap->_height;
                int depth   = vmap->_depth;
                int w_start = -width / 2;
                int h_start = -height / 2;
                int d_start = -depth / 2;

                auto xform = grid->transform();
                auto& tree = grid->tree();
                for (int ix = 0; ix < width; ix++) {
                  int ibipx = ix + w_start;
                  for (int iy = 0; iy < height; iy++) {
                    int ibipy = iy + h_start;
                    for (int iz = 0; iz < depth; iz++) {
                      int ibipz     = iz + d_start;
                      auto coord_vb = openvdb::Vec3f(center.x + ibipx, center.y + ibipy, center.z + ibipz);
                      // auto coord_ib = grid->worldToIndex(coord_vb); dont need this ?
                      float value  = vmap->_data[ix + iy * width + iz * width * height];
                      auto coord   = openvdb::Coord(coord_vb.x(), coord_vb.y(), coord_vb.z());
                      float prev   = tree.getValue(coord);
                      float newval = value * prev;
                      // prevent NAN's
                      newval = newval + 1e-6f;
                      tree.setValue(coord, newval);
                    }
                  }
                }
              })
          ///////////////////////////////////////////////////////
          .def(
              "csgDifference",
              [](vdb_floatgrid_ptr_t grid, vdb_floatgrid_ptr_t other) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto diff = openvdb::tools::csgDifferenceCopy(*grid, *other);
                return diff;
              })
          ///////////////////////////////////////////////////////
          .def(
              "csgUnion",
              [](vdb_floatgrid_ptr_t grid, vdb_floatgrid_ptr_t other) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto diff = openvdb::tools::csgUnionCopy(*grid, *other);
                return diff;
              })
          ///////////////////////////////////////////////////////
          .def(
              "csgIntersection",
              [](vdb_floatgrid_ptr_t grid, vdb_floatgrid_ptr_t other) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto diff = openvdb::tools::csgIntersectionCopy(*grid, *other);
                return diff;
              })
          ///////////////////////////////////////////////////////
          .def_property(
              "background",
              [](vdb_floatgrid_ptr_t grid) -> float { return grid->background(); },
              [](vdb_floatgrid_ptr_t grid, float value) {
                size_t grainSize = 32;
                openvdb::tools::changeLevelSetBackground(grid->tree(), value, true, grainSize);
              })
          ///////////////////////////////////////////////////////
          .def_property_readonly(
              "onValueSequence",
              [](vdb_floatgrid_ptr_t grid) -> citer_proxy_ptr {
                auto iter = std::make_shared<citer_proxy>(grid->cbeginValueOn());
                return iter;
              })
          .def_property_readonly("xform", [](vdb_floatgrid_ptr_t grid) -> vdb_transform_ptr_t { return grid->transformPtr(); })
          .def_property_readonly(
              "clone", [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t { return std::make_shared<vdb_floatgrid_t>(*grid); })
          ///////////////////////////////////////////////////////
          .def(
              "scatterVoxels",
              [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto result = grid->copyWithNewTree();
                std::random_device rand_dev;
                std::default_random_engine e1(rand_dev());
                std::uniform_int_distribution<int> uniform_dist(0, 2);
                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                  const auto& leaf = *leafIter;
                  for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                    float value = *voxelIter;
                    auto icoord = voxelIter.getCoord();
                    int itx     = uniform_dist(e1) - 1;
                    int ity     = uniform_dist(e1) - 1;
                    int itz     = uniform_dist(e1) - 1;
                    icoord      = icoord + openvdb::Coord(itx, ity, itz);
                    result->tree().setValueOn(icoord, value);
                  }
                }
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "scatterVoxels2",
              [](vdb_floatgrid_ptr_t grid) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto result            = grid->copyWithNewTree();
                static auto cq         = std::make_shared<cq_t>();
                int num_points         = grid->tree().activeLeafVoxelCount();
                std::atomic<int> count = num_points;
                std::random_device rand_dev;

                for (auto itl0 = grid->tree().cbeginRootChildren(); itl0; ++itl0) {
                  auto op = [=, &rand_dev]() {
                    std::default_random_engine e1(rand_dev());
                    std::uniform_int_distribution<int> uniform_dist(0, 2);
                    FloatVoxel fv;
                    for (auto itl1 = itl0->cbeginChildOn(); itl1; ++itl1) {
                      for (auto itl2 = itl1->cbeginChildOn(); itl2; ++itl2) {
                        const float* src_data = itl2->buffer().data();
                        for (auto it_vox = itl2->cbeginValueOn(); it_vox; ++it_vox) {
                          int itx  = uniform_dist(e1) - 1;
                          int ity  = uniform_dist(e1) - 1;
                          int itz  = uniform_dist(e1) - 1;
                          fv.coord = it_vox.getCoord() + openvdb::Coord(itx, ity, itz);
                          auto idx = it_vox.pos();
                          fv.value = src_data[idx];
                          cq->push(fv);
                        }
                      }
                    }
                  };
                  opq::concurrentQueue()->enqueue(op);
                }
                FloatVoxel fv;
                while ((count.load() > 0)) {
                  if (cq->try_pop(fv)) {
                    result->tree().setValueOn(fv.coord, fv.value);
                    count--;
                  } else {
                    sched_yield();
                  }
                }
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "translatedVoxels",
              [](vdb_floatgrid_ptr_t grid, fvec3 trans) -> vdb_floatgrid_ptr_t {
                py::gil_scoped_release release;
                auto result = grid->copyWithNewTree();
                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                  const auto& leaf = *leafIter;
                  for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                    float value = *voxelIter;
                    auto icoord = voxelIter.getCoord();
                    icoord      = icoord + openvdb::Coord(trans.x, trans.y, trans.z);
                    result->tree().setValueOn(icoord, value);
                  }
                }
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "setVoxel",
              [](vdb_floatgrid_ptr_t grid, fvec3 coord, float value) {
                py::gil_scoped_release release;
                grid->tree().setValue(openvdb::Coord(coord.x, coord.y, coord.z), value);
              })
          ///////////////////////////////////////////////////////
          .def(
              "worldToIndex",
              [](vdb_floatgrid_ptr_t grid, fvec3 coord) -> fvec3 {
                py::gil_scoped_release release;
                auto index = grid->transform().worldToIndex(openvdb::Vec3f(coord.x, coord.y, coord.z));
                return fvec3(index.x(), index.y(), index.z());
              })
          ///////////////////////////////////////////////////////
          .def(
              "drawLineI",
              [](vdb_floatgrid_ptr_t grid, fvec3 start, fvec3 end, float value) {
                py::gil_scoped_release release;
                // Calculate differences
                float dx = end.x - start.x;
                float dy = end.y - start.y;
                float dz = end.z - start.z;

                // Determine the number of steps needed
                float steps = std::max({std::fabs(dx), std::fabs(dy), std::fabs(dz)});

                // Calculate the increment in each coordinate
                float Xinc = dx / steps;
                float Yinc = dy / steps;
                float Zinc = dz / steps;

                // Initialize starting point
                float x = start.x;
                float y = start.y;
                float z = start.z;

                // Generate points along the line
                for (int i = 0; i <= steps; i++) {
                  int ix = static_cast<int>(std::round(x));
                  int iy = static_cast<int>(std::round(y));
                  int iz = static_cast<int>(std::round(z));
                  grid->tree().setValue(openvdb::Coord(ix, iy, iz), value);
                  x += Xinc;
                  y += Yinc;
                  z += Zinc;
                }
              })
          ///////////////////////////////////////////////////////
          .def(
              "toMesh",
              [](vdb_floatgrid_ptr_t grid, float isovalue) -> py::dict {
                std::vector<openvdb::Vec3s> points;
                std::vector<openvdb::Vec4I> quads;
                std::vector<openvdb::Vec3I> tris;
                {
                  py::gil_scoped_release release;

                  bool relax       = false;
                  float adaptivity = 0.0f;

                  openvdb::tools::volumeToMesh(*grid, points, tris, quads, isovalue, adaptivity, relax);
                }
                auto vertices = py::list();
                auto indices  = py::list();
                for (auto& point : points) {
                  auto world = grid->transform().indexToWorld(point);
                  vertices.append(fvec3(point.x(), point.y(), point.z()));
                }
                for (auto& quad : quads) {
                  indices.append(4);
                  indices.append(quad[3]);
                  indices.append(quad[2]);
                  indices.append(quad[1]);
                  indices.append(quad[0]);
                }
                for (auto& tri : tris) {
                  indices.append(3);
                  indices.append(tri[2]);
                  indices.append(tri[1]);
                  indices.append(tri[0]);
                }

                auto result        = py::dict();
                result["vertices"] = vertices;
                result["faces"]    = indices;
                return result;
              })
          ///////////////////////////////////////////////////////
          .def(
              "toTriMesh",
              [](vdb_floatgrid_ptr_t grid, float isovalue) -> py::dict {
                std::vector<openvdb::Vec3s> points;
                std::vector<openvdb::Vec4I> quads;
                std::vector<openvdb::Vec3I> tris;
                {
                  py::gil_scoped_release release;
                  openvdb::tools::volumeToMesh(*grid, points, quads, isovalue);
                }
                auto vertices = py::list();
                auto indices  = py::list();
                for (auto& point : points) {
                  auto world = grid->transform().indexToWorld(point);
                  vertices.append(fvec3(point.x(), point.y(), point.z()));
                }
                for (auto& quad : quads) {
                  indices.append(3);
                  indices.append(quad[0]);
                  indices.append(quad[2]);
                  indices.append(quad[1]);

                  indices.append(3);
                  indices.append(quad[3]);
                  indices.append(quad[2]);
                  indices.append(quad[0]);
                }
                auto result        = py::dict();
                result["vertices"] = vertices;
                result["faces"]    = indices;
                return result;
              })
          .def("saveToVDB", [](vdb_floatgrid_ptr_t grid, py::object path) {
            auto as_str     = py::str(path);
            auto as_std_str = as_str.cast<std::string>();
            py::gil_scoped_release release;

            openvdb::io::File file(as_std_str);
            openvdb::GridPtrVec grids;
            grids.push_back(grid);
            file.write(grids);
            file.close();
          });
  type_codec->registerStdCodec<vdb_floatgrid_ptr_t>(ovdb_fgrid_type);
  /////////////////////////////////////////////////////////////////////////////////
  // AX volume executable
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_ax_ve_type = py::class_<vdb_volume_exec_t, vdb_volume_exec_ptr_t>(ax, "VolumeExecutable")
                             .def_static(
                                 "compile",
                                 [](std::string code, vdb_custom_data_ptr_t cdata = nullptr) -> vdb_volume_exec_ptr_t {
                                   py::gil_scoped_release release;
                                   openvdb::ax::Compiler compiler;
                                   return compiler.compile<vdb_volume_exec_t>(code, cdata);
                                 })
                             .def("executeOnGrid", [](vdb_volume_exec_ptr_t ve, vdb_floatgrid_ptr_t grid) {
                               py::gil_scoped_release release;
                               ve->execute(*grid);
                             });
  type_codec->registerStdCodec<vdb_volume_exec_ptr_t>(ovdb_ax_ve_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cdata_type = py::class_<vdb_custom_data_t, vdb_custom_data_ptr_t>(ax, "CustomData")
                        .def(py::init<>())
                        .def("set", [](vdb_custom_data_ptr_t cdata, std::string key, py::object value) {
                          if (py::isinstance<py::int_>(value)) {
                            auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<int>>(key);
                            typed->setValue(py::cast<int>(value));
                          } else if (py::isinstance<py::float_>(value)) {
                            auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<float>>(key);
                            // printf("set float key<%s> value<%f>\n", key.c_str(), py::cast<float>(value));
                            typed->setValue(py::cast<float>(value));
                          } else {
                            OrkAssert(false); // unsupported type
                          }
                        });
  type_codec->registerStdCodec<vdb_custom_data_ptr_t>(cdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // openvdb::FloatGrid is already bound by nanobind in OpenVdb
  //  but we probably need it here also for lev2 gfx access
  /////////////////////////////////////////////////////////////////////////////////
  auto ovdb_v3grid_type =
      py::class_<vdb_vec3grid_t, vdb_basegrid_t, vdb_vec3grid_ptr_t>(ovdb, "Vec3FGrid")
          .def_static(
              "create",
              [](std::string name, vdb_transform_ptr_t xform, fvec3 background) -> vdb_vec3grid_ptr_t {
                py::gil_scoped_release release;
                auto bg   = openvdb::Vec3f(background.x, background.y, background.z);
                auto grid = std::make_shared<vdb_vec3grid_t>(bg);
                grid->setName(name);
                grid->setTransform(xform);
                // grid->setGridClass(openvdb::GRID_LEVEL_SET);
                return grid;
              })
          ///////////////////////////////////////////////////////
          .def(
              "fill",
              [](vdb_vec3grid_ptr_t grid, fvec3 center, float radius, fvec3 value) {
                py::gil_scoped_release release;
                openvdb::CoordBBox bbox;
                // printf("fill center<%f %f %f> radius<%f> value<%f %f %f>\n", center.x, center.y, center.z, radius, value.x,
                // value.y, value.z);
                auto coord_va = openvdb::Vec3f(center.x - radius, center.y - radius, center.z - radius);
                auto coord_vb = openvdb::Vec3f(center.x + radius, center.y + radius, center.z + radius);
                auto coord_ia = grid->worldToIndex(coord_va);
                auto coord_ib = grid->worldToIndex(coord_vb);

                bbox.expand(openvdb::Coord(coord_va.x(), coord_va.y(), coord_va.z()));
                bbox.expand(openvdb::Coord(coord_vb.x(), coord_vb.y(), coord_vb.z()));
                grid->fill(bbox, openvdb::Vec3f(value.x, value.y, value.z));
              })
          .def(
              "insertPoints",
              [](vdb_vec3grid_ptr_t grid, primitives::pointsdata_ptr_t points) {
                py::gil_scoped_release release;

                auto dblock       = points->_datablock;
                size_t num_points = points->_num_points;
                auto& tree                 = grid->tree();
                static size_t total_points = 0;
                total_points += num_points;
                size_t leafCount        = tree.leafCount();
                size_t activeVoxelCount = grid->activeVoxelCount();

                switch(points->_format){
                  case EVtxStreamFormat::V12C4:{
                    OrkAssert(dblock->length() == (num_points * sizeof(VtxV12C4)));
                    auto typed_points = (const VtxV12C4*)dblock->data();
                    for (size_t i = 0; i < num_points; i++) {
                      const auto& vtx = typed_points[i];
                      openvdb::Vec3f wpos(vtx.x, vtx.y, vtx.z);
                      auto ipos = grid->worldToIndex(wpos);
                      auto icoord = openvdb::Coord(ipos.x(), ipos.y(), ipos.z());
                      uint32_t abgr = vtx.color;
                      float r       = (abgr >> 16) & 0xff;
                      float g       = (abgr >> 8) & 0xff;
                      float b       = (abgr >> 0) & 0xff;
                      tree.setValue(icoord, openvdb::Vec3f(r,g,b));
                    }
                    break;
                  }
                  case EVtxStreamFormat::V12T8:{
                    auto typed_points = (const VtxV12T8*)dblock->data();
                    OrkAssert(dblock->length() == (num_points * sizeof(VtxV12T8)));
                    for (size_t i = 0; i < num_points; i++) {
                      const auto& vtx = typed_points[i];
                      openvdb::Vec3f wpos(vtx.pos.x, vtx.pos.y, vtx.pos.z);
                      auto ipos = grid->worldToIndex(wpos);
                      auto icoord = openvdb::Coord(ipos.x(), ipos.y(), ipos.z());
                      tree.setValue(icoord, openvdb::Vec3f(1,0,0));
                    }
                    break;
                  }
                  default:
                    OrkAssert(false);
                    break;
                }
              })
          ///////////////////////////////////////////////////////
          .def_property(
              "background",
              [](vdb_vec3grid_ptr_t grid) -> fvec3 { //
                auto bg = grid->background();
                return fvec3(bg.x(), bg.y(), bg.z());
              },
              [](vdb_vec3grid_ptr_t grid, fvec3 value) {
                size_t grainSize = 32;
                auto as_vec3f    = openvdb::Vec3f(value.x, value.y, value.z);
                // openvdb::tools::changeLevelSetBackground(grid->tree(), as_vec3f, true, grainSize);
              })
          ///////////////////////////////////////////////////////
          .def_property_readonly(
              "clone",
              [](vdb_vec3grid_ptr_t grid) -> vdb_vec3grid_ptr_t { //
                return std::make_shared<vdb_vec3grid_t>(*grid);   //
              })
          ///////////////////////////////////////////////////////
          .def("exportToOpenVdbFile", [](vdb_vec3grid_ptr_t grid, py::object path) {
            auto as_str     = py::str(path);
            auto as_std_str = as_str.cast<std::string>();
            py::gil_scoped_release release;

            openvdb::io::File file(as_std_str);
            openvdb::GridPtrVec grids;
            grids.push_back(grid);
            file.write(grids);
            file.close();
          });
  type_codec->registerStdCodec<vdb_vec3grid_ptr_t>(ovdb_v3grid_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2
