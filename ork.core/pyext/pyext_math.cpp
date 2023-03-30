///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math_plane(py::module& module_core);
void pyinit_math(py::module& module_core) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto fvec2_type = //
      py::class_<fvec2>(module_core, "vec2", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fvec2& vec) -> pybind11::buffer_info {
            auto data = vec.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,                  // Pointer to buffer
                sizeof(float),         // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                     // Number of dimensions
                {2},                   // Buffer dimensions
                {sizeof(float)});      // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<float, float>())
          .def_property(
              "x", [](const fvec2& vec) -> float { return vec.x; }, [](fvec2& vec, float val) { return vec.x = val; }) //
          .def_property(
              "y", [](const fvec2& vec) -> float { return vec.y; }, [](fvec2& vec, float val) { return vec.y = val; }) //
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
  type_codec->registerStdCodec<fvec2>(fvec2_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fvec3_type = //
      py::class_<fvec3>(module_core, "vec3", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fvec3& vec) -> pybind11::buffer_info {
            auto data = vec.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,                  // Pointer to buffer
                sizeof(float),         // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                     // Number of dimensions
                {3},                   // Buffer dimensions
                {sizeof(float)});      // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<float>())
          .def(py::init<float, float, float>())
          .def_property(
              "x",
              [](const fvec3& vec) -> float { return vec.x; },  //
              [](fvec3& vec, float val) { return vec.x = val; } //
              )
          .def_property(
              "y",
              [](const fvec3& vec) -> float { return vec.y; },  //
              [](fvec3& vec, float val) { return vec.y = val; } //
              )
          .def_property(
              "z",
              [](const fvec3& vec) -> float { return vec.z; },  //
              [](fvec3& vec, float val) { return vec.z = val; } //
              )
          .def_property_readonly(
              "as_list",
              [](const fvec3& vec) -> py::list { //
                py::list rval;
                rval.append(vec.x);
                rval.append(vec.y);
                rval.append(vec.z);
                return rval;
              })
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
          .def("transform", [](const fvec3& v, fmtx4 matrix) -> fvec3 { return fvec4(v, 1).transform(matrix).xyz(); })
          .def("rotx", &fvec3::rotateOnX)
          .def("roty", &fvec3::rotateOnY)
          .def("rotz", &fvec3::rotateOnZ)
          .def("xy", &fvec3::xy)
          .def("xz", &fvec3::xz)
          .def(py::self + py::self)
          .def(py::self - py::self)
          .def(py::self * py::self)
          .def(py::self * float())
          .def("set", [](fvec3& me, const fvec3& other) { me = other; })
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
  type_codec->registerStdCodec<fvec3>(fvec3_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fvec4_type = //
      py::class_<fvec4>(module_core, "vec4", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fvec4& vec) -> pybind11::buffer_info {
            auto data = vec.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,                  // Pointer to buffer
                sizeof(float),         // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                     // Number of dimensions
                {4},                   // Buffer dimensions
                {sizeof(float)});      // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<float, float, float, float>())
          .def(py::init<fvec3>())
          .def(py::init<fvec3, float>())
          .def(py::init<uint32_t>())
          .def_property(
              "x", [](const fvec4& vec) -> float { return vec.x; }, [](fvec4& vec, float val) { return vec.x = val; } //
              )

          .def_property(
              "y", [](const fvec4& vec) -> float { return vec.y; }, [](fvec4& vec, float val) { return vec.y = val; } //
              )
          .def_property(
              "z", [](const fvec4& vec) -> float { return vec.z; }, [](fvec4& vec, float val) { return vec.z = val; } //
              )
          .def_property(
              "w", [](const fvec4& vec) -> float { return vec.w; }, [](fvec4& vec, float val) { return vec.w = val; } //
              )
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
  type_codec->registerStdCodec<fvec4>(fvec4_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto fquat_type = //
      py::class_<fquat>(module_core, "quat", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fquat& quat) -> pybind11::buffer_info {
            auto data = quat.asArray(); // Pointer to buffer
            return pybind11::buffer_info(
                data,                   // Pointer to buffer
                sizeof(float),          // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                      // Number of dimensions
                {4},                    // Buffer dimensions
                {sizeof(float)});       // Strides (in bytes) for each index
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
  type_codec->registerStdCodec<fquat>(fquat_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mtx3_type = //
      py::class_<fmtx3>(module_core, "mtx3", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fmtx3& mtx) -> pybind11::buffer_info {
            auto data = mtx.asArray();               // Pointer to buffer
            return pybind11::buffer_info(
                data,                                // Pointer to buffer
                sizeof(float),                       // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                2,                                   // Number of dimensions
                {3, 3},                              // Buffer dimensions
                {sizeof(float) * 3, sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<const fmtx3&>())
          .def(py::init<const fquat&>())
          .def("setScale", (void(fmtx3::*)(float, float, float)) & fmtx3::setScale)
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
  type_codec->registerStdCodec<fmtx3>(mtx3_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mtx4_type = //
      py::class_<fmtx4>(module_core, "mtx4", pybind11::buffer_protocol())
          //////////////////////////////////////////////////////////////////////////
          .def_buffer([](fmtx4& mtx) -> pybind11::buffer_info {
            auto data = mtx.asArray();               // Pointer to buffer
            return pybind11::buffer_info(
                data,                                // Pointer to buffer
                sizeof(float),                       // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                2,                                   // Number of dimensions
                {4, 4},                              // Buffer dimensions
                {sizeof(float) * 4, sizeof(float)}); // Strides (in bytes) for each index
          })
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def(py::init<const fmtx4&>())
          .def(py::init<const fquat&>())
          .def_property_readonly("transposed", [](const fmtx4& inp) -> fmtx4 {
            return inp.transposed();
          })
          .def_property_readonly("inverse", &fmtx4::inverse)
          .def("zNormal", &fmtx4::xNormal)
          .def("yNormal", &fmtx4::yNormal)
          .def("xNormal", &fmtx4::zNormal)
          .def("transpose", &fmtx4::transpose)
          .def("normalize", &fmtx4::normalizeInPlace)
          .def("inverseOf", &fmtx4::inverseOf)
          .def("decompose", &fmtx4::decompose)
          .def("toRotMatrix3", &fmtx4::rotMatrix33)
          .def("toGlm", &fmtx4::asGlmMat4)
          .def(
              "getColumn",
              [](fmtx4 mtx, int column) -> fvec4 { //
                return mtx.column(column);
              })
          .def(
              "setColumn",
              [](fmtx4& mtx, int column, fvec4 c) { //
                mtx.setColumn(column, c);
              })
          .def(
              "getRow",
              [](fmtx4 mtx, int row) -> fvec4 { //
                return mtx.row(row);
              })
          .def(
              "setRow",
              [](fmtx4& mtx, int row, fvec4 c) { //
                mtx.setRow(row, c);
              })
          .def(
              "compose",
              [](fmtx4& mtx, const fvec3& pos, const fquat& rot, float scale) { //
                mtx.compose(pos, rot, scale);
              })
          .def(
              "compose",
              [](fmtx4& mtx, const fvec3& pos, const fquat& rot, const fvec3& vscale) { //
                mtx.compose(pos, rot, vscale);
              })
          .def(
              "dump",
              [](fmtx4 mtx, std::string name) { //
                mtx.dump(name);
              })
          .def_static("perspective", &fmtx4::createPerspectiveMatrix)
          .def_static(
              "composed",
              [](const fvec3& pos, const fquat& rot, float scale) -> fmtx4 {
                fmtx4 rval;
                rval.compose(pos, rot, scale);
                return rval;
              })
          .def_static(
              "composed",
              [](const fvec3& pos, const fquat& rot, fvec3 scale) -> fmtx4 {
                fmtx4 rval;
                rval.compose(pos, rot, scale);
                return rval;
              })
          .def_static(
              "deltaMatrix",
              [](fmtx4 from, fmtx4 to) -> fmtx4 {
                fmtx4 rval;
                rval.correctionMatrix(from, to);
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const fquat& q) -> fmtx4 {
                fmtx4 rval;
                rval.fromQuaternion(q);
                return rval;
              })
          .def_static(
              "rotMatrix",
              [](const fvec3& axis, float angle) -> fmtx4 {
                fmtx4 rval;
                rval.fromQuaternion(fquat(axis, angle));
                return rval;
              })
          .def_static(
              "transMatrix",
              [](float x, float y, float z) -> fmtx4 {
                fmtx4 rval;
                rval.setTranslation(x, y, z);
                return rval;
              })
          .def_static(
              "transMatrix",
              [](const fvec3& t) -> fmtx4 {
                fmtx4 rval;
                rval.setTranslation(t.x, t.y, t.z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](float x, float y, float z) -> fmtx4 {
                fmtx4 rval;
                rval.setScale(x, y, z);
                return rval;
              })
          .def_static(
              "scaleMatrix",
              [](const fvec3& scale) -> fmtx4 {
                fmtx4 rval;
                rval.setScale(scale.x, scale.y, scale.z);
                return rval;
              })
          .def_static(
              "unproject",
              [](fmtx4 rIMVP, const fvec3& ClipCoord, fvec3& rVObj) -> bool { return fmtx4::unProject(rIMVP, ClipCoord, rVObj); })
          .def_static(
              "lookAt",
              [](const fvec3& eye, const fvec3& tgt, fvec3& up) -> fmtx4 {
                fmtx4 rval;
                rval.lookAt(eye, tgt, up);
                return rval;
              })
          //.def("lookAt", &fmtx4::decompose)
          .def(py::self * py::self)
          .def(py::self == py::self)
          .def("__repr__", [](fmtx4 mtx) -> std::string {
            auto str = mtx.dump4x3cn();
            return str.c_str();
          });
  type_codec->registerStdCodec<fmtx4>(mtx4_type);
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
    if (dcxf._usedirectmatrix) {
      auto str = dcxf._directmatrix.dump4x3cn();
      fxs      = FormatString("Transform(precomposed) mtx(%s)", str.c_str());
    } else {
      auto o  = dcxf._translation;
      auto r  = dcxf._rotation;
      float s = dcxf._uniformScale;
      fxs     = FormatString("Transform(decomposed) p(%g,%g,%g) o(%g,%g,%g,%g) s:%g", o.x, o.y, o.z, r.w, r.x, r.y, r.z, s);
    }
    return fxs.c_str();
  };
  //
  auto dcxf_type = //
      py::class_<DecompTransform, decompxf_ptr_t>(module_core, "Transform")
          //////////////////////////////////////////////////////////////////////////
          .def(py::init<>())
          .def_property(
              "translation",
              [](decompxf_const_ptr_t dcxf) -> fvec3 { return dcxf->_translation; },
              [](decompxf_ptr_t dcxf, fvec3 inp) { dcxf->_translation = inp; })
          .def_property(
              "orientation",
              [](decompxf_const_ptr_t dcxf) -> fquat { return dcxf->_rotation; },
              [](decompxf_ptr_t dcxf, fquat inp) { dcxf->_rotation = inp; })
          .def_property(
              "scale",
              [](decompxf_const_ptr_t dcxf) -> float { return dcxf->_uniformScale; },
              [](decompxf_ptr_t dcxf, float sc) { dcxf->_uniformScale = sc; })
          .def_property(
              "directMatrix",
              [](decompxf_const_ptr_t dcxf) -> fmtx4 { return dcxf->_directmatrix; },
              [](decompxf_ptr_t dcxf, fmtx4 inp) {
                dcxf->_directmatrix    = inp;
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
  auto mathconstantsproxy_type   =                                                           //
      py::class_<MathConstantsProxy, mathconstantsproxy_ptr_t>(module_core, "mathconstants") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                                       //
              [type_codec](mathconstantsproxy_ptr_t proxy, const std::string& key) -> py::object { //
                svar64_t value;
                value.set<void*>(nullptr);
                if (key == "DTOR") {
                  value.set<float>(DTOR);
                }
                return type_codec->encode(value);
              });
  type_codec->registerStdCodec<mathconstantsproxy_ptr_t>(mathconstantsproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_math_plane(module_core);
}

} // namespace ork
