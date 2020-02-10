#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/object/Object.h>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

namespace std {
ostream& operator<<(ostream& ostr, const ::ork::fvec4& vec4) {
  ostr << "(" << vec4.x << "," << vec4.y << "," << vec4.z << vec4.w << ")";
  return ostr;
}
ostream& operator<<(ostream& ostr, const ::ork::fvec3& vec3) {
  ostr << "(" << vec3.x << "," << vec3.y << "," << vec3.z << ")";
  return ostr;
}
ostream& operator<<(ostream& ostr, const ::ork::fvec2& vec2) {
  ostr << "(" << vec2.x << "," << vec2.y << ")";
  return ostr;
}
} // namespace std

PYBIND11_MODULE(orkcore,m){
  using namespace ork;
  m.doc() = "Orkid Core Library (math,kernel,reflection,ect..)";
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fvec2>(m, "vec2")
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
   .def(py::self * py::self)               .def("__str__", [](const fvec2& v) -> std::string {
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
  py::class_<fvec3>(m, "vec3")
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
   .def("__str__", [](const fvec3& v) -> std::string {
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
  py::class_<fvec4>(m, "vec4")
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
   .def(py::self + py::self)
   .def(py::self - py::self)
   .def(py::self * py::self)
   .def("__str__", [](const fvec4& v) -> std::string {
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
  py::class_<fquat>(m, "quat")
   .def(py::init<>())
   .def(py::init<float, float, float, float>())
   .def(py::init<fvec3,float>())
   .def(py::init<fmtx3>())
   .def("mag",&fquat::Magnitude)
   .def("fromAxisAngle",&fquat::fromAxisAngle)
   .def("toAxisAngle",&fquat::toAxisAngle)
   .def("conjugate",&fquat::Magnitude)
   .def("square",&fquat::Square)
   .def("negate",&fquat::Negate)
   .def("normalize",&fquat::Normalize)
   .def(py::self * py::self)
   .def("__str__", [](const fquat& v) -> std::string {
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
  py::class_<fmtx3>(m, "mtx3")
   .def(py::init<>())
   .def(py::init<const fmtx3&>())
   .def(py::init<const fquat&>())
   .def("setScale",(void (fmtx3::*)(float,float,float)) &fmtx3::SetScale)
   .def("fromQuaternion",&fmtx3::FromQuaternion)
   .def("zNormal",&fmtx3::GetXNormal)
   .def("yNormal",&fmtx3::GetYNormal)
   .def("xNormal",&fmtx3::GetZNormal)
   .def("__str__", [](const fmtx3& mtx) -> std::string {
     auto str = mtx.dumpcn();
     return str.c_str();
   })
   .def("__repr__", [](const fmtx3& mtx) -> std::string {
     auto str = mtx.dumpcn();
     return str.c_str();
   });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fmtx4>(m, "mtx4")
   .def(py::init<>())
   .def(py::init<const fmtx4&>())
   .def("zNormal",&fmtx4::GetXNormal)
   .def("yNormal",&fmtx4::GetYNormal)
   .def("xNormal",&fmtx4::GetZNormal)
   .def("__str__", [](const fmtx4& mtx) -> std::string {
     auto str = mtx.dump4x3cn();
     return str.c_str();
   })
   .def("__repr__", [](const fmtx4& mtx) -> std::string {
     auto str = mtx.dump4x3cn();
     return str.c_str();
   });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ork::Object>(m, "ork_Object").def("clazz", [](ork::Object* o) -> std::string {
    auto clazz = rtti::downcast<object::ObjectClass*>(o->GetClass());
    auto name  = clazz->Name();
    return name.c_str();
  });
};

/*PYBIND11_EMBEDDED_MODULE(orkid,m){
py::module m_sub = m.def_submodule("core");
py::module core = py::module::import("core");
m_sub.attr("__dict__") = core;
//m_sub.def("submodule_func", []() { return "submodule_func()"; });

};*/
