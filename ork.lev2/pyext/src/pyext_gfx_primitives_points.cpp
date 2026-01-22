////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>

#if defined(ENABLE_PYTORCH)
#undef ThreadLocal           // conflicts with c10
#include <torch/extension.h> // for PyTorch C++ extension
#endif

#include "pyext.inl"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/openvdb.h>
#include <ork/kernel/memcpy.inl>

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

using vdb_floatgrid_t     = openvdb::FloatGrid;
using vdb_floatgrid_ptr_t = std::shared_ptr<vdb_floatgrid_t>;

namespace ork::lev2 {

void pyinit_gfx_primitives_points(py::module& primitives) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto pointdata_type =
      py::class_<primitives::PointsData, primitives::pointsdata_ptr_t>(primitives, "PointsData")
          .def(py::init<>([](datablock_ptr_t db, int num_points, crcstring_ptr_t format) {
            auto typed_fmt = (EVtxStreamFormat)format->hashed();
            return std::make_shared<primitives::PointsData>(db, num_points, typed_fmt);
          }))
          .def(
              "dump",
              [](primitives::pointsdata_ptr_t prim) { //
                auto db = prim->_datablock;
                auto pvtx = (const VtxV12C4*) db->data();
                printf("PointsData: num_points<%d>\n", prim->_num_points);
                fflush(stdout);
                for( size_t i=0; i<prim->_num_points; i++ ){
                  printf( "pt<%zu> pos<%f %f %f>\n", i, pvtx[i].x, pvtx[i].y, pvtx[i].z );
                }
              })
          .def(
              "transformInPlace",
              [](primitives::pointsdata_ptr_t prim, const fmtx4& mtx) { //
                prim->transformInPlace(mtx);
              })
          .def(
              "transformed",
              [](primitives::pointsdata_ptr_t prim, const fmtx4& mtx) -> primitives::pointsdata_ptr_t { //
                return prim->transformed(mtx);
              })
          .def(
              "depthClamped",
              [](primitives::pointsdata_ptr_t prim, float zmin, float zmax) -> primitives::pointsdata_ptr_t { //
                return prim->depthClamped(zmin, zmax);
              })
          .def(
              "colorClamped",
              [](primitives::pointsdata_ptr_t prim, float imin, float imax) -> primitives::pointsdata_ptr_t { //
                return prim->colorClamped(imin, imax);
              })
          .def(
              "colorHsvScaleBias",
              [](primitives::pointsdata_ptr_t prim, fvec2 hue, fvec2 sat, fvec2 val) -> primitives::pointsdata_ptr_t { //
                return prim->hsvScaleBias(hue, sat, val);
              })
          .def(
              "stochasticSample",
              [](primitives::pointsdata_ptr_t prim, float probablity) -> primitives::pointsdata_ptr_t { //
                return prim->stochasticSample(probablity);
              })
          .def_property_readonly(
              "rgbSwizzled",
              [](primitives::pointsdata_ptr_t prim) -> primitives::pointsdata_ptr_t { //
                return prim->swizzleRGB();
              })
          .def("convertToV12C4", [](primitives::pointsdata_ptr_t prim, image_ptr_t image) -> primitives::pointsdata_ptr_t { //
            return prim->convertToV12C4(image);
          });
  type_codec->registerStdCodec<primitives::pointsdata_ptr_t>(pointdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pointsprim_type = //
      py::class_<primitives::PointsPrimitive<VtxV12C4>, primitives::points_v12c4_ptr_t>(primitives, "PointsPrimitiveV12C4")
          .def(
              "create",
              [](int numpoints) -> primitives::points_v12c4_ptr_t {
                return std::make_shared<primitives::PointsPrimitive<VtxV12C4>>(numpoints);
              })
          .def(
              "createWithSSBO",
              [](int numpoints, fxshaderstoragebuffer_ptr_t ssbo) -> primitives::points_v12c4_ptr_t {
                return std::make_shared<primitives::PointsPrimitive<VtxV12C4>>(numpoints, ssbo.get());
              })
          .def_property(
              "debug",                                          //
              [](primitives::points_v12c4_ptr_t prim) -> bool { //
                return prim->_debug;
              },
              [](primitives::points_v12c4_ptr_t prim, bool bv) { //
                prim->_debug = bv;
              })
          .def(
              "createFromVdbFloatGrid",
              [](vdb_floatgrid_ptr_t grid, ctx_t context) -> primitives::points_v12c4_ptr_t {
                int num_points = grid->tree().activeLeafVoxelCount();
                // printf("num_points<%d>\n", num_points);
                auto prim        = std::make_shared<primitives::PointsPrimitive<VtxV12C4>>(num_points);
                VtxV12C4* points = prim->lock(context.get());
                int point_index  = 0;
                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                  const auto& leaf = *leafIter;

                  // Iterate over active voxels within the leaf
                  for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                    // Get the voxel coordinates and value
                    openvdb::Coord coord = voxelIter.getCoord();
                    float value          = *voxelIter;

                    // Convert voxel coordinates to world coordinates
                    openvdb::Vec3f worldPosition = grid->transform().indexToWorld(coord);

                    // Populate the points array (adapt as necessary)
                    points[point_index].x = worldPosition.x();
                    points[point_index].y = worldPosition.y();
                    points[point_index].z = worldPosition.z();
                    uint32_t bgra         = 0;
                    bgra |= (uint32_t(value * 255.0f) & 0xff) << 16;
                    bgra |= (uint32_t(value * 255.0f) & 0xff) << 8;
                    bgra |= (uint32_t(value * 255.0f) & 0xff) << 0;

                    points[point_index].color = bgra;

                    point_index++;
                  }
                }
                prim->unlock(context.get());
                return prim;
              })
