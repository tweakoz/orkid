////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/environment.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorFile.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>

using namespace ork;

const float BASE_X     = 610.0;
const float BASE_Z     = 23194.0;
const float PAW_RADIUS = 2000.0;

fvec3 ORIGIN(BASE_X, -1329, BASE_Z);

using namespace ork;
using namespace lev2;

int main(int argc, char** argv, char** envp) {

  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);

  /////////////////////////////////////////////////////////////////////////
  // Initialize environment
  /////////////////////////////////////////////////////////////////////////

  genviron.init_from_envp(envp);

  /////////////////////////////////////////////////////////////////////////
  // declare used ork modules
  /////////////////////////////////////////////////////////////////////////

  lev2::initModule(initdata);
  imgui::initModule(initdata);

  /////////////////////////////////////////////////////////////////////////
  // Create App
  /////////////////////////////////////////////////////////////////////////

  auto ezapp = OrkEzApp::create(initdata);
  auto ezwin = ezapp->_mainWindow;
  auto gfxwin = ezwin->_gfxwin;
  ezwin->_update_rendersync = false;

  /////////////////////////////////////////////////////////////////////////
  // setup compositor with deferred PBR based lighting model
  /////////////////////////////////////////////////////////////////////////

  auto compdata = std::make_shared<CompositingData>();
  compositorimpl_ptr_t compimpl;
  compositingpassdata_ptr_t passdata;

  /////////////////////////////////////////////////////////////////////////
  // default camera
  /////////////////////////////////////////////////////////////////////////

  auto cameras = std::make_shared<CameraDataLut>();
  auto camdata = std::make_shared<CameraData>();
  (*cameras)["spawncam"] = camdata;

  /////////////////////////////////////////////////////////////////////////
  // GPU init
  /////////////////////////////////////////////////////////////////////////

  auto renderer = std::make_shared<DefaultRenderer>();
  auto sframe = std::make_shared<StandardCompositorFrame>();
  sframe->_use_imgui_docking = true;

  ezapp->onGpuInit([&](Context* ctx) {
    printf( "onGpuInit ctx<%p>\n", (void*) ctx);
    ctx->debugPushGroup("main.onGpuInit");
    renderer->setContext(ctx);
    compdata->presetPBR();
    compdata->mbEnable = true;
    compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);

    compimpl         = compdata->createImpl();
    passdata = std::make_shared<lev2::CompositingPassData>();
    passdata->addStandardLayers();

    sframe->compositor = compimpl;
    sframe->renderer = renderer;
    sframe->passdata = passdata;
    sframe->_updrendersync = initdata->_update_rendersync;

    ctx->debugPopGroup();
  });

  ezapp->onGpuExit([&](Context* ctx) {
    printf( "onGpuExit ctx<%p>\n", (void*) ctx);
  });

  /////////////////////////////////////////////////////////////////////////
  // iterate from update thread
  /////////////////////////////////////////////////////////////////////////

  ezapp->onUpdateInit([=]() {
    printf( "onUpdateInit\n");
  });
  ezapp->onUpdateExit([=]() {
    printf( "onUpdateExit\n");
  });

  ezapp->onUpdate([=](ui::updatedata_ptr_t updateData) {

    float abstime = updateData->_abstime;

    //////////////////////////////////////////
    // Animate camera
    //////////////////////////////////////////

    float phase    = abstime * PI2 * 0.01f;
    float distance = 3150.0f + sinf(phase * 2.7) * 2500.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 target(0, 0, 0);
    fvec3 up(0, 1, 0);
    ORIGIN.x = BASE_X + sinf(phase) * PAW_RADIUS;
    ORIGIN.z = BASE_Z - cosf(phase) * PAW_RADIUS;

    camdata->Persp(10, 15000.0, 45.0);
    camdata->Lookat(ORIGIN + eye, ORIGIN + target, up);

    //////////////////////////////////////////
    // update -> renderer
    //////////////////////////////////////////

    sframe->withAcquiredUpdateDrawBuffer(
      0,
      initdata->_update_rendersync,
      [cameras](const AcquiredUpdateDrawBuffer& udb){
        udb._DB->copyCameras(*cameras);
    });
  });

  /////////////////////////////////////////////////////////////////////////
  // iterate from render thread (typically draws a frame)
  /////////////////////////////////////////////////////////////////////////

  ezapp->onDraw([&](ui::drawevent_constptr_t drawEvent) {
    sframe->_drawEvent = drawEvent;
    auto context = drawEvent->GetTarget();
    context->beginFrame();

    //////////////////////////////////////////////////////////////////
    // lambda which renders IMGUI related stuff
    //  called automatically at the appropriate time in the frame
    //   (typically post-compositor)
    //////////////////////////////////////////////////////////////////

    sframe->onImguiRender = [](const AcquiredRenderDrawBuffer& rdb) {
      ImGui::ShowDemoWindow();
    };

    //////////////////////////////////////////////////////////////////
    // draw the actual frame
    //////////////////////////////////////////////////////////////////
    
    sframe->render();

    context->endFrame();

  });

  /////////////////////////////////////////////////////////////////////////
  // update viewport size
  /////////////////////////////////////////////////////////////////////////

  ezapp->onResize([&](int w, int h) {
    compimpl->compositingContext().Resize(w, h);
    ezapp->_uicontext->_top->SetSize(w, h);
  });

  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}