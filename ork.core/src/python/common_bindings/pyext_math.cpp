///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/gradient.h>
#include <ork/math/multicurve.h>
#include <ork/math/noiselib.inl>
#include <ork/math/audiomath.h>
#include <ork/python/pycodec.inl>
#include <ork/math/box.h>

namespace py = pybind11;
using namespace pybind11::literals;
using adapter_t = ork::python::pybind11adapter;

#include <ork/python/common_bindings/pyext_math_la.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
void init_math_la_float(py::module& module_core,python::pb11_typecodec_ptr_t type_codec);
void init_math_la_double(py::module& module_core,python::pb11_typecodec_ptr_t type_codec);
void init_math(py::module& module_core,python::pb11_typecodec_ptr_t type_codec) {
  using aabb_ptr_t = std::shared_ptr<ork::AABox>;
  /////////////////////////////////////////////////////////////////////////////////
  auto aabb_type_t = py::class_<AABox, aabb_ptr_t>(module_core, "aabb") //
  .def(py::init<>())
  .def_property_readonly("center", [] (aabb_ptr_t self)-> fvec3 {
    return self->center();
  })
  .def_property_readonly("size", [] (aabb_ptr_t self)-> fvec3 {
    return self->size();
  })
  .def("intersect",[](aabb_ptr_t self, const fray3& ray, fvec3& isect_in, fvec3& isect_out) -> bool {
    return self->Intersect(ray, isect_in, isect_out);
  });
  type_codec->registerStdCodec<aabb_ptr_t>(aabb_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  struct MathConstantsProxy {};
  using mathconstantsproxy_ptr_t = std::shared_ptr<MathConstantsProxy>;
  auto mathconstantsproxy_type   =                                                           //
      py::class_<MathConstantsProxy, mathconstantsproxy_ptr_t>(module_core, "mathconstants") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                                       //
              [type_codec](mathconstantsproxy_ptr_t proxy, const std::string& key) -> py::object { //
                python::varval_t value;
                value.set<void*>(nullptr);
                if (key == "DTOR") {
                  value.set<float>(DTOR);
                }
                return type_codec->encode(value);
              });
  type_codec->registerStdCodec<mathconstantsproxy_ptr_t>(mathconstantsproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  init_math_la_float(module_core,type_codec);
  init_math_la_double(module_core,type_codec);
  /////////////////////////////////////////////////////////////////////////////////
    auto curve_type = //
      py::class_<MultiCurve1D,Object,multicurve1d_ptr_t>(module_core, "MultiCurve1D")
      .def(py::init<>())
      .def("splitSegment", [](multicurve1d_ptr_t self, int iseg) -> void { //
        self->SplitSegment(iseg);
      })
      .def("mergeSegment", [](multicurve1d_ptr_t self, int ifirstseg) -> void { //
        self->MergeSegment(ifirstseg);
      })
      .def("setSegmentType", [](multicurve1d_ptr_t self, int iseg, crcstring_ptr_t segtype) -> void { //
        auto etype = MultiCurveSegmentType(segtype->hashed());
        self->SetSegmentType(iseg, etype);
      })
      .def("sample", [](multicurve1d_ptr_t self, float fu) -> float { //
        return self->Sample(fu);
      })
      .def("setPoint", [](multicurve1d_ptr_t self, int ipoint, float fu, float fv) -> void { //
        self->SetPoint(ipoint, fu, fv);
      })
      .def_property("min", [](multicurve1d_ptr_t self) -> float { //
        return self->mMin;
      }, [](multicurve1d_ptr_t self, float fmin) -> void { //
        self->SetMin(fmin);
      })
      .def_property("max", [](multicurve1d_ptr_t self) -> float { //
        return self->mMax;
      }, [](multicurve1d_ptr_t self, float fmax) -> void { //
        self->SetMax(fmax);
      })
      .def_property_readonly("numSegments", [](multicurve1d_ptr_t self) -> int { //
        return self->GetNumSegments();
      })
      .def_property_readonly("numVertices", [](multicurve1d_ptr_t self) -> size_t { //
        return self->GetNumVertices();
      });
  type_codec->registerStdCodec<multicurve1d_ptr_t>(curve_type);
  /////////////////////////////////////////////////////////////////////////////////
    auto gradient_type = //
      py::class_<gradient_fvec4_t,Object,gradient_fvec4_ptr_t>(module_core, "GradientV4")
        .def(py::init<>())
        .def("addColorStop", [](gradient_fvec4_ptr_t self, float flerp, const fvec4& data) -> void { //
          self->addDataPoint(flerp, data);
        })
        .def("setColorStops", [](gradient_fvec4_ptr_t self, py::dict stops) {
          self->_data.clear();
          for (auto item : stops) {
              float flerp = item.first.cast<float>();
              fvec4 data = item.second.cast<fvec4>();
              self->addDataPoint(flerp, data);
          }
        })
        .def("sample", [](gradient_fvec4_ptr_t self, float fu) -> fvec4 { //
          return self->sample(fu);
        })
        .def("clear", [](gradient_fvec4_ptr_t self) -> void { //
          self->_data.clear();
        });
  type_codec->registerStdCodec<gradient_fvec4_ptr_t>(gradient_type);
  /////////////////////////////////////////////////////////////////////////////////
    auto u32vec4_type = //
      py::class_<u32vec4,u32vec4_ptr_t>(module_core, "u32vec4")
        .def(py::init<>())
        .def("__repr__", [](u32vec4_ptr_t value) -> std::string { //
          return FormatString("u32vec4<0x%08x 0x%08x 0x%08x 0x%08x>", value->x, value->y, value->z, value->w);
        });
  type_codec->registerStdCodec<u32vec4_ptr_t>(u32vec4_type);
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("dmtx4_to_fmtx4", [](const dmtx4& dmtx) -> fmtx4 { //
    return dmtx4_to_fmtx4(dmtx);
  });
  module_core.def("fmtx4_to_dmtx4", [](const fmtx4& dmtx) -> dmtx4 { //
    return fmtx4_to_dmtx4(dmtx);
  });
  module_core.def("log_base", [](float base, float inp) -> float { //
    return log_base(base, inp);
  });
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("mnoise", [](fvec3 input) -> float { //
    return libnoise::noise(input);
  });
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("clamp", [](float inp, float a, float b) -> float { //
    return ::std::clamp(inp, a, b);
  });
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("lerp_float", [](float a, float b, float index) -> float { //
    return ::std::lerp(a, b, index);
  });
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("smooth_step", [](float edge0, float edge1, float x) -> float { //
    return ::ork::audiomath::smoothstep(edge0, edge1, x);
  });
  }

} // namespace ork
