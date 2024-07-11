////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/ezapp.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/environment.h>

#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/imgui_ged.inl>
#include <ork/lev2/imgui/imgui_internal.h>
#include <ork/util/logger.h>

#include <ork/lev2/gfx/util/movie.inl>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-void-pointer-cast"

///////////////////////////////////////////////////////////////////////////////

#include <ork/ecs/ecseditor.inl>
#include <ork/ecs/manip.inl>
#include <ork/ecs/outliner.inl>
#include "scenegen.inl"

using namespace ork;
using namespace ork::lev2;
using namespace ork::ecs;

static logchannel_ptr_t logchan_editor = logger()->createChannel("EDITOR",fvec3(1,1,1));

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);

  ork::Timer timer;
  timer.Start();

  //////////////////////////////////////////////////////////
  // init application
  //////////////////////////////////////////////////////////

  lev2::initModule(init_data); // lev2 registration
  ecs::initModule(init_data); // ecs registration

  std::shared_ptr<MovieContext> movie = nullptr;
  
  //init_data->_ssaa_samples = 16;
  //init_data->_msaa_samples = 16;

  if(0){
    init_data->_top = 0;
    init_data->_left = 1440;
    init_data->_width = 1920;
    init_data->_height = 1080;
    movie = std::make_shared<MovieContext>();
    movie->init(init_data->_width,init_data->_height);
  }

  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;

  auto this_dir = ezapp->_orkidWorkspaceDir //
                  / "ork.ecs"               //
                  / "examples"              //
                  / "c++"                   //
                  / "editor";

  auto filecontext = FileEnv::createContextForUriBase("demo://", this_dir);
  filecontext->SetFilesystemBaseEnable(true);

  fmtx4 xfmtx;

  logchan_editor->log( "T2<%g>", timer.SecsSinceStart() );

  //////////////////////////////////////////////////////////

  lev2::rtgroup_ptr_t ecs_outgroup;
  lev2::rtbuffer_ptr_t ecs_outbuffer;

  ////////////////////////////
  // create ecs scene data
  ////////////////////////////

  auto scene = generateScene("demo://sceneout.ork");

  auto ecs_sg_sysdata  = scene->getTypedSystemData<SceneGraphSystemData>();
  auto ecs_ball_spawner = scene->findTypedObject<SpawnData>("ent_ball"_pool);

  OrkAssert(ecs_ball_spawner!=nullptr);
  
  //ecs_sg_sysdata->setInternalSceneParam("supersample",int(16));
  ecs_sg_sysdata->setInternalSceneParam("preset",std::string("ForwardPBR"));

  ////////////////////////////
  // create controller / bind scene to it
  ////////////////////////////

  auto controller = std::make_shared<Controller>();
  controller->bindScene(scene);

  ////////////////////////////
  // enqueue spawning of a bunch of "a1" entities
  ////////////////////////////

  float at_timestamp = 0.0f;

  logchan_editor->log( "T3<%g>", timer.SecsSinceStart() );

  //////////////////////////////////////////////////////////
  // create our simulation
  //////////////////////////////////////////////////////////

  //auto compositordata = std::make_shared<CompositingData>();
  //auto compositorimpl = compositordata->createImpl();
  //auto renderer = std::make_shared<IRenderer>();
  //auto CPD = std::make_shared<CompositingPassData>();
  //CPD->addStandardLayers();
  //compositordata->mbEnable = true;
  auto ecs_camera = std::make_shared<CameraData>();
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////

  ezapp->onGpuInit([&](Context* ctx) {
   logchan_editor->log( "T4<%g>", timer.SecsSinceStart() );
    //ecs_outgroup  = std::make_shared<RtGroup>(ctx, 100, 100, MsaaSamples::MSAA_1X);
    //ecs_outbuffer = ecs_outgroup->createRenderTarget(EBufferFormat::RGBA32F);
    //ecs_sg_sysdata->bindToRtGroup(ecs_outgroup);
    ecs_sg_sysdata->bindToCamera(ecs_camera);

    //compositordata->presetDeferredPBR();
    //renderer->setContext(ctx);


  });

  //////////////////////////////////////////////////////////
  // onUpdateInit (always called after onGpuInit() is complete...)
  //////////////////////////////////////////////////////////

  sys_ref_t _sgsystem; // retain because we use in onUpdate handler

  controller->createSimulation();

  auto START_SCENE = [&]() {
    auto mtop = [&]() {
      auto utop = [&]() {
        controller->startSimulation();                               // start simulation
        _sgsystem = controller->findSystem<ecs::SceneGraphSystem>(); // opaque handle to sg system
      };
      opq::updateSerialQueue()->enqueue(utop);
    };
    opq::mainSerialQueue()->enqueue(mtop);
  };
  auto STOP_SCENE = [&]() {
    controller->stopSimulation(); // stop simulation
  };

  ezapp->onUpdateInit([&]() { 
    logchan_editor->log( "T5<%g>", timer.SecsSinceStart() );
    START_SCENE();    
  }); // ezapp->onUpdateInit([&]() {

  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //  it will never be called after onUpdateExit() is invoked...
  //////////////////////////////////////////////////////////

  double spawn_timer = 0.0;

  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;

    spawn_timer += dt;

    //printf( "spawn_timer<%g>\n", spawn_timer );
    if( spawn_timer>1.0 ){
      auto SAD = std::make_shared<SpawnAnonDynamic>();
      SAD->_edataname = "ent_ball"_pool; // by anon we mean "unnamed"
      float x = controller->random(-3,3);
      float z = controller->random(-3,3);
      SAD->_overridexf = std::make_shared<DecompTransform>();
      SAD->_overridexf->set(fvec3(x,10,z),fquat(),1);
      auto ent     = controller->spawnAnonDynamicEntity(SAD);
      spawn_timer = 0.0;
    }

    ////////////////////////////
    // compute camera data
    //  theoretically this could be done by a camera animation component on an entity as well
    //   were one to exist...
    ////////////////////////////
    ///
    if (_sgsystem._sysID != NO_OBJECT_ID) {
      float phase    = abstime * PI2 * 0.01f;
      float distance = 20.0f;
      DataTable camera_data;
      camera_data["eye"_tok]  = fvec3(0,10,0)+fvec3::unitCircleXZ(phase) * distance;
      camera_data["tgt"_tok]  = fvec3(0, 4, 0);
      camera_data["up"_tok]   = fvec3(0, 1, 0);
      camera_data["near"_tok] = 0.1f;
      camera_data["far"_tok]  = 100.0f;
      camera_data["fovy"_tok] = 45.0f;
      controller->systemNotify(_sgsystem, SceneGraphSystem::UpdateCamera._token, camera_data);
      ////////////////////////////
    }
    controller->update();
    ////////////////////////////
  });

  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  auto sframe_ecs = std::make_shared<StandardCompositorFrame>();

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    sframe_ecs->_drawEvent = drwev;
    ///////////////////////////////////////////////////////////////////////
    auto context = drwev->GetTarget();
    //context->beginFrame();

    ///////////////////////////////////////////////////////////////////////
    // first render ecs into sframe_ecs
    //  this will consume drawbufs owned by the ecs simulation
    //  updated from controller->update() in the update thread
    ///////////////////////////////////////////////////////////////////////

    context->beginFrame();
    controller->renderWithStandardCompositorFrame(sframe_ecs);
    context->endFrame();

    ///////////////////////////////////////////////////////////////////////
    // done with frame
    ///////////////////////////////////////////////////////////////////////

    //context->endFrame();

    ///////////////////////////////////////////////////////////////////////
    // render to movie ?
    ///////////////////////////////////////////////////////////////////////

    if(movie){
      //sframe_top->onPostCompositorRender = [&](const AcquiredDrawQueueForRendering& rdb) {

        //auto rtbuf_accum = rdb._RCFD.getUserProperty("rtb_accum"_crc).get<rtbuffer_ptr_t>();
        auto fbi = context->FBI();
        //fbi->capture(rtbuf_accum.get(),"demo://output.png");
        CaptureBuffer capbuf;
        bool ok = fbi->captureAsFormat(nullptr, &capbuf, EBufferFormat::RGB8);
        movie->writeFrame(capbuf);

    }

    ///////////////////////////////////////////////////////////////////////

  });

  //////////////////////////////////////////////////////////
  // when resizing the app - we need to resize the entire rendering pipe
  //////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) {
    ecs::DataTable fbsize_data;
    fbsize_data["width"_tok]  = w;
    fbsize_data["height"_tok] = h;
    controller->systemNotify(_sgsystem, SceneGraphSystem::UpdateFramebufferSize, fbsize_data);
    //gpurec->_sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });


  //////////////////////////////////////////////////////////
  // updateExit handler, called once on update thread
  //  at app exit, always called before onGpuExit()
  //////////////////////////////////////////////////////////

  ezapp->onUpdateExit([&]() {
    logchan_editor->log("ONUPDATEEXIT!");
    controller->stopSimulation();
  });

  //////////////////////////////////////////////////////////
  // gpuExit handler, called once on main(rendering) thread
  //  at app exit, always called after onUpdateExit()
  //////////////////////////////////////////////////////////

  ezapp->onGpuExit([&](Context* ctx) {
    logchan_editor->log("ONGPUEXIT!");
    movie = nullptr; 
    controller->gpuExit(ctx); // clean up controller's GPU resources
    controller = nullptr;     // release controller

  });

  //////////////////////////////////////////////////////////
  // main thread run loop
  //////////////////////////////////////////////////////////

  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}

#pragma GCC diagnostic pop
