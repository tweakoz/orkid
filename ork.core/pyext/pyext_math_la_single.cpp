///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include "pyext_math_la.inl"
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math_la_float(py::module& module_core) {
  pyinit_math_la_t<float>(module_core, "");
  auto type_codec = python::TypeCodec::instance();
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
  ///////////////////////////////////////////////////////////////////////////////
}

} // namespace ork