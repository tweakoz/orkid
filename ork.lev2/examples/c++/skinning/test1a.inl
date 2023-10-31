
skinning_test_ptr_t createTest1A(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test1AIMPL {

    Test1AIMPL(GpuResources* gpurec)
        : _gpurec(gpurec) {

      auto model_load_req         = std::make_shared<asset::LoadRequest>();
      auto anim_load_req          = std::make_shared<asset::LoadRequest>();
      model_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-mesh";
      anim_load_req->_asset_path  = "data://tests/blender-rigtest/blender-rigtest-anim1";

      _char_modelasset = asset::AssetManager<XgmModelAsset>::load(model_load_req);
      OrkAssert(_char_modelasset);
      model_load_req->waitForCompletion();

      _model    = _char_modelasset->getSharedModel();
      auto skeldump = _model->_skeleton->dump(fvec3(1, 1, 1));
      printf("skeldump<%s>\n", skeldump.c_str());

      _char_animasset = asset::AssetManager<XgmAnimAsset>::load(anim_load_req);
      OrkAssert(_char_animasset);
      anim_load_req->waitForCompletion();

      _char_drawable = std::make_shared<ModelDrawable>();
      _char_drawable->bindModel(_model);
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
      _char_animinst->bindToSkeleton(_model->_skeleton);

      auto localpose = modelinst->_localPose;
      auto worldpose = modelinst->_worldPose;

      localpose->bindPose();
      _char_animinst->_current_frame = 0;
      _char_animinst->applyToPose(localpose);
      localpose->blendPoses();
      localpose->concatenate();
      worldpose->apply(fmtx4(), localpose);

      _skel_applicator = std::make_shared<XgmSkelApplicator>(_model->_skeleton);
      //_skel_applicator->bindToBone("Bone");
      _skel_applicator->bindToBone("Bone.001");
      _skel_applicator->bindToBone("Bone.002");
      _skel_applicator->bindToBone("Bone.003");
      _skel_applicator->bindToBone("Bone.004");
      _skel_applicator->bindToBone("Bone.004.end");

      _timer.Start();
    }

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    xgmskelapplicator_ptr_t _skel_applicator;
    GpuResources* _gpurec = nullptr;
    xgmmodel_ptr_t _model = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    ork::Timer _timer;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->_impl.makeShared<Test1AIMPL>(gpurec);

  /////////////////////////////////////////////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1AIMPL>();
    impl->_char_node->_enabled = true;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1AIMPL>();
    impl->_char_node->_enabled = false;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test1AIMPL>();

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
    auto impl   = test->_impl.getShared<Test1AIMPL>();
    auto gpurec = impl->_gpurec;
    gpurec->_pbrcommon->_depthFogDistance = 4000.0f;
    gpurec->_pbrcommon->_depthFogPower    = 5.0f;
    gpurec->_pbrcommon->_skyboxLevel      = 1;
    gpurec->_pbrcommon->_diffuseLevel     = 1;
    gpurec->_pbrcommon->_specularLevel    = 1;

    float time  = impl->_timer.SecsSinceStart();

    ///////////////////////////////////////////////////////////
    // apply base animation
    ///////////////////////////////////////////////////////////

    auto modelinst  = impl->_char_drawable->_modelinst;
    auto localpose = modelinst->_localPose;
    localpose->bindPose();
    localpose->blendPoses();
    localpose->concatenate();

    ///////////////////////////////////////////////////////////
    // compute bone lengths
    ///////////////////////////////////////////////////////////

    auto skel = impl->_model->_skeleton;

    int ib0 = skel->jointIndex("Bone");
    int ib1 = skel->jointIndex("Bone.001");
    int ib2 = skel->jointIndex("Bone.002");
    int ib3 = skel->jointIndex("Bone.003");
    int ib4 = skel->jointIndex("Bone.004");

    auto len = [&](int ia, int ib) -> float{ //
        return (skel->_bindMatrices[ia].translation() //
              - skel->_bindMatrices[ib].translation()).length();
    };

    auto l0 = len(ib0,ib1);
    auto l1 = len(ib1,ib2);
    auto l2 = len(ib2,ib3);
    auto l3 = len(ib3,ib4);
    auto l4 = 1.0f;

    auto target = fvec3(sinf(time),sinf(time*0.25),-cosf(time)).normalized();

    ///////////////////////////////////////////////////////////
    // fill in pose
    ///////////////////////////////////////////////////////////

    fmtx4 myz;
    myz.rotateOnX(-90*DTOR);

    fmtx4 mtx;

    mtx.lookAt(fvec3(0,0,0),target*l0,fvec3(0,1,0));
    localpose->_concat_matrices[ib0] = mtx.inverse() * myz;
      
    mtx.lookAt(target*l0,target*(l0+l1),fvec3(0,1,0));
    localpose->_concat_matrices[ib1] = mtx.inverse() * myz;

    mtx.lookAt(target*(l0+l1),target*(l0+l1+l2),fvec3(0,1,0));
    localpose->_concat_matrices[ib2] = mtx.inverse() * myz;

    mtx.lookAt(target*(l0+l1+l2),target*(l0+l1+l2+l3),fvec3(0,1,0));
    localpose->_concat_matrices[ib3] = mtx.inverse() * myz;

    mtx.lookAt(target*(l0+l1+l2+l3),target*(l0+l1+l2+l3+l4),fvec3(0,1,0));
    localpose->_concat_matrices[ib4] = mtx.inverse() * myz;
  };

  /////////////////////////////////////////////////////////////////////////////

  return test;
}

/*



*/
