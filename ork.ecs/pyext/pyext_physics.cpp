////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/physics/bullet.h>
#include <ork/ecs/physics/CharacterController.h> // E.2-walk
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_physics(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
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
              "sleepThresholdLinear",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_sleepThresholdLinear; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_sleepThresholdLinear = val; })
          .def_property(
              "sleepThresholdAngular",
              [](bulletcompdata_ptr_t physc) -> float { return physc->_sleepThresholdAngular; },
              [](bulletcompdata_ptr_t& physc, float val) { physc->_sleepThresholdAngular = val; })
          .def_property(
              "allowSleeping",
              [](bulletcompdata_ptr_t physc) -> bool { return physc->_allowSleeping; },
              [](bulletcompdata_ptr_t& physc, bool val) { physc->_allowSleeping = val; })
          .def_property(
              "notifyCollisions", // E.2-walk: contacts -> PythonSystem "Collision" notifies
              [](bulletcompdata_ptr_t physc) -> bool { return physc->_notifyCollisions; },
              [](bulletcompdata_ptr_t& physc, bool val) { physc->_notifyCollisions = val; })
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
                  auto as_table = result.getShared<DataTable>();
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
              [](bulletcompdata_ptr_t& physc, fvec3 val) { physc->_angularFactor = val; })
          .def_property(
              "instance_node_name",
              [](bulletcompdata_ptr_t physc) -> std::string { return physc->_instanceNodeName; },
              [](bulletcompdata_ptr_t& physc, std::string val) { physc->_instanceNodeName = val; })
      .def(
          "declareNodeInstance",
          [](bulletcompdata_ptr_t physc, ::ork::lev2::scenegraph::node_instance_data_ptr_t nid) { //
            physc->_INSTANCEDATA = nid;
          });

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
  // E.2-walk: per-item proxy compound for a placed ScatterSet (kind+dims ride the items).
  auto shapescatter_type =
      py::class_<BulletShapeScatterData, BulletShapeBaseData, bulletshapescatterdata_ptr_t>(module_ecs, "BulletShapeScatterData")
          .def(py::init<>())
          .def_property(
              "scatter_asset",
              [](const bulletshapescatterdata_ptr_t& shape) -> std::string { return shape->_scatter_asset; },
              [](bulletshapescatterdata_ptr_t& shape, std::string val) { shape->_scatter_asset = val; })
          .def_property(
              "sink",
              [](const bulletshapescatterdata_ptr_t& shape) -> std::string { return shape->_sink; },
              [](bulletshapescatterdata_ptr_t& shape, std::string val) { shape->_sink = val; })
          .def_property(
              "ogeo_path",
              [](const bulletshapescatterdata_ptr_t& shape) -> std::string { return shape->_ogeo_path; },
              [](bulletshapescatterdata_ptr_t& shape, std::string val) { shape->_ogeo_path = val; });
  type_codec->registerStdCodec<bulletshapescatterdata_ptr_t>(shapescatter_type);
  /////////////////////////////////////////////////////////////////////////////////
  // roads: the WALKABLE RIBBON proxy swept from the street_spine baked artifact
  // (physics-proxy law — derives from generating data, never the render mesh).
  auto shapespine_type =
      py::class_<BulletShapeSpineData, BulletShapeBaseData, bulletshapespinedata_ptr_t>(module_ecs, "BulletShapeSpineData")
          .def(py::init<>())
          .def_property(
              "spine_asset",
              [](const bulletshapespinedata_ptr_t& shape) -> std::string { return shape->_spine_asset; },
              [](bulletshapespinedata_ptr_t& shape, std::string val) { shape->_spine_asset = val; })
          .def_property(
              "ogeo_path",
              [](const bulletshapespinedata_ptr_t& shape) -> std::string { return shape->_ogeo_path; },
              [](bulletshapespinedata_ptr_t& shape, std::string val) { shape->_ogeo_path = val; })
          .def_property(
              "shoulder_m",
              [](const bulletshapespinedata_ptr_t& shape) -> float { return shape->_shoulder_m; },
              [](bulletshapespinedata_ptr_t& shape, float val) { shape->_shoulder_m = val; })
          .def_property(
              "lift_m",
              [](const bulletshapespinedata_ptr_t& shape) -> float { return shape->_lift_m; },
              [](bulletshapespinedata_ptr_t& shape, float val) { shape->_lift_m = val; })
          .def_property(
              "ground_asset",
              [](const bulletshapespinedata_ptr_t& shape) -> std::string { return shape->_ground_asset; },
              [](bulletshapespinedata_ptr_t& shape, std::string val) { shape->_ground_asset = val; });
  type_codec->registerStdCodec<bulletshapespinedata_ptr_t>(shapespine_type);
  /////////////////////////////////////////////////////////////////////////////////
  // E.2-walk: the asset-wired heightfield collider — hf_asset resolves the baked
  // HeightField artifact + manifest scale, so physics collides with what renders.
  using bulletshapeterraindata_ptr_t = std::shared_ptr<BulletShapeTerrainData>;
  auto shapeterrain_type =
      py::class_<BulletShapeTerrainData, BulletShapeBaseData, bulletshapeterraindata_ptr_t>(module_ecs, "BulletShapeTerrainData")
          .def(py::init<>())
          .def_property(
              "hf_asset",
              [](const bulletshapeterraindata_ptr_t& shape) -> std::string { return shape->_hf_asset; },
              [](bulletshapeterraindata_ptr_t& shape, std::string val) { shape->_hf_asset = val; })
          .def_property(
              "heightmap_path",
              [](const bulletshapeterraindata_ptr_t& shape) -> std::string { return shape->_heightMapPath.c_str(); },
              [](bulletshapeterraindata_ptr_t& shape, std::string val) { shape->_heightMapPath = file::Path(val.c_str()); })
          .def_property(
              "world_size",
              [](const bulletshapeterraindata_ptr_t& shape) -> float { return shape->_worldSize; },
              [](bulletshapeterraindata_ptr_t& shape, float val) { shape->_worldSize = val; })
          .def_property(
              "world_height",
              [](const bulletshapeterraindata_ptr_t& shape) -> float { return shape->_worldHeight; },
              [](bulletshapeterraindata_ptr_t& shape, float val) { shape->_worldHeight = val; })
          .def_property(
              "render_dimension",
              [](const bulletshapeterraindata_ptr_t& shape) -> int { return shape->_render_dimension; },
              [](bulletshapeterraindata_ptr_t& shape, int val) { shape->_render_dimension = val; })
          // W·M SURFACE RESPONSE — physics leg. surface_weights names the RGBA class-weight
          // capture the material also consumes ("" = feature off); friction_rows = M[:,friction]
          // (per-class friction deltas dotted with the 4 class weights at each contact point).
          .def_property(
              "surface_weights",
              [](const bulletshapeterraindata_ptr_t& shape) -> std::string { return shape->_surface_weights_channel; },
              [](bulletshapeterraindata_ptr_t& shape, std::string val) { shape->_surface_weights_channel = val; })
          .def_property(
              "friction_rows",
              [](const bulletshapeterraindata_ptr_t& shape) -> py::list {
                py::list result;
                for (auto f : shape->_friction_rows) result.append(f);
                return result;
              },
              [](bulletshapeterraindata_ptr_t& shape, py::list lst) {
                shape->_friction_rows.clear();
                for (auto& item : lst) shape->_friction_rows.push_back(item.cast<float>());
              });
  type_codec->registerStdCodec<bulletshapeterraindata_ptr_t>(shapeterrain_type);
  /////////////////////////////////////////////////////////////////////////////////
  // E.2-walk: the reusable walk-on-terrain behavior (CharacterController). Properties
  // snake_case per the pyext convention; pairs with a sibling capsule BulletObjectComponent.
  auto charctl_type =
      py::class_<CharacterControllerComponentData, ComponentData, charactercontrollercomponentdata_ptr_t>(
          module_ecs, "CharacterControllerComponentData")
          .def(py::init<>())
          .def_property(
              "move_force",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_moveForce; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_moveForce = v; })
          .def_property(
              "max_speed",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_maxSpeed; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_maxSpeed = v; })
          .def_property(
              "jump_impulse",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_jumpImpulse; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_jumpImpulse = v; })
          .def_property(
              "turn_rate",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_turnRate; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_turnRate = v; })
          .def_property(
              "brake",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_brake; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_brake = v; })
          .def_property(
              "turn_decay",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_turnDecay; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_turnDecay = v; })
          .def_property(
              "drive_friction",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_driveFriction; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_driveFriction = v; })
          .def_property(
              "rest_friction",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_restFriction; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_restFriction = v; })
          .def_property(
              "eye_height",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_eyeHeight; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_eyeHeight = v; })
          .def_property(
              "cam_distance",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_camDistance; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_camDistance = v; })
          .def_property(
              "fovy_deg",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_fovyDeg; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_fovyDeg = v; })
          .def_property(
              "cam_near",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_camNear; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_camNear = v; })
          .def_property(
              "cam_far",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_camFar; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_camFar = v; })
          .def_property(
              "kill_z_drop",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_killZDrop; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_killZDrop = v; })
          .def_property(
              "spawn_above_ground",
              [](const charactercontrollercomponentdata_ptr_t& c) -> float { return c->_spawnAboveGround; },
              [](charactercontrollercomponentdata_ptr_t& c, float v) { c->_spawnAboveGround = v; })
          .def_property(
              "force_name",
              [](const charactercontrollercomponentdata_ptr_t& c) -> std::string { return c->_forceName; },
              [](charactercontrollercomponentdata_ptr_t& c, std::string v) { c->_forceName = v; });
  type_codec->registerStdCodec<charactercontrollercomponentdata_ptr_t>(charctl_type);
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
              "submesh",
              [](const bulletshapemeshdata_ptr_t& shape) -> meshutil::submesh_ptr_t { return shape->_submesh; },
              [](bulletshapemeshdata_ptr_t& shape, meshutil::submesh_ptr_t val) { //
                shape->_submesh = val;
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
                              [](bulletsysdata_ptr_t& sysdata, bool val) { sysdata->_debug = val; })
                          .def_property(
                              "test_deactivation",
                              [](const bulletsysdata_ptr_t& sysdata) -> bool { return sysdata->_test_deactivation; },
                              [](bulletsysdata_ptr_t& sysdata, bool val) { sysdata->_test_deactivation = val; });
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
