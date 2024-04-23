
skinning_test_ptr_t createTest1(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test1IMPL {

    Test1IMPL(GpuResources* gpurec)
        : _gpurec(gpurec) {

      auto model_load_req         = std::make_shared<asset::LoadRequest>();
      auto anim_load_req          = std::make_shared<asset::LoadRequest>();
      model_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-mesh";
      anim_load_req->_asset_path  = "data://tests/blender-rigtest/blender-rigtest-anim1";

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

      auto node_name = FormatString("test1-node<%p>", (void*)this);

      _char_node = gpurec->_sg_layer->createDrawableNode(node_name, _char_drawable);

      auto modelinst = _char_drawable->_modelinst;
      // modelinst->setBlenderZup(true);
      modelinst->enableSkinning();
      modelinst->enableAllMeshes();
      modelinst->_drawSkeleton = true;

      auto anim      = _char_animasset->_animation;
      _char_animinst = std::make_shared<XgmAnimInst>();
      _char_animinst->bindAnim(anim);
      _char_animinst->SetWeight(1.0f);
      _char_animinst->_mask->EnableAll();
      _char_animinst->_use_temporal_lerp = true;
      _char_animinst->bindToSkeleton(model->_skeleton);

      auto localpose = modelinst->_localPose;
      auto worldpose = modelinst->_worldPose;

      localpose->bindPose();
      _char_animinst->_current_frame = 0;
      _char_animinst->applyToPose(localpose);
      localpose->blendPoses();
      localpose->concatenate();
      worldpose->apply(fmtx4(), localpose);

      _skel_applicator = std::make_shared<XgmSkelApplicator>(model->_skeleton);
      _skel_applicator->bindToBone("Bone.001");
      _skel_applicator->bindToBone("Bone.002");
      _skel_applicator->bindToBone("Bone.003");

      _timer.Start();
    }

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    xgmskelapplicator_ptr_t _skel_applicator;
    GpuResources* _gpurec = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    ork::Timer _timer;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->_impl.makeShared<Test1IMPL>(gpurec);

  /////////////////////////////////////////////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1IMPL>();
    impl->_char_node->_enabled = true;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1IMPL>();
    impl->_char_node->_enabled = false;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test1IMPL>();

    ///////////////////////////
    // set model base transformation
    ///////////////////////////

    fvec3 position(0, 0, 0);
    fquat orientation;
    float scale = 6.0f;
    impl->_char_node->_dqxfdata._worldTransform->set(position, orientation, scale);
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onDraw = [test]() {
    auto impl   = test->_impl.getShared<Test1IMPL>();
    auto gpurec = impl->_gpurec;
    gpurec->_pbrcommon->_depthFogDistance = 10000.0f;
    gpurec->_pbrcommon->_depthFogPower    = 1.0f;
    gpurec->_pbrcommon->_skyboxLevel      = 1;
    gpurec->_pbrcommon->_diffuseLevel     = 1;
    gpurec->_pbrcommon->_specularLevel    = 1;

    float time  = impl->_timer.SecsSinceStart();
    float frame = (time * 30.0f * gpurec->_animspeed);

    ///////////////////////////////////////////////////////////
    // apply base animation
    ///////////////////////////////////////////////////////////

    auto anim = impl->_char_animasset->_animation;
    impl->_char_animinst->_current_frame = fmod(frame, float(anim->_numframes));
    impl->_char_animinst->SetWeight(1);
    auto modelinst  = impl->_char_drawable->_modelinst;
    auto localpose = modelinst->_localPose;
    localpose->bindPose();
    impl->_char_animinst->applyToPose(localpose);
    localpose->blendPoses();
    localpose->concatenate();

    ///////////////////////////////////////////////////////////
    // use skel applicator on post concatenated bones
    ///////////////////////////////////////////////////////////

    impl->_skel_applicator->apply([&](int index) {
      fmtx4 rotmtx;
      rotmtx.setRotateY(time*index);
      auto& ci = localpose->_concat_matrices[index];
      ci       = (ci*rotmtx);
    });

  };

  /////////////////////////////////////////////////////////////////////////////

  return test;
}

/*



*/
