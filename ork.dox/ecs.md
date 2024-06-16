# Entity Component System 

---

### Features:

 1. Flexible ECS architecture, allows developer to pivot and adapt ECS to a wide array of stimulus (input) drivers.

 2. Supports multiple simultaneous ECS instances.

 3. Clear separation of external (stimulus) and internal processes, even from separate threads.

 4. Supports stimulus event trace record and playback (useful for repeatable deterministic debugging and diagnostics, even if the stimulus source is non-deterministic (for example, a live game server). The developer could also play back a pre-recorded stimuli stream with a modified scene - so long as the addressed objects are still present. This feature came into existence due to my experience at working on OpenGL command stream capture and playback (for the purposes of graphics debugging, profiling, and other tooling), and an extreme frustration with fixing simulation related bugs in MMORPG's for which stimulus came from game servers and local UX - which would make reproduction extremely difficult, and a manually laborous process.
 
 5. SceneGraph Component/System wraps lev2 rendering. ECS simulation occurs on update thread, and data is passed to rendering thread via this SceneGraph system.
 
 6. Lua Component/System allows for lua driven behaviors
 
 7. Bullet Physics Component/System allows for physics driven behaviors

 8. Included ImGui based Editor.
 
 9. Scenes can be serialized/deserialized to/from standard orkid reflection based serialization JSON data.

---

### ECS Architecture


The ECS is split into a few dimensions:

A. The Data aspect. Document objects that one would edit, serialize/deserialize, or generate procedurally:

* From the perspective of the simulation, all Data aspect objects are immutable.

* The Scene - the root ECS "document". It is a representation of the starting conditions of a simulation, containing a set of *Archetypes* (aka Prefabs), a set of *SpawnData* and a set of *SystemData*.

* Archetype - a named ECS data object containing a set of *ComponentData*

* ComponentData - initialization data for a specific *component* of an *entity*. ComponentData's themselves have a subordinate set of component specific data objects related to the domain of the component - eg physics data, audio data, visualization data, etc..

* SpawnData - a named and placed spawner that can spawn an entity of a specific archetype. Can either statically spawn (autospawn) on simulation startup or dynamically later as the simulation progresses. Can also provide overrides for ComponentData.

* SystemData - initialization data for specific systems 

B. The Simulation aspect. Mutable objects that evolve over time as part of a simulation.

* The Controller - The "frontend" of the simulation. The developer tends to interact with this, and interact with it in the *controller thread* , this can be the main thread , a "network" thread, or pretty much any thread except the *update thread*, though it is recommended to only message the controller from 1 thread - though technically it should be OK submitting controller commands from multiple threads so long as the serialized stream makes sense. The controller can start, stop, restart, pause, and send stimuli to the simulation. The controller can also "trace" all simulation bound stimuli to JSON, and replay traced JSON allowing the developer to debug simulations deterministically even when stimuli originated from non deterministic sources. All stimuli from controller to simulation is synchronized across threads through serialization into a command queue.

* The Simulation - The root level simulation object. Contains a set of *Entities* and *Systems*. Entities contain *Components*, and Systems reference entity components and are responsible for the "updating" of state, updating always occurs in the *update* thread.

* Entity - An addressable  molecule of state with an assigned set of components in the simulation. 

* Component - An addressable atom of state with an associated state mutator (system). Components can also have *entity-scoped* subordinate objects related to the domain of the component/system - which will not be listed here.

* System - An addressable state mutator responsible for the simulation of a specific aspect of the whole of the simulation. eg. physics, scripting, scenegraph, etc.. Systems can also have subordinate objects related to the domain of the system and *not* associated with a specific entity - these are *system-scoped* as opposed to *entity-scoped*. 

* Reference Objects - the controller interacts with entities, components, sub-components, systems and sub-systems via opaque handles called *ref objects*, eg ent_ref_t, component_ref_t, system_ref_t, etc... - this is done instead of using direct pointers so that all interactions are serialized through the command queue - this is important for the tracing functionality and also helps maintain thread safety.

* Scripting Systems - Lua and the upcoming Python Scripting component/systems are able to interact with the simulation using direct pointers since they run only on the update thread and are therefore synchronized with the simulation. To help enforce determinism, all IO, threads and other sources of non-determinism are disabled or hidden in the internal scripting contexts. pseudo-random numbers should be retrieved from the simulation itself (since given the same controller stimulus, the results should be deterministic). 

![ECS Architecture:1](EcsArchitectureDiagram.png)

---

### ECS Lifecycle 

The simulation and subobjects are subject to a strict lifecycle (states), these include:

* Initialized - the object has been initialized.
* Composed - the object (and all siblings/children) have been initialized.
* Linked - the object and all siblings/children have had the opportunity to be made aware of one another and connect.
* Staged - the object is ready for presentation (eg - visible)
* Activated - the object is dynamically mutating as part of the running simulation.

There are obviously state transitions required to go from one state to the other, visible below.

![ECS Lifecycle:2](ECSLifecycle.png)

---

### Example code 

```cpp
void main(int argc, char** argv, char** envp) {

  //////////////////////////////////////////////////////////
  // init application
  //////////////////////////////////////////////////////////

  // auto OK - type is in the template argument! - use the DRY principle
  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp); 

  lev2::initModule(init_data); // lev2 registration
  ecs::initModule(init_data); // ecs registration

  auto ezapp  = OrkEzApp::create(init_data);

  //////////////////////////////////////////////////////////

  auto modeldata = std::make_shared<ModelDrawableData>("data://tests/pbr_calib");

  ////////////////////////////
  // create ecs scene data
  ////////////////////////////

  auto scene = std::make_shared<SceneData>();

  auto ecs_sg_sysdata  = scene->getTypedSystemData<SceneGraphSystemData>();
  auto ecs_lua_sysdata = scene->getTypedSystemData<LuaSystemData>();

  auto ecs_arch        = scene->createSceneObject<Archetype>("a1"_pool);
  auto ecs_sg_compdata = ecs_arch->addComponent<SceneGraphComponentData>();
  ecs_sg_compdata->createNodeOnLayer("modelnode", modeldata, "sg_default");

  auto ecs_lua_compdata = ecs_arch->addComponent<LuaComponentData>();
  ecs_lua_compdata->SetPath("test.lua");

  auto ecs_spawner = scene->createSceneObject<SpawnData>("e1"_pool);
  ecs_spawner->SetArchetype(ecs_arch);
  ecs_spawner->setAutoSpawn(false); // disable spawn at scene startup

  ////////////////////////////
  // declare drawable data
  //   declaring the drawable data before the simulation starts
  //   insures the assets are loaded before simulation start.
  ////////////////////////////

  auto modeldata2 = std::make_shared<ModelDrawableData>("data://tests/pbr1/pbr1.glb");
  ecs_sg_sysdata->declarePrefetchDrawableData(modeldata2);

  ////////////////////////////
  // create controller / bind scene to it
  ////////////////////////////

  auto controller = std::make_shared<Controller>();
  controller->bindScene(scene);

  ////////////////////////////
  // enqueue spawning of a bunch of "a1" entities
  ////////////////////////////

  SpawnAnonDynamic SAD{._edataname = "e1"_pool}; 

  float at_timestamp = 0.0f;

  for (int i = 0; i < 250; i++) {
    at_timestamp += controller->random(0.5, 1.5);

    controller->realtimeDelayedOperation(at_timestamp, [=]() {
      auto ent     = controller->spawnAnonDynamicEntity(SAD);
      auto luacomp = controller->findEntityComponent<LuaComponentData>(ent);
      auto sgcomp  = controller->findEntityComponent<SceneGraphComponentData>(ent);
      float size   = controller->random(1, 5);

      // invoke the SETSCALE event handler on this lua component's designated script
      //  what this does, depends on the implementation of that script

      controller->realtimeDelayedOperation(2, [=]() { //
        controller->componentNotify(luacomp, "SETSCALE"_tok, size);
       });

      controller->realtimeDelayedOperation(3, [=]() { //
        controller->componentNotify(luacomp, "SETSCALE"_tok, 0.05); 
      });

      /////////////////////////////////
      // create a component owned dynamic scene graph node
      //  you can modify or delete it by sending
      //  other requests or events to the sg component directly
      /////////////////////////////////

      controller->realtimeDelayedOperation(4.0, [=]() { //

        DataTable node_data;
        node_data["modeldata"_tok]    = modeldata2;
        node_data["uniformScale"_tok] = 0.2f;
        node_data["nodeName"]         = "MainNode"s;

        auto handle_to_node = // returns opaque handle to sg node
            controller->componentRequest(
                sgcomp,                       // sg component of entity
                SceneGraphSystem::CreateNode, // component request ID
                node_data);                   // data with which to initialize sg node

        controller->realtimeDelayedOperation(0.5, [=]() {
          controller->componentNotify(
              sgcomp,                        // sg component of entity
              SceneGraphSystem::DestroyNode, // component event ID
              handle_to_node);               // node to destroy
        });
      });
    });
  }

  //////////////////////////////////////////////////////////
  // create our simulation
  //////////////////////////////////////////////////////////

  controller->createSimulation();

  //////////////////////////////////////////////////////////
  // onUpdateInit (always called after onGpuInit() is complete...)
  //////////////////////////////////////////////////////////

  sys_ref_t _sgsystem; // retain because we use in onUpdate handler

  ezapp->onUpdateInit([&]() {

    controller->startSimulation(); // start simulation

    _sgsystem = controller->findSystem<ecs::SceneGraphSystem>(); // opaque handle to sg system

    /////////////////////////////////
    // create a system owned dynamic scene graph node
    //  you can modify or delete it by sending
    //  other requests or events to the sg system directly
    /////////////////////////////////

    controller->realtimeDelayedOperation(1, [=]() {
      DataTable node_data;
      node_data["modeldata"_tok] = modeldata2;
      node_data["nodeName"]      = "MainNode"s;

      response_ref_t global_node = controller->systemRequest( // returns an opaque handle to a node
          _sgsystem,                                          // scene graph system
          SceneGraphSystem::CreateNode,                       // request ID
          node_data);                                         // data with which to initialize node

      controller->realtimeDelayedOperation(4, [=]() {
        controller->systemNotify(          //
            _sgsystem,                     // scene graph system
            SceneGraphSystem::DestroyNode, // event ID
            global_node);                  // opaque node handle to destroy
      });                                  // controller->realtimeDelayedOperation(4,[=](){
    });                                    // controller->realtimeDelayedOperation(1,[=](){
  });                                      // qtapp->onUpdateInit([&]() {

  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //  it will never be called after onUpdateExit() is invoked...
  //////////////////////////////////////////////////////////

  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ////////////////////////////
    // compute camera data
    //  theoretically this could be done by a camera animation component on an entity as well
    //   were one to exist...
    ////////////////////////////
    float phase    = abstime * PI2 * 0.1f;
    float distance = 20.0f;
    DataTable camera_data;
    camera_data["eye"_tok]  = fvec3::unitCircleXZ(phase) * distance;
    camera_data["tgt"_tok]  = fvec3(0, 0, 0);
    camera_data["up"_tok]   = fvec3(0, 1, 0);
    camera_data["near"_tok] = 0.1f;
    camera_data["far"_tok]  = 100.0f;
    camera_data["fovy"_tok] = 45.0f;
    controller->systemNotify(_sgsystem, SceneGraphSystem::UpdateCamera._token, camera_data);
    ////////////////////////////
    controller->update();
    ////////////////////////////
  });

  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  auto sframe_ecs = std::make_shared<StandardCompositorFrame>();

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    auto context             = drwev->GetTarget();
    sframe_ecs->_drawEvent   = drwev;
    ///////////////////////////////////////////////////////////////////////
    context->beginFrame();
    controller->renderWithStandardCompositorFrame(sframe_ecs);
    context->endFrame();
  });

  //////////////////////////////////////////////////////////
  // when resizing the app - we need to resize the entire rendering pipe
  //////////////////////////////////////////////////////////

  ezapp->onResize([&](int w, int h) {
    DataTable fbsize_data;
    fbsize_data["width"_tok]  = w;
    fbsize_data["height"_tok] = h;
    controller->systemNotify(_sgsystem, SceneGraphSystem::UpdateFramebufferSize, fbsize_data);
  });

  //////////////////////////////////////////////////////////
  // updateExit handler, called once on update thread
  //  at app exit, always called before onGpuExit()
  //////////////////////////////////////////////////////////

  ezapp->onUpdateExit([&]() {
    controller->updateExit();
  });

  //////////////////////////////////////////////////////////
  // gpuExit handler, called once on main(rendering) thread
  //  at app exit, always called after onUpdateExit()
  //////////////////////////////////////////////////////////

  ezapp->onGpuExit([&](Context* ctx) {
    controller->gpuExit(ctx); // clean up controller's GPU resources
    controller = nullptr;     // release controller
  });

  //////////////////////////////////////////////////////////
  // main thread run loop
  //////////////////////////////////////////////////////////

  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
```

