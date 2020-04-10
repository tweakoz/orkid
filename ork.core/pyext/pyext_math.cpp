#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math(py::module& module_core) {
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fvec2>(module_core, "vec2")
      .def(py::init<>())
      .def(py::init<float, float>())
      .def_property("x", &fvec2::GetX, &fvec2::SetX)
      .def_property("y", &fvec2::GetY, &fvec2::SetY)
      .def("dot", &fvec2::Dot)
      .def("perp", &fvec2::PerpDot)
      .def("mag", &fvec2::Mag)
      .def("magsquared", &fvec2::MagSquared)
      .def("lerp", &fvec2::Lerp)
      .def("serp", &fvec2::Serp)
      .def("normal", &fvec2::Normal)
      .def("normalize", &fvec2::Normalize)
      .def(py::self + py::self)
      .def(py::self - py::self)
      .def(py::self * py::self)
      .def(
          "__str__",
          [](const fvec2& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec2(%g,%g)", v.x, v.y);
            return fxs.c_str();
          })
      .def("__repr__", [](const fvec2& v) -> std::string {
        fxstring<64> fxs;
        fxs.format("vec2(%g,%g)", v.x, v.y);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fvec3>(module_core, "vec3")
      .def(py::init<>())
      .def(py::init<float, float, float>())
      .def_property("x", &fvec3::GetX, &fvec3::SetX)
      .def_property("y", &fvec3::GetY, &fvec3::SetY)
      .def_property("z", &fvec3::GetZ, &fvec3::SetZ)
      .def("dot", &fvec3::Dot)
      .def("cross", &fvec3::Cross)
      .def("mag", &fvec3::Mag)
      .def("length", &fvec3::Mag)
      .def("magsquared", &fvec3::MagSquared)
      .def("lerp", &fvec3::Lerp)
      .def("serp", &fvec3::Serp)
      .def("reflect", &fvec3::Reflect)
      .def("saturated", &fvec3::saturated)
      .def("clamped", &fvec3::clamped)
      .def("normal", &fvec3::Normal)
      .def("normalize", &fvec3::Normalize)
      .def("rotx", &fvec3::RotateX)
      .def("roty", &fvec3::RotateY)
      .def("rotz", &fvec3::RotateZ)
      .def("xy", &fvec3::GetXY)
      .def("xz", &fvec3::GetXZ)
      .def(py::self + py::self)
      .def(py::self - py::self)
      .def(py::self * py::self)
      .def(py::self * float())
      .def(
          "__str__",
          [](const fvec3& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec3(%g,%g,%g)", v.x, v.y, v.z);
            return fxs.c_str();
          })
      .def("__repr__", [](const fvec3& v) -> std::string {
        fxstring<64> fxs;
        fxs.format("vec3(%g,%g,%g)", v.x, v.y, v.z);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fvec4>(module_core, "vec4")
      .def(py::init<>())
      .def(py::init<float, float, float, float>())
      .def(py::init<fvec3>())
      .def(py::init<fvec3, float>())
      .def(py::init<uint32_t>())
      .def_property("x", &fvec4::GetX, &fvec4::SetX)
      .def_property("y", &fvec4::GetY, &fvec4::SetY)
      .def_property("z", &fvec4::GetZ, &fvec4::SetZ)
      .def_property("w", &fvec4::GetW, &fvec4::SetW)
      .def("dot", &fvec4::Dot)
      .def("cross", &fvec4::Cross)
      .def("mag", &fvec4::Mag)
      .def("length", &fvec4::Mag)
      .def("magsquared", &fvec4::MagSquared)
      .def("lerp", &fvec4::Lerp)
      .def("serp", &fvec4::Serp)
      .def("saturated", &fvec4::Saturate)
      .def("normal", &fvec4::Normal)
      .def("normalize", &fvec4::Normalize)
      .def("rotx", &fvec4::RotateX)
      .def("roty", &fvec4::RotateY)
      .def("rotz", &fvec4::RotateZ)
      .def("xyz", &fvec4::xyz)
      .def("transform", &fvec4::Transform)
      .def("perspectiveDivided", &fvec4::perspectiveDivided)
      .def_property_readonly("rgbaU32", [](const fvec4& v) -> uint32_t { return v.GetRGBAU32(); })
      .def_property_readonly("argbU32", [](const fvec4& v) -> uint32_t { return v.GetARGBU32(); })
      .def_property_readonly("abgrU32", [](const fvec4& v) -> uint32_t { return v.GetABGRU32(); })
      .def(py::self + py::self)
      .def(py::self - py::self)
      .def(py::self * py::self)
      .def(py::self * float())
      .def(
          "__str__",
          [](const fvec4& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec4(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
            return fxs.c_str();
          })
      .def("__repr__", [](const fvec4& v) -> std::string {
        fxstring<64> fxs;
        fxs.format("vec4(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fquat>(module_core, "quat")
      .def(py::init<>())
      .def(py::init<float, float, float, float>())
      .def(py::init<fvec3, float>())
      .def(py::init<fmtx3>())
      .def("mag", &fquat::Magnitude)
      .def("fromAxisAngle", &fquat::fromAxisAngle)
      .def("toAxisAngle", &fquat::toAxisAngle)
      .def("conjugate", &fquat::Magnitude)
      .def("square", &fquat::Square)
      .def("negate", &fquat::Negate)
      .def("normalize", &fquat::Normalize)
      .def(py::self * py::self)
      .def(
          "__str__",
          [](const fquat& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("quat(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
            return fxs.c_str();
          })
      .def("__repr__", [](const fquat& v) -> std::string {
        fxstring<64> fxs;
        fxs.format("quat(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fmtx3>(module_core, "mtx3")
      .def(py::init<>())
      .def(py::init<const fmtx3&>())
      .def(py::init<const fquat&>())
      .def("setScale", (void (fmtx3::*)(float, float, float)) & fmtx3::SetScale)
      .def("fromQuaternion", &fmtx3::FromQuaternion)
      .def("zNormal", &fmtx3::GetXNormal)
      .def("yNormal", &fmtx3::GetYNormal)
      .def("xNormal", &fmtx3::GetZNormal)
      .def(py::self * py::self)
      .def(py::self == py::self)
      .def("__repr__", [](const fmtx3& mtx) -> std::string {
        auto str = mtx.dumpcn();
        return str.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fmtx4>(module_core, "mtx4")
      .def(py::init<>())
      .def(py::init<const fmtx4&>())
      .def(py::init<const fquat&>())
      .def("zNormal", &fmtx4::GetXNormal)
      .def("yNormal", &fmtx4::GetYNormal)
      .def("xNormal", &fmtx4::GetZNormal)
      .def("transpose", &fmtx4::Transpose)
      .def("normalize", &fmtx4::Normalize)
      .def("inverse", &fmtx4::inverse)
      .def("inverseOf", &fmtx4::inverseOf)
      .def("decompose", &fmtx4::decompose)
      .def("toRotMatrix3", &fmtx4::rotMatrix33)
      .def_static("perspective", &fmtx4::perspective)
      .def_static(
          "compose",
          [](const fvec3& pos, const fquat& rot, float scale) -> fmtx4 {
            fmtx4 rval;
            rval.compose(pos, rot, scale);
            return rval;
          })
      .def_static(
          "deltaMatrix",
          [](const fmtx4& from, const fmtx4& to) -> fmtx4 {
            fmtx4 rval;
            rval.CorrectionMatrix(from, to);
            return rval;
          })
      .def_static(
          "rotMatrix",
          [](const fquat& q) -> fmtx4 {
            fmtx4 rval;
            rval.FromQuaternion(q);
            return rval;
          })
      .def_static(
          "rotMatrix",
          [](const fvec3& axis, float angle) -> fmtx4 {
            fmtx4 rval;
            rval.FromQuaternion(fquat(axis, angle));
            return rval;
          })
      .def_static(
          "transMatrix",
          [](float x, float y, float z) -> fmtx4 {
            fmtx4 rval;
            rval.SetTranslation(x, y, z);
            return rval;
          })
      .def_static(
          "transMatrix",
          [](const fvec3& t) -> fmtx4 {
            fmtx4 rval;
            rval.SetTranslation(t.x, t.y, t.z);
            return rval;
          })
      .def_static(
          "scaleMatrix",
          [](float x, float y, float z) -> fmtx4 {
            fmtx4 rval;
            rval.SetScale(x, y, z);
            return rval;
          })
      .def_static(
          "scaleMatrix",
          [](const fvec3& scale) -> fmtx4 {
            fmtx4 rval;
            rval.SetScale(scale.x, scale.y, scale.z);
            return rval;
          })
      .def_static(
          "unproject",
          [](const fmtx4& rIMVP, const fvec3& ClipCoord, fvec3& rVObj) -> bool {
            return fmtx4::UnProject(rIMVP, ClipCoord, rVObj);
          })
      .def_static(
          "lookAt",
          [](const fvec3& eye, const fvec3& tgt, fvec3& up) -> fmtx4 {
            fmtx4 rval;
            rval.LookAt(eye, tgt, up);
            return rval;
          })
      //.def("LookAt", &fmtx4::decompose)
      .def(py::self * py::self)
      .def(py::self == py::self)
      .def("__repr__", [](const fmtx4& mtx) -> std::string {
        auto str = mtx.dump4x3cn();
        return str.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Frustum>(module_core, "Frustum")
      .def(py::init<>())
      .def("set", [](Frustum& frustum, const fmtx4& VMatrix, const fmtx4& PMatrix) { frustum.Set(VMatrix, PMatrix); })
      .def("set", [](Frustum& frustum, const fmtx4& IVPMatrix) { frustum.Set(IVPMatrix); })
      .def("nearCorner", [](const Frustum& frustum, int index) -> fvec3 { return frustum.mNearCorners[std::clamp(index, 0, 3)]; })
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
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fplane3>(module_core, "plane")
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
          [](fplane3& plane, const fvec3& pta, const fvec3& ptb, const fvec3& ptc) { plane.CalcPlaneFromTriangle(pta, ptb, ptc); })
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
}

} // namespace ork
