////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <iostream>

#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/vr/vr.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/imgui_ged.inl>
#include <ork/lev2/imgui/imgui_internal.h>
///////////////////////////////////////////////////////////////////////////////

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets

///////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(appinitdata_ptr_t init_data, //
               Context* ctx, //
               bool use_forward, //
               bool use_vr) { //

    if(use_vr){
      auto vrdev = orkidvr::novr::novr_device();
      orkidvr::setDevice(vrdev);
      vrdev->overrideSize(init_data->_width,init_data->_height);
    }

    _camlut                = std::make_shared<CameraDataLut>();
    _camdata               = std::make_shared<CameraData>();
    (*_camlut)["spawncam"] = _camdata;

    _char_drawable = std::make_shared<ModelDrawable>();

    //////////////////////////////////////////////
    // create scenegraph
    //////////////////////////////////////////////

    _sg_params                                         = std::make_shared<varmap::VarMap>();
    _sg_params->makeValueForKey<std::string>("preset") = use_forward ? "ForwardPBR" : "DeferredPBR";

    if(use_vr){
      _sg_params->makeValueForKey<std::string>("preset") = "PBRVR";
    }


    _sg_scene        = std::make_shared<scenegraph::Scene>(_sg_params);
    auto sg_layer    = _sg_scene->createLayer("default");
    auto sg_compdata = _sg_scene->_compositorData;
    auto nodetek     = sg_compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
    auto rendnode    = nodetek->tryRenderNodeAs<ork::lev2::pbr::deferrednode::DeferredCompositingNodePbr>();
    auto pbrcommon   = rendnode->_pbrcommon;

    pbrcommon->_depthFogDistance = 4000.0f;
    pbrcommon->_depthFogPower    = 5.0f;
    pbrcommon->_skyboxLevel      = 2.5;
    pbrcommon->_diffuseLevel     = 2.4; 
    pbrcommon->_specularLevel    = 15.2; 

    //////////////////////////////////////////////////////////

    ctx->debugPushGroup("main.onGpuInit");

    _char_modelasset = asset::AssetManager<XgmModelAsset>::load("data://tests/chartest/char_mesh");
    _char_animasset = asset::AssetManager<XgmAnimAsset>::load("data://tests/chartest/char_idle");

    auto model = _char_modelasset->getSharedModel();
    _char_drawable->bindModel(model);
    _char_drawable->_name = "char";
    auto modelinst = _char_drawable->_modelinst;
    //modelinst->setBlenderZup(true);
    modelinst->enableSkinning();
    modelinst->enableAllMeshes();

    //model->mSkeleton.mTopNodesMatrix.compose(fvec3(),fquat(),0.0001);

    auto skeldump = model->mSkeleton.dump(fvec3(1,1,1));
    printf( "skeldump<%s>\n", skeldump.c_str() );

    auto anim = _char_animasset->GetAnim();
    _char_animinst = std::make_shared<XgmAnimInst>();
    _char_animinst->bindAnim(anim);
    _char_animinst->SetWeight(1.0f);
    _char_animinst->RefMask().EnableAll();
    modelinst->_localPose.bindAnimInst(*_char_animinst);

    //////////////////////////////////////////////
    // scenegraph nodes
    //////////////////////////////////////////////

    _char_node = sg_layer->createDrawableNode("mesh-node", _char_drawable);

    ctx->debugPopGroup();
  }

  //lev2::xgmworldpose_ptr _char_worldpose;
  lev2::xgmanimmask_ptr_t _char_animmask;
  lev2::xgmaniminst_ptr_t _char_animinst;
  lev2::xgmanimassetptr_t _char_animasset; // retain anim
  lev2::xgmmodelassetptr_t _char_modelasset; // retain model
  model_drawable_ptr_t _char_drawable;
  scenegraph::node_ptr_t _char_node;

  varmap::varmap_ptr_t _sg_params;
  scenegraph::scene_ptr_t _sg_scene;

  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;
};

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);

  auto desc = init_data->commandLineOptions("model3dpbr example Options");
  desc->add_options()                  //
      ("help", "produce help message") //
      ("msaa", po::value<int>()->default_value(1), "msaa samples(*1,4,9,16,25)")
      ("ssaa", po::value<int>()->default_value(1), "ssaa samples(*1,4,9,16,25)")
      ("forward", po::bool_switch()->default_value(false), "forward renderer")
      ("fullscreen", po::bool_switch()->default_value(false), "fullscreen mode")                              
      ("left",  po::value<int>()->default_value(100), "left window offset")                              
      ("top",  po::value<int>()->default_value(100), "top window offset")                              
      ("width",  po::value<int>()->default_value(1280), "window width")                              
      ("height",  po::value<int>()->default_value(720), "window height")
      ("usevr",  po::bool_switch()->default_value(false), "use vr output");                             

  auto vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }
  init_data->_fullscreen = vars["fullscreen"].as<bool>();;
  init_data->_top = vars["top"].as<int>();
  init_data->_left = vars["left"].as<int>();
  init_data->_width = vars["width"].as<int>();
  init_data->_height = vars["height"].as<int>();

  init_data->_msaa_samples = vars["msaa"].as<int>();
  init_data->_ssaa_samples = vars["ssaa"].as<int>();

  printf( "_msaa_samples<%d>\n", init_data->_msaa_samples );
  bool use_forward = vars["forward"].as<bool>();
  bool use_vr = vars["usevr"].as<bool>();
  //////////////////////////////////////////////////////////
  init_data->_imgui = true;
  init_data->_application_name = "ork.model3dpbr";
  //////////////////////////////////////////////////////////
  auto ezapp  = OrkEzApp::create(init_data);
  std::shared_ptr<GpuResources> gpurec;
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { gpurec = std::make_shared<GpuResources>(init_data, ctx, use_forward,use_vr); });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();
  auto dbufcontext = std::make_shared<DrawBufContext>();
  auto sframe = std::make_shared<StandardCompositorFrame>();
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime+dt+.016;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.01f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    gpurec->_camdata->Lookat(eye, tgt, up);
    gpurec->_camdata->Persp(1, 50.0, 45.0);

    ////////////////////////////////////////
    // enqueue scenegraph to renderer
    ////////////////////////////////////////

    gpurec->_sg_scene->enqueueToRenderer(gpurec->_camlut);

    ////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {


    static int counter = 0;
    gpurec->_char_animinst->_current_frame = counter;
    gpurec->_char_animinst->SetWeight(1.0f);

    auto modelinst = gpurec->_char_drawable->_modelinst;
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    //localpose.applyAnimInst(*(gpurec->_char_animinst));
    localpose.blendPoses();
    localpose.concatenate();

    //auto lpdump = localpose.dump();
    //printf( "%s\n", lpdump.c_str() );

    fmtx4 world;
    world.compose(fvec3(0,0,0),fquat(),0.01); 

    worldpose.apply(world,localpose);

    if(counter==3){
      //OrkAssert(false);
    }
    counter = (counter+1) % 40;

    auto context = drwev->GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD.setUserProperty("vrcam"_crc, (const CameraData*) gpurec->_camdata.get() );
    gpurec->_sg_scene->renderOnContext(context, RCFD);
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) {
    gpurec->_sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  ezapp->onGpuExit([&](Context* ctx) { gpurec = nullptr; });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
