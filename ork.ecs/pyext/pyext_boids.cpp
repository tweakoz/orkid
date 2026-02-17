////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/BoidsComponent.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_boids(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto boidscompdata_type = //
      py::class_<BoidsComponentData, ComponentData, boidscompdata_ptr_t>(
          module_ecs, "BoidsComponentData")
          .def(
              "__repr__",
              [](const boidscompdata_ptr_t& cd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BoidsComponentData(%p)", cd.get());
                return fxs.c_str();
              })
          .def_property(
              "flockID",
              [](boidscompdata_ptr_t cd) -> int { return cd->_flockID; },
              [](boidscompdata_ptr_t cd, int val) { cd->_flockID = val; })
          .def_property(
              "mode",
              [](boidscompdata_ptr_t cd) -> int { return (int)cd->_mode; },
              [](boidscompdata_ptr_t cd, int val) { cd->_mode = (BoidsMode)val; })
          .def_property(
              "separationWeight",
              [](boidscompdata_ptr_t cd) -> float { return cd->_separationWeight; },
              [](boidscompdata_ptr_t cd, float val) { cd->_separationWeight = val; })
          .def_property(
              "separationRadius",
              [](boidscompdata_ptr_t cd) -> float { return cd->_separationRadius; },
              [](boidscompdata_ptr_t cd, float val) { cd->_separationRadius = val; })
          .def_property(
              "alignmentWeight",
              [](boidscompdata_ptr_t cd) -> float { return cd->_alignmentWeight; },
              [](boidscompdata_ptr_t cd, float val) { cd->_alignmentWeight = val; })
          .def_property(
              "alignmentRadius",
              [](boidscompdata_ptr_t cd) -> float { return cd->_alignmentRadius; },
              [](boidscompdata_ptr_t cd, float val) { cd->_alignmentRadius = val; })
          .def_property(
              "cohesionWeight",
              [](boidscompdata_ptr_t cd) -> float { return cd->_cohesionWeight; },
              [](boidscompdata_ptr_t cd, float val) { cd->_cohesionWeight = val; })
          .def_property(
              "cohesionRadius",
              [](boidscompdata_ptr_t cd) -> float { return cd->_cohesionRadius; },
              [](boidscompdata_ptr_t cd, float val) { cd->_cohesionRadius = val; })
          .def_property(
              "maxForce",
              [](boidscompdata_ptr_t cd) -> float { return cd->_maxForce; },
              [](boidscompdata_ptr_t cd, float val) { cd->_maxForce = val; })
          .def_property(
              "maxSpeed",
              [](boidscompdata_ptr_t cd) -> float { return cd->_maxSpeed; },
              [](boidscompdata_ptr_t cd, float val) { cd->_maxSpeed = val; })
          .def_property(
              "wanderStrength",
              [](boidscompdata_ptr_t cd) -> float { return cd->_wanderStrength; },
              [](boidscompdata_ptr_t cd, float val) { cd->_wanderStrength = val; })
          .def_property(
              "groundHeight",
              [](boidscompdata_ptr_t cd) -> float { return cd->_groundHeight; },
              [](boidscompdata_ptr_t cd, float val) { cd->_groundHeight = val; })
          .def_property(
              "homeWeight",
              [](boidscompdata_ptr_t cd) -> float { return cd->_homeWeight; },
              [](boidscompdata_ptr_t cd, float val) { cd->_homeWeight = val; })
          .def_property(
              "homeRadius",
              [](boidscompdata_ptr_t cd) -> float { return cd->_homeRadius; },
              [](boidscompdata_ptr_t cd, float val) { cd->_homeRadius = val; });
  type_codec->registerStdCodec<boidscompdata_ptr_t>(boidscompdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto boidssysdata_type = //
      py::class_<BoidsSystemData, SystemData, boidssysdata_ptr_t>(
          module_ecs, "BoidsSystemData")
          .def(
              "__repr__",
              [](boidssysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BoidsSystemData(%p)", sd.get());
                return fxs.c_str();
              });
  type_codec->registerStdCodec<boidssysdata_ptr_t>(boidssysdata_type);
}
} // namespace ork::ecs
