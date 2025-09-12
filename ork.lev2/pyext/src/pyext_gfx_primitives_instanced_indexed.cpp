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
  using prim_t = primitives::InstancedIndexedPrimitive;
  using ptr_t = primitives::instanced_indexed_primitive_ptr_t;
  ////////////////////////////////////////////////////////////////////////////////
  auto instancedprim_type = //
    py::class_<prim_t, ptr_t>(primitives, "InstancedIndexedQuadPrimitive", pybind11::buffer_protocol())
         .def_buffer([](prim_t& self) -> pybind11::buffer_info {
           auto data = self._locked_data; // const uint32_t*
           OrkAssert(data); // must be locked!
           size_t N  = self._num_instances;
           return pybind11::buffer_info( (uint32_t*) data,                                            //
                                         sizeof(uint32_t),                                //
                                         pybind11::format_descriptor<uint32_t>::format(), //
                                         1,                                               //
                                         { N },                                           //
                                         { sizeof(uint32_t) });                           //
          })
        .def(
            "create",
            [](ctx_t& context, int num_instances) -> ptr_t {
              std::vector<uint16_t> quad_indices = {0, 1, 2, 3};
              return std::make_shared<prim_t>(
                context.get(), quad_indices, PrimitiveType::TRIANGLESTRIP, num_instances);
            })
        .def(
            "lock",
            [](ptr_t prim, ctx_t& context, int num_instances) -> py::array_t<uint32_t> {
              auto buffer = (uint32_t*) prim->lock(context.get(), num_instances);
              return py::array_t<uint32_t>(prim->_num_instances, buffer, py::none());
            })
        .def_property("num_instances", [](ptr_t prim) { //
          return prim->_num_instances; //
        },
        [](ptr_t prim, int v) { //
          prim->_num_instances = v; //
        })
        .def("unlock", [](ptr_t prim, ctx_t& context) { return prim->unlock(context.get()); })
        .def("createNode", createNodeLambdaFromPrimType<ptr_t>());
  type_codec->registerStdCodec<ptr_t>(instancedprim_type);
}

} // namespace ork::lev2
