////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/entity.inl>
#include <ork/ecs/archetype.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_archetype(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Archetype,SceneObject,archetype_ptr_t>(module_ecs, "Archetype", py::module_local())
      .def(
          "__repr__",
          [](const archetype_ptr_t& arch) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::Archetype(%p)", (void*) arch.get());
            return fxs.c_str();
          })
      .def("declareComponent", [](archetype_ptr_t& arch, std::string classname) -> componentdata_ptr_t {
        auto X = arch->addComponentWithClassName(classname.c_str());
        return X;
      });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_archetype(py::module& module_ecs) {

/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {