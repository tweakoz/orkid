////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/ezapp.h>

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

using namespace ork;
using namespace ork::lev2;
using namespace ork::ecs;

struct Resources {

  controller_ptr_t _controller;
  sgsystemdata_ptr_t _sgsysdata;
  scenedata_ptr_t _scenedata;
  modeldrawabledata_ptr_t _modeldata, _modeldata2;
	ork::file::Path _this_dir;

  Resources(const ork::file::Path& this_dir) {

  	_this_dir = this_dir;

    _modeldata  = std::make_shared<ModelDrawableData>("data://tests/pbr_calib");
    _modeldata2 = std::make_shared<ModelDrawableData>("data://tests/pbr1/pbr1.glb");

    ////////////////////////////
    // create ecs scene data
    ////////////////////////////

    _scenedata = std::make_shared<SceneData>();

    auto ecs_lua_sysdata  = _scenedata->getTypedSystemData<LuaSystemData>();

    auto ecs_arch        = _scenedata->createSceneObject<Archetype>("a1"_pool);
    _sgsysdata           = _scenedata->getTypedSystemData<SceneGraphSystemData>();
    auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
    ecs_sg_compdata->createNodeOnLayer("modelnode", _modeldata, "sg_default");

    auto ecs_lua_compdata = ecs_arch->addComponent<LuaComponentData>();
    ecs_lua_compdata->SetPath("demo://shinyball.lua");

    auto ecs_spawner = _scenedata->createSceneObject<SpawnData>("e1"_pool);
    ecs_spawner->SetArchetype(ecs_arch);
    ecs_spawner->setAutoSpawn(false); // disable spawn at scene startup

    ////////////////////////////
    // declare drawable data
    //   declaring the drawable data before the simulation starts
    //   insures the assets are loaded before simulation start.
    ////////////////////////////

    _sgsysdata->declarePrefetchDrawableData(_modeldata);
    _sgsysdata->declarePrefetchDrawableData(_modeldata2);

    ////////////////////////////
    // create controller / bind scene to it
    ////////////////////////////

    _controller = std::make_shared<Controller>();
    _controller->bindScene(_scenedata);

  }

  void beginReading(){
    _controller->readTrace(_this_dir/"ecstrace.json");
    _controller->createSimulation();

  }
  void beginWriting(){
    _controller->beginWriteTrace(_this_dir/"ecstrace.json");
    _controller->createSimulation();
  }

  void onGpuInit(Context* ctx) {
  }

  void onUpdateInit() {
    _controller->startSimulation(); // start simulation
  }
  void onUpdateExit() {
    _controller->updateExit();
  }

  void onGpuExit(Context* ctx) {
    _controller->gpuExit(ctx);
  }
};

using resources_ptr_t = std::shared_ptr<Resources>;
