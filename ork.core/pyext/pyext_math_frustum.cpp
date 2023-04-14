///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////
void pyinit_math_plane(py::module& module_core) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto frustum_type =
      py::class_<Frustum, frustum_ptr_t>(module_core, "Frustum")
          .def(py::init<>())
          .def(py::init([](const fmtx4& VMatrix, const fmtx4& PMatrix) {
            auto rval = std::make_shared<Frustum>();
            rval->set(VMatrix, PMatrix);
            return rval;
          }))
          .def("set", [](Frustum& frustum, const fmtx4& VMatrix, const fmtx4& PMatrix) { frustum.set(VMatrix, PMatrix); })
          .def("set", [](Frustum& frustum, const fmtx4& IVPMatrix) { frustum.set(IVPMatrix); })
          .def(
              "nearCorner",
              [](const Frustum& frustum, int index) -> fvec3 { return frustum.mNearCorners[std::clamp(index, 0, 3)]; })
          .def("farCorner", [](const Frustum& frustum, int index) -> fvec3 { return frustum.mFarCorners[std::clamp(index, 0, 3)]; })
          .def_property_readonly("center", [](const Frustum& frustum) -> fvec3 { return frustum.mCenter; })
          .def_property_readonly("xNormal", [](const Frustum& frustum) -> fvec3 { return frustum.mXNormal; })
          .def_property_readonly("yNormal", [](const Frustum& frustum) -> fvec3 { return frustum.mYNormal; })
          .def_property_readonly("zNormal", [](const Frustum& frustum) -> fvec3 { return frustum.mZNormal; })
          .def_property_readonly("nearPlane", [](const Frustum& frustum) -> fplane3 { return frustum._nearPlane; })
          .def_property_readonly("farPlane", [](const Frustum& frustum) -> fplane3 { return frustum._farPlane; })
          .def_property_readonly("leftPlane", [](const Frustum& frustum) -> fplane3 { return frustum._leftPlane; })
          .def_property_readonly("rightPlane", [](const Frustum& frustum) -> fplane3 { return frustum._rightPlane; })
          .def_property_readonly("topPlane", [](const Frustum& frustum) -> fplane3 { return frustum._topPlane; })
          .def_property_readonly("bottomPlane", [](const Frustum& frustum) -> fplane3 { return frustum._bottomPlane; })
          .def("contains", [](const Frustum& frustum, const fvec3& point) -> bool { return frustum.contains(point); });
  type_codec->registerStdCodec<frustum_ptr_t>(frustum_type);
}
///////////////////////////////////////////////////////////////////////////////
}