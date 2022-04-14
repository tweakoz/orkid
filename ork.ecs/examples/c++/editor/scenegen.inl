////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once  

#include <ork/ecs/ecs.h>
#include <ork/ecs/datatable.h>
#include <ork/ecs/system.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/lua/LuaComponent.h>

#include <ork/ecs/scene.inl>
#include <ork/ecs/archetype.inl>
#include <ork/ecs/controller.inl>
#include <ork/ecs/physics/bullet.h>

namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

using namespace ::ork::lev2;
using path_t = ork::file::Path;

void saveScene(path_t path, scenedata_ptr_t scene){
  ork::reflect::serdes::JsonSerializer ser;
  auto topnode    = ser.serializeRoot(scene);
  auto resultdata = ser.output();
  ork::File outputfile(path, ork::EFM_WRITE);
  outputfile.Write(resultdata.c_str(), resultdata.length());
}

///////////////////////////////////////////////////////////////////////////////

scenedata_ptr_t generateScene(path_t path){

  ///////////////////////////////////////////
  // generate it
  ///////////////////////////////////////////

  auto scene = std::make_shared<SceneData>();
 	auto ecs_sg_sysdata  = scene->getTypedSystemData<SceneGraphSystemData>();
  auto ecs_phys_sysdata  = scene->getTypedSystemData<BulletSystemData>();
  ecs_phys_sysdata->mbDEBUG = false;
  ///////////////////////////////////////////
  // ground
  ///////////////////////////////////////////

  if(0) { // plane based ground
    auto ecs_arch        = scene->createSceneObject<Archetype>("arch_xground"_pool);
    auto ecs_physics_compdata = ecs_arch->addComponent<BulletObjectComponentData>();
    auto phys_shape = std::make_shared<BulletShapePlaneData>();
    ecs_physics_compdata->_shapedata = phys_shape;
    ecs_physics_compdata->_mass = 0.0f;
    ecs_physics_compdata->_allowSleeping = true;
    auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_xground"_pool);
    ecs_spawner->SetArchetype(ecs_arch);
  }
  else if(1) { // mesh based ground
    auto ecs_arch        = scene->createSceneObject<Archetype>("arch_xground"_pool);
    auto ecs_physics_compdata = ecs_arch->addComponent<BulletObjectComponentData>();
    auto phys_shape = std::make_shared<BulletShapeModelData>();
    ecs_physics_compdata->_shapedata = phys_shape;
    phys_shape->_meshpath = "data://tests/bridge.glb";
    ecs_physics_compdata->_mass = 0.0f;
    ecs_physics_compdata->_allowSleeping = true;
    auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_xground"_pool);
    ecs_spawner->transform()->_translation = fvec3(0,-2,0);
    ecs_spawner->SetArchetype(ecs_arch);
  }

  ///////////////////////////////////////////
  // ball
  ///////////////////////////////////////////

  {
	  auto modeldata = std::make_shared<ModelDrawableData>("data://tests/pbr1/pbr1.glb");
	  auto ecs_arch        = scene->createSceneObject<Archetype>("arch_ball"_pool);
  	auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
  	ecs_sg_compdata->createNodeOnLayer("modelnode", modeldata, "sg_default");
    auto ecs_physics_compdata = ecs_arch->addComponent<BulletObjectComponentData>();

    if(0){
      auto phys_shape = std::make_shared<BulletShapeModelData>();
      ecs_physics_compdata->_shapedata = phys_shape;
      phys_shape->_meshpath = "data://tests/pbr1/pbr1.glb";
    }
    else{
      auto phys_shape = std::make_shared<BulletShapeSphereData>();
      phys_shape->_radius = 2.2f;
      ecs_physics_compdata->_allowSleeping = false;
      ecs_physics_compdata->_shapedata = phys_shape;
    }

    auto grav_force = std::make_shared<DirectionalForceData>();
    grav_force->_direction = fvec3(0,-1,0);
    grav_force->_force = 0.98;

    ecs_physics_compdata->_forcedatas["gravity"] = grav_force;
  	auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_ball"_pool);
  	ecs_spawner->SetArchetype(ecs_arch);
    ecs_spawner->transform()->_translation = fvec3(-2.377,7.9,-3.52);
	  ecs_sg_sysdata->declarePrefetchDrawableData(modeldata);
  }

  ///////////////////////////////////////////

  if(1){
	  auto modeldata = std::make_shared<ModelDrawableData>("data://tests/bridge.glb");
	  auto ecs_arch        = scene->createSceneObject<Archetype>("arch_terrain"_pool);
  	auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
  	ecs_sg_compdata->createNodeOnLayer("modelnode", modeldata, "sg_default");
  	auto ecs_lua_compdata = ecs_arch->addComponent<LuaComponentData>();
  	ecs_lua_compdata->SetPath("demo://shinyball.lua");
  	auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_terrain"_pool);
  	ecs_spawner->SetArchetype(ecs_arch);
  	ecs_spawner->transform()->_translation = fvec3(0,-2,0);
	  ecs_sg_sysdata->declarePrefetchDrawableData(modeldata);
  }

  {
    auto pointlight = PointLightData::instantiate();
    pointlight->SetColor(fvec3(10));
    pointlight->_radius = 10;
    pointlight->_falloff = 10;
    auto ecs_arch        = scene->createSceneObject<Archetype>("arch_light"_pool);
    auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
    ecs_sg_compdata->createNodeOnLayer("lightnode", pointlight, "sg_default");
    auto ecs_lua_compdata = ecs_arch->addComponent<LuaComponentData>();
    ecs_lua_compdata->SetPath("demo://shinyball.lua");
    auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_light"_pool);
    ecs_spawner->SetArchetype(ecs_arch);
    ecs_spawner->transform()->_translation = fvec3(0,0,0);
    ecs_sg_sysdata->declarePrefetchDrawableData(pointlight);
  }

  ///////////////////////////////////////////
  // save it to disk
  ///////////////////////////////////////////

  saveScene(path,scene);
  //OrkAssert(false); // must Archetype->compose first !!!


  ///////////////////////////////////////////

  return scene;
}

///////////////////////////////////////////////////////////////////////////////

scenedata_ptr_t loadScene(path_t path){

  scenedata_ptr_t rval;

  if (FileEnv::GetRef().DoesFileExist(path)) {
    ork::File inputfile(path, ork::EFM_READ);
    size_t length = 0;
    //std::string objstr;
    inputfile.GetLength(length);
    auto buffer = (char*) malloc(length+1);

    //objstr.resize(length);
    
    inputfile.Read(buffer, length);
    buffer[length] = 0;
    inputfile.Close();
    object_ptr_t instance_out;
    auto deser = std::make_shared<reflect::serdes::JsonDeserializer>(buffer);
    deser->deserializeTop(instance_out);
    rval = std::dynamic_pointer_cast<SceneData>(instance_out);
    deser = nullptr;
  }

  return rval;

}

} //namespace ork::ecs {
