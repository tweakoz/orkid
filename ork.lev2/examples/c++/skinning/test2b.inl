
skinning_test_ptr_t createTest2B(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test2BIMPL {

    Test2BIMPL(GpuResources* gpurec)
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

      auto node_name = FormatString("test2-node<%p>", (void*)this);

      _char_node = gpurec->_sg_layer->createDrawableNode(node_name, _char_drawable);

      auto modelinst = _char_drawable->_modelinst;
      modelinst->enableSkinning();
      modelinst->enableAllMeshes();
      modelinst->_drawSkeleton = true;

      auto anim      = _char_animasset->GetAnim();
      _char_animinst = std::make_shared<XgmAnimInst>();
      _char_animinst->bindAnim(anim);
      _char_animinst->SetWeight(1.0f);
      _char_animinst->RefMask().EnableAll();
      _char_animinst->_use_temporal_lerp = true;
      _char_animinst->bindToSkeleton(model->mSkeleton);

      ///////////////////////////////////////////////////////////////
      // default pose
      ///////////////////////////////////////////////////////////////

      auto& localpose = modelinst->_localPose;
      auto& worldpose = modelinst->_worldPose;

      localpose.bindPose();
      _char_animinst->_current_frame = 0;
      _char_animinst->applyToPose(localpose);
      localpose.blendPoses();
      localpose.concatenate();
      worldpose.apply(fmtx4(), localpose);

      ///////////////////////////////////////////////////////////////
      // create IK chain
      ///////////////////////////////////////////////////////////////

      _ikchain = std::make_shared<IkChain>(model->mSkeleton);
      _ikchain->bindToBone("mixamorig.RightArm");
      _ikchain->bindToBone("mixamorig.RightForeArm");
      //_ikchain->bindToBone("mixamorig.RightHand");
      _ikchain->prepare();

      ///////////////////////////////////////////////////////////////

      _timer.Start();
    }

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    GpuResources* _gpurec = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    ikchain_ptr_t _ikchain;

    ork::Timer _timer;
  };

  //////////////////////////////////////

  test->_impl.makeShared<Test2BIMPL>(gpurec);

  //////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test2BIMPL>();
    impl->_char_node->_enabled = true;
  };
  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test2BIMPL>();
    impl->_char_node->_enabled = false;
  };

  //////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test2BIMPL>();

    fvec3 position(0, 0, 0);
    fquat orientation;
    float scale = 1.0f;
    impl->_char_node->_dqxfdata._worldTransform->set(position, orientation, scale);
  };

  //////////////////////////////////////

  test->onDraw = [test]() {
    auto impl   = test->_impl.getShared<Test2BIMPL>();
    auto gpurec = impl->_gpurec;
    gpurec->_pbrcommon->_depthFogDistance = 4000.0f;
    gpurec->_pbrcommon->_depthFogPower    = 5.0f;
    gpurec->_pbrcommon->_skyboxLevel      = 0.25;
    gpurec->_pbrcommon->_diffuseLevel     = 0.2;
    gpurec->_pbrcommon->_specularLevel    = 3.2;

    ///////////////////////////////////////////////////////////
    // use skel applicator on post concatenated bones
    ///////////////////////////////////////////////////////////

    float time  = impl->_timer.SecsSinceStart();
    float frame = (time * 30.0f * gpurec->_animspeed);

    auto anim = impl->_char_animasset->GetAnim();

    impl->_char_animinst->_current_frame = fmod(frame, float(anim->_numframes));
    impl->_char_animinst->SetWeight(0.5f);

    auto modelinst  = impl->_char_drawable->_modelinst;
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    impl->_char_animinst->applyToPose(localpose);
    localpose.blendPoses();
    localpose.concatenate();

    /////////////////////////////////
    // perform IK
    /////////////////////////////////

    auto model = impl->_char_modelasset->getSharedModel();
    int hjoint = model->mSkeleton.jointIndex("mixamorig.RightHand");
    fmtx4 hmtx = localpose._concat_matrices[hjoint];
    fvec3 target = hmtx.translation();

    impl->_ikchain->compute(localpose,target);


  };

  //////////////////////////////////////

  return test;
}