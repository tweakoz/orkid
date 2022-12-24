////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "skinning.inl"

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);

  OrkEzApp::createDefaultOptions(
      init_data,                                                                  //
      "skinning example")                                                         //
      ("testnum", po::value<int>()->default_value(0), "animation test level")     //
      ("fbase", po::value<std::string>()->default_value(""), "set user fbase")    //
      ("mesh", po::value<std::string>()->default_value(""), "mesh file override") //
      ("anim", po::value<std::string>()->default_value(""), "animation file override");

  auto vars = *init_data->parse();

  //////////////////////////////////////////////////////////
  int testnum       = vars["testnum"].as<int>();
  std::string fbase = vars["fbase"].as<std::string>();
  //////////////////////////////////////////////////////////
  if (fbase.length()) {
    auto fdevctx = FileEnv::createContextForUriBase("fbase://", fbase);
  }
  //////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////
  init_data->_imgui            = true;
  init_data->_application_name = "ork.skinning";
  //////////////////////////////////////////////////////////
  auto ezapp = OrkEzApp::create(init_data);
  std::shared_ptr<GpuResources> gpurec;
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { //
    gpurec = std::make_shared<GpuResources>(init_data, ctx); //
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  auto dbufcontext = std::make_shared<DrawBufContext>();
  auto sframe      = std::make_shared<StandardCompositorFrame>();
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    gpurec->_uicamera->aper = 45.0 * DTOR;
    gpurec->_uicamera->updateMatrices();

    (*gpurec->_camdata) = gpurec->_uicamera->_camcamdata;

    ////////////////////////////////////////
    // update active test
    ////////////////////////////////////////

    gpurec->onUpdate(updata);

    ////////////////////////////////////////
    // enqueue scenegraph to renderer
    ////////////////////////////////////////

    gpurec->_sg_scene->enqueueToRenderer(gpurec->_camlut);

    ////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  auto sframe_imgui                = std::make_shared<StandardCompositorFrame>();
  sframe_imgui->_use_imgui_docking = true;
  bool _imgui_open                 = true;

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    sframe_imgui->_drawEvent = drwev;
    auto context             = drwev->GetTarget();
    context->beginFrame();

    ///////////////// render active test //////////////////////

    gpurec->_active_test->onDraw();
    gpurec->_sg_scene->renderWithStandardCompositorFrame(sframe_imgui);

    ///////////////// end render 3d content /////////////////////

    sframe_imgui->onImguiRender = [&](const AcquiredRenderDrawBuffer& rdb) {
      ImGuiStyle& style       = ImGui::GetStyle();
      style.WindowRounding    = 5.3f;
      style.FrameRounding     = 2.3f;
      style.ScrollbarRounding = 2.3f;
      style.FrameBorderSize   = 1.0f;

      IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

      ImGuiWindowFlags window_flags = 0;

      if (!ImGui::Begin("Editor", &_imgui_open, window_flags)) {
        ImGui::End();
        return;
      }

      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

      ImGui::Spacing();

      ImGuiComboFlags flags            = 0;
      const char* test_items[]         = {"Test0", "Test1", "Test2", "Test3"};
      static int test_item_current_idx = 0; 
      const char* combo_preview_value =
          test_items[test_item_current_idx]; // Pass in the preview value visible before opening the combo (it could be anything)
      if (ImGui::BeginCombo("combo 1", combo_preview_value, flags)) {
        for (int n = 0; n < IM_ARRAYSIZE(test_items); n++) {
          const bool is_selected = (test_item_current_idx == n);
          if (ImGui::Selectable(test_items[n], is_selected))
            test_item_current_idx = n;

          // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      gpurec->_active_test = gpurec->_sktests[test_item_current_idx];

      ////////////////////////////////////////////////
      // draw editor 3D viewport
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

      gpurec->_rtgroup->Resize(vpsize_w, vpsize_h);
      gpurec->_sg_scene->_compositorImpl->compositingContext().Resize(vpsize_w, vpsize_h);
      gpurec->_uicamera->_vpdim = fvec2(vpsize_w, vpsize_h);

      // compute viewport extents

      auto pa = ori;
      auto pb = ori + ImVec2(wsiz.x, 0);
      auto pc = ori + ImVec2(wsiz.x, wsiz.y);
      auto pd = ori + ImVec2(0, wsiz.y);

      ////////////////////////////////////////////////
      // grab texture from ECS rtgroup/rtbuffer
      ////////////////////////////////////////////////

      auto tex = gpurec->_rtbuffer->texture();

      ////////////////////////////////////////////////

      if (auto as_texid = tex->_varmap.typedValueForKey<GLuint>("gltexobj")) {
        win->DrawList->AddImageQuad(
            (ImTextureID)as_texid.value(), // GL texture handle
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

      ImGui::End();

      ImGui::PopItemWidth();
      ImGui::End();
    };

    ///////////////////////////////////////////////////////////////////////
    // done with frame
    ///////////////////////////////////////////////////////////////////////

    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { //
  });
  ezapp->onGpuExit([&](Context* ctx) { gpurec = nullptr; });
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool handled = gpurec->_uicamera->UIEventHandler(ev);
    return ui::HandlerResult();
  });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
