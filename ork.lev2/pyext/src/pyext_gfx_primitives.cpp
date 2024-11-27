////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/util/NullInterrupter.h>
#include <ork/python/obind/nanobind.h>

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
            printf("num_points<%d>\n", num_points);
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
          .def("updateWithVdbFloatGrid", [](primitives::points_v12c4_ptr_t prim, vdb_floatgrid_ptr_t grid, ctx_t context)  {
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
                auto& out_point = points[point_index++];
                out_point.x = wpos.x();
                out_point.y = wpos.y();
                out_point.z = wpos.z();
                out_point.color = (grey << 16)|(grey << 8)|(grey << 0);
              }
            }
            prim->unlock(context.get());
            return prim;
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
}
} // namespace ork::lev2
