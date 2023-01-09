
skinning_test_ptr_t createTest1C(GpuResources* gpurec) {

  using namespace lev2;

  auto test = std::make_shared<SkinningTest>(gpurec);

  //////////////////////////////////////

  struct Test1CIMPL {

    Test1CIMPL(GpuResources* gpurec)
        : _gpurec(gpurec) {

      auto model_load_req         = std::make_shared<asset::LoadRequest>();
      auto anim_load_req          = std::make_shared<asset::LoadRequest>();
      model_load_req->_asset_path = "data://tests/blender-rigtest/blender-rigtest-mesh";
      anim_load_req->_asset_path  = "data://tests/blender-rigtest/blender-rigtest-anim1";

      _char_modelasset = asset::AssetManager<XgmModelAsset>::load(model_load_req);
      OrkAssert(_char_modelasset);
      model_load_req->waitForCompletion();

      _model        = _char_modelasset->getSharedModel();
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

      auto drw = std::make_shared<CallbackDrawable>(nullptr);
      drw->SetRenderCallback([this](lev2::RenderContextInstData& RCID) { //
        auto context                      = RCID.context();
        static pbrmaterial_ptr_t material = default3DMaterial(context);
        material->_variant                = "vertexcolor"_crcu;
        auto fxcache = material->fxInstanceCache();
        OrkAssert(fxcache);
        RenderContextInstData RCIDCOPY = RCID;
        RCIDCOPY._isSkinned            = false;
        RCIDCOPY._fx_instance_cache    = fxcache;
        this->onDebugDraw(RCIDCOPY);
      });
      _dbgdraw_node = gpurec->_sg_layer->createDrawableNode("skdebugnode", drw);

      _timer.Start();
    }

    /////////////////////////////////////////////////////////////////////////////

    void onDebugDraw(const RenderContextInstData& RCID) const {

      auto RCFD    = RCID._RCFD;
      auto context = RCFD->_target;
      auto fxcache = RCID._fx_instance_cache;

      auto fxinst = fxcache->findfxinst(RCID);
      OrkAssert(fxinst);

      using vertex_t = SVtxV12N12B12T8C4;

      VtxWriter<vertex_t> vw;
      auto add_vertex = [&](const fvec3 pos, const fvec3& col) {
        vertex_t hvtx;
        hvtx.mPosition = pos;
        hvtx.mColor    = (col*5).ABGRU32();
        hvtx.mUV0      = fvec2(0, 0);
        hvtx.mNormal   = fvec3(0, 0, 0);
        hvtx.mBiNormal = fvec3(1, 1, 0);
        vw.AddVertex(hvtx);
      };

      auto& vtxbuf   = GfxEnv::GetSharedDynamicVB2();
      int inumpoints = 6;
      vw.Lock(context, &vtxbuf, inumpoints);
      printf( "_target<%g %g %g>\n", _target.x, _target.y, _target.z );
      add_vertex(_target - fvec3(-1, 0, 0), fvec3(1, 1, 1));
      add_vertex(_target + fvec3(-1, 0, 0), fvec3(1, 1, 1));
      add_vertex(_target - fvec3(0, -1, 0), fvec3(1, 1, 1));
      add_vertex(_target + fvec3(0, -1, 0), fvec3(1, 1, 1));
      add_vertex(_target - fvec3(0, 0, -1), fvec3(1, 1, 1));
      add_vertex(_target + fvec3(0, 0, -1), fvec3(1, 1, 1));
      vw.UnLock(context);

      context->PushModColor(fvec4::White());
      context->MTXI()->PushMMatrix(fmtx4::Identity());
      fxinst->wrappedDrawCall(RCID, [&]() { context->GBI()->DrawPrimitiveEML(vw, PrimitiveType::LINES); });
      context->MTXI()->PopMMatrix();
      context->PopModColor();
    };

    /////////////////////////////////////////////////////////////////////////////

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    xgmskelapplicator_ptr_t _skel_applicator;
    GpuResources* _gpurec = nullptr;
    model_ptr_t _model    = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    scenegraph::node_ptr_t _dbgdraw_node;
    ork::Timer _timer;

    fvec3 _target;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->_impl.makeShared<Test1CIMPL>(gpurec);

  /////////////////////////////////////////////////////////////////////////////

  test->onActivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1CIMPL>();
    impl->_char_node->_enabled = true;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onDeactivate = [test]() {
    auto impl                  = test->_impl.getShared<Test1CIMPL>();
    impl->_char_node->_enabled = false;
  };

  /////////////////////////////////////////////////////////////////////////////

  test->onUpdate = [test](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime + dt + .016;

    auto impl = test->_impl.getShared<Test1CIMPL>();

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
    auto impl                             = test->_impl.getShared<Test1CIMPL>();
    auto gpurec                           = impl->_gpurec;
    gpurec->_pbrcommon->_depthFogDistance = 4000.0f;
    gpurec->_pbrcommon->_depthFogPower    = 5.0f;
    gpurec->_pbrcommon->_skyboxLevel      = 1;
    gpurec->_pbrcommon->_diffuseLevel     = 1;
    gpurec->_pbrcommon->_specularLevel    = 1;

    float time = impl->_timer.SecsSinceStart();

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
    XgmLocalPose BINDPOSE(skel);
    BINDPOSE.bindPose();
    BINDPOSE.concatenate();

    int jntindices[5];
    int inv_jntindices[5];

    jntindices[0] = skel.jointIndex("Bone");
    jntindices[1] = skel.jointIndex("Bone.001");
    jntindices[2] = skel.jointIndex("Bone.002");
    jntindices[3] = skel.jointIndex("Bone.003");
    jntindices[4] = skel.jointIndex("Bone.004");

    inv_jntindices[skel.jointIndex("Bone")]     = 0;
    inv_jntindices[skel.jointIndex("Bone.001")] = 1;
    inv_jntindices[skel.jointIndex("Bone.002")] = 2;
    inv_jntindices[skel.jointIndex("Bone.003")] = 3;
    inv_jntindices[skel.jointIndex("Bone.004")] = 4;

    auto len = [&](int ia, int ib) -> float {      //
      return (BINDPOSE._concat_matrices[ia].translation() //
              - BINDPOSE._concat_matrices[ib].translation())
          .length();
    };

    float lens[5];

    lens[0] = len(jntindices[0], jntindices[1]);
    lens[1] = len(jntindices[1], jntindices[2]);
    lens[2] = len(jntindices[2], jntindices[3]);
    lens[3] = len(jntindices[3], jntindices[4]);
    lens[4] = 1.0f;


    skel.mTopNodesMatrix.dump("TOP");

    for (int i = 0; i < 5; i++) {
      printf("BONELEN<%d> : %g\n", i, lens[i]);
    }

    ///////////////////////////////////////////////////////////

    fmtx4 pN[6];

    pN[0] = localpose._concat_matrices[jntindices[0]];
    pN[1] = localpose._concat_matrices[jntindices[1]];
    pN[2] = localpose._concat_matrices[jntindices[2]];
    pN[3] = localpose._concat_matrices[jntindices[3]];
    pN[4] = localpose._concat_matrices[jntindices[4]];

    impl->_target = fvec3(sinf(time), 1, -cosf(time))*25.0;

    ///////////////////////////////////////////////////////////
    // fill in pose
    ///////////////////////////////////////////////////////////

    pN[5].setTranslation(impl->_target); // set end of chain to target

    for (int outer_loop = 0; outer_loop < 4; outer_loop++) {

      for (int i = 4; i >= 0; i--) {

        auto piv    = pN[i].translation();
        auto tgt    = pN[i + 1].translation();
        auto ori    = fvec3(0, 1, 0).transform(pN[i]).normalized();
        auto del    = (tgt - piv).normalized();
        auto cross  = ori.crossWith(del).normalized();
        float theta = acos(del.dotWith(ori));

        fquat Q(cross, theta);

        auto piv_t = pN[i].translationOnly();

        fmtx4 R = piv_t * fmtx4(Q) * piv_t.inverse();

        printf( "piv<%d> <%g %g %g>\n", i, piv.x, piv.y, piv.z);

        pN[i] = R * pN[i];


        pN[i].dump(FormatString("PN[%d]",i));

      }
    }

    for (int i = 0; i < 5; i++) {
      localpose._concat_matrices[jntindices[i]] = pN[i];
    }
  };

  /////////////////////////////////////////////////////////////////////////////

  return test;
}

