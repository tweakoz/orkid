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

    auto vars = *init_data->parse();

    //////////////////////////////////////////////////////////
    int testnum = vars["testnum"].as<int>();
    //////////////////////////////////////////////////////////

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

    auto model_load_req = std::make_shared<asset::LoadRequest>();
    auto anim_load_req = std::make_shared<asset::LoadRequest>();

    switch(testnum){
      case 0:
        model_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-mesh";
        anim_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-anim1";
        break;
      case 1:
        model_load_req->_asset_path = "data://tests/misc_gltf_samples/RiggedFigure/RiggedFigure";
        anim_load_req->_asset_path = "data://tests/misc_gltf_samples/RiggedFigure/RiggedFigure";
        break;
      case 2:
        model_load_req->_asset_path = "data://tests/chartest/char_mesh";
        anim_load_req->_asset_path = "data://tests/chartest/char_testanim1";
        break;
      case 3:
        model_load_req->_asset_path = "data://tests/hfstest/hfs_rigtest";
        anim_load_req->_asset_path = "data://tests/hfstest/hfs_rigtest_anim";
        break;
      default:
        OrkAssert(false);
        break;
    }

    auto mesh_override = vars["mesh"].as<std::string>();
    auto anim_override = vars["anim"].as<std::string>();

    if(mesh_override.length()){
      model_load_req->_asset_path = mesh_override;
    }
    if(anim_override.length()){
      anim_load_req->_asset_path = anim_override;
    }

    _char_modelasset = asset::AssetManager<XgmModelAsset>::load(model_load_req);
    OrkAssert(_char_modelasset);
    model_load_req->waitForCompletion();

    auto model = _char_modelasset->getSharedModel();
    auto skeldump = model->mSkeleton.dump(fvec3(1,1,1));
    printf( "skeldump<%s>\n", skeldump.c_str() );

    _char_animasset = asset::AssetManager<XgmAnimAsset>::load(anim_load_req);
    OrkAssert(_char_animasset);
    anim_load_req->waitForCompletion();

    _char_drawable->bindModel(model);
    _char_drawable->_name = "char";
    auto modelinst = _char_drawable->_modelinst;
    //modelinst->setBlenderZup(true);
    modelinst->enableSkinning();
    modelinst->enableAllMeshes();

    //model->mSkeleton.mTopNodesMatrix.compose(fvec3(),fquat(),0.0001);


    auto anim = _char_animasset->GetAnim();
    _char_animinst = std::make_shared<XgmAnimInst>();
    _char_animinst->bindAnim(anim);
    _char_animinst->SetWeight(1.0f);
    _char_animinst->RefMask().EnableAll();
    //_char_animinst->RefMask().Disable(model->mSkeleton,"mixamorig.RightShoulder");
    //_char_animinst->RefMask().Disable(model->mSkeleton,"mixamorig.RightArm");
    //_char_animinst->RefMask().Disable(model->mSkeleton,"mixamorig.RightForeArm");
    //_char_animinst->RefMask().Disable(model->mSkeleton,"mixamorig.RightHand");
    _char_animinst->_use_temporal_lerp = true;
    _char_animinst->bindToSkeleton(model->mSkeleton);

    _char_animinst2 = std::make_shared<XgmAnimInst>();
    _char_animinst2->bindAnim(anim);
    _char_animinst2->SetWeight(0.5);
    _char_animinst2->RefMask().DisableAll();
    _char_animinst2->RefMask().Enable(model->mSkeleton,"mixamorig.RightShoulder");
    _char_animinst2->RefMask().Enable(model->mSkeleton,"mixamorig.RightArm");
    _char_animinst2->RefMask().Enable(model->mSkeleton,"mixamorig.RightForeArm");
    _char_animinst2->RefMask().Enable(model->mSkeleton,"mixamorig.RightHand");
    _char_animinst2->_use_temporal_lerp = true;
    _char_animinst2->bindToSkeleton(model->mSkeleton);

    _char_animinst3 = std::make_shared<XgmAnimInst>();
    _char_animinst3->bindAnim(anim);
    _char_animinst3->SetWeight(0.5);
    _char_animinst3->RefMask().DisableAll();
    _char_animinst3->RefMask().Enable(model->mSkeleton,"mixamorig.RightShoulder");
    _char_animinst3->RefMask().Enable(model->mSkeleton,"mixamorig.RightArm");
    _char_animinst3->RefMask().Enable(model->mSkeleton,"mixamorig.RightForeArm");
    _char_animinst3->RefMask().Enable(model->mSkeleton,"mixamorig.RightHand");
    _char_animinst3->_use_temporal_lerp = true;
    _char_animinst3->bindToSkeleton(model->mSkeleton);

    _char_applicatorL = std::make_shared<lev2::XgmSkelApplicator>(model->mSkeleton);
    _char_applicatorL->bindToBone("mixamorig.LeftShoulder");
    _char_applicatorL->bindToBone("mixamorig.LeftArm");
    _char_applicatorL->bindToBone("mixamorig.LeftForeArm");
    _char_applicatorL->bindToBone("mixamorig.LeftHand");
    _char_applicatorL->bindToBone("mixamorig.LeftHandIndex1");
    _char_applicatorL->bindToBone("mixamorig.LeftHandIndex2");
    _char_applicatorL->bindToBone("mixamorig.LeftHandIndex3");
    _char_applicatorL->bindToBone("mixamorig.LeftHandIndex4");
    _char_applicatorL->bindToBone("mixamorig.LeftHandThumb1");
    _char_applicatorL->bindToBone("mixamorig.LeftHandThumb2");
    _char_applicatorL->bindToBone("mixamorig.LeftHandThumb3");
    _char_applicatorL->bindToBone("mixamorig.LeftHandThumb4");

    _char_applicatorR = std::make_shared<lev2::XgmSkelApplicator>(model->mSkeleton);
    _char_applicatorR->bindToBone("mixamorig.RightShoulder");
    _char_applicatorR->bindToBone("mixamorig.RightArm");
    _char_applicatorR->bindToBone("mixamorig.RightForeArm");
    _char_applicatorR->bindToBone("mixamorig.RightHand");
    _char_applicatorR->bindToBone("mixamorig.RightHandIndex1");
    _char_applicatorR->bindToBone("mixamorig.RightHandIndex2");
    _char_applicatorR->bindToBone("mixamorig.RightHandIndex3");
    _char_applicatorR->bindToBone("mixamorig.RightHandIndex4");
    _char_applicatorR->bindToBone("mixamorig.RightHandThumb1");
    _char_applicatorR->bindToBone("mixamorig.RightHandThumb2");
    _char_applicatorR->bindToBone("mixamorig.RightHandThumb3");
    _char_applicatorR->bindToBone("mixamorig.RightHandThumb4");

  //OrkAssert(false);
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    _char_animinst->_current_frame = 0; 
    _char_animinst->applyToPose(localpose);
    localpose.blendPoses();
    localpose.concatenate();
    worldpose.apply(fmtx4(),localpose);
    //OrkAssert(false);

    //////////////////////////////////////////////
    // scenegraph nodes
    //////////////////////////////////////////////

    _char_node = sg_layer->createDrawableNode("mesh-node", _char_drawable);

    ctx->debugPopGroup();
  }

  lev2::xgmanimmask_ptr_t _char_animmask;
  lev2::xgmaniminst_ptr_t _char_animinst;
  lev2::xgmaniminst_ptr_t _char_animinst2;
  lev2::xgmaniminst_ptr_t _char_animinst3;
  lev2::xgmanimassetptr_t _char_animasset; // retain anim
  lev2::xgmmodelassetptr_t _char_modelasset; // retain model
  lev2::xgmskelapplicator_ptr_t _char_applicatorL;
  lev2::xgmskelapplicator_ptr_t _char_applicatorR;

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
      ("usevr",  po::bool_switch()->default_value(false), "use vr output")                          
      ("testnum",  po::value<int>()->default_value(0), "animation test level")
      ("fbase",   po::value<std::string>()->default_value(""), "set user fbase")
      ("mesh",  po::value<std::string>()->default_value(""), "mesh file override")                             
      ("anim",  po::value<std::string>()->default_value(""), "animation file override");                             

  auto vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }
  //////////////////////////////////////////////////////////
  int testnum = vars["testnum"].as<int>();
  std::string fbase = vars["fbase"].as<std::string>();
  //////////////////////////////////////////////////////////
  if(fbase.length()){
    auto fdevctx = FileEnv::createContextForUriBase("fbase://", fbase);
  }
  //////////////////////////////////////////////////////////

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
  float animspeed = 1.0f;
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime+dt+.016;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = PI*abstime*0.05;

    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    auto eye = fvec3(sinf(phase), 0.3f, -cosf(phase));
    float wsca = 1.0f;
    float near = 1;
    float far = 500.0f;
    float fovy = 45.0f;

    switch(testnum){
      case 0:
        far = 50.0f;
        eye *= 10.0f;
        wsca = 0.15f;
        break;
      case 1:
        eye += fvec3(0,-1,0);
        eye *= 10.0f;
        wsca = 10.0f;
        break;
      case 2:
        tgt += fvec3(0,50,0);
        eye *= 100.0f;
        eye += fvec3(0,-50,0);
        wsca = 1.5f;
        break;
      case 3:
        eye *= 100.0f;
        wsca = 100.1f;
        break;
      default:
        break;
    }      
    gpurec->_camdata->Persp(near,far,fovy);
    gpurec->_camdata->Lookat(eye, tgt, up);

    ////////////////////////////////////////
    // set character node's world transform
    ////////////////////////////////////////

    fvec3 wpos(0,0,0);
    fquat wori;//fvec3(0,1,0),phase+PI);

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
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {

    float time = timer.SecsSinceStart();
    float frame = (time*30.0f*animspeed);

    auto anim = gpurec->_char_animasset->GetAnim();


    gpurec->_char_animinst->_current_frame = fmod(frame,float(anim->_numframes));
    gpurec->_char_animinst->SetWeight(0.5f);
    gpurec->_char_animinst2->_current_frame = fmod(frame*1.3,float(anim->_numframes));
    gpurec->_char_animinst2->SetWeight(0.5);
    gpurec->_char_animinst3->_current_frame = fmod(frame,float(anim->_numframes));
    gpurec->_char_animinst3->SetWeight(0.75);

    auto modelinst = gpurec->_char_drawable->_modelinst;
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    gpurec->_char_animinst->applyToPose(localpose);
    //gpurec->_char_animinst2->applyToPose(localpose);
    //gpurec->_char_animinst3->applyToPose(localpose);
    localpose.blendPoses();

    //auto lpdump = localpose.dump();
    //printf( "%s\n", lpdump.c_str() );

    localpose.concatenate();

    ///////////////////////////////////////////////////////////
    // use skel applicator on post concatenated bones
    ///////////////////////////////////////////////////////////



    auto model = gpurec->_char_modelasset->getSharedModel();
    auto& skel = model->skeleton();


    if(fmod(time,10)<5){

      int ji_lshoulder = skel.jointIndex("mixamorig.LeftShoulder");
      auto lshoulder_base = localpose._concat_matrices[ji_lshoulder];
      auto lshoulder_basei = lshoulder_base.inverse();

      fmtx4 rotmtx;
      rotmtx.setRotateY((sinf(time*5) * 7.5)*DTOR);
      rotmtx = lshoulder_basei*rotmtx*lshoulder_base;

      gpurec->_char_applicatorL->apply([&](int index){
        auto& ci = localpose._concat_matrices[index];
        ci = (rotmtx*ci);
      });
    }
    else{

      int ji_rshoulder = skel.jointIndex("mixamorig.RightShoulder");
      auto rshoulder_base = localpose._concat_matrices[ji_rshoulder];
      auto rshoulder_basei = rshoulder_base.inverse();

      fmtx4 rotmtx;
      rotmtx.setRotateZ((sinf(time*5) * 7.5)*DTOR);

      rotmtx = rshoulder_basei*rotmtx*rshoulder_base;

      gpurec->_char_applicatorR->apply([&](int index){
        auto& ci = localpose._concat_matrices[index];
        ci = (rotmtx*ci);
      });
    }
    ///////////////////////////////////////////////////////////

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
