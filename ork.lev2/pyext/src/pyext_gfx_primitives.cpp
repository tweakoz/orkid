////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/openvdb.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

using shape_t = pybind11::detail::any_container<ssize_t>;
using vdb_floatgrid_t       = openvdb::FloatGrid;
using vdb_floatgrid_ptr_t   = std::shared_ptr<vdb_floatgrid_t>;

template <typename T> std::function<scenegraph::drawable_node_ptr_t (T,std::string, scenegraph::layer_ptr_t, fxpipeline_ptr_t)> createNodeLambdaFromPrimType() {
  return 
  [](T prim,
     std::string named, //
     scenegraph::layer_ptr_t layer,
     fxpipeline_ptr_t mtl_inst) -> scenegraph::drawable_node_ptr_t { //
    auto node                                                 //
        = prim->createNode(named, layer, mtl_inst);
    node->_userdata->template makeValueForKey<T>("_primitive") = prim; // hold on to reference
    return node;
  };
}

void pyinit_primitives(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto primitives = module_lev2.def_submodule("primitives", "BuiltIn Primitives");
  /////////////////////////////////////////////////////////////////////////////////
  auto pointdata_type = py::class_<primitives::PointsData,primitives::pointsdata_ptr_t>(primitives, "PointsData")
      .def(py::init<>([](datablock_ptr_t db, int num_points, crcstring_ptr_t format) {
        auto typed_fmt = (EVtxStreamFormat) format->hashed();
        return std::make_shared<primitives::PointsData>(db, num_points, typed_fmt);
      }))
      .def("transformInPlace", [](primitives::pointsdata_ptr_t prim, const fmtx4& mtx) { //
        prim->transformInPlace(mtx);
      })
      .def("transformed", [](primitives::pointsdata_ptr_t prim, const fmtx4& mtx) -> primitives::pointsdata_ptr_t { //
        return prim->transformed(mtx);
      })
      .def("depthClamped", [](primitives::pointsdata_ptr_t prim, float zmin, float zmax) -> primitives::pointsdata_ptr_t { //
        return prim->depthClamped(zmin,zmax);
      })
      .def("convertToV12C4", [](primitives::pointsdata_ptr_t prim, image_ptr_t image) -> primitives::pointsdata_ptr_t { //
        return prim->convertToV12C4(image);
      });
  type_codec->registerStdCodec<primitives::pointsdata_ptr_t>(pointdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cubeprim_type = //
    py::class_<primitives::CubePrimitive,primitives::cube_ptr_t>(primitives, "CubePrimitive")
      .def(py::init<>())
      .def_property(
          "size",
          [](const primitives::CubePrimitive& prim) -> float { return prim._size; },
          [](primitives::CubePrimitive& prim, const float& value) { prim._size = value; })

      .def_property(
          "topColor",
          [](const primitives::CubePrimitive& prim) -> dvec4 { return prim._colorTop; },
          [](primitives::CubePrimitive& prim, const dvec4& value) { prim._colorTop = value; })

      .def_property(
          "bottomColor",
          [](const primitives::CubePrimitive& prim) -> dvec4 { return prim._colorBottom; },
          [](primitives::CubePrimitive& prim, const dvec4& value) { prim._colorBottom = value; })

      .def_property(
          "frontColor",
          [](const primitives::CubePrimitive& prim) -> dvec4 { return prim._colorFront; },
          [](primitives::CubePrimitive& prim, const dvec4& value) { prim._colorFront = value; })

      .def_property(
          "backColor",
          [](const primitives::CubePrimitive& prim) -> dvec4 { return prim._colorBack; },
          [](primitives::CubePrimitive& prim, const dvec4& value) { prim._colorBack = value; })

      .def_property(
          "leftColor",
          [](const primitives::CubePrimitive& prim) -> dvec4 { return prim._colorLeft; },
          [](primitives::CubePrimitive& prim, const dvec4& value) { prim._colorLeft = value; })

      .def_property(
          "rightColor",
          [](const primitives::CubePrimitive& prim) -> dvec4 { return prim._colorRight; },
          [](primitives::CubePrimitive& prim, const dvec4& value) { prim._colorRight = value; })

      .def("gpuInit", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.gpuInit(context.get()); })
      .def("renderEML", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.renderEML(context.get()); })
      .def("createDrawable", [](primitives::CubePrimitive& prim, fxpipeline_ptr_t mtl_inst) -> drawable_ptr_t {
        return prim.createDrawable(mtl_inst);
      })
      .def("createDrawableData", [](primitives::CubePrimitive& prim, fxpipeline_ptr_t mtl_inst) -> callback_drawabledata_ptr_t {
        return prim.createDrawableData(mtl_inst);
      })
      .def("createNode", createNodeLambdaFromPrimType<primitives::cube_ptr_t>());
  type_codec->registerStdCodec<primitives::cube_ptr_t>(cubeprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto frusprim_type = //
      py::class_<primitives::FrustumPrimitive, primitives::frustum_ptr_t>(primitives, "FrustumPrimitive")
          .def(py::init<>())
          .def_property(
              "frustum",
              [](primitives::frustum_ptr_t prim) -> dfrustum { return prim->_frustum; },
              [](primitives::frustum_ptr_t prim, const dfrustum& value) { prim->_frustum = value; })

          .def_property(
              "topColor",
              [](primitives::frustum_ptr_t prim) -> dvec4 { return prim->_colorTop; },
              [](primitives::frustum_ptr_t prim, const dvec4& value) { prim->_colorTop = value; })

          .def_property(
              "bottomColor",
              [](primitives::frustum_ptr_t prim) -> dvec4 { return prim->_colorBottom; },
              [](primitives::frustum_ptr_t prim, const dvec4& value) { prim->_colorBottom = value; })

          .def_property(
              "farColor",
              [](primitives::frustum_ptr_t prim) -> dvec4 { return prim->_colorFar; },
              [](primitives::frustum_ptr_t prim, const dvec4& value) { prim->_colorFar = value; })

          .def_property(
              "nearColor",
              [](primitives::frustum_ptr_t prim) -> dvec4 { return prim->_colorNear; },
              [](primitives::frustum_ptr_t prim, const dvec4& value) { prim->_colorNear = value; })

          .def_property(
              "leftColor",
              [](primitives::frustum_ptr_t prim) -> dvec4 { return prim->_colorLeft; },
              [](primitives::frustum_ptr_t prim, const dvec4& value) { prim->_colorLeft = value; })

          .def_property(
              "rightColor",
              [](primitives::frustum_ptr_t prim) -> dvec4 { return prim->_colorRight; },
              [](primitives::frustum_ptr_t prim, const dvec4& value) { prim->_colorRight = value; })

          .def("gpuInit", [](primitives::frustum_ptr_t prim, ctx_t& context) { prim->gpuInit(context.get()); })
          .def("renderEML", [](primitives::frustum_ptr_t prim, ctx_t& context) { prim->renderEML(context.get()); })
          .def("createNode", createNodeLambdaFromPrimType<primitives::frustum_ptr_t>())
          .def("createNodeWithMaterial",[](primitives::frustum_ptr_t prim, //
                                           std::string named, //
                                           scenegraph::layer_ptr_t layer, // 
                                           material_ptr_t material ) -> scenegraph::drawable_node_ptr_t { //
                auto node                                                 
                    = prim->createNodeWithMaterial(named, layer, material);
                node->_userdata->template makeValueForKey<primitives::frustum_ptr_t>("_primitive") = prim; // hold on to reference
                return node;
              });

  type_codec->registerStdCodec<primitives::frustum_ptr_t>(frusprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pointsprim_type = //
      py::class_<primitives::PointsPrimitive<VtxV12C4>, primitives::points_v12c4_ptr_t>(primitives, "PointsPrimitiveV12C4")
          .def("create", [](int numpoints) -> primitives::points_v12c4_ptr_t {
            return std::make_shared<primitives::PointsPrimitive<VtxV12C4>>(numpoints);
          })
          .def("createFromVdbFloatGrid", [](vdb_floatgrid_ptr_t grid, ctx_t context) -> primitives::points_v12c4_ptr_t {

            int num_points   = grid->tree().activeLeafVoxelCount();
            //printf("num_points<%d>\n", num_points);
            auto prim = std::make_shared<primitives::PointsPrimitive<VtxV12C4>>(num_points);
            VtxV12C4* points = prim->lock(context.get());
            int point_index = 0;
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
          .def("updateWithVdbFloatGrid", [](primitives::points_v12c4_ptr_t prim, //
                                            vdb_floatgrid_ptr_t grid, //
                                            ctx_t context)  {
            py::gil_scoped_release release;
            int num_points   = grid->tree().activeLeafVoxelCount();
            OrkAssert(num_points<prim->_capacity)
            //printf("num_points<%d>\n", num_points);
            VtxV12C4* points = prim->lock(context.get(),num_points);
            int point_index = 0;
            auto& xform = grid->transform();
            for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
              const auto& leaf = *leafIter;

              // Iterate over active voxels within the leaf
              for (auto voxelIter = leaf.cbeginValueOn(); voxelIter; ++voxelIter) {
                openvdb::Coord icoord = voxelIter.getCoord();
                openvdb::Vec3f wpos = xform.indexToWorld(icoord);
                float value          = (*voxelIter)*255.0f;
                auto grey = uint32_t(value) & 0xff;
                if(point_index<num_points){
                  //OrkAssert(point_index<num_points);
                  auto& out_point = points[point_index++];
                  out_point.x = wpos.x();
                  out_point.y = wpos.y();
                  out_point.z = wpos.z();
                  out_point.color = (grey << 16)|(grey << 8)|(grey << 0);
                }
              }
            }
            prim->unlock(context.get());
            return prim;
          })
          .def("updateWithVdbVec3Grid", [](primitives::points_v12c4_ptr_t prim, //
                                            vdb_vec3grid_ptr_t grid, //
                                            ctx_t context)  {
            py::gil_scoped_release release;
            int num_points   = grid->tree().activeLeafVoxelCount();
            OrkAssert(num_points<prim->_capacity)
            //printf("updateWithVdbVec3Grid:num_points<%d>\n", num_points);
            VtxV12C4* points = prim->lock(context.get(),num_points);
            int point_index = 0;
            auto& xform = grid->transform();
            //ork::Timer timer;
            //timer.Start();
            for (auto voxelIter = grid->cbeginValueOn(); voxelIter; ++voxelIter) {
                openvdb::Coord icoord = voxelIter.getCoord();
                openvdb::Vec3f wpos = xform.indexToWorld(icoord);
                auto value          = (*voxelIter);
                if(point_index<num_points){
                  //OrkAssert(point_index<num_points);
                  auto& out_point = points[point_index++];
                  out_point.x = wpos.x();
                  out_point.y = wpos.y();
                  out_point.z = wpos.z();

                  float r = value.x()*255.0f;
                  float g = value.y()*255.0f;
                  float b = value.z()*255.0f;
                  uint32_t r8 = uint32_t(r) & 0xff;
                  uint32_t g8 = uint32_t(g) & 0xff;
                  uint32_t b8 = uint32_t(b) & 0xff;
                  out_point.color = (b8 << 16)|(g8 << 8)|(r8 << 0);
                }
            }
            //float elapsed = timer.SecsSinceStart();
            //printf("updateWithVdbVec3Grid:elapsed<%f>\n", elapsed);
            prim->unlock(context.get());
            return prim;
          })
          .def("updateWithVdbTestGrid", [](primitives::points_v12c4_ptr_t prim, //
                                            vdb_grid_test_ptr_t grid, //
                                            float colorscale,
                                            ctx_t context)  {
            py::gil_scoped_release release;
            int num_points   = grid->tree().activeLeafVoxelCount();
            OrkAssert(num_points<prim->_capacity)
            //printf("updateWithVdbVec3Grid:num_points<%d>\n", num_points);
            VtxV12C4* points = prim->lock(context.get(),num_points);
            int point_index = 0;
            auto& xform = grid->transform();
            //ork::Timer timer;
            //timer.Start();
            for (auto voxelIter = grid->cbeginValueOn(); voxelIter; ++voxelIter) {
                openvdb::Coord icoord = voxelIter.getCoord();
                openvdb::Vec3f wpos = xform.indexToWorld(icoord);
                const TestGridCell& TGC          = (*voxelIter);
                if(point_index<num_points){
                  //OrkAssert(point_index<num_points);
                  auto& out_point = points[point_index++];
                  out_point.x = wpos.x();
                  out_point.y = wpos.y();
                  out_point.z = wpos.z();
                  out_point.color = (TGC._rgb*colorscale).saturated().ABGRU32();
                }
            }
            //float elapsed = timer.SecsSinceStart();
            //printf("updateWithVdbVec3Grid:elapsed<%f>\n", elapsed);
            prim->unlock(context.get());
          })
          .def("lock", [](primitives::points_v12c4_ptr_t prim, ctx_t& context) -> py::array_t<VtxV12C4> {
            auto buffer = prim->lock(context.get());
            return py::array_t<VtxV12C4>(prim->_numpoints,buffer,py::none());
          })
          .def("unlock", [](primitives::points_v12c4_ptr_t prim, ctx_t& context){
            return prim->unlock(context.get());
          })
          .def( "createNode", createNodeLambdaFromPrimType<primitives::points_v12c4_ptr_t>() );
  type_codec->registerStdCodec<primitives::points_v12c4_ptr_t>(pointsprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tiled_pointsprim_type = //
      py::class_<primitives::TiledPointsPrimitive<VtxV12C4>, primitives::tiled_points_v12c4_ptr_t>(primitives, "TiledPointsPrimitiveV12C4")
          .def("create", [](int numpoints) -> primitives::tiled_points_v12c4_ptr_t {
            return std::make_shared<primitives::TiledPointsPrimitive<VtxV12C4>>();
          })
          .def("updateWithVdbTestGrid", [](primitives::tiled_points_v12c4_ptr_t prim, //
                                            vdb_grid_test_ptr_t grid, //
                                            float colorscale,
                                            ctx_t context)  {
            py::gil_scoped_release release;

            auto& tree = grid->tree();
            auto& xform = grid->transform();
            int i2_dim = vdb_tree_test_int2_t::DIM;
            auto i2_wdim = grid->indexToWorld(openvdb::Vec3f(i2_dim,i2_dim,i2_dim));

            auto& root = tree.root();
            std::vector<const vdb_tree_test_int2_t*> l2_nodes;
            root.getNodes(l2_nodes);
            int num_l2_tiles = l2_nodes.size(); 

            //////////////////////////////////////////////////////
            // collect tiles to be updated
            //////////////////////////////////////////////////////

            for( auto l2_node : l2_nodes ){
              uint64_t hash = l2_node->hash();

              primitives::tiled_points_v12c4_t::tile_ptr_t prim_tile;

              auto it = prim->_tiles.find(hash);
              if(it != prim->_tiles.end() ){
                prim_tile = it->second;
                prim_tile->_userdata.set<const vdb_tree_test_int2_t*>(l2_node);
              }
              else{
                prim_tile = std::make_shared<primitives::tiled_points_v12c4_t::Tile>();
                prim_tile->_capacity = i2_dim*i2_dim*i2_dim;
                prim_tile->_vertexBuffer = std::make_shared<primitives::tiled_points_v12c4_t::vtx_buf_t>(prim_tile->_capacity ,0);
                prim_tile->_userdata.set<const vdb_tree_test_int2_t*>(l2_node);
                prim->_tiles[hash] = prim_tile;
              }

              int version = l2_node->getVersion();
              if( prim_tile->_version != version ){
                prim_tile->_update_priority++;
              }

            } // for( auto l2_node : l2_nodes ){

            //////////////////////////////////////////////////////
            // prioritize tiles to be updated
            //////////////////////////////////////////////////////

            using tile_list_t = std::vector<primitives::tiled_points_v12c4_t::tile_ptr_t>;
            std::map<int,tile_list_t> tiles_to_update;

            for( auto tile : prim->_tiles ){
              int pri = tile.second->_update_priority;
              tiles_to_update[pri].push_back(tile.second);
            }

            //////////////////////////////////////////////////////
            // update the tiles
            //////////////////////////////////////////////////////

            constexpr int max_tiles_to_update = 20;

            int updated_tile_counter = 0;
            for( auto it = tiles_to_update.rbegin(); it != tiles_to_update.rend(); ++it ){

              auto& tile_list = it->second;

              for( auto prim_tile : tile_list ){

                auto l2_node = prim_tile->_userdata.get<const vdb_tree_test_int2_t*>();

                auto points = (VtxV12C4*) context->GBI()->LockVB(*(prim_tile->_vertexBuffer),0,prim_tile->_capacity);
                int voxel_index = 0;
                const int numvoxels = l2_node->onVoxelCount();
                for( auto it = l2_node->cbeginChildOn(); it; ++it ){
                  const auto& leafnode = *it;
                  for( auto it_leaf = leafnode.cbeginValueOn(); it_leaf; ++it_leaf ){
                    auto icoord = it_leaf.getCoord();
                    auto wpos = xform.indexToWorld(icoord);
                    const TestGridCell& TGC = (*it_leaf);
                    auto& out_point = points[voxel_index++];
                    out_point.x = wpos.x();
                    out_point.y = wpos.y();
                    out_point.z = wpos.z();
                    out_point.color = (TGC._rgb*colorscale).saturated().ABGRU32();
                    voxel_index++;
                  }
                }
                OrkAssert(voxel_index<prim_tile->_capacity);
                prim_tile->_numpoints = voxel_index;

                prim_tile->_version = l2_node->getVersion();
                prim_tile->_update_priority = 0; // reset update priority

                context->GBI()->UnLockVB(*(prim_tile->_vertexBuffer));

                updated_tile_counter++;

                if( updated_tile_counter >= max_tiles_to_update ){
                  break;
                }

              }
              if( updated_tile_counter >= max_tiles_to_update ){
                break;
              }
            }

            //////////////////////////////////////////////////////

          })
          .def( "createNode", createNodeLambdaFromPrimType<primitives::tiled_points_v12c4_ptr_t>() );
      type_codec->registerStdCodec<primitives::tiled_points_v12c4_ptr_t>(tiled_pointsprim_type);
  /////////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
