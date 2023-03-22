////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <pybind11/numpy.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

using shape_t = pybind11::detail::any_container<ssize_t>;

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
  auto type_codec = python::TypeCodec::instance();
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
          [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorTop; },
          [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorTop = value; })

      .def_property(
          "bottomColor",
          [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorBottom; },
          [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorBottom = value; })

      .def_property(
          "frontColor",
          [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorFront; },
          [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorFront = value; })

      .def_property(
          "backColor",
          [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorBack; },
          [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorBack = value; })

      .def_property(
          "leftColor",
          [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorLeft; },
          [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorLeft = value; })

      .def_property(
          "rightColor",
          [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorRight; },
          [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorRight = value; })

      .def("gpuInit", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.gpuInit(context.get()); })
      .def("renderEML", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.renderEML(context.get()); })
      .def("createNode", createNodeLambdaFromPrimType<primitives::cube_ptr_t>());
  type_codec->registerStdCodec<primitives::cube_ptr_t>(cubeprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto frusprim_type = //
      py::class_<primitives::FrustumPrimitive, primitives::frustum_ptr_t>(primitives, "FrustumPrimitive")
          .def(py::init<>())
          .def_property(
              "frustum",
              [](primitives::frustum_ptr_t prim) -> Frustum { return prim->_frustum; },
              [](primitives::frustum_ptr_t prim, const Frustum& value) { prim->_frustum = value; })

          .def_property(
              "topColor",
              [](primitives::frustum_ptr_t prim) -> fvec4 { return prim->_colorTop; },
              [](primitives::frustum_ptr_t prim, const fvec4& value) { prim->_colorTop = value; })

          .def_property(
              "bottomColor",
              [](primitives::frustum_ptr_t prim) -> fvec4 { return prim->_colorBottom; },
              [](primitives::frustum_ptr_t prim, const fvec4& value) { prim->_colorBottom = value; })

          .def_property(
              "farColor",
              [](primitives::frustum_ptr_t prim) -> fvec4 { return prim->_colorFar; },
              [](primitives::frustum_ptr_t prim, const fvec4& value) { prim->_colorFar = value; })

          .def_property(
              "nearColor",
              [](primitives::frustum_ptr_t prim) -> fvec4 { return prim->_colorNear; },
              [](primitives::frustum_ptr_t prim, const fvec4& value) { prim->_colorNear = value; })

          .def_property(
              "leftColor",
              [](primitives::frustum_ptr_t prim) -> fvec4 { return prim->_colorLeft; },
              [](primitives::frustum_ptr_t prim, const fvec4& value) { prim->_colorLeft = value; })

          .def_property(
              "rightColor",
              [](primitives::frustum_ptr_t prim) -> fvec4 { return prim->_colorRight; },
              [](primitives::frustum_ptr_t prim, const fvec4& value) { prim->_colorRight = value; })

          .def("gpuInit", [](primitives::frustum_ptr_t prim, ctx_t& context) { prim->gpuInit(context.get()); })
          .def("renderEML", [](primitives::frustum_ptr_t prim, ctx_t& context) { prim->renderEML(context.get()); })
          .def("createNode", createNodeLambdaFromPrimType<primitives::frustum_ptr_t>());
  type_codec->registerStdCodec<primitives::frustum_ptr_t>(frusprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pointsprim_type = //
      py::class_<primitives::PointsPrimitive<VtxV12C4>, primitives::points_v12c4_ptr_t>(primitives, "PointsPrimitiveV12C4")
          .def("create", [](int numpoints){
            return std::make_shared<primitives::PointsPrimitive<VtxV12C4>>(numpoints);
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
