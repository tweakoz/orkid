
skinning_test_ptr_t createTest2(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test2IMPL {

    Test2IMPL(GpuResources* gpurec)
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

      auto node_name = FormatString("test2-node<%p>", (void*)this);

      _char_node = gpurec->_sg_layer->createDrawableNode(node_name, _char_drawable);

      auto modelinst = _char_drawable->_modelinst;
      // modelinst->setBlenderZup(true);
      modelinst->enableSkinning();
      modelinst->enableAllMeshes();
      modelinst->_drawSkeleton = true;

      // model->_skeleton->mTopNodesMatrix.compose(fvec3(),fquat(),0.0001);

      auto anim      = _char_animasset->_animation;
      _char_animinst = std::make_shared<XgmAnimInst>();
      _char_animinst->bindAnim(anim);
      _char_animinst->SetWeight(1.0f);
      _char_animinst->_mask->EnableAll();
      _char_animinst->_use_temporal_lerp = true;
      _char_animinst->bindToSkeleton(model->_skeleton);

      auto& localpose = modelinst->_localPose;
      auto& worldpose = modelinst->_worldPose;

      localpose.bindPose();
      _char_animinst->_current_frame = 0;
      _char_animinst->applyToPose(localpose);
      localpose.blendPoses();
      localpose.concatenate();
      worldpose.apply(fmtx4(), localpose);
      // OrkAssert(false);

      auto rarm  = model->_skeleton->bindMatrixByName("mixamorig.RightArm");
      auto rfarm = model->_skeleton->bindMatrixByName("mixamorig.RightForeArm");
      auto rhand = model->_skeleton->bindMatrixByName("mixamorig.RightHand");

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

      _timer.Start();
    }

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    GpuResources* _gpurec = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    float _rarm_len    = 0.0f;
    float _rfarm_len   = 0.0f;
    float _rarm_scale  = 0.0f;
    float _rfarm_scale = 0.0f;
    ork::Timer _timer;
  };

  //////////////////////////////////////

  test->_impl.makeShared<Test2IMPL>(gpurec);

  //////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test2IMPL>();
    impl->_char_node->_enabled = true;
  };
  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test2IMPL>();
    impl->_char_node->_enabled = false;
  };

  //////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test2IMPL>();

    fvec3 position(0, 0, 0);
    fquat orientation;
    float scale = 1.0f;
    impl->_char_node->_dqxfdata._worldTransform->set(position, orientation, scale);
  };

  test->onDraw = [test]() {
    auto impl   = test->_impl.getShared<Test2IMPL>();
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

    auto anim = impl->_char_animasset->_animation;

    impl->_char_animinst->_current_frame = fmod(frame, float(anim->_numframes));
    impl->_char_animinst->SetWeight(0.5f);

    auto modelinst  = impl->_char_drawable->_modelinst;
    auto& localpose = modelinst->_localPose;
    auto& worldpose = modelinst->_worldPose;

    localpose.bindPose();
    impl->_char_animinst->applyToPose(localpose);
    localpose.blendPoses();
    localpose.concatenate();

    auto model = impl->_char_modelasset->getSharedModel();
    auto& skel = model->skeleton();

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

    auto elbow_pos    = rhand.translation() - (wrist_ynormal * impl->_rfarm_len);
    auto elbow_normal = (elbow_pos - rshoulder.translation()).normalized();

    fmtx4 elbowR, elbowS, elbowT;
    elbowR.fromNormalVectors(
        wrist_xnormal,  //
        -wrist_ynormal, //
        wrist_znormal);
    elbowS.setScale(impl->_rfarm_scale);
    elbowT.setTranslation(elbow_pos);

    rfarm = elbowT * (elbowR * elbowS);

    fmtx4 MM, MS;
    MM.correctionMatrix(rshoulder, rfarm);
    MS.setScale(impl->_rarm_scale);

    ///////////////////////

    // localpose._concat_matrices[ji_rfarm] = (MS*MM)*rshoulder;
    // localpose._concat_matrices[ji_rfarm] = rhand;
  };

  //////////////////////////////////////

  return test;
}