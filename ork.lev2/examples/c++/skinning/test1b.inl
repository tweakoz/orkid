
skinning_test_ptr_t createTest1B(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test1BIMPL {

    Test1BIMPL(GpuResources* gpurec)
        : _gpurec(gpurec) {

      auto model_load_req         = std::make_shared<asset::LoadRequest>();
      auto anim_load_req          = std::make_shared<asset::LoadRequest>();
      model_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-mesh";
      anim_load_req->_asset_path  = "data://tests/blender-rigtest/blender-rigtest-anim1";

      _char_modelasset = asset::AssetManager<XgmModelAsset>::load(model_load_req);
      OrkAssert(_char_modelasset);
      model_load_req->waitForCompletion();

      _model    = _char_modelasset->getSharedModel();
      auto skeldump = _model->mSkeleton.dump(fvec3(1, 1, 1));
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

      auto anim      = _char_animasset->GetAnim();
      _char_animinst = std::make_shared<XgmAnimInst>();
      _char_animinst->bindAnim(anim);
      _char_animinst->SetWeight(1.0f);
      _char_animinst->RefMask().EnableAll();
      _char_animinst->_use_temporal_lerp = true;
      _char_animinst->bindToSkeleton(_model->mSkeleton);

      auto& localpose = modelinst->_localPose;
      auto& worldpose = modelinst->_worldPose;

      localpose.bindPose();
      _char_animinst->_current_frame = 0;
      _char_animinst->applyToPose(localpose);
      localpose.blendPoses();
      localpose.concatenate();
      worldpose.apply(fmtx4(), localpose);

      _skel_applicator = std::make_shared<XgmSkelApplicator>(_model->mSkeleton);
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
    model_ptr_t _model = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    ork::Timer _timer;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->_impl.makeShared<Test1BIMPL>(gpurec);

  /////////////////////////////////////////////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1BIMPL>();
    impl->_char_node->_enabled = true;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1BIMPL>();
    impl->_char_node->_enabled = false;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test1BIMPL>();

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
    auto impl   = test->_impl.getShared<Test1BIMPL>();
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
    auto& localpose = modelinst->_localPose;
    localpose.bindPose();
    localpose.blendPoses();
    localpose.concatenate();

    ///////////////////////////////////////////////////////////
    // compute bone lengths
    ///////////////////////////////////////////////////////////

    const auto& skel = impl->_model->mSkeleton;

    int ib0 = skel.jointIndex("Bone");
    int ib1 = skel.jointIndex("Bone.001");
    int ib2 = skel.jointIndex("Bone.002");
    int ib3 = skel.jointIndex("Bone.003");
    int ib4 = skel.jointIndex("Bone.004");

    auto len = [&](int ia, int ib) -> float{ //
        return (skel._bindMatrices[ia].translation() //
              - skel._bindMatrices[ib].translation()).length();
    };

    float lens[5];
    
    lens[0] = len(ib0,ib1);
    lens[1] = len(ib1,ib2);
    lens[2] = len(ib2,ib3);
    lens[3] = len(ib3,ib4);
    lens[4] = 1.0f;

    ///////////////////////////////////////////////////////////

    fvec3 pN[5];


    pN[0] = localpose._concat_matrices[ib0].translation();
    pN[1] = localpose._concat_matrices[ib1].translation();
    pN[2] = localpose._concat_matrices[ib2].translation();
    pN[3] = localpose._concat_matrices[ib3].translation();
    pN[4] = localpose._concat_matrices[ib4].translation();

    kln::point kpN[5];

    kpN[0] = kln::point(pN[0].x,pN[0].y,pN[0].z);
    kpN[1] = kln::point(pN[1].x,pN[1].y,pN[1].z);
    kpN[2] = kln::point(pN[2].x,pN[2].y,pN[2].z);
    kpN[3] = kln::point(pN[3].x,pN[3].y,pN[3].z);
    kpN[4] = kln::point(pN[4].x,pN[4].y,pN[4].z);

    auto target = fvec3(sinf(time),sinf(time*0.25),-cosf(time)).normalized()*10.0;
    auto ktgt = kln::point(target.x,target.y,target.z);

    ///////////////////////////////////////////////////////////
    // fill in pose
    ///////////////////////////////////////////////////////////

    fmtx4 myz;
    myz.rotateOnX(-90*DTOR);


    for( int outer_loop=0; outer_loop<4; outer_loop++){

        kpN[4] = ktgt; // set end of chain to target

        for( int i=3; i>0; i-- ){ // run backwards, restore lengths
            float l = lens[i];
            auto CA = pN[i];
            auto CB = pN[i+1];
            auto KCB = kpN[i+1];
            auto CJ = (CB-CA).normalized();
            kpN[i] = kln::translator(-l,CJ.x,CJ.y,CJ.z)(KCB);
            pN[i] = fvec3(kpN[i].x(),kpN[i].y(),kpN[i].z());
        }

        for( int i=1; i<5; i++ ){ // run forwards, restore lengths again
            float l = lens[i];
            auto CA = pN[i-1];
            auto KCA = kpN[i-1];
            auto CB = pN[i];
            auto CJ = (CB-CA).normalized();
            kpN[i] = kln::translator(-l,CJ.x,CJ.y,CJ.z)(KCA);
            pN[i] = fvec3(kpN[i].x(),kpN[i].y(),kpN[i].z());
        }

    }

    auto do_mtx = [&](int i){
        auto pa = pN[i];
        auto pb = pN[i+1];
        fmtx4 mtx;
        mtx.lookAt(pa, //
                   pb, //
                   fvec3(0,1,0)); //
        localpose._concat_matrices[i] = mtx;//* myz;
    };

    do_mtx(0);
    do_mtx(1);
    do_mtx(2);
    do_mtx(3);
    do_mtx(4);

  };

  /////////////////////////////////////////////////////////////////////////////

  return test;
}

/*



*/
