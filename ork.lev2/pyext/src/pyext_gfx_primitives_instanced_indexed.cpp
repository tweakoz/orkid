////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>

#include "pyext.inl"
#include <pybind11/numpy.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/kernel/memcpy.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_primitives_instanced_indexed(py::module& primitives) {
  auto type_codec = python::pb11_typecodec_t::instance();

  ////////////////////////////////////////////////////////////////////////////////
  auto instancedprim_type = //
    py::class_<primitives::InstancedIndexedPrimitive, primitives::instanced_indexed_primitive_ptr_t>(primitives, "InstancedIndexedQuadPrimitive")
        .def(
            "create",
            [](ctx_t& context, int num_instances) -> primitives::instanced_indexed_primitive_ptr_t {
              std::vector<uint16_t> quad_indices = {0, 1, 2, 3};
              return std::make_shared<primitives::InstancedIndexedPrimitive>(
                context.get(), quad_indices, PrimitiveType::TRIANGLESTRIP, num_instances);
            })
        .def(
            "lock",
            [](primitives::instanced_indexed_primitive_ptr_t prim, ctx_t& context, int num_instances) -> py::array_t<SVtxVU32Inst> {
              auto buffer = prim->lock(context.get(), num_instances);
              return py::array_t<SVtxVU32Inst>(prim->_num_instances, buffer, py::none());
            })
        .def("unlock", [](primitives::instanced_indexed_primitive_ptr_t prim, ctx_t& context) { return prim->unlock(context.get()); })
        .def("createNode", createNodeLambdaFromPrimType<primitives::instanced_indexed_primitive_ptr_t>());
  type_codec->registerStdCodec<primitives::instanced_indexed_primitive_ptr_t>(instancedprim_type);
}

} // namespace ork::lev2