#if defined(ENABLE_PYTORCH)
          .def(
              "updatePositionWithTorchTensor",
              [](primitives::points_v12c4_ptr_t prim, torchtensor_ptr_t l2tensor, size_t start, ctx_t context) {
                /////////////////////////
                // wait for tensor to be on CPU
                /////////////////////////
                if (0) {
                  py::gil_scoped_release release;
                  bool is_cpu = (l2tensor->_state.load() == 1);
                  while (not is_cpu) {
                    is_cpu = (l2tensor->_state.load() == 1);
                    if (not is_cpu) {
                      ::ork::usleep(10);
                    }
                  }
                }
                auto as_tt  = l2tensor->_impl.get<torch::Tensor>();
                bool dim_ok = (as_tt.dim() == 3); // 3rd dim is channels
                if (not dim_ok) {
                  printf("ERROR: tensor dim<%d> is not 3\n", int(as_tt.dim()));
                  OrkAssert(false);
                }
                // printf("dim_ok<%d>\n", (int) dim_ok);
                size_t num_points = as_tt.size(1);
                OrkAssert(as_tt.is_contiguous());
                OrkAssert(as_tt.is_cpu());
                OrkAssert(as_tt.dtype() == torch::kFloat32);
                OrkAssert((start + num_points) <= prim->_capacity) auto src_data = (const float*)as_tt.data_ptr();
                auto dst_data                                                    = (VtxV12C4*)prim->lock(context.get(), num_points);
                for (size_t i = 0; i < num_points; i++) {
                  size_t j  = (start + i) * 3;
                  auto& out = dst_data[i];
                  out.x     = src_data[j + 0];
                  out.y     = src_data[j + 1];
                  out.z     = src_data[j + 2];
                }
                prim->unlock(context.get());
              })
