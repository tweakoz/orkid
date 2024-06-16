////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/physics/bullet.h>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_physics(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto bullc_type =
      py::class_<BulletObjectComponentData, ComponentData, bulletcompdata_ptr_t>(module_ecs, "BulletObjectComponentData")
          .def(
              "__repr__",
              [](bulletcompdata_ptr_t physc) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BulletObjectComponentData(%p)", physc.get());
                return fxs.c_str();
              })
          .def(
              "declareForce",
              [](bulletcompdata_ptr_t physc,            //
                 std::string name,                      //
                 forcecontrollerdata_ptr_t forcedata) { //
                physc->_forcedatas[name] = forcedata;
              })
          .def_property(
              "mass",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_mass; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_mass = val; })
          .def_property(
              "friction",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_friction; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_friction = val; })
          .def_property(
              "restitution",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_restitution; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_restitution = val; })
          .def_property(
              "angularDamping",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_angularDamping; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_angularDamping = val; })
          .def_property(
              "linearDamping",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_linearDamping; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_linearDamping = val; })
          .def_property(
              "allowSleeping",
              [](bulletcompdata_ptr_t physc) -> bool { return physc->_allowSleeping; },
              [](bulletcompdata_ptr_t& physc, bool val) { physc->_allowSleeping = val; })
          .def_property(
              "isKinematic",
              [](bulletcompdata_ptr_t physc) -> bool { return physc->_isKinematic; },
              [](bulletcompdata_ptr_t& physc, bool val) { physc->_isKinematic = val; })
          .def_property(
              "disablePhysics",
              [](bulletcompdata_ptr_t physc) -> bool { return physc->_disablePhysics; },
              [](bulletcompdata_ptr_t& physc, bool val) { physc->_disablePhysics = val; })
          .def(
              "onCollision",
              [type_codec](bulletcompdata_ptr_t physc, py::function pyfn) { //
                physc->_collisionCallback = [=](const evdata_t& result) {
                  auto& as_table = result.get<DataTable>();
                  auto encoded   = type_codec->encode64(as_table);
                  py::gil_scoped_acquire acquire;
                  pyfn(encoded);
                };
              })
          .def_property(
              "groupAssign",
              [](bulletcompdata_ptr_t physc) -> uint32_t { return physc->_groupAssign; },
              [](bulletcompdata_ptr_t& physc, uint32_t val) { physc->_groupAssign = val; })
          .def_property(
              "groupCollidesWith",
              [](bulletcompdata_ptr_t physc) -> uint32_t { return physc->_groupCollidesWith; },
              [](bulletcompdata_ptr_t& physc, uint32_t val) { physc->_groupCollidesWith = val; })
          .def_property(
              "shape",
              [](bulletcompdata_ptr_t physc) -> shapedata_ptr_t { return physc->_shapedata; },
              [](bulletcompdata_ptr_t& physc, shapedata_ptr_t val) { physc->_shapedata = val; })
          .def_property(
              "angularFactor",
              [](bulletcompdata_ptr_t physc) -> fvec3 { return physc->_angularFactor; },
              [](bulletcompdata_ptr_t& physc, fvec3 val) { physc->_angularFactor = val; });

  type_codec->registerStdCodec<bulletcompdata_ptr_t>(bullc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapebase_type = py::class_<BulletShapeBaseData, ork::Object, shapedata_ptr_t>(module_ecs, "BulletShapeBaseData")
                            .def("__repr__", [](const shapedata_ptr_t& shape) -> std::string {
                              fxstring<256> fxs;
                              fxs.format("ecs::BulletShapeBaseData(%p)", shape.get());
                              return fxs.c_str();
                            });
  type_codec->registerStdCodec<shapedata_ptr_t>(shapebase_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapesphere_type =
      py::class_<BulletShapeSphereData, BulletShapeBaseData, bulletshapespheredata_ptr_t>(module_ecs, "BulletShapeSphereData")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const bulletshapespheredata_ptr_t& shape) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BulletShapeSphereData(%p)", shape.get());
                return fxs.c_str();
              })
          .def_property(
              "radius",
              [](const bulletshapespheredata_ptr_t& shape) -> float { return shape->_radius; },
              [](bulletshapespheredata_ptr_t& shape, float val) { shape->_radius = val; });
  type_codec->registerStdCodec<bulletshapespheredata_ptr_t>(shapesphere_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapecapsule_type =
      py::class_<BulletShapeCapsuleData, BulletShapeBaseData, bulletshapecapsuledata_ptr_t>(module_ecs, "BulletShapeCapsuleData")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const bulletshapecapsuledata_ptr_t& shape) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BulletShapeCapsuleData(%p)", shape.get());
                return fxs.c_str();
              })
          .def_property(
              "radius",
              [](const bulletshapecapsuledata_ptr_t& shape) -> float { return shape->mfRadius; },
              [](bulletshapecapsuledata_ptr_t& shape, float val) { shape->mfRadius = val; })
          .def_property(
              "extent",
              [](const bulletshapecapsuledata_ptr_t& shape) -> float { return shape->mfExtent; },
              [](bulletshapecapsuledata_ptr_t& shape, float val) { shape->mfExtent = val; });
  type_codec->registerStdCodec<bulletshapecapsuledata_ptr_t>(shapecapsule_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapeplane_type =
      py::class_<BulletShapePlaneData, BulletShapeBaseData, bulletshapeplanedata_ptr_t>(module_ecs, "BulletShapePlaneData")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const bulletshapeplanedata_ptr_t& shape) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BulletShapePlaneData(%p)", shape.get());
                return fxs.c_str();
              })
          .def_property(
              "normal",
              [](const bulletshapeplanedata_ptr_t& shape) -> fvec3 { return shape->_nrm; },
              [](bulletshapeplanedata_ptr_t& shape, fvec3 val) { shape->_nrm = val; })
          .def_property(
              "position",
              [](const bulletshapeplanedata_ptr_t& shape) -> fvec3 { return shape->_pos; },
              [](bulletshapeplanedata_ptr_t& shape, fvec3 val) { shape->_pos = val; });
  type_codec->registerStdCodec<bulletshapeplanedata_ptr_t>(shapeplane_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto shapemesh_type =
      py::class_<BulletShapeMeshData, BulletShapeBaseData, bulletshapemeshdata_ptr_t>(module_ecs, "BulletShapeMeshData")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const bulletshapemeshdata_ptr_t& shape) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::BulletShapeMeshData(%p)", shape.get());
                return fxs.c_str();
              })
          .def_property(
              "meshpath",
              [](const bulletshapemeshdata_ptr_t& shape) -> std::string { return shape->_meshpath.c_str(); },
              [](bulletshapemeshdata_ptr_t& shape, std::string val) { //
                shape->_meshpath = val;
              })
          .def_property(
              "scale",
              [](const bulletshapemeshdata_ptr_t& shape) -> fvec3 { return shape->_scale; },
              [](bulletshapemeshdata_ptr_t& shape, fvec3 val) { shape->_scale = val; })
          .def_property(
              "translation",
              [](const bulletshapemeshdata_ptr_t& shape) -> fvec3 { return shape->_translation; },
              [](bulletshapemeshdata_ptr_t& shape, fvec3 val) { shape->_translation = val; });
  type_codec->registerStdCodec<bulletshapemeshdata_ptr_t>(shapemesh_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bullsys_type = py::class_<BulletSystemData, SystemData, bulletsysdata_ptr_t>(module_ecs, "BulletSystemData")
                          .def(
                              "__repr__",
                              [](const bulletsysdata_ptr_t& sysdata) -> std::string {
                                fxstring<256> fxs;
                                fxs.format("ecs::BulletSystemData(%p)", sysdata.get());
                                return fxs.c_str();
                              })
                          .def_property(
                              "linGravity",
                              [](const bulletsysdata_ptr_t& sysdata) -> const fvec3& { return sysdata->_lingravity; },
                              [](bulletsysdata_ptr_t& sysdata, const fvec3& val) { sysdata->_lingravity = val; })
                          .def_property(
                              "expGravity",
                              [](const bulletsysdata_ptr_t& sysdata) -> const fvec3& { return sysdata->_expgravity; },
                              [](bulletsysdata_ptr_t& sysdata, const fvec3& val) { sysdata->_expgravity = val; })
                          .def_property(
                              "timeScale",
                              [](const bulletsysdata_ptr_t& sysdata) -> float { return sysdata->mfTimeScale; },
                              [](bulletsysdata_ptr_t& sysdata, float val) { sysdata->mfTimeScale = val; })
                          .def_property(
                              "simulationRate",
                              [](const bulletsysdata_ptr_t& sysdata) -> float { return sysdata->mSimulationRate; },
                              [](bulletsysdata_ptr_t& sysdata, float val) { sysdata->mSimulationRate = val; })
                          .def_property(
                              "debug",
                              [](const bulletsysdata_ptr_t& sysdata) -> bool { return sysdata->_debug; },
                              [](bulletsysdata_ptr_t& sysdata, bool val) { sysdata->_debug = val; });
  type_codec->registerStdCodec<bulletsysdata_ptr_t>(bullsys_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bullfc_type = py::class_<BulletObjectForceControllerData, ork::Object, forcecontrollerdata_ptr_t>(
                         module_ecs, "BulletObjectForceControllerData")
                         .def("__repr__", [](const forcecontrollerdata_ptr_t& fcdata) -> std::string {
                           fxstring<256> fxs;
                           fxs.format("ecs::BulletObjectForceControllerData(%p)", fcdata.get());
                           return fxs.c_str();
                         });
  /////////////////////////////////////////////////////////////////////////////////
  auto bulldfc_type =
      py::class_<DirectionalForceData, BulletObjectForceControllerData, directionalfcdata_ptr_t>(module_ecs, "DirectionalForceData")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const directionalfcdata_ptr_t& fcdata) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::DirectionalForceData(%p)", fcdata.get());
                return fxs.c_str();
              })
          .def_property(
              "magnitude",
              [](const directionalfcdata_ptr_t& fcdata) -> float { return fcdata->_force; },
              [](directionalfcdata_ptr_t& fcdata, float val) { fcdata->_force = val; })
          .def_property(
              "direction",
              [](const directionalfcdata_ptr_t& fcdata) -> const fvec3& { return fcdata->_direction; },
              [](directionalfcdata_ptr_t& fcdata, const fvec3& val) { fcdata->_direction = val; });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_system(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
