////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/ezapp.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/string/deco.inl>

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

  bool _imgui_open               = true;
  static int transport_mode      = 0;
  static int prev_transport_mode = 0;

  logchan_editor->log( "T0<%g>", timer.SecsSinceStart() );

  //////////////////////////////////////////////////////////
  // init application
  //////////////////////////////////////////////////////////

  lev2::initModule(init_data); // lev2 registration
  imgui::initModule(init_data);
  ecs::initModule(init_data); // ecs registration

  logchan_editor->log( "T1<%g>", timer.SecsSinceStart() );

  std::shared_ptr<MovieContext> movie = nullptr;
  
  if(0){
    init_data->_top = 0;
    init_data->_left = 1440;
    init_data->_width = 1920;
    init_data->_height = 1080;
    movie = std::make_shared<MovieContext>();
    movie->init(init_data->_width,init_data->_height);
  }

  auto ezapp  = OrkEzApp::create(init_data);
  auto qtwin  = ezapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;

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

  lev2::rtgroup_ptr_t outgroup;
  lev2::rtbuffer_ptr_t outbuffer;
  std::shared_ptr<lev2::TextureAsset> play_icon, pause_icon;

  ////////////////////////////
  // create ecs scene data
  ////////////////////////////

  auto scene = generateScene("demo://sceneout.ork");
  //auto scene = loadScene("demo://sceneout.ork");
  // saveScene("demo://sceneout2.ork",scene); validate deserialized matches serialized

  EcsEditor ecsdedit(scene);
  auto ecs_sg_sysdata  = scene->getTypedSystemData<SceneGraphSystemData>();

  //ecs_sg_sysdata->setSceneParam("supersample",int(4));

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

  auto compositordata = std::make_shared<CompositingData>();
  auto compositorimpl = compositordata->createImpl();
  auto renderer = std::make_shared<DefaultRenderer>();
  auto CPD = std::make_shared<CompositingPassData>();
  CPD->addStandardLayers();
  compositordata->mbEnable = true;
  auto ecs_camera = std::make_shared<CameraData>();
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////

  ezapp->onGpuInit([&](Context* ctx) {
   logchan_editor->log( "T4<%g>", timer.SecsSinceStart() );
    outgroup  = std::make_shared<RtGroup>(ctx, 100, 100, MsaaSamples::MSAA_1X);
    outbuffer = outgroup->createRenderTarget(EBufferFormat::RGBA32F);
    ecs_sg_sysdata->bindToRtGroup(outgroup);
    ecs_sg_sysdata->bindToCamera(ecs_camera);

    play_icon  = asset::AssetManager<TextureAsset>::load("lev2://textures/play_icon");
    pause_icon = asset::AssetManager<TextureAsset>::load("lev2://textures/pause_icon");
    compositordata->presetDeferredPBR();
    renderer->setContext(ctx);


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

  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ////////////////////////////////////////////////////////
    // handle transport
    ////////////////////////////////////////////////////////

    if (transport_mode == 0 and prev_transport_mode == 1) {
      START_SCENE();
    }
    if (transport_mode == 1 and prev_transport_mode == 0) {
      STOP_SCENE();
    }

    prev_transport_mode = transport_mode;

    ////////////////////////////
    // compute camera data
    //  theoretically this could be done by a camera animation component on an entity as well
    //   were one to exist...
    ////////////////////////////
    ///
    if (_sgsystem._sysID != NO_OBJECT_ID) {
      float phase    = 0; // abstime * PI2 * 0.1f;
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
    }
    controller->update();
    ////////////////////////////
  });

  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  auto sframe_outer = std::make_shared<StandardCompositorFrame>();
  sframe_outer->_use_imgui_docking = true;

  auto sframe_ecs = std::make_shared<StandardCompositorFrame>();

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    sframe_outer->_drawEvent = drwev;
    sframe_ecs->_drawEvent = drwev;
    ///////////////////////////////////////////////////////////////////////
    sframe_outer->compositor = compositorimpl;
    sframe_outer->renderer   = renderer;
    sframe_outer->passdata   = CPD;
    sframe_outer->_updrendersync = init_data->_update_rendersync;
    ///////////////////////////////////////////////////////////////////////
    auto context = drwev->GetTarget();
    context->beginFrame();
    controller->renderWithStandardCompositorFrame(sframe_ecs);
    sframe_outer->attachDrawBufContext(sframe_ecs->_dbufcontextSFRAME);
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
      if (!ImGui::Begin("ObjectEditor", &_imgui_open, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
      }

      // e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

      // ImGui::Text("dear imgui says hello. (%s)", IMGUI_VERSION);
      ImGui::Spacing();

      editor::EditorContext edctx(rdb);

      ImGui::Begin("Transport");

      for (int i = 0; i < 2; i++) {
        auto texture_asset = (i == 0) ? play_icon : pause_icon;
        auto texture       = texture_asset->GetTexture();
        int tex_w          = 32;
        int tex_h          = 32;
        auto texid         = texture->_varmap.typedValueForKey<GLuint>("gltexobj");

        ImGui::PushID(i);
        int frame_padding = -1 + i;                               // -1 == uses default padding (style.FramePadding)
        ImVec2 size       = ImVec2(32.0f, 32.0f);                 // Size of the image we want to make visible
        ImVec2 uv0        = ImVec2(0.0f, 0.0f);                   // UV coordinates for lower-left
        ImVec2 uv1        = ImVec2(32.0f / tex_w, 32.0f / tex_h); // UV coordinates for (32,32) in our texture
        ImVec4 bg_col     = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);       // Black background
        ImVec4 tint_col   = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);       // No tint
        if (transport_mode == i) {
          tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        if (ImGui::ImageButton( (void*) texid.value(), size, uv0, uv1, frame_padding, bg_col, tint_col)) {
          transport_mode = i;
        }
        ImGui::PopID();
        ImGui::SameLine();
      }

      ImGui::End();

      Outliner(edctx, ecsdedit);

      editor::imgui::ObjectEditor(edctx, ecsdedit._currentobject);

      ////////////////////////////////////////////////
      // draw editor viewport
      ////////////////////////////////////////////////

      ImGui::Begin("Viewport");

      ImGuiIO& io    = ImGui::GetIO();
      ImVec2 origpos = *io.MouseClickedPos;
      auto win       = ImGui::GetCurrentWindow();
      auto ori       = win->DC.CursorPos;
      auto wpos      = ImGui::GetWindowPos();
      auto wsiz      = ImGui::GetWindowSize();
      bool in_window = origpos.x >= wpos.x and origpos.x < wpos.x + wsiz.x;
      in_window &= origpos.y >= wpos.y and origpos.y < wpos.y + wsiz.y;

      int vpsize_w = wsiz.x;
      int vpsize_h = wsiz.y;

      DataTable fbsize_data;
      fbsize_data["width"_tok]  = vpsize_w;
      fbsize_data["height"_tok] = vpsize_h;
      controller->systemNotify(_sgsystem, SceneGraphSystem::UpdateFramebufferSize, fbsize_data);
      outgroup->Resize(vpsize_w, vpsize_h);

      auto pa = ori;
      auto pb = ori + ImVec2(wsiz.x, 0);
      auto pc = ori + ImVec2(wsiz.x, wsiz.y);
      auto pd = ori + ImVec2(0, wsiz.y);

      auto tex = outbuffer->texture();

      ////////////////////////////////////////////////

      if (auto as_texid = tex->_varmap.typedValueForKey<GLuint>("gltexobj")) {
        win->DrawList->AddImageQuad(
            (void*)as_texid.value(), // GL texture handle
            pb,
            pa,
            pd,
            pc,
            ImVec2(1, 1), // uva,
            ImVec2(0, 1), // uvb,
            ImVec2(0, 0), // uvc,
            ImVec2(1, 0), // uvd,
            0xffffffff);
      }

      ////////////////////////////////////////////////

      if (auto manip_target = edctx._varmap.typedValueForKey<ork::xfnode_ptr_t>("manip_target")) {

        auto manip_op       = edctx._varmap.typedValueForKey<ImGuizmo::OPERATION>("manip.op").value();
        auto manip_mode     = edctx._varmap.typedValueForKey<ImGuizmo::MODE>("manip.mode").value();
        auto manip_use_snap = edctx._varmap.typedValueForKey<bool>("manip.use_snap").value();
        auto manip_snap     = edctx._varmap.typedValueForKey<fvec3>("manip.snapval");

        auto transform = manip_target.value()->_transform;

        float aspect = wsiz.x / wsiz.y;

        auto camera  = ecs_camera.get();
        auto cammtx  = camera->computeMatrices(aspect);
        fmtx4 as_mtx = transform->composed();

        ImGuizmo::SetDrawlist(win->DrawList);
        ImGuizmo::SetRect(ori.x, ori.y, wsiz.x, wsiz.y);
        ImGuizmo::Enable(true);

        ///////////////////////////////////////////////////////////////////////////

        static fmtx4 _tempmatrix;

        auto v_array = cammtx._vmatrix.asArray();
        auto p_array = cammtx._pmatrix.asArray();
        auto m_array = _tempmatrix.asArray();

        auto try_xfstate = ImGuizmo::shouldManipulate(v_array,p_array,m_array);
        switch (try_xfstate) {
          case ImGuizmo::TransformState::BEGIN:{
            logchan_editor->log("BEGIN");
            _tempmatrix = (transform->composed());
            break;
          }
          case ImGuizmo::TransformState::TRANSFORMING: {
            // deco::prints(_testmatrix.dump4x3cn(), true);
            // logchan_editor->log("transformed<%p>", manip_target.value().get() );
            // transform->decompose(result);
            logchan_editor->log("TRANSFORMING");
            break;
          }
          case ImGuizmo::TransformState::END_APPLY:
            logchan_editor->log("END_APPLY");
            break;
          case ImGuizmo::TransformState::END_CANCEL:
            logchan_editor->log("END_CANCEL");
            break;
          case ImGuizmo::TransformState::NONE:
            break;
        }

        ///////////////////////////////////////////////////////////////////////////

        bool was_manipulated = ImGuizmo::Manipulate(
            v_array, //
            p_array, //
            manip_op,                  //
            manip_mode,                //
            m_array,     // matrix
            nullptr,     // delta matrix
            nullptr);                  // manip_use_snap ? &manip_snap.value().x : NULL);

        ///////////////////////////////////////////////////////////////////////////

        switch (try_xfstate) {
          case ImGuizmo::TransformState::TRANSFORMING:{
            if( was_manipulated ){
              transform->decompose(_tempmatrix);
            }
            break;
          }
          default:
            break;
        }
      }

      ImGui::End();
      ////////////////////////////////////////////////

      ImGui::PopItemWidth();

      // bool mouse_over_quizmo = EditTransform(*spncam, xfmtx);

      ImGui::End();
    };
    sframe_outer->render();
    context->endFrame();

    if(movie){
      //sframe_top->onPostCompositorRender = [&](const AcquiredRenderDrawBuffer& rdb) {

        //auto rtbuf_accum = rdb._RCFD.getUserProperty("rtb_accum"_crc).get<rtbuffer_ptr_t>();
        auto fbi = context->FBI();
        //fbi->capture(rtbuf_accum.get(),"demo://output.png");
        CaptureBuffer capbuf;
        bool ok = fbi->captureAsFormat(nullptr, &capbuf, EBufferFormat::RGB8);
        movie->writeFrame(capbuf);

    }

    //controller->renderWithStandardCompositorFrame(sframe_outer);
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
