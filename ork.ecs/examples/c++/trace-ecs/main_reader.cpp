////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "resources.inl"

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  //////////////////////////////////////////////////////////
  // init application
  //////////////////////////////////////////////////////////

  lev2::initModule(init_data); // lev2 registration
  ecs::initModule(init_data);  // ecs registration

  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;

  auto this_dir = ezapp->_orkidWorkspaceDir //
                  / "ork.ecs"               //
                  / "examples"              //
                  / "c++"                   //
                  / "trace-ecs";

  auto filecontext = FileEnv::createContextForUriBase("demo://", this_dir);
  filecontext->SetFilesystemBaseEnable(true);

  //////////////////////////////////////////////////////////

  auto resources = std::make_shared<Resources>(this_dir);

  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////

  ezapp->onGpuInit([&](Context* ctx) {
    resources->onGpuInit(ctx);
    printf("ONGPUINIT!\n");
  });

  //////////////////////////////////////////////////////////
  // onUpdateInit (always called after onGpuInit() is complete...)
  //////////////////////////////////////////////////////////

  ezapp->onUpdateInit([&]() {
    printf("ONUPDATEINIT!\n");
    resources->beginReading();
    resources->onUpdateInit();
  }); // ezapp->onUpdateInit([&]() {

  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //  it will never be called after onUpdateExit() is invoked...
  //////////////////////////////////////////////////////////

  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) { resources->_controller->update(); });

  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    resources->_controller->render(drwev);
  });

  //////////////////////////////////////////////////////////
  // when resizing the app - we need to resize the entire rendering pipe
  //////////////////////////////////////////////////////////

  ezapp->onResize([&](int w, int h) {});

  //////////////////////////////////////////////////////////
  // updateExit handler, called once on update thread
  //  at app exit, always called before onGpuExit()
  //////////////////////////////////////////////////////////

  ezapp->onUpdateExit([&]() {
    printf("ONUPDATEEXIT!\n");
    resources->onUpdateExit();
  });

  //////////////////////////////////////////////////////////
  // gpuExit handler, called once on main(rendering) thread
  //  at app exit, always called after onUpdateExit()
  //////////////////////////////////////////////////////////

  ezapp->onGpuExit([&](Context* ctx) {
    resources->onGpuExit(ctx);
    resources = nullptr;
  });

  //////////////////////////////////////////////////////////
  // main thread run loop
  //////////////////////////////////////////////////////////

  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
