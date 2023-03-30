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
  /////////////////////////////////////////////////////////////////////////////////
  auto fplane3_type =
      py::class_<fplane3, fplane3_ptr_t>(module_core, "plane", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fplane3& plane) -> pybind11::buffer_info {
            auto data = &plane.n.x; // Pointer to buffer
            return pybind11::buffer_info(
                data,               // Pointer to buffer
                sizeof(float),      // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                  // Number of dimensions
                {4},                // Buffer dimensions
                {sizeof(float)});   // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<const fvec4&>())
          .def(py::init<const fvec3&, float>())
          .def(py::init<const fvec3&, const fvec3&, const fvec3&>())
          .def(py::init<float, float, float, float>())
          .def_property_readonly("d", [](const fplane3& thisplane) -> float { return thisplane.d; })
          .def_property_readonly("normal", [](const fplane3& thisplane) -> fvec3 { return thisplane.n; })
          .def(
              "fromNormalAndOrigin",
              [](fplane3& plane, const fvec3& normal, const fvec3& origin) { plane.CalcFromNormalAndOrigin(normal, origin); })
          .def(
              "fromTriangle",
              [](fplane3& plane, const fvec3& pta, const fvec3& ptb, const fvec3& ptc) {
                plane.CalcPlaneFromTriangle(pta, ptb, ptc);
              })
          .def("isPointInFront", [](const fplane3& plane, const fvec3& point) -> bool { return plane.IsPointInFront(point); })
          .def("isPointBehind", [](const fplane3& plane, const fvec3& point) -> bool { return plane.IsPointBehind(point); })
          .def("isPointOn", [](const fplane3& plane, const fvec3& point) -> bool { return plane.IsOn(point); })
          .def("distanceToPoint", [](const fplane3& plane, const fvec3& point) -> float { return plane.pointDistance(point); })
          .def(
              "intersectWithPlane",
              [](const fplane3& thisplane, const fplane3& otherplane, fvec3& outorigin, fvec3& outdir) -> bool {
                return thisplane.PlaneIntersect(otherplane, outorigin, outdir);
              })
          .def("__repr__", [](const fplane3& thisplane) -> std::string {
            fxstring<64> fxs;
            fvec3 n = thisplane.n;
            float d = thisplane.d;
            fxs.format("Plane(n<%g %g %g> d<%g>)", n.x, n.y, n.z, d);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<fplane3_ptr_t>(fplane3_type);
}
///////////////////////////////////////////////////////////////////////////////
}