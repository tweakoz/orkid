////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.inl"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/openvdb.h>
#include <ork/kernel/memcpy.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_primitives_frustum(py::module& primitives) {
  auto type_codec = python::pb11_typecodec_t::instance();
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
              })
              .def_property(
                  "debugState",
                  [](primitives::frustum_ptr_t prim) -> bool {
                    return prim->_primitive._stateDebugger;
                  },
                  [](primitives::frustum_ptr_t prim, bool value) {
                    prim->_primitive._stateDebugger = value;
                  });

  type_codec->registerStdCodec<primitives::frustum_ptr_t>(frusprim_type);
}
} // namespace ork::lev2