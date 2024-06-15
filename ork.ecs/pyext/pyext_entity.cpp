////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_entity(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto entity_type = py::class_<pyentity_ptr_t>(module_ecs, "Entity")
      .def(
          "__repr__",
          [](const pyentity_ptr_t& ent) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::Entity(%p)", ent.get());
            return fxs.c_str();
          })
          .def_property_readonly("transform", [](pyentity_ptr_t ent) -> decompxf_ptr_t { return ent->transform(); })
          .def_property_readonly("transformNode", [](pyentity_ptr_t ent) -> xfnode_ptr_t { return ent->transformNode(); });

  type_codec->registerRawPtrCodec<pyentity_ptr_t, Entity*>(entity_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_entity(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {