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
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>

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
  auto ecs_lua_sysdata  = scene->getTypedSystemData<LuaSystemData>();
  auto ecs_phys_sysdata  = scene->getTypedSystemData<BulletSystemData>();
  ecs_phys_sysdata->_debug = true;

  ///////////////////////////////////////////
  // set pbr params
  ///////////////////////////////////////////

  ecs_sg_sysdata->setInternalSceneParam("DepthFogDistance",10000.0f);
  ecs_sg_sysdata->setInternalSceneParam("DepthFogPower",2.1f);
  ecs_sg_sysdata->setInternalSceneParam("AmbientIntensity",fvec3(1,1,1));

  ///////////////////////////////////////////
  // grid
  ///////////////////////////////////////////

  if(1) { // plane based ground
    auto ecs_arch        = scene->createSceneObject<Archetype>("arch_grid"_pool);
    auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_grid"_pool);
    auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
    auto ecs_griddata = std::make_shared<GridDrawableData>();
    ecs_sg_compdata->createNodeOnLayer("gridnode", ecs_griddata, "sg_default");
    ecs_spawner->SetArchetype(ecs_arch);
    ecs_spawner->transform()->_translation = fvec3(0,-0.01,0);
  }

  ///////////////////////////////////////////
  // ground
  ///////////////////////////////////////////

  if(1) { // plane based ground
    auto ecs_arch        = scene->createSceneObject<Archetype>("arch_xground"_pool);
    auto ecs_physics_compdata = ecs_arch->addComponent<BulletObjectComponentData>();
    auto phys_shape = std::make_shared<BulletShapePlaneData>();
    ecs_physics_compdata->_shapedata = phys_shape;
    ecs_physics_compdata->_mass = 0.0f;
    ecs_physics_compdata->_allowSleeping = true;
    auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_xground"_pool);
    ecs_spawner->SetArchetype(ecs_arch);
  }
  else { // mesh based ground
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
  // sponza
  ///////////////////////////////////////////

  {
    auto modeldata = std::make_shared<ModelDrawableData>("data://tests/sponza/sponza2.glb");
    auto ecs_arch        = scene->createSceneObject<Archetype>("arch_sponza"_pool);
    auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
    ecs_sg_compdata->createNodeOnLayer("modelnode", modeldata, "sg_default");
    auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_sponza"_pool);
    ecs_spawner->SetArchetype(ecs_arch);
    ecs_spawner->transform()->_translation = fvec3(0,-0.01,0);
    ecs_spawner->transform()->_uniformScale = 14.0f;
  }

  ///////////////////////////////////////////
  // lights
  ///////////////////////////////////////////
  {
    struct LightInfo{
      std::string _id;
      archetype_ptr_t _archetype;
      pointlightdata_ptr_t _lightdata;
      sgcomponentdata_ptr_t _sgcompdata;
      //spawndata_ptr_t _spawndata;
    };

    using lightinfo_ptr_t = std::shared_ptr<LightInfo>;

    std::unordered_map<uint64_t,lightinfo_ptr_t> _lightinfos;

      auto create_light = [&](std::string entname, //
                              fvec3 pos, //
                              float intensity) {
        boost::Crc64 hasher;
        //hasher.accumulateItem<fvec3>(pos);
        hasher.accumulateItem<float>(intensity);
        hasher.finish();
        uint64_t hash = hasher.result();

        auto it_linfo = _lightinfos.find(hash);

        lightinfo_ptr_t light_info;

        if(it_linfo==_lightinfos.end()){

          auto id_str = FormatString("id-%zx", hash);

          light_info = std::make_shared<LightInfo>();

          auto arch_name = addPooledStringFromStdString("arch-light-"+id_str);

          light_info->_id = id_str;
          light_info->_lightdata      = std::make_shared<ork::lev2::PointLightData>();
          light_info->_lightdata->_radius  = 1000.0f;
          light_info->_lightdata->_falloff = 0.5f;
          light_info->_lightdata->SetColor(fvec3::White() * intensity);
          light_info->_archetype        = scene->createSceneObject<Archetype>(arch_name);
          light_info->_sgcompdata = light_info->_archetype->addComponent<SceneGraphComponentData>();
          light_info->_sgcompdata->createNodeOnLayer("lightnode", light_info->_lightdata, "sg_default");

          _lightinfos[hash] = light_info;
        }
        else{
          light_info = it_linfo->second;
        }

        auto ps_ent_name = addPooledStringFromStdString(entname+light_info->_id);
        auto spawndata = scene->createSceneObject<SpawnData>(ps_ent_name);
        spawndata->SetArchetype(light_info->_archetype);
        spawndata->transform()->_translation = pos;
      };

      create_light("ent_light1",fvec3(-50, 15, 0), 4000.0f);
      create_light("ent_light2",fvec3(50, 15, 0), 4000.0f);
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
      phys_shape->_radius = 2.5f;
      ecs_physics_compdata->_allowSleeping = true;
      ecs_physics_compdata->_shapedata = phys_shape;
    }

    auto grav_force = std::make_shared<DirectionalForceData>();
    grav_force->_direction = fvec3(0,-1,0);
    grav_force->_force = 9.8;

    ecs_physics_compdata->_forcedatas["gravity"] = grav_force;
  	auto ecs_spawner = scene->createSceneObject<SpawnData>("ent_ball"_pool);
  	ecs_spawner->SetArchetype(ecs_arch);
    ecs_spawner->transform()->_translation = fvec3(-2.377,7.9,-3.52);
    ecs_spawner->setAutoSpawn(false);
	  ecs_sg_sysdata->declarePrefetchDrawableData(modeldata);
  }

  ///////////////////////////////////////////

  if(0){
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

  //saveScene(path,scene);
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
