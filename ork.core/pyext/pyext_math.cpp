///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
            auto data = vec.asArray(); // Pointer to buffer
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
          .def_property("x", [](fvec2& vec,float val) { return vec.x = val; }, //
                             [](const fvec2& vec) -> float { return vec.x; })

          .def_property("y", [](fvec2& vec,float val) { return vec.y = val; }, //
                             [](const fvec2& vec) -> float { return vec.y; })
          .def("dot", &fvec2::dotWith)
          .def("perp", &fvec2::perpDotWith)
          .def("mag", &fvec2::magnitude)
          .def("magsquared", &fvec2::magnitudeSquared)
          .def("lerp", &fvec2::lerp)
          .def("serp", &fvec2::serp)
          .def("normalized", &fvec2::normalized)
          .def("normalize", &fvec2::normalizeInPlace)
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
            auto data = vec.asArray(); // Pointer to buffer
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
          .def_property("x", [](fvec3& vec,float val) { return vec.x = val; }, //
                             [](const fvec3& vec) -> float { return vec.x; })

          .def_property("y", [](fvec3& vec,float val) { return vec.y = val; }, //
                             [](const fvec3& vec) -> float { return vec.y; })
          .def_property("z", [](fvec3& vec,float val) { return vec.z = val; }, //
                             [](const fvec3& vec) -> float { return vec.z; })
          .def("angle", &fvec3::angle)
          .def("orientedAngle", &fvec3::orientedAngle)
          .def("dot", &fvec3::dotWith)
          .def("cross", &fvec3::crossWith)
          .def("mag", &fvec3::magnitude)
          .def("length", &fvec3::magnitude)
          .def("magsquared", &fvec3::magnitudeSquared)
          .def("lerp", &fvec3::lerp)
          .def("serp", &fvec3::serp)
          .def("reflect", &fvec3::reflect)
          .def("saturated", &fvec3::saturated)
          .def("clamped", &fvec3::clamped)
          .def("normalized", &fvec3::normalized)
          .def("normalize", &fvec3::normalizeInPlace)
          .def("transform", [](fvec3& v, fmtx4 matrix) -> fvec3 {
            return fvec4(v,1).transform(matrix).xyz();
           })
          .def("rotx", &fvec3::rotateOnX)
          .def("roty", &fvec3::rotateOnY)
          .def("rotz", &fvec3::rotateOnZ)
          .def("xy", &fvec3::xy)
          .def("xz", &fvec3::xz)
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
            auto data = vec.asArray(); // Pointer to buffer
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
          .def_property("x", [](fvec4& vec,float val) { return vec.x = val; }, //
                             [](const fvec4& vec) -> float { return vec.x; })

          .def_property("y", [](fvec4& vec,float val) { return vec.y = val; }, //
                             [](const fvec4& vec) -> float { return vec.y; })
          .def_property("z", [](fvec4& vec,float val) { return vec.z = val; }, //
                             [](const fvec4& vec) -> float { return vec.z; })
          .def_property("w", [](fvec4& vec,float val) { return vec.w = val; }, //
                             [](const fvec4& vec) -> float { return vec.w; })
          .def("dot", &fvec4::dotWith)
          .def("cross", &fvec4::crossWith)
          .def("mag", &fvec4::magnitude)
          .def("length", &fvec4::magnitude)
          .def("magSquared", &fvec4::magnitudeSquared)
          .def("lerp", &fvec4::lerp)
          .def("serp", &fvec4::serp)
          .def("saturated", &fvec4::saturated)
          .def("normalized", &fvec4::normalized)
          .def("normalize", &fvec4::normalizeInPlace)
          .def("rotx", &fvec4::rotateOnX)
          .def("roty", &fvec4::rotateOnY)
          .def("rotz", &fvec4::rotateOnZ)
          .def("xyz", &fvec4::xyz)
          .def("transform", &fvec4::transform)
          .def("perspectiveDivided", &fvec4::perspectiveDivided)
          .def_property_readonly("rgbaU32", [](const fvec4& v) -> uint32_t { return v.RGBAU32(); })
          .def_property_readonly("argbU32", [](const fvec4& v) -> uint32_t { return v.ARGBU32(); })
          .def_property_readonly("abgrU32", [](const fvec4& v) -> uint32_t { return v.ABGRU32(); })
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
            auto data = quat.asArray(); // Pointer to buffer
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
          .def("normalize", &fquat::normalizeInPlace)
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
            auto data = mtx.asArray(); // Pointer to buffer
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
          .def("setScale", (void (fmtx3::*)(float, float, float)) & fmtx3::setScale)
          .def("fromQuaternion", &fmtx3::fromQuaternion)
          .def("zNormal", &fmtx3::xNormal)
          .def("yNormal", &fmtx3::yNormal)
          .def("xNormal", &fmtx3::zNormal)
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
            auto data = mtx.asArray(); // Pointer to buffer
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
          .def("zNormal", &fmtx4::xNormal)
          .def("yNormal", &fmtx4::yNormal)
          .def("xNormal", &fmtx4::zNormal)
          .def("transpose", &fmtx4::transpose)
          .def("normalize", &fmtx4::normalizeInPlace)
          .def("inverse", &fmtx4::inverse)
          .def("inverseOf", &fmtx4::inverseOf)
          .def("decompose", &fmtx4::decompose)
          .def("toRotMatrix3", &fmtx4::rotMatrix33)
	        .def("toGlm", &fmtx4::asGlmMat4)
          .def(
              "getColumn",
              [](fmtx4_ptr_t mtx, int column) -> fvec4 { //
                return mtx->column(column);
              })
          .def(
              "setColumn",
              [](fmtx4_ptr_t mtx, int column, fvec4 c) { //
                return mtx->setColumn(column,c);
              })
          .def(
              "getRow",
              [](fmtx4_ptr_t mtx, int row) -> fvec4 { //
                return mtx->row(row);
              })
          .def(
              "setRow",
              [](fmtx4_ptr_t mtx, int row, fvec4 c) { //
                return mtx->setRow(row,c);
              })
          .def(
              "compose",
              [](fmtx4_ptr_t mtx, const fvec3& pos, const fquat& rot, float scale) { //
                mtx->compose(pos, rot, scale);
              })
          .def_static("perspective", &fmtx4::createPerspectiveMatrix)
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
                rval->correctionMatrix(*from.get(), *to.get());
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const fquat& q) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->fromQuaternion(q);
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const fvec3& axis, float angle) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->fromQuaternion(fquat(axis, angle));
                return rval;
              })
          .def_static(
              "transMatrix",
              [](float x, float y, float z) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->setTranslation(x, y, z);
                return rval;
              })
          .def_static(
              "transMatrix",
              [](const fvec3& t) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->setTranslation(t.x, t.y, t.z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](float x, float y, float z) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->setScale(x, y, z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](const fvec3& scale) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->setScale(scale.x, scale.y, scale.z);
                return rval;
              })
          .def_static(
              "unproject",
              [](fmtx4_ptr_t rIMVP, const fvec3& ClipCoord, fvec3& rVObj) -> bool {
                return fmtx4::unProject(*rIMVP.get(), ClipCoord, rVObj);
              })
          .def_static(
              "lookAt",
              [](const fvec3& eye, const fvec3& tgt, fvec3& up) -> fmtx4_ptr_t {
                auto rval = std::make_shared<fmtx4>();
                rval->lookAt(eye, tgt, up);
                return rval;
              })
          //.def("lookAt", &fmtx4::decompose)
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
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto dcxf2str = [](const DecompTransform& dcxf) -> std::string {
    std::string fxs;
    if(dcxf._usedirectmatrix){
      auto str = dcxf._directmatrix.dump4x3cn();
      fxs = FormatString("Transform(precomposed) mtx(%s)", str.c_str() );
    }
    else{
      auto o = dcxf._translation;
      auto r = dcxf._rotation;
      float s = dcxf._uniformScale;
      fxs = FormatString("Transform(decomposed) p(%g,%g,%g) o(%g,%g,%g,%g) s:%g", o.x, o.y, o.z, r.w, r.x, r.y, r.z, s);
    }
    return fxs.c_str();
  };
  //
  auto dcxf_type = //
      py::class_<DecompTransform, decompxf_ptr_t>(module_core, "Transform")
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def_property("translation", 
            [](decompxf_const_ptr_t dcxf) -> fvec3 { return dcxf->_translation; },
            [](decompxf_ptr_t dcxf, fvec3 inp) { dcxf->_translation = inp; })
          .def_property("orientation", 
            [](decompxf_const_ptr_t dcxf) -> fquat { return dcxf->_rotation; },
            [](decompxf_ptr_t dcxf, fquat inp) { dcxf->_rotation=inp; })
          .def_property("scale", 
            [](decompxf_const_ptr_t dcxf) -> float { return dcxf->_uniformScale; },
            [](decompxf_ptr_t dcxf, float sc) { dcxf->_uniformScale = sc; })
          .def_property("directMatrix", 
            [](decompxf_const_ptr_t dcxf) -> fmtx4 { return dcxf->_directmatrix; },
            [](decompxf_ptr_t dcxf, fmtx4 inp) { 
              dcxf->_directmatrix = inp;
              dcxf->_usedirectmatrix = true;
            })
          .def_property_readonly("composed", [](decompxf_const_ptr_t dcxf) -> fmtx4 { return dcxf->composed(); })
          .def("__str__", dcxf2str)
          .def("__repr__", dcxf2str);
  dcxf_type.doc() = "Transform (de-composed, or pre-composed : set directMatrix to use pre-composed)";
  type_codec->registerStdCodec<decompxf_ptr_t>(dcxf_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct MathConstantsProxy {};
  using mathconstantsproxy_ptr_t = std::shared_ptr<MathConstantsProxy>;
  auto mathconstantsproxy_type   =                                                        //
      py::class_<MathConstantsProxy, mathconstantsproxy_ptr_t>(module_core, "mathconstants") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                           //
              [type_codec](mathconstantsproxy_ptr_t proxy, const std::string& key) -> py::object { //

                svar64_t value; 
                value.set<void*>(nullptr);
                if(key == "DTOR"){
                  value.set<float>(DTOR);
                }
                return type_codec->encode(value);
              });
  type_codec->registerStdCodec<mathconstantsproxy_ptr_t>(mathconstantsproxy_type);
}

} // namespace ork
