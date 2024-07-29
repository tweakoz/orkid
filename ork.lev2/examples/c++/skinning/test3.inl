
skinning_test_ptr_t createTest3(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test3IMPL {

    Test3IMPL(GpuResources* gpurec)
        : _gpurec(gpurec) {

      auto model_load_req         = std::make_shared<asset::LoadRequest>();
      auto anim_load_req          = std::make_shared<asset::LoadRequest>();
      model_load_req->_asset_path = "data://tests/chartest/char_mesh";
      anim_load_req->_asset_path  = "data://tests/chartest/char_testanim1";

      _char_modelasset = asset::AssetManager<XgmModelAsset>::load(model_load_req);
      OrkAssert(_char_modelasset);
      model_load_req->waitForCompletion();

      auto model    = _char_modelasset->getSharedModel();
      auto skeldump = model->_skeleton->dump(fvec3(1, 1, 1));
      printf("skeldump<%s>\n", skeldump.c_str());

      _char_animasset = asset::AssetManager<XgmAnimAsset>::load(anim_load_req);
      OrkAssert(_char_animasset);
      anim_load_req->waitForCompletion();

      _char_drawable = std::make_shared<ModelDrawable>();
      _char_drawable->bindModel(model);
      _char_drawable->_name = "char";

      auto node_name = FormatString("test3-node<%p>", (void*)this);

      _char_node = gpurec->_sg_layer->createDrawableNode(node_name, _char_drawable);

      auto modelinst = _char_drawable->_modelinst;
      modelinst->enableSkinning();
      modelinst->enableAllMeshes();
      modelinst->_drawSkeleton = true;

      auto anim      = _char_animasset->_animation;

      _char_animinstA = std::make_shared<XgmAnimInst>();
      auto maskA = _char_animinstA->_mask;
      _char_animinstA->bindAnim(anim);
      _char_animinstA->SetWeight(1.0f);
      _char_animinstA->_use_temporal_lerp = true;
      maskA->EnableAll();
      maskA->Disable(model->_skeleton,"mixamorig.LeftShoulder");
      maskA->Disable(model->_skeleton,"mixamorig.LeftArm");
      maskA->Disable(model->_skeleton,"mixamorig.LeftForeArm");
      maskA->Disable(model->_skeleton,"mixamorig.LeftHand");
      maskA->Disable(model->_skeleton,"mixamorig.RightShoulder");
      maskA->Disable(model->_skeleton,"mixamorig.RightArm");
      maskA->Disable(model->_skeleton,"mixamorig.RightForeArm");
      maskA->Disable(model->_skeleton,"mixamorig.RightHand");
      _char_animinstA->bindToSkeleton(model->_skeleton);

      _char_animinstB = std::make_shared<XgmAnimInst>();
      auto maskB = _char_animinstB->_mask;
      _char_animinstB->bindAnim(anim);
      _char_animinstB->SetWeight(1.0f);
      _char_animinstB->_use_temporal_lerp = true;
      maskB->DisableAll();
      maskB->Enable(model->_skeleton,"mixamorig.LeftShoulder");
      maskB->Enable(model->_skeleton,"mixamorig.LeftArm");
      maskB->Enable(model->_skeleton,"mixamorig.LeftForeArm");
      maskB->Enable(model->_skeleton,"mixamorig.LeftHand");
      _char_animinstB->bindToSkeleton(model->_skeleton);

      _char_animinstC = std::make_shared<XgmAnimInst>();
      auto maskC = _char_animinstC->_mask;
      _char_animinstC->bindAnim(anim);
      _char_animinstC->SetWeight(1.0f);
      _char_animinstC->_use_temporal_lerp = true;
      maskC->DisableAll();
      maskC->Enable(model->_skeleton,"mixamorig.RightShoulder");
      maskC->Enable(model->_skeleton,"mixamorig.RightArm");
      maskC->Enable(model->_skeleton,"mixamorig.RightForeArm");
      maskC->Enable(model->_skeleton,"mixamorig.RightHand");
      _char_animinstC->bindToSkeleton(model->_skeleton);

      auto localpose = modelinst->_localPose;
      auto worldpose = modelinst->_worldPose;

      localpose->bindPose();
      _char_animinstA->_current_frame = 0;
      _char_animinstA->applyToPose(localpose);
      _char_animinstB->applyToPose(localpose);
      _char_animinstC->applyToPose(localpose);
      localpose->blendPoses();
      localpose->concatenate();
      worldpose->apply(fmtx4(), localpose);

      _timer.Start();
    }

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    GpuResources* _gpurec = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinstA;
    lev2::xgmaniminst_ptr_t _char_animinstB;
    lev2::xgmaniminst_ptr_t _char_animinstC;
    scenegraph::node_ptr_t _char_node;
    ork::Timer _timer;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->_impl.makeShared<Test3IMPL>(gpurec);

  /////////////////////////////////////////////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test3IMPL>();
    impl->_char_node->_enabled = true;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test3IMPL>();
    impl->_char_node->_enabled = false;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test3IMPL>();

    fvec3 position(0, 0, 0);
    fquat orientation;
    float scale = 6.0f;
    impl->_char_node->_dqxfdata._worldTransform->set(position, orientation, scale);
  };

  /////////////////////////////////////////////////////////////////////////////


  test->onDraw = [test]() {
    auto impl   = test->_impl.getShared<Test3IMPL>();
    auto gpurec = impl->_gpurec;
    gpurec->_pbrcommon->_depthFogDistance = 10000.0f;
    gpurec->_pbrcommon->_depthFogPower    = 1.0f;
    gpurec->_pbrcommon->_skyboxLevel      = 1.0;
    gpurec->_pbrcommon->_diffuseLevel     = 1.0;
    gpurec->_pbrcommon->_specularLevel    = 3.2;

    ///////////////////////////////////////////////////////////
    // use skel applicator on post concatenated bones
    ///////////////////////////////////////////////////////////

    float time  = impl->_timer.SecsSinceStart();
    float frame = (time * 30.0f * gpurec->_animspeed);

    auto anim = impl->_char_animasset->_animation;

    impl->_char_animinstA->_current_frame = fmod(frame, float(anim->_numframes));
    impl->_char_animinstA->SetWeight(1);
    impl->_char_animinstB->_current_frame = fmod(frame*1.3, float(anim->_numframes));
    impl->_char_animinstB->SetWeight(1);
    impl->_char_animinstC->_current_frame = fmod(frame*1.7, float(anim->_numframes));
    impl->_char_animinstC->SetWeight(1);

    auto modelinst  = impl->_char_drawable->_modelinst;
    auto localpose = modelinst->_localPose;
    auto worldpose = modelinst->_worldPose;

    localpose->bindPose();
    impl->_char_animinstA->applyToPose(localpose);
    impl->_char_animinstB->applyToPose(localpose);
    impl->_char_animinstC->applyToPose(localpose);
    localpose->blendPoses();
    localpose->concatenate();
  };

  //////////////////////////////////////

  return test;
}
