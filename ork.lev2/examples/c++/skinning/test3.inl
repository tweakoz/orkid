
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
      auto skeldump = model->mSkeleton.dump(fvec3(1, 1, 1));
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

      auto anim      = _char_animasset->GetAnim();

      _char_animinstA = std::make_shared<XgmAnimInst>();
      _char_animinstA->bindAnim(anim);
      _char_animinstA->SetWeight(1.0f);
      _char_animinstA->_use_temporal_lerp = true;
      _char_animinstA->RefMask().EnableAll();
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.LeftShoulder");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.LeftArm");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.LeftForeArm");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.LeftHand");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.RightShoulder");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.RightArm");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.RightForeArm");
      _char_animinstA->RefMask().Disable(model->mSkeleton,"mixamorig.RightHand");
      _char_animinstA->bindToSkeleton(model->mSkeleton);

      _char_animinstB = std::make_shared<XgmAnimInst>();
      _char_animinstB->bindAnim(anim);
      _char_animinstB->SetWeight(1.0f);
      _char_animinstB->_use_temporal_lerp = true;
      _char_animinstB->RefMask().DisableAll();
      _char_animinstB->RefMask().Enable(model->mSkeleton,"mixamorig.LeftShoulder");
      _char_animinstB->RefMask().Enable(model->mSkeleton,"mixamorig.LeftArm");
      _char_animinstB->RefMask().Enable(model->mSkeleton,"mixamorig.LeftForeArm");
      _char_animinstB->RefMask().Enable(model->mSkeleton,"mixamorig.LeftHand");
      _char_animinstB->bindToSkeleton(model->mSkeleton);

      _char_animinstC = std::make_shared<XgmAnimInst>();
      _char_animinstC->bindAnim(anim);
      _char_animinstC->SetWeight(1.0f);
      _char_animinstC->_use_temporal_lerp = true;
      _char_animinstC->RefMask().DisableAll();
      _char_animinstC->RefMask().Enable(model->mSkeleton,"mixamorig.RightShoulder");
      _char_animinstC->RefMask().Enable(model->mSkeleton,"mixamorig.RightArm");
      _char_animinstC->RefMask().Enable(model->mSkeleton,"mixamorig.RightForeArm");
      _char_animinstC->RefMask().Enable(model->mSkeleton,"mixamorig.RightHand");
      _char_animinstC->bindToSkeleton(model->mSkeleton);

      auto& localpose = modelinst->_localPose;
      auto& worldpose = modelinst->_worldPose;

      localpose.bindPose();
      _char_animinstA->_current_frame = 0;
      _char_animinstA->applyToPose(localpose);
      _char_animinstB->applyToPose(localpose);
      _char_animinstC->applyToPose(localpose);
      localpose.blendPoses();
      localpose.concatenate();
      worldpose.apply(fmtx4(), localpose);

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

    float animspeed = 1.0f;

    ///////////////////////////////////////////////////////////
    // use skel applicator on post concatenated bones
    ///////////////////////////////////////////////////////////

    float time  = impl->_timer.SecsSinceStart();
    float frame = (time * 30.0f * animspeed);

    auto anim = impl->_char_animasset->GetAnim();

    impl->_char_animinstA->_current_frame = fmod(frame, float(anim->_numframes));
    impl->_char_animinstA->SetWeight(0.33f);
    impl->_char_animinstB->_current_frame = fmod(frame*2, float(anim->_numframes));
    impl->_char_animinstB->SetWeight(0.33f);
    impl->_char_animinstC->_current_frame = fmod(frame*5, float(anim->_numframes));
    impl->_char_animinstC->SetWeight(0.33f);

    auto modelinst  = impl->_char_drawable->_modelinst;
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    impl->_char_animinstA->applyToPose(localpose);
    impl->_char_animinstB->applyToPose(localpose);
    impl->_char_animinstC->applyToPose(localpose);
    localpose.blendPoses();
    localpose.concatenate();
  };

  //////////////////////////////////////

  return test;
}

/*

    auto anim = gpurec->_char_animasset->GetAnim();

    gpurec->_char_animinst->_current_frame = fmod(frame, float(anim->_numframes));
    gpurec->_char_animinst->SetWeight(0.5f);
    //gpurec->_char_animinst2->_current_frame = fmod(frame * 1.3, float(anim->_numframes));
    //gpurec->_char_animinst2->SetWeight(0.5);
    //gpurec->_char_animinst3->_current_frame = fmod(frame, float(anim->_numframes));
    //gpurec->_char_animinst3->SetWeight(0.75);
    localpose.bindPose();
    gpurec->_char_animinst->applyToPose(localpose);
    // gpurec->_char_animinst2->applyToPose(localpose);
    // gpurec->_char_animinst3->applyToPose(localpose);
    localpose.blendPoses();
    // auto lpdump = localpose.dump();
    // printf( "%s\n", lpdump.c_str() );

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

    //_char_animinst2 = std::make_shared<XgmAnimInst>();
    //_char_animinst2->bindAnim(anim);
    //_char_animinst2->SetWeight(0.5);
    //_char_animinst2->RefMask().DisableAll();
    //_char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightShoulder");
    //_char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightArm");
    //_char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightForeArm");
    //_char_animinst2->RefMask().Enable(model->mSkeleton, "mixamorig.RightHand");
    //_char_animinst2->_use_temporal_lerp = true;
    //_char_animinst2->bindToSkeleton(model->mSkeleton);

    //_char_animinst3 = std::make_shared<XgmAnimInst>();
    //_char_animinst3->bindAnim(anim);
    //_char_animinst3->SetWeight(0.5);
    //_char_animinst3->RefMask().DisableAll();
    //_char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightShoulder");
    //_char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightArm");
    //_char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightForeArm");
    //_char_animinst3->RefMask().Enable(model->mSkeleton, "mixamorig.RightHand");
    //_char_animinst3->_use_temporal_lerp = true;
    //_char_animinst3->bindToSkeleton(model->mSkeleton);

*/