#endif
          .def(
              "updateWithPointsData",
              [](primitives::points_v12c4_ptr_t prim, primitives::pointsdata_ptr_t pdata, ctx_t context) {
                py::gil_scoped_release release;
                size_t dblock_len = pdata->_datablock->length();
                OrkAssert(dblock_len % sizeof(VtxV12C4) == 0);
                size_t num_points                                         = dblock_len / sizeof(VtxV12C4);
                OrkAssert(num_points <= prim->_capacity) VtxV12C4* points = prim->lock(context.get(), num_points);
                memcpy_fast(points, pdata->_datablock->data(), dblock_len);
                prim->unlock(context.get());
              })
          .def(
              "updateWithV12C4DataBlock",
              [](primitives::points_v12c4_ptr_t prim, datablock_ptr_t dblock, ctx_t context, bool swizzle_rgb = false) {
                py::gil_scoped_release release;
                size_t dblock_len = dblock->length();
                OrkAssert(dblock_len % sizeof(VtxV12C4) == 0);
                size_t num_points                                         = dblock_len / sizeof(VtxV12C4);
                OrkAssert(num_points <= prim->_capacity) VtxV12C4* points = prim->lock(context.get(), num_points);
                memcpy_fast(points, dblock->data(), dblock_len);
                if (swizzle_rgb) {
                  for (size_t i = 0; i < num_points; i++) {
                    auto& vtx      = points[i];
                    uint32_t color = vtx.color;
                    uint32_t r     = (color >> 16) & 0xff;
                    uint32_t g     = (color >> 8) & 0xff;
                    uint32_t b     = (color >> 0) & 0xff;
                    vtx.color      = (b << 16) | (g << 8) | (r << 0);
                  }
                }
                prim->unlock(context.get());
              })
          .def(
              "updateWithVdbFloatGrid",
              [](primitives::points_v12c4_ptr_t prim, //
                 vdb_floatgrid_ptr_t grid,            //
                 ctx_t context) {
                py::gil_scoped_release release;
                int num_points = grid->tree().activeLeafVoxelCount();
                OrkAssert(num_points < prim->_capacity)
                    // printf("num_points<%d>\n", num_points);
                    VtxV12C4* points = prim->lock(context.get(), num_points);
                int point_index      = 0;
                auto& xform          = grid->transform();
                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                  const auto& leaf = *leafIter;

                  // Iterate over active voxels within the leaf
                  for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                    openvdb::Coord icoord = voxelIter.getCoord();
                    openvdb::Vec3f wpos   = xform.indexToWorld(icoord);
                    float value           = (*voxelIter) * 255.0f;
                    auto grey             = uint32_t(value) & 0xff;
                    if (point_index < num_points) {
                      // OrkAssert(point_index<num_points);
                      auto& out_point = points[point_index++];
                      out_point.x     = wpos.x();
                      out_point.y     = wpos.y();
                      out_point.z     = wpos.z();
                      out_point.color = (grey << 16) | (grey << 8) | (grey << 0);
                    }
                  }
                }
                prim->unlock(context.get());
                return prim;
              })
          .def(
              "updateWithVdbVec3Grid",
              [](primitives::points_v12c4_ptr_t prim, //
                 vdb_vec3grid_ptr_t grid,             //
                 ctx_t context) {
                py::gil_scoped_release release;
                int num_points = grid->tree().activeLeafVoxelCount();
                OrkAssert(num_points < prim->_capacity)
                    // printf("updateWithVdbVec3Grid:num_points<%d>\n", num_points);
                    VtxV12C4* points = prim->lock(context.get(), num_points);
                int point_index      = 0;
                auto& xform          = grid->transform();
                // ork::Timer timer;
                // timer.Start();
                for (auto voxelIter = grid->cbeginValueOn(); voxelIter; ++voxelIter) {
                  openvdb::Coord icoord = voxelIter.getCoord();
                  openvdb::Vec3f wpos   = xform.indexToWorld(icoord);
                  auto value            = (*voxelIter);
                  if (point_index < num_points) {
                    // OrkAssert(point_index<num_points);
                    auto& out_point = points[point_index++];
                    out_point.x     = wpos.x();
                    out_point.y     = wpos.y();
                    out_point.z     = wpos.z();

                    float r         = value.x() * 255.0f;
                    float g         = value.y() * 255.0f;
                    float b         = value.z() * 255.0f;
                    uint32_t r8     = uint32_t(r) & 0xff;
                    uint32_t g8     = uint32_t(g) & 0xff;
                    uint32_t b8     = uint32_t(b) & 0xff;
                    out_point.color = (b8 << 16) | (g8 << 8) | (r8 << 0);
                  }
                }
                // float elapsed = timer.SecsSinceStart();
                // printf("updateWithVdbVec3Grid:elapsed<%f>\n", elapsed);
                prim->unlock(context.get());
                return prim;
              })
          .def(
              "updateWithVdbTestGrid",
              [](primitives::points_v12c4_ptr_t prim, //
                 vdb_grid_test_ptr_t grid,            //
                 float colorscale,
                 ctx_t context) {
                py::gil_scoped_release release;
                int num_points = grid->tree().activeLeafVoxelCount();
                OrkAssert(num_points < prim->_capacity)
                    // printf("updateWithVdbVec3Grid:num_points<%d>\n", num_points);
                    VtxV12C4* points = prim->lock(context.get(), num_points);
                int point_index      = 0;
                auto& xform          = grid->transform();
                // ork::Timer timer;
                // timer.Start();
                for (auto voxelIter = grid->cbeginValueOn(); voxelIter; ++voxelIter) {
                  openvdb::Coord icoord   = voxelIter.getCoord();
                  openvdb::Vec3f wpos     = xform.indexToWorld(icoord);
                  const TestGridCell& TGC = (*voxelIter);
                  if (point_index < num_points) {
                    // OrkAssert(point_index<num_points);
                    auto& out_point = points[point_index++];
                    out_point.x     = wpos.x();
                    out_point.y     = wpos.y();
                    out_point.z     = wpos.z();
                    out_point.color = (TGC._rgb * colorscale).saturated().ABGRU32();
                  }
                }
                // float elapsed = timer.SecsSinceStart();
                // printf("updateWithVdbVec3Grid:elapsed<%f>\n", elapsed);
                prim->unlock(context.get());
              })
          .def(
              "lock",
              [](primitives::points_v12c4_ptr_t prim, ctx_t& context) -> py::array_t<VtxV12C4> {
                auto buffer = prim->lock(context.get());
                return py::array_t<VtxV12C4>(prim->_numpoints, buffer, py::none());
              })
          .def("unlock", [](primitives::points_v12c4_ptr_t prim, ctx_t& context) { return prim->unlock(context.get()); })
          .def("createNode", createNodeLambdaFromPrimType<primitives::points_v12c4_ptr_t>());
  type_codec->registerStdCodec<primitives::points_v12c4_ptr_t>(pointsprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pointsprimt8_type = //
      py::class_<primitives::PointsPrimitive<VtxV12T8>, primitives::points_v12t8_ptr_t>(primitives, "PointsPrimitiveV12T8")
          .def(
              "create",
              [](int numpoints) -> primitives::points_v12t8_ptr_t {
                return std::make_shared<primitives::PointsPrimitive<VtxV12T8>>(numpoints);
              })
#if defined(ENABLE_PYTORCH)
          .def(
              "updatePositionWithTorchTensor",
              [](primitives::points_v12t8_ptr_t prim, torchtensor_ptr_t l2tensor, size_t start, ctx_t context) {
                /////////////////////////
                // wait for tensor to be on CPU
                /////////////////////////
                if (0) {
                  py::gil_scoped_release release;
                  bool is_cpu = (l2tensor->_state.load() == 1);
                  while (not is_cpu) {
                    is_cpu = (l2tensor->_state.load() == 1);
                    if (not is_cpu) {
                      ::ork::usleep(10);
                    }
                  }
                }
                auto as_tt  = l2tensor->_impl.get<torch::Tensor>();
                bool dim_ok = (as_tt.dim() == 3); // 3rd dim is channels
                if (not dim_ok) {
                  printf("ERROR: tensor dim<%d> is not 3\n", int(as_tt.dim()));
                  OrkAssert(false);
                }
                // printf("dim_ok<%d>\n", (int) dim_ok);
                size_t num_points = as_tt.size(1);
                OrkAssert(as_tt.is_contiguous());
                OrkAssert(as_tt.is_cpu());
                OrkAssert(as_tt.dtype() == torch::kFloat32);
                OrkAssert((start + num_points) <= prim->_capacity) auto src_data = (const float*)as_tt.data_ptr();
                auto dst_data                                                    = (VtxV12T8*)prim->lock(context.get(), num_points);
                for (size_t i = 0; i < num_points; i++) {
                  size_t j  = (start + i) * 3;
                  auto& out = dst_data[i];
                  out.pos.x     = src_data[j + 0];
                  out.pos.y     = src_data[j + 1];
                  out.pos.z     = src_data[j + 2];
                }
                prim->unlock(context.get());
              })
              .def(
                "updateUv0WithTorchTensor",
                [](primitives::points_v12t8_ptr_t prim, torchtensor_ptr_t l2tensor, size_t start, ctx_t context) {
                  /////////////////////////
                  // wait for tensor to be on CPU
                  /////////////////////////
                  if (0) {
                    py::gil_scoped_release release;
                    bool is_cpu = (l2tensor->_state.load() == 1);
                    while (not is_cpu) {
                      is_cpu = (l2tensor->_state.load() == 1);
                      if (not is_cpu) {
                        ::ork::usleep(10);
                      }
                    }
                  }
                  auto as_tt  = l2tensor->_impl.get<torch::Tensor>();
                  bool dim_ok = (as_tt.dim() == 2); // 3rd dim is channels
                  if (not dim_ok) {
                    printf("ERROR: tensor dim<%d> is not 2\n", int(as_tt.dim()));
                    OrkAssert(false);
                  }
                  // printf("dim_ok<%d>\n", (int) dim_ok);
                  size_t num_points = as_tt.size(1);
                  OrkAssert(as_tt.is_contiguous());
                  OrkAssert(as_tt.is_cpu());
                  OrkAssert(as_tt.dtype() == torch::kFloat32);
                  OrkAssert((start + num_points) <= prim->_capacity) auto src_data = (const float*)as_tt.data_ptr();
                  auto dst_data                                                    = (VtxV12T8*)prim->lock(context.get(), num_points);
                  for (size_t i = 0; i < num_points; i++) {
                    size_t j  = (start + i) * 2;
                    auto& out = dst_data[i];
                    out.uv0.x     = src_data[j + 0];
                    out.uv0.y     = src_data[j + 1];
                  }
                  prim->unlock(context.get());
                })
                .def("createNode", createNodeLambdaFromPrimType<primitives::points_v12t8_ptr_t>())
                #endif
                ;
  type_codec->registerStdCodec<primitives::points_v12t8_ptr_t>(pointsprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pointsprimu32_type = //
    py::class_<primitives::PointsPrimitive<SVtxVU32>, primitives::points_vu32_ptr_t>(primitives, "PointsPrimitiveVU32")
        .def(
            "create",
            [](int numpoints) -> primitives::points_vu32_ptr_t {
              return std::make_shared<primitives::PointsPrimitive<SVtxVU32>>(numpoints);
            })
        .def(
            "lock",
            [](primitives::points_vu32_ptr_t prim, ctx_t& context, int num_points) -> py::array_t<SVtxVU32> {
              auto buffer = prim->lock(context.get(), num_points);
              return py::array_t<SVtxVU32>(prim->_numpoints, buffer, py::none());
            })
        .def("unlock", [](primitives::points_vu32_ptr_t prim, ctx_t& context) { return prim->unlock(context.get()); })
        .def("createNode", createNodeLambdaFromPrimType<primitives::points_vu32_ptr_t>());
  type_codec->registerStdCodec<primitives::points_vu32_ptr_t>(pointsprim_type);
  ////////////////////////////////////////////////////////////////////////////////
  auto tiled_pointsprim_type = //
      py::class_<primitives::TiledPointsPrimitive<VtxV12C4>, primitives::tiled_points_v12c4_ptr_t>(
          primitives, "TiledPointsPrimitiveV12C4")
          .def(
              "create",
              [](int numpoints) -> primitives::tiled_points_v12c4_ptr_t {
                return std::make_shared<primitives::TiledPointsPrimitive<VtxV12C4>>();
              })
          .def_property(
              "max_tile_update_rate",
              [](primitives::tiled_points_v12c4_ptr_t prim) -> int { return prim->_max_tile_update_rate; },
              [](primitives::tiled_points_v12c4_ptr_t prim, int value) { prim->_max_tile_update_rate = value; })
          .def(
              "updateWithVdbTestGrid",
              [](primitives::tiled_points_v12c4_ptr_t prim, //
                 vdb_grid_test_ptr_t grid,                  //
                 float colorscale,
                 ctx_t context) {
                py::gil_scoped_release release;

                auto& tree           = grid->tree();
                auto& xform          = grid->transform();
                constexpr int i2_dim = vdb_tree_test_int2_t::DIM;
                constexpr int i3_dim = vdb_tree_test_leaf_t::DIM;
                auto i2_wdim         = grid->indexToWorld(openvdb::Vec3f(i2_dim, i2_dim, i2_dim));
                auto i3_wdim         = grid->indexToWorld(openvdb::Vec3f(i3_dim, i3_dim, i3_dim));

                auto& root = tree.root();
                std::vector<const vdb_tree_test_int2_t*> l2_nodes;
                root.getNodes(l2_nodes);

                int num_l2_tiles = l2_nodes.size();

                //////////////////////////////////////////////////////
                // collect tiles to be updated
                //////////////////////////////////////////////////////

                for (auto l2_node : l2_nodes) {
                  uint64_t hash = l2_node->hash();

                  primitives::tiled_points_v12c4_t::tile_ptr_t prim_tile;

                  auto it = prim->_tiles.find(hash);
                  if (it != prim->_tiles.end()) {
                    prim_tile = it->second;
                    prim_tile->_userdata.set<const vdb_tree_test_int2_t*>(l2_node);
                  } else {
                    prim_tile            = std::make_shared<primitives::tiled_points_v12c4_t::Tile>();
                    prim_tile->_capacity = 1024;
                    prim_tile->_vertexBuffer =
                        std::make_shared<primitives::tiled_points_v12c4_t::vtx_buf_t>(prim_tile->_capacity, 0);
                    prim_tile->_userdata.set<const vdb_tree_test_int2_t*>(l2_node);
                    prim->_tiles[hash] = prim_tile;
                  }

                  int version = l2_node->getVersion();
                  if (prim_tile->_version != version) {
                    prim_tile->_update_priority++;
                  }

                } // for( auto l2_node : l2_nodes ){

                //////////////////////////////////////////////////////
                // prioritize tiles to be updated
                //////////////////////////////////////////////////////

                using tile_list_t = std::vector<primitives::tiled_points_v12c4_t::tile_ptr_t>;
                std::map<int, tile_list_t> tiles_to_update;

                for (auto tile : prim->_tiles) {
                  int pri = tile.second->_update_priority;
                  tiles_to_update[pri].push_back(tile.second);
                }

                //////////////////////////////////////////////////////
                // update the tiles
                //////////////////////////////////////////////////////

                int maxrate = prim->_max_tile_update_rate;

                int updated_tile_counter = 0;
                for (auto it = tiles_to_update.rbegin(); it != tiles_to_update.rend(); ++it) {

                  auto& tile_list = it->second;

                  for (auto prim_tile : tile_list) {

                    auto l2_node = prim_tile->_userdata.get<const vdb_tree_test_int2_t*>();

                    ////////////////////////////////////////////////////////
                    // lock current vtxbuf
                    ////////////////////////////////////////////////////////

                    auto cur_vtxbuf = prim_tile->_vertexBuffer;
                    size_t capacity = prim_tile->_capacity;
                    auto points     = (VtxV12C4*)context->GBI()->LockVB(*cur_vtxbuf, 0, capacity);

                    ////////////////////////////////////////////////////////
                    // write into vtxbuf until full
                    ////////////////////////////////////////////////////////

                    int voxels_needed           = 0;
                    int voxels_actually_written = 0;
                    const int numvoxels         = l2_node->onVoxelCount();
                    for (auto it = l2_node->cbeginChildOn(); it; ++it) {
                      const auto& leafnode = *it;
                      for (auto it_leaf = leafnode.cbeginValueOn(); it_leaf; ++it_leaf) {
                        auto icoord = it_leaf.getCoord();
                        auto wpos   = xform.indexToWorld(icoord);
                        if (wpos.length() > 0.01f) {
                          const TestGridCell& TGC = (*it_leaf);
                          if (voxels_needed < capacity) {

                            auto& out_point = points[voxels_actually_written++];
                            out_point.x     = wpos.x();
                            out_point.y     = wpos.y();
                            out_point.z     = wpos.z();
                            out_point.color = (TGC._rgb * colorscale).saturated().ABGRU32();
                          }
                          voxels_needed++;
                        }
                      }
                    }

                    ////////////////////////////////////////////////////////
                    // register tile update
                    ////////////////////////////////////////////////////////

                    prim_tile->_update_priority = 0; // reset update priority
                    prim_tile->_version         = l2_node->getVersion();
                    prim_tile->_numpoints       = voxels_actually_written;

                    ////////////////////////////////////////////////////////
                    // if was filled, increase capacity
                    //  copy current to new
                    //  replace current with new
                    ////////////////////////////////////////////////////////

                    if (voxels_needed > prim_tile->_capacity) {
                      int voxtimes2 = voxels_needed << 1;
                      if (prim_tile->_capacity < voxtimes2) {
                        prim_tile->_capacity = voxtimes2;
                      }
                      auto new_vtxbuf = std::make_shared<primitives::tiled_points_v12c4_t::vtx_buf_t>(voxtimes2, 0);
                      auto points2    = (VtxV12C4*)context->GBI()->LockVB(*new_vtxbuf, 0, voxtimes2);
                      memcpy_fast(points2, points, voxels_actually_written * sizeof(VtxV12C4));
                      // clear rest of new buffer
                      size_t num_points_not_written = (voxtimes2 - voxels_actually_written);
                      if (0)
                        memset(
                            points2 + voxels_actually_written,          // dest
                            0,                                          // value
                            num_points_not_written * sizeof(VtxV12C4)); // count
                      context->GBI()->UnLockVB(*new_vtxbuf);
                      prim_tile->_vertexBuffer    = new_vtxbuf;
                      prim_tile->_update_priority = 1 << 20; // reset update priority
                      prim_tile->_version         = -2;      // mark dirty again, since we could not update all...
                    }

                    ////////////////////////////////////////////////////////
                    // unlock current vtxbuf
                    ////////////////////////////////////////////////////////

                    context->GBI()->UnLockVB(*cur_vtxbuf);

                    ////////////////////////////////////////////////////////

                    updated_tile_counter++;

                    if ((maxrate > 0) and (updated_tile_counter >= prim->_max_tile_update_rate)) {
                      break;
                    }
                  }
                  if ((maxrate > 0) and (updated_tile_counter >= prim->_max_tile_update_rate)) {
                    break;
                  }
                }

                //////////////////////////////////////////////////////
              })
          .def("createNode", createNodeLambdaFromPrimType<primitives::tiled_points_v12c4_ptr_t>());
  type_codec->registerStdCodec<primitives::tiled_points_v12c4_ptr_t>(tiled_pointsprim_type);
}

} // namespace ork::lev2
