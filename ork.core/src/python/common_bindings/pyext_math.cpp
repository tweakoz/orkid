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
#include <ork/math/transform_curve.h>
#include <ork/math/noiselib.inl>
#include <ork/math/audiomath.h>
#include <ork/python/pycodec.inl>
#include <ork/math/box.h>
#include <ork/math/sphere.h>
#include <ork/math/math_types.inl>

namespace py = pybind11;
using namespace pybind11::literals;
using adapter_t = ork::python::pybind11adapter;

#include <ork/python/common_bindings/pyext_math_la.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
void init_math_la_float(py::module& module_core,python::pb11_typecodec_ptr_t type_codec);
void init_math_la_double(py::module& module_core,python::pb11_typecodec_ptr_t type_codec);
void init_math(py::module& module_core,python::pb11_typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  using mtxprov_t = py::class_<MatrixProvider, matrix_provider_ptr_t>;
  auto mtxprov_type = mtxprov_t(module_core, "MatrixProvider");
  type_codec->registerStdCodec<matrix_provider_ptr_t>(mtxprov_type);
  /////////////////////////////////////////////////////////////////////////////////
  using sphere_ptr_t = std::shared_ptr<Sphere>;
  auto sphere_t = py::class_<Sphere, sphere_ptr_t>(module_core, "Sphere") //
  .def(py::init<>([](const fvec3& center, float radius) -> sphere_ptr_t {
    return std::make_shared<ork::Sphere>(center, radius);
  }))
  .def_property_readonly("center", [] (sphere_ptr_t self)-> fvec3 {
    return self->mCenter;
  })
  .def_property_readonly("radius", [] (sphere_ptr_t self)-> float {
    return self->mRadius;
  })
  .def("intersect",[](sphere_ptr_t self, //
                      const fray3& ray) -> py::dict { //
    fvec3 isect_in;
    fvec3 isect_out;
    fvec3 isect_normal;
    bool bintersect = self->Intersect(ray, isect_in, isect_out, isect_normal);
    py::dict rval;
    rval["did_intersect"] = bintersect;
    rval["isect_in"]      = isect_in;
    rval["isect_out"]     = isect_out;
    rval["isect_normal"]  = isect_normal;
    return rval;
  });
  type_codec->registerStdCodec<sphere_ptr_t>(sphere_t);
  /////////////////////////////////////////////////////////////////////////////////
  using aabb_ptr_t = std::shared_ptr<ork::AABox>;
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
                else if (key == "PI2") {
                  value.set<float>(PI2);
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
  using namespace ork::math;
  /////////////////////////////////////////////////////////////////////////////////
  auto tcpoint_type = //
      py::class_<TransformCurvePoint, std::shared_ptr<TransformCurvePoint>>(module_core, "TransformCurvePoint")
          .def(py::init<>())
          .def_readwrite("time", &TransformCurvePoint::_time)
          .def_readwrite("position", &TransformCurvePoint::_position)
          .def_readwrite("rotation", &TransformCurvePoint::_rotation)
          .def_readwrite("scale", &TransformCurvePoint::_scale)
          .def_readwrite("tangent_out", &TransformCurvePoint::_tangent_out)
          .def_readwrite("tangent_in", &TransformCurvePoint::_tangent_in);
  type_codec->registerStdCodec<std::shared_ptr<TransformCurvePoint>>(tcpoint_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tcsample_type = //
      py::class_<TransformCurveSample, std::shared_ptr<TransformCurveSample>>(module_core, "TransformCurveSample")
          .def(py::init<>())
          .def_readonly("position", &TransformCurveSample::_position)
          .def_readonly("rotation", &TransformCurveSample::_rotation)
          .def_readonly("scale", &TransformCurveSample::_scale)
          .def_readonly("tangent", &TransformCurveSample::_tangent);
  type_codec->registerStdCodec<std::shared_ptr<TransformCurveSample>>(tcsample_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tcurve_type = //
      py::class_<TransformCurve, Object, transformcurve_ptr_t>(module_core, "TransformCurve")
          .def(py::init<>())
          .def(
              "addPoint",
              [](transformcurve_ptr_t self, const TransformCurvePoint& pt) -> int { //
                return self->addPoint(pt);
              })
          .def(
              "removePoint",
              [](transformcurve_ptr_t self, int index) { //
                self->removePoint(index);
              })
          .def(
              "setPoint",
              [](transformcurve_ptr_t self, int index, const TransformCurvePoint& pt) { //
                self->setPoint(index, pt);
              })
          .def(
              "getPoint",
              [](transformcurve_ptr_t self, int index) -> TransformCurvePoint { //
                return self->getPoint(index);
              })
          .def_property_readonly(
              "numPoints",
              [](transformcurve_ptr_t self) -> int { //
                return self->numPoints();
              })
          .def(
              "setSegmentType",
              [](transformcurve_ptr_t self, int seg_index, std::string type_str) { //
                CurveSegmentType t = CurveSegmentType::LINEAR;
                if (type_str == "BEZIER")
                  t = CurveSegmentType::BEZIER;
                else if (type_str == "CATMULL_ROM")
                  t = CurveSegmentType::CATMULL_ROM;
                self->setSegmentType(seg_index, t);
              })
          .def(
              "getSegmentType",
              [](transformcurve_ptr_t self, int seg_index) -> std::string { //
                auto t = self->getSegmentType(seg_index);
                switch (t) {
                  case CurveSegmentType::LINEAR:
                    return "LINEAR";
                  case CurveSegmentType::BEZIER:
                    return "BEZIER";
                  case CurveSegmentType::CATMULL_ROM:
                    return "CATMULL_ROM";
                  default:
                    return "LINEAR";
                }
              })
          .def(
              "sample",
              [](transformcurve_ptr_t self, float t) -> TransformCurveSample { //
                return self->sample(t);
              })
          .def(
              "samplePosition",
              [](transformcurve_ptr_t self, float t) -> fvec3 { //
                return self->samplePosition(t);
              })
          .def(
              "sampleMatrix",
              [](transformcurve_ptr_t self, float t) -> fmtx4 { //
                return self->sampleMatrix(t);
              });
  type_codec->registerStdCodec<transformcurve_ptr_t>(tcurve_type);
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
        .def(py::init<uint32_t, uint32_t, uint32_t, uint32_t>())
        .def_readwrite("x", &u32vec4::x)
        .def_readwrite("y", &u32vec4::y)
        .def_readwrite("z", &u32vec4::z)
        .def_readwrite("w", &u32vec4::w)
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
  /////////////////////////////////////////////////////////////////////////////////
  // Klein Geometric Algebra bindings
  /////////////////////////////////////////////////////////////////////////////////
  using kln_rotor_ptr_t = std::shared_ptr<kln::rotor>;
  auto kln_rotor_type = py::class_<kln::rotor, kln_rotor_ptr_t>(module_core, "Rotor")
    // Default constructor (identity rotor)
    .def(py::init<>([]() -> kln_rotor_ptr_t {
      return std::make_shared<kln::rotor>();
    }))
    // Construct from angle (radians) and axis (normalized)
    .def(py::init<>([](float angle_radians, float ax, float ay, float az) -> kln_rotor_ptr_t {
      return std::make_shared<kln::rotor>(angle_radians, ax, ay, az);
    }), py::arg("angle"), py::arg("ax"), py::arg("ay"), py::arg("az"),
    "Create rotor from angle (radians) and axis (ax, ay, az)")
    // Convenience: create 2D rotor (rotation about Z axis)
    .def_static("fromAngle2D", [](float angle_radians) -> kln_rotor_ptr_t {
      return std::make_shared<kln::rotor>(angle_radians, 0.f, 0.f, 1.f);
    }, py::arg("angle"), "Create 2D rotor (rotation about Z axis)")
    // Multiply rotors (compose rotations)
    .def("__mul__", [](kln_rotor_ptr_t self, kln_rotor_ptr_t other) -> kln_rotor_ptr_t {
      return std::make_shared<kln::rotor>((*self) * (*other));
    })
    // In-place multiply
    .def("__imul__", [](kln_rotor_ptr_t self, kln_rotor_ptr_t other) -> kln_rotor_ptr_t {
      *self = (*self) * (*other);
      return self;
    })
    // Reverse (conjugate) - inverse for unit rotors
    .def("reverse", [](kln_rotor_ptr_t self) -> kln_rotor_ptr_t {
      return std::make_shared<kln::rotor>(~(*self));
    }, "Return the reverse (conjugate) of the rotor")
    // Normalize
    .def("normalize", [](kln_rotor_ptr_t self) -> kln_rotor_ptr_t {
      self->normalize();
      return self;
    }, "Normalize the rotor in place")
    .def("normalized", [](kln_rotor_ptr_t self) -> kln_rotor_ptr_t {
      kln::rotor r = *self;
      r.normalize();
      return std::make_shared<kln::rotor>(r);
    }, "Return a normalized copy of the rotor")
    // Extract angle for 2D rotor (rotation about Z)
    .def("angle2D", [](kln_rotor_ptr_t self) -> float {
      // For a rotor r = cos(θ/2) + sin(θ/2)*e12
      // The scalar part is cos(θ/2), stored in the first component
      // We can extract the angle using atan2
      float scalar = self->scalar();
      float e12 = self->e12();
      return 2.0f * std::atan2(e12, scalar);
    }, "Extract the rotation angle (radians) for a 2D rotor about Z axis")
    // Access components
    .def_property_readonly("scalar", [](kln_rotor_ptr_t self) -> float {
      return self->scalar();
    })
    .def_property_readonly("e12", [](kln_rotor_ptr_t self) -> float {
      return self->e12();
    })
    .def_property_readonly("e31", [](kln_rotor_ptr_t self) -> float {
      return self->e31();
    })
    .def_property_readonly("e23", [](kln_rotor_ptr_t self) -> float {
      return self->e23();
    })
    .def("__repr__", [](kln_rotor_ptr_t self) -> std::string {
      return FormatString("Rotor(scalar=%f, e23=%f, e31=%f, e12=%f)",
                          self->scalar(), self->e23(), self->e31(), self->e12());
    });
  type_codec->registerStdCodec<kln_rotor_ptr_t>(kln_rotor_type);
  /////////////////////////////////////////////////////////////////////////////////
  }

} // namespace ork
