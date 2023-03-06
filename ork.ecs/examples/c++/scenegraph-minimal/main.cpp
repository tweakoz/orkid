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

#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/imgui_ged.inl>

using namespace ork;
using namespace ork::lev2;
using namespace ork::ecs;
using namespace std::literals;

int main(int argc, char** argv, char** envp) {

  bool _imgui_open = true;

  //////////////////////////////////////////////////////////
  // init application
  //////////////////////////////////////////////////////////

  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);

  lev2::initModule(init_data); // lev2 registration
  imgui::initModule(init_data);
  ecs::initModule(init_data); // ecs registration

  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;

  auto this_dir = ezapp->_orkidWorkspaceDir //
                  / "ork.ecs"               //
                  / "examples"              //
                  / "c++"                   //
                  / "scenegraph-minimal";

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
  ecs_lua_compdata->SetPath(this_dir / "shinyball.lua");

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

  SpawnAnonDynamic SAD{._edataname = "e1"_pool}; // by anon we mean "unnamed"

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
      //  in this case with shinyball.lua it will set the node's uniform scale
      //    interpolation target (where the interpolation happens in lua)

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
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////

  ezapp->onGpuInit([&](Context* ctx) {

  });

  //////////////////////////////////////////////////////////
  // onUpdateInit (always called after onGpuInit() is complete...)
  //////////////////////////////////////////////////////////

  sys_ref_t _sgsystem; // retain because we use in onUpdate handler

  ezapp->onUpdateInit([&]() {
    printf("ONUPDATEINIT!\n");

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
  });                                      // ezapp->onUpdateInit([&]() {

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

  auto sframe_outer                = std::make_shared<StandardCompositorFrame>();
  sframe_outer->_use_imgui_docking = true;
  auto sframe_ecs                  = std::make_shared<StandardCompositorFrame>();

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    auto context             = drwev->GetTarget();
    sframe_outer->_drawEvent = drwev;
    sframe_ecs->_drawEvent   = drwev;
    ///////////////////////////////////////////////////////////////////////
    context->beginFrame();
    controller->renderWithStandardCompositorFrame(sframe_ecs);
    ///////////////////////////////////////////////////////////////////////
    sframe_outer->onImguiRender = [&](const AcquiredRenderDrawBuffer& rdb) {
      ImGuiStyle& style       = ImGui::GetStyle();
      style.WindowRounding    = 5.3f;
      style.FrameRounding     = 2.3f;
      style.ScrollbarRounding = 2.3f;
      style.FrameBorderSize   = 1.0f;

      // Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
      // Most ImGui functions would normally just crash if the context is missing.
      IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

      ImGuiWindowFlags window_flags = 0;

      // Main body of the Demo window starts here.
      if (!ImGui::Begin("ECS Scene", &_imgui_open, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
      }

      // e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

      // ImGui::Text("dear imgui says hello. (%s)", IMGUI_VERSION);
      ImGui::Spacing();

      editor::EditorContext edctx(rdb);
      editor::imgui::ObjectEditor(edctx, scene);

      ImGui::PopItemWidth();
      ImGui::End();
    };
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////
    // draw the actual frame
    //////////////////////////////////////////////////////////////////
    sframe_outer->render();
    context->endFrame();

    // ezwin->withStandardCompositorFrameRender(drawEvent, sframe);
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
    printf("ONUPDATEEXIT!\n");
    controller->updateExit();
  });

  //////////////////////////////////////////////////////////
  // gpuExit handler, called once on main(rendering) thread
  //  at app exit, always called after onUpdateExit()
  //////////////////////////////////////////////////////////

  ezapp->onGpuExit([&](Context* ctx) {
    printf("ONGPUEXIT!\n");
    controller->gpuExit(ctx); // clean up controller's GPU resources
    controller = nullptr;     // release controller
  });

  //////////////////////////////////////////////////////////
  // main thread run loop
  //////////////////////////////////////////////////////////

  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
