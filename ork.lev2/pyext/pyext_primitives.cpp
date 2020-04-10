#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_primitives(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto primitives = module_lev2.def_submodule("primitives", "BuiltIn Primitives");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<primitives::CubePrimitive>(primitives, "CubePrimitive")
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
      .def("draw", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.draw(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<primitives::FrustumPrimitive>(primitives, "FrustumPrimitive")
      .def(py::init<>())
      .def_property(
          "frustum",
          [](const primitives::FrustumPrimitive& prim) -> Frustum { return prim._frustum; },
          [](primitives::FrustumPrimitive& prim, const Frustum& value) { prim._frustum = value; })

      .def_property(
          "topColor",
          [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorTop; },
          [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorTop = value; })

      .def_property(
          "bottomColor",
          [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorBottom; },
          [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorBottom = value; })

      .def_property(
          "frontColor",
          [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorFront; },
          [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorFront = value; })

      .def_property(
          "backColor",
          [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorBack; },
          [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorBack = value; })

      .def_property(
          "leftColor",
          [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorLeft; },
          [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorLeft = value; })

      .def_property(
          "rightColor",
          [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorRight; },
          [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorRight = value; })

      .def("gpuInit", [](primitives::FrustumPrimitive& prim, ctx_t& context) { prim.gpuInit(context.get()); })
      .def("draw", [](primitives::FrustumPrimitive& prim, ctx_t& context) { prim.draw(context.get()); })
      .def(
          "createNode",
          [](primitives::FrustumPrimitive& prim,
             std::string named, //
             scenegraph::layer_ptr_t layer,
             materialinst_ptr_t mtl_inst) -> scenegraph::node_ptr_t { //
            return prim.createNode(named, layer, mtl_inst);
          });
  /////////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
