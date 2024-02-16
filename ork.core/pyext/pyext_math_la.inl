///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void pyinit_math_la_t(py::module& module_core, //
                      std::string pfx ) { //

  using vec2_t = Vector2<T>;
  using vec3_t = Vector3<T>;
  using vec4_t = Vector4<T>;
  using quat_t = Quaternion<T>;
  using mat3_t = Matrix33<T>;
  using mat4_t = Matrix44<T>;
  using ray3_t = Ray3<T>;
  using plane_t = Plane<T>;
  using frustum_t = TFrustum<T>;
  using frustum_ptr_t = std::shared_ptr<frustum_t>;
  auto vec2_name = pfx+"vec2";
  auto vec3_name = pfx+"vec3";
  auto vec4_name = pfx+"vec4";
  auto quat_name = pfx+"quat";
  auto mat3_name = pfx+"mtx3";
  auto mat4_name = pfx+"mtx4";
  auto ray3_name = pfx+"ray3";
  auto plane_name = pfx+"plane";
  auto frustum_name = pfx+"frustum";


  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto vec2_type = //
      py::class_<vec2_t>(module_core, vec2_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](vec2_t& vec) -> pybind11::buffer_info {
            auto data = vec.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(T), // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                1,                // Number of dimensions
                {2},              // Buffer dimensions
                {sizeof(T)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<T, T>())
          .def_property(
              "x", [](const vec2_t& vec) -> T { return vec.x; }, [](vec2_t& vec, T val) { return vec.x = val; }) //
          .def_property(
              "y", [](const vec2_t& vec) -> T { return vec.y; }, [](vec2_t& vec, T val) { return vec.y = val; }) //
          .def("dot", &vec2_t::dotWith)
          .def("perp", &vec2_t::perpDotWith)
          .def("angle", &vec2_t::angle)
          .def("orientedAngle", &vec2_t::orientedAngle)
          .def("mag", &vec2_t::magnitude)
          .def("magsquared", &vec2_t::magnitudeSquared)
          .def("lerp", &vec2_t::lerp)
          .def("serp", &vec2_t::serp)
          .def("normalized", &vec2_t::normalized)
          .def("normalize", &vec2_t::normalizeInPlace)
          .def_property_readonly(
              "length", [](const vec2_t& vec) -> T { return vec.magnitude(); })
          .def_property_readonly(
              "lengthSquared", [](const vec2_t& vec) -> T { return vec.magnitudeSquared(); })
          .def(py::self + py::self)
          .def(py::self - py::self)
          .def(py::self * py::self)
          .def(
              "__str__",
              [](const vec2_t& v) -> std::string {
                fxstring<64> fxs;
                fxs.format("vec2(%g,%g)", v.x, v.y);
                return fxs.c_str();
              })
          .def("__repr__", [](const vec2_t& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec2_t(%g,%g)", v.x, v.y);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<vec2_t>(vec2_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto vec3_type = //
      py::class_<vec3_t>(module_core, vec3_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](vec3_t& vec) -> pybind11::buffer_info {
            auto data = vec.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(T), // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                1,                // Number of dimensions
                {3},              // Buffer dimensions
                {sizeof(T)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<T>())
          .def(py::init<T, T, T>())
          .def_property(
              "x",
              [](const vec3_t& vec) -> T { return vec.x; },  //
              [](vec3_t& vec, T val) { return vec.x = val; } //
              )
          .def_property(
              "y",
              [](const vec3_t& vec) -> T { return vec.y; },  //
              [](vec3_t& vec, T val) { return vec.y = val; } //
              )
          .def_property(
              "z",
              [](const vec3_t& vec) -> T { return vec.z; },  //
              [](vec3_t& vec, T val) { return vec.z = val; } //
              )
          .def_property_readonly(
              "as_list",
              [](const vec3_t& vec) -> py::list { //
                py::list rval;
                rval.append(vec.x);
                rval.append(vec.y);
                rval.append(vec.z);
                return rval;
              })
          .def_property_readonly(
              "length", [](const vec3_t& vec) -> T { return vec.magnitude(); })
          .def_property_readonly(
              "lengthSquared", [](const vec3_t& vec) -> T { return vec.magnitudeSquared(); })
          .def(
              "hashed",
              [](const vec3_t& vec, T quant) -> uint64_t { //
                return vec.hash(quant);
              })
          .def("angle", &vec3_t::angle)
          .def("orientedAngle", &vec3_t::orientedAngle)
          .def("dot", &vec3_t::dotWith)
          .def("cross", &vec3_t::crossWith)
          .def("mag", &vec3_t::magnitude)
//          .def("length", &vec3_t::magnitude)
          .def("magsquared", &vec3_t::magnitudeSquared)
          .def("lerp", &vec3_t::lerp)
          .def("serp", &vec3_t::serp)
          .def("reflect", &vec3_t::reflect)
          .def("saturated", &vec3_t::saturated)
          .def("clamped", &vec3_t::clamped)
          .def("normalized", &vec3_t::normalized)
          .def("normalize", &vec3_t::normalizeInPlace)
          .def("transform", [](const vec3_t& v, mat4_t matrix) -> vec3_t { return vec4_t(v, 1).transform(matrix).xyz(); })
          .def("rotx", &vec3_t::rotateOnX)
          .def("roty", &vec3_t::rotateOnY)
          .def("rotz", &vec3_t::rotateOnZ)
          .def("xy", &vec3_t::xy)
          .def("xz", &vec3_t::xz)
          .def(py::self + py::self)
          .def(py::self - py::self)
          .def(py::self * py::self)
          .def(py::self * T())
          .def("hsv2rgb", [](const vec3_t& hsv) -> vec3_t { //
            vec3_t RGB;
            RGB.setHSV(hsv.x, hsv.y, hsv.z);
            return RGB;
          })
          .def("set", [](vec3_t& me, const vec3_t& other) { me = other; })
          .def(
              "__str__",
              [](const vec3_t& v) -> std::string {
                fxstring<64> fxs;
                fxs.format("vec3(%g,%g,%g)", v.x, v.y, v.z);
                return fxs.c_str();
              })
          .def("__repr__", [](const vec3_t& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec3_t(%g,%g,%g)", v.x, v.y, v.z);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<vec3_t>(vec3_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto vec4_type = //
      py::class_<vec4_t>(module_core, vec4_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](vec4_t& vec) -> pybind11::buffer_info {
            auto data = vec.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(T), // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                1,                // Number of dimensions
                {4},              // Buffer dimensions
                {sizeof(T)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<T>())
          .def(py::init<T, T, T, T>())
          .def(py::init<vec3_t>())
          .def(py::init<vec3_t, T>())
          .def(py::init<uint32_t>())
          .def_property(
              "x", [](const vec4_t& vec) -> T { return vec.x; }, [](vec4_t& vec, T val) { return vec.x = val; } //
              )

          .def_property(
              "y", [](const vec4_t& vec) -> T { return vec.y; }, [](vec4_t& vec, T val) { return vec.y = val; } //
              )
          .def_property(
              "z", [](const vec4_t& vec) -> T { return vec.z; }, [](vec4_t& vec, T val) { return vec.z = val; } //
              )
          .def_property(
              "w", [](const vec4_t& vec) -> T { return vec.w; }, [](vec4_t& vec, T val) { return vec.w = val; } //
              )
          .def("dot", &vec4_t::dotWith)
          .def("cross", &vec4_t::crossWith)
          .def("mag", &vec4_t::magnitude)
          .def("length", &vec4_t::magnitude)
          .def("magSquared", &vec4_t::magnitudeSquared)
          .def("lerp", &vec4_t::lerp)
          .def("serp", &vec4_t::serp)
          .def("saturated", &vec4_t::saturated)
          .def("normalized", &vec4_t::normalized)
          .def("normalize", &vec4_t::normalizeInPlace)
          .def("rotx", &vec4_t::rotateOnX)
          .def("roty", &vec4_t::rotateOnY)
          .def("rotz", &vec4_t::rotateOnZ)
          .def("xyz", &vec4_t::xyz)
          .def("transform", &vec4_t::transform)
          .def("perspectiveDivided", &vec4_t::perspectiveDivided)
          .def_property_readonly("rgbaU32", [](const vec4_t& v) -> uint32_t { return v.RGBAU32(); })
          .def_property_readonly("argbU32", [](const vec4_t& v) -> uint32_t { return v.ARGBU32(); })
          .def_property_readonly("abgrU32", [](const vec4_t& v) -> uint32_t { return v.ABGRU32(); })
          .def(py::self + py::self)
          .def(py::self - py::self)
          .def(py::self * py::self)
          .def(py::self * T())
          .def(
              "__str__",
              [](const vec4_t& v) -> std::string {
                fxstring<64> fxs;
                fxs.format("vec4(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
                return fxs.c_str();
              })
          .def("__repr__", [](const vec4_t& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec4_t(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<vec4_t>(vec4_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto quat_type = //
      py::class_<quat_t>(module_core, quat_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](quat_t& quat) -> pybind11::buffer_info {
            auto data = quat.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(T), // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                1,                // Number of dimensions
                {4},              // Buffer dimensions
                {sizeof(T)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<T, T, T, T>())
          .def(py::init<vec3_t, T>())
          .def(py::init<mat3_t>())
          .def("toMatrix", &quat_t::toMatrix)
          .def("norm", &quat_t::norm)
          .def("fromAxisAngle", &quat_t::fromAxisAngle)
          .def_static("createFromAxisAngle", [](vec3_t& axis, T angle) -> quat_t {
            quat_t rval;
            rval.fromAxisAngle(vec4_t(axis, angle));
            return rval;
          })
          .def("fromMatrix3", &quat_t::fromMatrix3)
          .def("fromMatrix4", &quat_t::fromMatrix)
          .def("toAxisAngle", &quat_t::toAxisAngle)
          .def("conjugate", &quat_t::conjugate)
          .def("square", &quat_t::square)
          .def("negate", &quat_t::negate)
          .def("normalize", &quat_t::normalizeInPlace)
          .def(py::self * py::self)
          .def_property("x", [](const quat_t& quat) -> T { return quat.x; }, [](quat_t& quat, T val) { return quat.x = val; })
          .def_property("y", [](const quat_t& quat) -> T { return quat.y; }, [](quat_t& quat, T val) { return quat.y = val; })
          .def_property("z", [](const quat_t& quat) -> T { return quat.z; }, [](quat_t& quat, T val) { return quat.z = val; })
          .def_property("w", [](const quat_t& quat) -> T { return quat.w; }, [](quat_t& quat, T val) { return quat.w = val; })
          .def(
              "__str__",
              [](const quat_t& v) -> std::string {
                fxstring<64> fxs;
                fxs.format("quat(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
                return fxs.c_str();
              })
          .def("__repr__", [](const quat_t& v) -> std::string {
            fxstring<64> fxs;
            fxs.format("quat(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<quat_t>(quat_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mtx3_type = //
      py::class_<mat3_t>(module_core, mat3_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](mat3_t& mtx) -> pybind11::buffer_info {
            auto data = mtx.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(T), // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                2,                                   // Number of dimensions
                {3, 3},                              // Buffer dimensions
                {sizeof(T) * 3, sizeof(T)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<const mat3_t&>())
          .def(py::init<const quat_t&>())
          .def("setScale", (void(mat3_t::*)(T, T, T)) & mat3_t::setScale)
          .def("setColumn", (void(mat3_t::*)(int icol, const vec3_t&)) & mat3_t::setColumn)
          .def_property_readonly("inverse", [](const mat3_t& inp) -> mat3_t { 
              mat3_t inv = inp;
              inv.inverse();
              return inv;
            })
          .def("fromQuaternion", &mat3_t::fromQuaternion)
          .def("zNormal", &mat3_t::xNormal)
          .def("yNormal", &mat3_t::yNormal)
          .def("xNormal", &mat3_t::zNormal)
          .def(py::self * py::self)
          .def(py::self == py::self)
          .def("__repr__", [](const mat3_t& mtx) -> std::string {
            auto str = mtx.dumpcn();
            return str.c_str();
          });
  type_codec->registerStdCodec<mat3_t>(mtx3_type);
      /////////////////////////////////////////////////////////////////////////////////
  auto mat4_type = //
      py::class_<mat4_t>(module_core, mat4_name.c_str(), pybind11::buffer_protocol(),
        "4x4 GLM derived matrix class supporting buffer protocol.\n\n"
        "This class represents a 4x4 matrix and supports the buffer protocol,\n"
        "allowing it to be used in contexts that require direct buffer access.\n"
        "The buffer exposes the matrix data as a contiguous array of scalars\n"
        "(floats or doubles depending on the build), arranged in row-major order.\n"
        "This class is useful for efficient data exchange with libraries like NumPy,\n"
        "enabling direct manipulation of matrix data without copying.\n")      
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](mat4_t& mtx) -> pybind11::buffer_info {
            auto data = mtx.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,           // Pointer to buffer
                sizeof(T), // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                2,                                     // Number of dimensions
                {4, 4},                                // Buffer dimensions
                {sizeof(T) * 4, sizeof(T)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<const mat4_t&>())
          .def(py::init<const quat_t&>())
          .def_property_readonly("transposed", [](const mat4_t& inp) -> mat4_t { return inp.transposed(); })
          .def_property_readonly("inverse", &mat4_t::inverse)
          .def_property_readonly("determinant", &mat4_t::determinant)
          .def_property_readonly("determinant3x3", &mat4_t::determinant3x3)
          .def_property_readonly("eigenvalues", &mat4_t::eigenvalues)
          .def_property_readonly("eigenvectors", &mat4_t::eigenvectors)
          .def("zNormal", &mat4_t::xNormal)
          .def("yNormal", &mat4_t::yNormal)
          .def("xNormal", &mat4_t::zNormal)
          .def("transpose", &mat4_t::transpose)
          .def("normalize", &mat4_t::normalizeInPlace)
          .def("inverseOf", &mat4_t::inverseOf)
          .def("decompose", &mat4_t::decompose)
          .def("toRotMatrix3", &mat4_t::rotMatrix33)
          .def("toRotMatrix4", &mat4_t::rotMatrix44)
          .def("toGlm", &mat4_t::asGlmMat4)
          .def_property_readonly("translation", &mat4_t::translation)
          .def(
              "getColumn",
              [](mat4_t mtx, int icolumn) -> vec4_t { //
                OrkPyAssert(icolumn >= 0 && icolumn < 4);
                return mtx.column(icolumn);
              })
          .def(
              "setColumn",
              [](mat4_t& mtx, int icolumn, vec4_t c) { //
                OrkPyAssert(icolumn >= 0 && icolumn < 4);
                mtx.setColumn(icolumn, c);
              })
          .def(
              "getRow",
              [](mat4_t mtx, int irow) -> vec4_t { //
                OrkPyAssert(irow >= 0 && irow < 4);
                return mtx.row(irow);
              })
          .def(
              "setRow",
              [](mat4_t& mtx, int irow, vec4_t c) { //
                OrkPyAssert(irow >= 0 && irow < 4);
                mtx.setRow(irow, c);
              })
          .def(
              "compose",
              [](mat4_t& mtx, const vec3_t& pos, const quat_t& rot, T scale) { //
                mtx.compose(pos, rot, scale);
              })
          .def(
              "compose",
              [](mat4_t& mtx, const vec3_t& pos, const quat_t& rot, const vec3_t& vscale) { //
                mtx.compose(pos, rot, vscale);
              })
          .def(
              "dump",
              [](mat4_t mtx, std::string name) { //
                mtx.dump(name);
              })
          .def( 
              "dump4x4cn",
              [](mat4_t mtx) -> std::string { //
                return mtx.dump4x4cn();
              })
          .def_static("perspective", &mat4_t::createPerspectiveMatrix)
          .def_static(
              "perspectiveWithFovs",
              [](T fovy_radians, T fovh_radians, T near, T far) -> mat4_t {
                T slope_a      = tanf(fovy_radians * 0.5);
                T slope_b      = tanf(fovh_radians * 0.5);
                T aspect_ratio = slope_b / slope_a;
                return mat4_t::createPerspectiveMatrix(fovy_radians, aspect_ratio, near, far);
              })
          .def_static(
              "composed",
              [](const vec3_t& pos, const quat_t& rot, T scale) -> mat4_t {
                mat4_t rval;
                rval.compose(pos, rot, scale);
                return rval;
              })
          .def_static(
              "composed",
              [](const vec3_t& pos, const quat_t& rot, vec3_t scale) -> mat4_t {
                mat4_t rval;
                rval.compose(pos, rot, scale);
                return rval;
              })
          .def_static(
              "deltaMatrix",
              [](mat4_t from, mat4_t to) -> mat4_t {
                mat4_t rval;
                rval.correctionMatrix(from, to);
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const quat_t& q) -> mat4_t {
                mat4_t rval;
                rval.fromQuaternion(q);
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const vec3_t& axis, T angle) -> mat4_t {
                mat4_t rval;
                rval.fromQuaternion(quat_t(axis, angle));
                return rval;
              })
          .def_static(
              "transMatrix",
              [](T x, T y, T z) -> mat4_t {
                mat4_t rval;
                rval.setTranslation(x, y, z);
                return rval;
              })
          .def_static(
              "transMatrix",
              [](const vec3_t& t) -> mat4_t {
                mat4_t rval;
                rval.setTranslation(t.x, t.y, t.z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](T x, T y, T z) -> mat4_t {
                mat4_t rval;
                rval.setScale(x, y, z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](const vec3_t& scale) -> mat4_t {
                mat4_t rval;
                rval.setScale(scale.x, scale.y, scale.z);
                return rval;
              })
          .def_static(
              "unproject",
              [](mat4_t rIMVP, const vec3_t& ClipCoord, vec3_t& rVObj) -> bool { return mat4_t::unProject(rIMVP, ClipCoord, rVObj); })
          .def_static(
              "lookAt",
              [](const vec3_t& eye, const vec3_t& tgt, vec3_t& up) -> mat4_t {
                mat4_t rval;
                rval.lookAt(eye, tgt, up);
                return rval;
              })
          //.def("lookAt", &mat4_t::decompose)
          .def(py::self * py::self)
          .def(py::self == py::self)
          .def("__repr__", [](mat4_t mtx) -> std::string {
            auto str = mtx.dump4x3cn();
            return str.c_str();
          });
  type_codec->registerStdCodec<mat4_t>(mat4_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ray3_type = //
      py::class_<ray3_t>(module_core, ray3_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<const vec3_t&, const vec3_t&>())
          .def_property_readonly("origin", [](ray3_t ray) -> vec3_t { return ray.mOrigin; })
          .def_property_readonly("direction", [](ray3_t ray) -> vec3_t { return ray.mDirection; })
          .def(
              "__str__",
              [ray3_name](ray3_t r) -> std::string {
                fxstring<64> fxs;
                auto o = r.mOrigin;
                auto d = r.mDirection;
                fxs.format("%s o(%g,%g,%g) d(%g,%g,%g)", ray3_name.c_str(), o.x, o.y, o.z, d.x, d.y, d.z);
                return fxs.c_str();
              })
          .def("__repr__", [ray3_name](ray3_t r) -> std::string {
            fxstring<64> fxs;
            auto o = r.mOrigin;
            auto d = r.mDirection;
            fxs.format("%s o(%g,%g,%g) d(%g,%g,%g)", ray3_name.c_str(), o.x, o.y, o.z, d.x, d.y, d.z);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<fray3_ptr_t>(ray3_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto plane_t_type =
      py::class_<plane_t>(module_core, plane_name.c_str(), pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](plane_t& plane) -> pybind11::buffer_info {
            auto data = &plane.n.x; // Pointer to buffer
            return pybind11::buffer_info(
                data,               // Pointer to buffer
                sizeof(T),      // Size of one scalar
                pybind11::format_descriptor<T>::format(),
                1,                  // Number of dimensions
                {4},                // Buffer dimensions
                {sizeof(T)});   // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<const vec4_t&>())
          .def(py::init<const vec3_t&, T>())
          .def(py::init<const vec3_t&, const vec3_t&, const vec3_t&>())
          .def(py::init<T, T, T, T>())
          .def_property_readonly("d", [](const plane_t& thisplane) -> T { return thisplane.d; })
          .def_property_readonly("hashed", [](const plane_t& thisplane) -> uint64_t { 
            return thisplane.hash();
            })
          .def_property_readonly("normal", [](const plane_t& thisplane) -> vec3_t { return thisplane.n; })
          .def(
              "fromNormalAndOrigin",
              [](plane_t& plane, const vec3_t& normal, const vec3_t& origin) { plane.CalcFromNormalAndOrigin(normal, origin); })
          .def(
              "fromTriangle",
              [](plane_t& plane, const vec3_t& pta, const vec3_t& ptb, const vec3_t& ptc) {
                plane.CalcPlaneFromTriangle(pta, ptb, ptc);
              })
          .def("reflect", [](const plane_t& plane, const vec3_t& point) -> vec3_t { return plane.reflect(point); })
          .def("isPointInFront", [](const plane_t& plane, const vec3_t& point) -> bool { return plane.isPointInFront(point); })
          .def("isPointBehind", [](const plane_t& plane, const vec3_t& point) -> bool { return plane.isPointBehind(point); })
          .def("isPointCoplanar", [](const plane_t& plane, const vec3_t& point) -> bool { return plane.isPointCoplanar(point); })
          .def("distanceToPoint", [](const plane_t& plane, const vec3_t& point) -> T { return plane.pointDistance(point); })
          .def(
              "intersectWithRay",
              [](const plane_t& thisplane, const ray3_t& ray) -> py::dict {
                vec3_t outpoint;
                T outdistance = 0.0f;
                bool did_intersect = thisplane.Intersect(ray, outdistance, outpoint);
                py::dict rval;
                did_intersect &= (outdistance >= 0.0f);
                rval["did_intersect"] = did_intersect;
                rval["distance"]      = outdistance;
                rval["point"]         = outpoint;
                return rval;
              })
          .def(
              "intersectWithPlane",
              [](const plane_t& thisplane, const plane_t& otherplane, vec3_t& outorigin, vec3_t& outdir) -> bool {
                return thisplane.PlaneIntersect(otherplane, outorigin, outdir);
              })
          .def("__repr__", [plane_name](const plane_t& thisplane) -> std::string {
            fxstring<64> fxs;
            vec3_t n = thisplane.n;
            T d = thisplane.d;
            fxs.format("%s(n<%g %g %g> d<%g>)", plane_name.c_str(), n.x, n.y, n.z, d);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<plane_t>(plane_t_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto frustum_type =
      py::class_<frustum_t,frustum_ptr_t>(module_core, frustum_name.c_str())
          .def(py::init<>())
          .def(py::init([](const mat4_t& VMatrix, const mat4_t& PMatrix) {
            auto rval = std::make_shared<frustum_t>();
            rval->set(VMatrix, PMatrix);
            return rval;
          }))
          .def("set", [](frustum_ptr_t instance, const mat4_t& VMatrix, const mat4_t& PMatrix) { instance->set(VMatrix, PMatrix); })
          .def("set", [](frustum_ptr_t instance, const mat4_t& IVPMatrix) { instance->set(IVPMatrix); })
          .def(
              "nearCorner",
              [](frustum_ptr_t instance, int index) -> vec3_t { return instance->mNearCorners[std::clamp(index, 0, 3)]; })
          .def("farCorner", [](frustum_ptr_t instance, int index) -> vec3_t { return instance->mFarCorners[std::clamp(index, 0, 3)]; })
          .def_property_readonly("center", [](frustum_ptr_t instance) -> vec3_t { return instance->mCenter; })
          .def_property_readonly("xNormal", [](frustum_ptr_t instance) -> vec3_t { return instance->mXNormal; })
          .def_property_readonly("yNormal", [](frustum_ptr_t instance) -> vec3_t { return instance->mYNormal; })
          .def_property_readonly("zNormal", [](frustum_ptr_t instance) -> vec3_t { return instance->mZNormal; })
          .def_property_readonly("nearPlane", [](frustum_ptr_t instance) -> plane_t { return instance->_nearPlane; })
          .def_property_readonly("farPlane", [](frustum_ptr_t instance) -> plane_t { return instance->_farPlane; })
          .def_property_readonly("leftPlane", [](frustum_ptr_t instance) -> plane_t { return instance->_leftPlane; })
          .def_property_readonly("rightPlane", [](frustum_ptr_t instance) -> plane_t { return instance->_rightPlane; })
          .def_property_readonly("topPlane", [](frustum_ptr_t instance) -> plane_t { return instance->_topPlane; })
          .def_property_readonly("bottomPlane", [](frustum_ptr_t instance) -> plane_t { return instance->_bottomPlane; })
          .def("contains", [](frustum_ptr_t instance, const vec3_t& point) -> bool { return instance->contains(point); });
  type_codec->registerStdCodec<frustum_ptr_t>(frustum_type);
  }
///////////////////////////////////////////////////////////////////////////////
}
