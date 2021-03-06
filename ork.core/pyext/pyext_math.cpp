#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math(py::module& module_core) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto fvec2_type = //
      py::class_<fvec2, fvec2_ptr_t>(module_core, "vec2", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fvec2& vec) -> pybind11::buffer_info {
            auto data = vec.GetArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                // Number of dimensions
                {2},              // Buffer dimensions
                {sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
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
  type_codec->registerStdCodec<fvec2_ptr_t>(fvec2_type);
  type_codec->registerStdCodec<fvec2>(fvec2_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fvec3_type = //
      py::class_<fvec3, fvec3_ptr_t>(module_core, "vec3", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fvec3& vec) -> pybind11::buffer_info {
            auto data = vec.GetArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                // Number of dimensions
                {3},              // Buffer dimensions
                {sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
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
          .def("set", [](fvec3_ptr_t me, fvec3_ptr_t other) { (*me.get()) = (*other.get()); })
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
  type_codec->registerStdCodec<fvec3_ptr_t>(fvec3_type);
  type_codec->registerStdCodec<fvec3>(fvec3_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fvec4_type = //
      py::class_<fvec4, fvec4_ptr_t>(module_core, "vec4", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fvec4& vec) -> pybind11::buffer_info {
            auto data = vec.GetArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                // Number of dimensions
                {4},              // Buffer dimensions
                {sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
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
  type_codec->registerStdCodec<fvec4_ptr_t>(fvec4_type);
  type_codec->registerStdCodec<fvec4>(fvec4_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fquat_type = //
      py::class_<fquat, fquat_ptr_t>(module_core, "quat", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fquat& quat) -> pybind11::buffer_info {
            auto data = &quat.x; // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                // Number of dimensions
                {4},              // Buffer dimensions
                {sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<float, float, float, float>())
          .def(py::init<fvec3, float>())
          .def(py::init<fmtx3>())
          .def("norm", &fquat::norm)
          .def("fromAxisAngle", &fquat::fromAxisAngle)
          .def("toAxisAngle", &fquat::toAxisAngle)
          .def("conjugate", &fquat::conjugate)
          .def("square", &fquat::square)
          .def("negate", &fquat::negate)
          .def("normalize", &fquat::normalize)
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
  type_codec->registerStdCodec<fquat_ptr_t>(fquat_type);
  type_codec->registerStdCodec<fquat>(fquat_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mtx3_type = //
      py::class_<fmtx3, fmtx3_ptr_t>(module_core, "mtx3", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fmtx3& mtx) -> pybind11::buffer_info {
            auto data = mtx.GetArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                2,                                   // Number of dimensions
                {3, 3},                              // Buffer dimensions
                {sizeof(float) * 3, sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
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
  type_codec->registerStdCodec<fmtx3_ptr_t>(mtx3_type);
  type_codec->registerStdCodec<fmtx3>(mtx3_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mtx4_type = //
      py::class_<fmtx4, fmtx4_ptr_t>(module_core, "mtx4", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fmtx4& mtx) -> pybind11::buffer_info {
            auto data = mtx.GetArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                2,                                   // Number of dimensions
                {4, 4},                              // Buffer dimensions
                {sizeof(float) * 4, sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
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
          .def(
              "compose",
              [](fmtx4_ptr_t mtx, const fvec3& pos, const fquat& rot, float scale) { //
                mtx->compose(pos, rot, scale);
              })
          .def_static("perspective", &fmtx4::perspective)
          .def_static(
              "composed",
              [](const fvec3& pos, const fquat& rot, float scale) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->compose(pos, rot, scale);
                return rval;
              })
          .def_static(
              "deltaMatrix",
              [](fmtx4_ptr_t from, fmtx4_ptr_t to) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->CorrectionMatrix(*from.get(), *to.get());
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const fquat& q) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->FromQuaternion(q);
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const fvec3& axis, float angle) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->FromQuaternion(fquat(axis, angle));
                return rval;
              })
          .def_static(
              "transMatrix",
              [](float x, float y, float z) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->SetTranslation(x, y, z);
                return rval;
              })
          .def_static(
              "transMatrix",
              [](const fvec3& t) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->SetTranslation(t.x, t.y, t.z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](float x, float y, float z) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->SetScale(x, y, z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](const fvec3& scale) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->SetScale(scale.x, scale.y, scale.z);
                return rval;
              })
          .def_static(
              "unproject",
              [](fmtx4_ptr_t rIMVP, const fvec3& ClipCoord, fvec3& rVObj) -> bool {
                return fmtx4::UnProject(*rIMVP.get(), ClipCoord, rVObj);
              })
          .def_static(
              "lookAt",
              [](const fvec3& eye, const fvec3& tgt, fvec3& up) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->LookAt(eye, tgt, up);
                return rval;
              })
          //.def("LookAt", &fmtx4::decompose)
          .def(py::self * py::self)
          .def(py::self == py::self)
          .def("__repr__", [](fmtx4_ptr_t mtx) -> std::string {
            auto str = mtx->dump4x3cn();
            return str.c_str();
          });
  type_codec->registerStdCodec<fmtx4_ptr_t>(mtx4_type);
  type_codec->registerStdCodec<fmtx4>(mtx4_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto frustum_type =
      py::class_<Frustum, frustum_ptr_t>(module_core, "Frustum")
          .def(py::init<>())
          .def(py::init([](const fmtx4& VMatrix, const fmtx4& PMatrix) {
            auto rval = std::make_shared<Frustum>();
            rval->Set(VMatrix, PMatrix);
            return rval;
          }))
          .def("set", [](Frustum& frustum, const fmtx4& VMatrix, const fmtx4& PMatrix) { frustum.Set(VMatrix, PMatrix); })
          .def("set", [](Frustum& frustum, const fmtx4& IVPMatrix) { frustum.Set(IVPMatrix); })
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
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                // Number of dimensions
                {4},              // Buffer dimensions
                {sizeof(float)}); // Strides (in bytes) for each index
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

  /////////////////////////////////////////////////////////////////////////////////
  auto fray3_type = //
      py::class_<fray3, fray3_ptr_t>(module_core, "ray3", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<const fvec3&, const fvec3&>())
          .def_property_readonly("origin", [](fray3_ptr_t ray) -> fvec3 { return ray->mOrigin; })
          .def_property_readonly("direction", [](fray3_ptr_t ray) -> fvec3 { return ray->mDirection; })
          .def(
              "__str__",
              [](fray3_ptr_t r) -> std::string {
                fxstring<64> fxs;
                auto o = r->mOrigin;
                auto d = r->mDirection;
                fxs.format("ray3 o(%g,%g,%g) d(%g,%g,%g)", o.x, o.y, o.z, d.x, d.y, d.z);
                return fxs.c_str();
              })
          .def("__repr__", [](fray3_ptr_t r) -> std::string {
            fxstring<64> fxs;
            auto o = r->mOrigin;
            auto d = r->mDirection;
            fxs.format("ray3 o(%g,%g,%g) d(%g,%g,%g)", o.x, o.y, o.z, d.x, d.y, d.z);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<fray3_ptr_t>(fray3_type);
}

} // namespace ork
