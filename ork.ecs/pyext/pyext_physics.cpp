////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/physics/bullet.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_physics(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto bullc_type = py::class_<BulletObjectComponentData, ComponentData, bulletcompdata_ptr_t>(module_ecs, "BulletObjectComponentData")
      .def(
          "__repr__",
          [](const bulletcompdata_ptr_t& physc) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::BulletObjectComponentData(%p)", physc.get());
            return fxs.c_str();
          })
          .def_property("mass",
                        [](const bulletcompdata_ptr_t& physc) -> float { return physc->_mass; },
                        [](bulletcompdata_ptr_t& physc, float val) { physc->_mass = val; })
          .def_property("friction",
                        [](const bulletcompdata_ptr_t& physc) -> float { return physc->_friction; },
                        [](bulletcompdata_ptr_t& physc, float val) { physc->_friction = val; })
          .def_property("restitution",
                        [](const bulletcompdata_ptr_t& physc) -> float { return physc->_restitution; },
                        [](bulletcompdata_ptr_t& physc, float val) { physc->_restitution = val; })
          .def_property("angularDamping",
                        [](const bulletcompdata_ptr_t& physc) -> float { return physc->_angularDamping; },
                        [](bulletcompdata_ptr_t& physc, float val) { physc->_angularDamping = val; })
          .def_property("linearDamping",
                        [](const bulletcompdata_ptr_t& physc) -> float { return physc->_linearDamping; },
                        [](bulletcompdata_ptr_t& physc, float val) { physc->_linearDamping = val; })
          .def_property("allowSleeping",
                        [](const bulletcompdata_ptr_t& physc) -> bool { return physc->_allowSleeping; },
                        [](bulletcompdata_ptr_t& physc, bool val) { physc->_allowSleeping = val; })
          .def_property("isKinematic",
                        [](const bulletcompdata_ptr_t& physc) -> bool { return physc->_isKinematic; },
                        [](bulletcompdata_ptr_t& physc, bool val) { physc->_isKinematic = val; })
          .def_property("disablePhysics",
                        [](const bulletcompdata_ptr_t& physc) -> bool { return physc->_disablePhysics; },
                        [](bulletcompdata_ptr_t& physc, bool val) { physc->_disablePhysics = val; })
          .def_property("shape", [](const bulletcompdata_ptr_t& physc) -> shapedata_ptr_t { return physc->_shapedata; },
                        [](bulletcompdata_ptr_t& physc, shapedata_ptr_t val) { physc->_shapedata = val; });

  type_codec->registerStdCodec<bulletsysdata_ptr_t>(bullc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapebase_type = py::class_<BulletShapeBaseData,ork::Object,shapedata_ptr_t>(module_ecs, "BulletShapeBaseData")
      .def(
          "__repr__",
          [](const shapedata_ptr_t& shape) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::BulletShapeBaseData(%p)", shape.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<shapedata_ptr_t>(shapebase_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapesphere_type = py::class_<BulletShapeSphereData,BulletShapeBaseData,bulletshapespheredata_ptr_t>(module_ecs, "BulletShapeSphereData")
      .def(py::init<>())
      .def(
          "__repr__",
          [](const bulletshapespheredata_ptr_t& shape) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::BulletShapeSphereData(%p)", shape.get());
            return fxs.c_str();
          })
          .def_property("radius",
                        [](const bulletshapespheredata_ptr_t& shape) -> float { return shape->_radius; },
                        [](bulletshapespheredata_ptr_t& shape, float val) { shape->_radius = val; });
  type_codec->registerStdCodec<bulletshapespheredata_ptr_t>(shapesphere_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bullsys_type = py::class_<BulletSystemData,SystemData,bulletsysdata_ptr_t>(module_ecs, "BulletSystemData")
      .def(
          "__repr__",
          [](const bulletsysdata_ptr_t& sysdata) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::BulletSystemData(%p)", sysdata.get());
            return fxs.c_str();
          })
          .def_property("lingravity",
                        [](const bulletsysdata_ptr_t& sysdata) -> const fvec3& { return sysdata->_lingravity; },
                        [](bulletsysdata_ptr_t& sysdata, const fvec3& val) { sysdata->_lingravity = val; })
          .def_property("expgravity",
                        [](const bulletsysdata_ptr_t& sysdata) -> const fvec3& { return sysdata->_expgravity; },
                        [](bulletsysdata_ptr_t& sysdata, const fvec3& val) { sysdata->_expgravity = val; });
  type_codec->registerStdCodec<bulletsysdata_ptr_t>(bullsys_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bullfc_type = py::class_<BulletObjectForceControllerData,ork::Object,forcecontrollerdata_ptr_t>(module_ecs, "BulletObjectForceControllerData")
      .def(
          "__repr__",
          [](const forcecontrollerdata_ptr_t& fcdata) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::BulletObjectForceControllerData(%p)", fcdata.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto bulldfc_type = py::class_<DirectionalForceData,BulletObjectForceControllerData,directionalfcdata_ptr_t>(module_ecs, "DirectionalForceData")
      .def(py::init<>())
      .def(
          "__repr__",
          [](const directionalfcdata_ptr_t& fcdata) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::DirectionalForceData(%p)", fcdata.get());
            return fxs.c_str();
          })
          .def_property("force",
                        [](const directionalfcdata_ptr_t& fcdata) -> float { return fcdata->_force; },
                        [](directionalfcdata_ptr_t& fcdata, float val) { fcdata->_force = val; })
          .def_property("direction",
                        [](const directionalfcdata_ptr_t& fcdata) -> const fvec3& { return fcdata->_direction; },
                        [](directionalfcdata_ptr_t& fcdata, const fvec3& val) { fcdata->_direction = val; });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_system(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {
