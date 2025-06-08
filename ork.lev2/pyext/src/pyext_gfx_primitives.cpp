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

using shape_t             = pybind11::detail::any_container<ssize_t>;

void pyinit_gfx_primitives_rigid(py::module& module_lev2);
void pyinit_gfx_primitives_points(py::module& primitives);
void pyinit_gfx_primitives_instanced_indexed(py::module& primitives);
void pyinit_gfx_primitives_frustum(py::module& primitives);

void pyinit_primitives(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto primitives = module_lev2.def_submodule("primitives", "BuiltIn Primitives");
  /////////////////////////////////////////////////////////////////////////////////
  auto cubeprim_type = //
      py::class_<primitives::CubePrimitive, primitives::cube_ptr_t>(primitives, "CubePrimitive")
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
          .def(
              "createDrawable",
              [](primitives::CubePrimitive& prim, fxpipeline_ptr_t mtl_inst) -> drawable_ptr_t {
                return prim.createDrawable(mtl_inst);
              })
          .def(
              "createDrawableData",
              [](primitives::CubePrimitive& prim, fxpipeline_ptr_t mtl_inst) -> callback_drawabledata_ptr_t {
                return prim.createDrawableData(mtl_inst);
              })
          .def("createNode", createNodeLambdaFromPrimType<primitives::cube_ptr_t>());
  type_codec->registerStdCodec<primitives::cube_ptr_t>(cubeprim_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_gfx_primitives_rigid(module_lev2); // todo parent under primitives
  pyinit_gfx_primitives_frustum(primitives);
  pyinit_gfx_primitives_instanced_indexed(primitives);
  pyinit_gfx_primitives_points(primitives);
}
} // namespace ork::lev2
