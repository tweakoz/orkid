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
    init_data, //
    "skinning example" ) //
    ("testnum", po::value<int>()->default_value(0), "animation test level") //
    ("fbase", po::value<std::string>()->default_value(""), "set user fbase") //
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
  bool use_forward = vars["forward"].as<bool>();
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
  ezapp->onGpuInit([&](Context* ctx) { gpurec = std::make_shared<GpuResources>(init_data, ctx, use_forward); });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();
  auto dbufcontext = std::make_shared<DrawBufContext>();
  auto sframe      = std::make_shared<StandardCompositorFrame>();
  float animspeed  = 1.0f;
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase = 4; // PI * abstime * 0.05;

    float wsca = 1.0f;
    float near = 1;
    float far  = 500.0f;
    float fovy = 45.0f;

    switch (testnum) {
      case 0:
        far = 50.0f;
        wsca = 0.15f;
        break;
      case 1:
        wsca = 10.0f;
        break;
      case 2:
        wsca = 6.0f;
        break;
      case 3:
        wsca = 100.1f;
        break;
      default:
        break;
    }

    gpurec->_uicamera->aper = fovy * DTOR;
    gpurec->_uicamera->updateMatrices();

    (*gpurec->_camdata) = gpurec->_uicamera->_camcamdata;

    ////////////////////////////////////////
    // set character node's world transform
    ////////////////////////////////////////

    fvec3 wpos(0, 0, 0);
    fquat wori; // fvec3(0,1,0),phase+PI);

    gpurec->_char_node->_dqxfdata._worldTransform->set(wpos, wori, wsca);

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

    ///////////////// render 3d content /////////////////////

    float time  = timer.SecsSinceStart();
    float frame = (time * 30.0f * animspeed);

    auto anim = gpurec->_char_animasset->GetAnim();

    gpurec->_char_animinst->_current_frame = fmod(frame, float(anim->_numframes));
    gpurec->_char_animinst->SetWeight(0.5f);
    gpurec->_char_animinst2->_current_frame = fmod(frame * 1.3, float(anim->_numframes));
    gpurec->_char_animinst2->SetWeight(0.5);
    gpurec->_char_animinst3->_current_frame = fmod(frame, float(anim->_numframes));
    gpurec->_char_animinst3->SetWeight(0.75);

    auto modelinst  = gpurec->_char_drawable->_modelinst;
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    gpurec->_char_animinst->applyToPose(localpose);
    // gpurec->_char_animinst2->applyToPose(localpose);
    // gpurec->_char_animinst3->applyToPose(localpose);
    localpose.blendPoses();

    // auto lpdump = localpose.dump();
    // printf( "%s\n", lpdump.c_str() );

    localpose.concatenate();

    ///////////////////////////////////////////////////////////
    // use skel applicator on post concatenated bones
    ///////////////////////////////////////////////////////////

    auto model = gpurec->_char_modelasset->getSharedModel();
    auto& skel = model->skeleton();
    if (0) { // fmod(time, 10) < 5) {

      int ji_lshoulder     = skel.jointIndex("mixamorig.LeftShoulder");
      auto lshoulder_base  = localpose._concat_matrices[ji_lshoulder];
      auto lshoulder_basei = lshoulder_base.inverse();

      fmtx4 rotmtx;
      rotmtx.setRotateY((sinf(time * 5) * 7.5) * DTOR);
      rotmtx = lshoulder_basei * rotmtx * lshoulder_base;

      gpurec->_char_applicatorL->apply([&](int index) {
        auto& ci = localpose._concat_matrices[index];
        ci       = (rotmtx * ci);
      });
    }
    if (1) { // else{

      int ji_rshoulder = skel.jointIndex("mixamorig.RightShoulder");
      int ji_rarm      = skel.jointIndex("mixamorig.RightArm");
      int ji_rfarm     = skel.jointIndex("mixamorig.RightForeArm");
      int ji_rhand     = skel.jointIndex("mixamorig.RightHand");

      const auto& rshoulder = localpose._concat_matrices[ji_rshoulder];
      auto rshoulder_i      = rshoulder.inverse();

      auto rarm    = localpose._concat_matrices[ji_rarm];
      auto rarm_i  = rarm.inverse();
      auto rfarm   = localpose._concat_matrices[ji_rfarm];
      auto rfarm_i = rfarm.inverse();

      localpose._boneprops[ji_rarm] = 1;

      ///////////////////////

      auto rhand         = localpose._concat_matrices[ji_rhand];
      auto wrist_xnormal = rhand.xNormal();
      auto wrist_ynormal = rhand.yNormal();
      auto wrist_znormal = rhand.zNormal();

      auto elbow_pos    = rhand.translation() - (wrist_ynormal * gpurec->_rfarm_len);
      auto elbow_normal = (elbow_pos - rshoulder.translation()).normalized();

      fmtx4 elbowR, elbowS, elbowT;
      elbowR.fromNormalVectors(
          wrist_xnormal,  //
          -wrist_ynormal, //
          wrist_znormal);
      elbowS.setScale(gpurec->_rfarm_scale);
      elbowT.setTranslation(elbow_pos);

      rfarm = elbowT * (elbowR * elbowS);

      fmtx4 MM, MS;
      MM.correctionMatrix(rshoulder, rfarm);
      MS.setScale(gpurec->_rarm_scale);

      ///////////////////////

      // localpose._concat_matrices[ji_rfarm] = (MS*MM)*rshoulder;
      // localpose._concat_matrices[ji_rfarm] = rhand;
    }

    ///////////////////////////////////////////////////////////

    gpurec->_sg_scene->renderWithStandardCompositorFrame(sframe_imgui);

    ///////////////// end render 3d content /////////////////////

    sframe_imgui->onImguiRender = [&](const AcquiredRenderDrawBuffer& rdb) {
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

      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

      ImGui::Spacing();

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

      gpurec->_rtgroup->Resize(vpsize_w,vpsize_h);
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
    // imgui render
    ///////////////////////////////////////////////////////////////////////

    sframe_imgui->_drawEvent = drwev;
    sframe_imgui->compositor = gpurec->_sg_scene->_compositorImpl;
    sframe_imgui->renderer   = gpurec->_sg_scene->_renderer;
    sframe_imgui->passdata   = gpurec->_sg_scene->_topCPD;
    sframe_imgui->_updrendersync = init_data->_update_rendersync;
    sframe_imgui->pushEmptyUpdateDrawBuf(); // produce empty drawbuf update
    //sframe_imgui->render();                 // will consume drawbufs from sframe_imgui

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
