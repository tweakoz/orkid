////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "resources.inl"

///////////////////////////////////////////////////////////////////////////////
// Pseudo Network Client
//  (for now just an event generator that runs in its own thread)
//  one can pretend events are coming from a network socket...
///////////////////////////////////////////////////////////////////////////////

struct PseudoClientImpl {
  /////////////////////////////////////
  resources_ptr_t _resources;
  controller_ptr_t _controller;
  ork::Thread _thread;
  bool _oktoexit   = false;
  float _wait_time = 1.0f;
  /////////////////////////////////////
  PseudoClientImpl(std::string clname, resources_ptr_t resources)
      : _resources(resources)
      , _controller(resources->_controller)
      , _thread(clname, this) {
    _oktoexit = false;
  }
  /////////////////////////////////////
  ~PseudoClientImpl() {
  }
  /////////////////////////////////////
  void start() {
    _thread.start([&](anyp data) { this->clientThreadLoop(); });
  }
  /////////////////////////////////////
  void stop() {
    _oktoexit = true;
    _thread.join();
  }
  /////////////////////////////////////
  void clientThreadLoop() {


    while (not _oktoexit) {

      float wait_time = _controller->random(0.5, 1.5);
      usleep(int(wait_time * 1e6));

      // delayed spawn 

      _controller->realtimeDelayedOperation(0.0,[=](){

        // delayed operations occur on update thread

        auto SAD = std::make_shared<SpawnAnonDynamic>();
        SAD->_edataname = "e1"_pool; // by anon we mean "unnamed"
        auto ent = _controller->spawnAnonDynamicEntity(SAD);
        auto luacomp = _controller->findEntityComponent<LuaComponentData>(ent);

        // invoke the SETSCALE event handler on this lua component's designated script
        //  what this does, depends on the implementation of that script
        //  in this case with shinyball.lua it will set the node's uniform scale
        //    interpolation target (where the interpolation happens in lua)

        float wait_time = _controller->random(1, 2);

        _controller->realtimeDelayedOperation(wait_time,[=](){

          float size   = _controller->random(1, 5);
          _controller->componentNotify(luacomp, "SETSCALE"_tok, size);

          /////////////
          // despawn at a later time...
          /////////////

          float wait_time = _controller->random(4, 5);

          _controller->realtimeDelayedOperation(wait_time,[=](){
            printf("CLIENT<%p> DELETING CLIENT ENTITY <%llu>\n", (void*)this, ent._entID );
            _controller->despawnEntity(ent);
          }); // despawn at a later time...

        }); // invoke the SETSCALE

      }); // delayed spawn 

    }
  }
};

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

  int client_index = 0;
  auto clname1     = FormatString("Client%d", client_index++);
  auto clname2     = FormatString("Client%d", client_index++);
  auto clname3     = FormatString("Client%d", client_index++);
  auto clname4     = FormatString("Client%d", client_index++);
  auto clname5     = FormatString("Client%d", client_index++);
  auto client1     = std::make_shared<PseudoClientImpl>(clname1, resources);
  auto client2     = std::make_shared<PseudoClientImpl>(clname2, resources);
  auto client3     = std::make_shared<PseudoClientImpl>(clname3, resources);
  auto client4     = std::make_shared<PseudoClientImpl>(clname4, resources);
  auto client5     = std::make_shared<PseudoClientImpl>(clname5, resources);

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

  sys_ref_t _sgsystem;

  ezapp->onUpdateInit([&]() {
    printf("ONUPDATEINIT!\n");
    resources->beginWriting();
    resources->onUpdateInit();

    auto c = resources->_controller;

    _sgsystem = c->findSystem<ecs::SceneGraphSystem>(); // opaque handle to sg system

    /////////////////////////////////
    // create a system owned dynamic scene graph node
    //  you can modify or delete it by sending
    //  other requests or events to the sg system directly
    /////////////////////////////////

    c->realtimeDelayedOperation(1.0,[=](){

      DataTable node_data;
      node_data["modeldata"_tok] = resources->_modeldata2;

      response_ref_t global_node = c->systemRequest( // returns an opaque handle to a node
          _sgsystem,                                           // scene graph system
          SceneGraphSystem::CreateNode,                        // request ID
          node_data);                                          // data with which to initialize node


      c->realtimeDelayedOperation(5.0,[=](){
        c->systemNotify(         //
            _sgsystem,                     // scene graph system
            SceneGraphSystem::DestroyNode, // event ID
            global_node);                  // opaque node handle to destroy
      });

    });

    /////////////////////////////////
    // start up pseudo network clients
    /////////////////////////////////

    client1->start();
    client2->start();
    client3->start();
    client4->start();
    client5->start();
    
  }); // ezapp->onUpdateInit([&]() {

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
    resources->_controller->systemNotify(
        _sgsystem,                  //
        SceneGraphSystem::UpdateCamera._token, //
        camera_data);
    ////////////////////////////
    resources->_controller->update();
    ////////////////////////////
  });

  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    resources->_controller->render(drwev);
  });

  //////////////////////////////////////////////////////////
  // when resizing the app - we need to resize the entire rendering pipe
  //////////////////////////////////////////////////////////

  ezapp->onResize([&](int w, int h) {
    DataTable fbsize_data;
    fbsize_data["width"_tok]  = w;
    fbsize_data["height"_tok] = h;
    resources->_controller->systemNotify(
        _sgsystem,                    //
        SceneGraphSystem::UpdateFramebufferSize, //
        fbsize_data);
  });

  //////////////////////////////////////////////////////////
  // updateExit handler, called once on update thread
  //  at app exit, always called before onGpuExit()
  //////////////////////////////////////////////////////////

  ezapp->onUpdateExit([&]() {
    printf("ONUPDATEEXIT!\n");
    client1->stop();
    client2->stop();
    client3->stop();
    client4->stop();
    client5->stop();
    client1 = nullptr;
    client2 = nullptr;
    client3 = nullptr;
    client4 = nullptr;
    client5 = nullptr;
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
