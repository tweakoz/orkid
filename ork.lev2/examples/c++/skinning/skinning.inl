
#pragma once 
///////////////////////////////////////////////////////////////////////////////
#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
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
#include <ork/lev2/gfx/camera/uicam.h>
///////////////////////////////////////////////////////////////////////////////
using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets
///////////////////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(
      appinitdata_ptr_t init_data, //
      Context* ctx,                //
      bool use_forward) {          //

    auto vars = *init_data->parse();

    //////////////////////////////////////////////////////////
    int testnum = vars["testnum"].as<int>();
    //////////////////////////////////////////////////////////

    _camlut                = std::make_shared<CameraDataLut>();
    _camdata               = std::make_shared<CameraData>();
    (*_camlut)["spawncam"] = _camdata;

    _char_drawable = std::make_shared<ModelDrawable>();

    //////////////////////////////////////////////
    // create scenegraph
    //////////////////////////////////////////////

    _rtgroup = std::make_shared<lev2::RtGroup>(ctx,1,1,MsaaSamples::MSAA_1X);
    _rtbuffer = _rtgroup->createRenderTarget(EBufferFormat::RGBA8);
    _sg_params                                         = std::make_shared<varmap::VarMap>();
    _sg_params->makeValueForKey<std::string>("preset") = use_forward ? "ForwardPBR" : "DeferredPBR";
    _sg_params->makeValueForKey<lev2::rtgroup_ptr_t>("outputRTG") = _rtgroup;

    _sg_scene        = std::make_shared<scenegraph::Scene>(_sg_params);
    auto sg_layer    = _sg_scene->createLayer("default");
    auto sg_compdata = _sg_scene->_compositorData;
    auto nodetek     = sg_compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
    auto rendnode    = nodetek->tryRenderNodeAs<ork::lev2::pbr::deferrednode::DeferredCompositingNodePbr>();
    auto pbrcommon   = rendnode->_pbrcommon;

    pbrcommon->_depthFogDistance = 4000.0f;
    pbrcommon->_depthFogPower    = 5.0f;
    pbrcommon->_skyboxLevel      = 0.25;
    pbrcommon->_diffuseLevel     = 0.2;
    pbrcommon->_specularLevel    = 3.2;

    auto outpnode = nodetek->tryOutputNodeAs<ScreenOutputCompositingNode>();
    
    //////////////////////////////////////////////////////////

    ctx->debugPushGroup("main.onGpuInit");

    auto model_load_req = std::make_shared<asset::LoadRequest>();
    auto anim_load_req  = std::make_shared<asset::LoadRequest>();

    switch (testnum) {
      case 0:
        model_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-mesh";
        anim_load_req->_asset_path  = "data://tests/blender-rigtest/blender-rigtest-anim1";
        break;
      case 1:
        model_load_req->_asset_path = "data://tests/misc_gltf_samples/RiggedFigure/RiggedFigure";
        anim_load_req->_asset_path  = "data://tests/misc_gltf_samples/RiggedFigure/RiggedFigure";
        break;
      case 2:
        model_load_req->_asset_path = "data://tests/chartest/char_mesh";
        anim_load_req->_asset_path  = "data://tests/chartest/char_testanim1";
        break;
      case 3:
        model_load_req->_asset_path = "data://tests/hfstest/hfs_rigtest";
        anim_load_req->_asset_path  = "data://tests/hfstest/hfs_rigtest_anim";
        break;
      default:
        OrkAssert(false);
        break;
    }

    auto mesh_override = vars["mesh"].as<std::string>();
    auto anim_override = vars["anim"].as<std::string>();

    if (mesh_override.length()) {
      model_load_req->_asset_path = mesh_override;
    }
    if (anim_override.length()) {
      anim_load_req->_asset_path = anim_override;
    }

    _char_modelasset = asset::AssetManager<XgmModelAsset>::load(model_load_req);
    OrkAssert(_char_modelasset);
    model_load_req->waitForCompletion();

    auto model    = _char_modelasset->getSharedModel();
    auto skeldump = model->mSkeleton.dump(fvec3(1, 1, 1));
    printf("skeldump<%s>\n", skeldump.c_str());

    _char_animasset = asset::AssetManager<XgmAnimAsset>::load(anim_load_req);
    OrkAssert(_char_animasset);
    anim_load_req->waitForCompletion();

    _char_drawable->bindModel(model);
    _char_drawable->_name = "char";
    auto modelinst        = _char_drawable->_modelinst;
    // modelinst->setBlenderZup(true);
    modelinst->enableSkinning();
    modelinst->enableAllMeshes();
    modelinst->_drawSkeleton = true;

    // model->mSkeleton.mTopNodesMatrix.compose(fvec3(),fquat(),0.0001);

    auto anim      = _char_animasset->GetAnim();
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
    _char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightShoulder");
    _char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightArm");
    _char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightForeArm");
    _char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightHand");
    _char_animinst2->_use_temporal_lerp = true;
    _char_animinst2->bindToSkeleton(model->mSkeleton);

    _char_animinst3 = std::make_shared<XgmAnimInst>();
    _char_animinst3->bindAnim(anim);
    _char_animinst3->SetWeight(0.5);
    _char_animinst3->RefMask().DisableAll();
    _char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightShoulder");
    _char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightArm");
    _char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightForeArm");
    _char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightHand");
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

    _char_applicatorR1 = std::make_shared<lev2::XgmSkelApplicator>(model->mSkeleton);
    _char_applicatorR1->bindToBone("mixamorig.RightArm");

    _char_applicatorR2 = std::make_shared<lev2::XgmSkelApplicator>(model->mSkeleton);
    _char_applicatorR2->bindToBone("mixamorig.RightForeArm");

    // OrkAssert(false);
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    _char_animinst->_current_frame = 0;
    _char_animinst->applyToPose(localpose);
    localpose.blendPoses();
    localpose.concatenate();
    worldpose.apply(fmtx4(), localpose);
    // OrkAssert(false);

    auto rarm  = model->mSkeleton.bindMatrixByName("mixamorig.RightArm");
    auto rfarm = model->mSkeleton.bindMatrixByName("mixamorig.RightForeArm");
    auto rhand = model->mSkeleton.bindMatrixByName("mixamorig.RightHand");

    fvec3 rarm_pos, rfarm_pos;
    fquat rarm_quat, rfarm_quat;
    float rarm_scale, rfarm_scale;

    rarm.decompose(rarm_pos, rarm_quat, rarm_scale);
    rfarm.decompose(rfarm_pos, rfarm_quat, rfarm_scale);

    auto v_farm_arm  = rfarm.translation() - rarm.translation();
    auto v_hand_farm = rhand.translation() - rfarm.translation();

    _rarm_len    = v_farm_arm.length();
    _rfarm_len   = v_hand_farm.length();
    _rarm_scale  = rarm_scale;
    _rfarm_scale = rfarm_scale;

    //////////////////////////////////////////////
    // scenegraph nodes
    //////////////////////////////////////////////

    _char_node = sg_layer->createDrawableNode("mesh-node", _char_drawable);

    _uicamera                 = std::make_shared<EzUiCam>();
    _uicamera->_constrainZ    = true;
    _uicamera->_base_zmoveamt = 2.0f;
    _uicamera->mfLoc          = 100.0f;
    ctx->debugPopGroup();
  }

  lev2::rtbuffer_ptr_t _rtbuffer;
  lev2::rtgroup_ptr_t _rtgroup;

  lev2::xgmanimmask_ptr_t _char_animmask;
  lev2::xgmaniminst_ptr_t _char_animinst;
  lev2::xgmaniminst_ptr_t _char_animinst2;
  lev2::xgmaniminst_ptr_t _char_animinst3;
  lev2::xgmanimassetptr_t _char_animasset;   // retain anim
  lev2::xgmmodelassetptr_t _char_modelasset; // retain model
  lev2::xgmskelapplicator_ptr_t _char_applicatorL;
  lev2::xgmskelapplicator_ptr_t _char_applicatorR1;
  lev2::xgmskelapplicator_ptr_t _char_applicatorR2;
  lev2::xgmskelapplicator_ptr_t _char_applicatorR3;
  lev2::ezuicam_ptr_t _uicamera;
  model_drawable_ptr_t _char_drawable;
  scenegraph::node_ptr_t _char_node;

  varmap::varmap_ptr_t _sg_params;
  scenegraph::scene_ptr_t _sg_scene;

  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;

  float _rarm_len    = 0.0f;
  float _rfarm_len   = 0.0f;
  float _rarm_scale  = 0.0f;
  float _rfarm_scale = 0.0f;
};
