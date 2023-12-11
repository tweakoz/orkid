
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

      _ikchain = std::make_shared<IkChain>(_model->_skeleton);
      _ikchain->bindToJointNamed("Bone");
      _ikchain->bindToJointNamed("Bone.001");
      _ikchain->bindToJointNamed("Bone.002");
      _ikchain->bindToJointNamed("Bone.003");
      _ikchain->bindToJointNamed("Bone.004");
      _ikchain->prepare();

      auto localpose = modelinst->_localPose;
      auto worldpose = modelinst->_worldPose;

      localpose->bindPose();
      _char_animinst->_current_frame = 0;
      _char_animinst->applyToPose(localpose);
      localpose->blendPoses();
      localpose->concatenate();
      worldpose->apply(fmtx4(), localpose);


      auto drw = std::make_shared<CallbackDrawable>(nullptr);
      drw->SetRenderCallback([this](lev2::RenderContextInstData& RCID) { //
        auto context                      = RCID.context();
        static pbrmaterial_ptr_t material = default3DMaterial(context);
        material->_variant                = "vertexcolor"_crcu;
        auto fxcache                      = material->pipelineCache();
        OrkAssert(fxcache);
        RenderContextInstData RCIDCOPY = RCID;
        RCIDCOPY._isSkinned            = false;
        RCIDCOPY._pipeline_cache    = fxcache;
        _gpurec->drawTarget(RCIDCOPY,_target);
      });
      _dbgdraw_node = gpurec->_sg_layer->createDrawableNode("skdebugnode", drw);



      _timer.Start();
    }

    /////////////////////////////////////////////////////////////////////////////

    xgmanimassetptr_t _char_animasset;   // retain anim
    xgmmodelassetptr_t _char_modelasset; // retain model
    GpuResources* _gpurec = nullptr;
    xgmmodel_ptr_t _model    = nullptr;
    model_drawable_ptr_t _char_drawable;
    lev2::xgmaniminst_ptr_t _char_animinst;
    scenegraph::node_ptr_t _char_node;
    scenegraph::node_ptr_t _dbgdraw_node;
    ork::Timer _timer;
    ikchain_ptr_t _ikchain;

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
    float scale = 1.0f;
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

    ///////////////////////////////////////////////////////////
    // apply base animation
    ///////////////////////////////////////////////////////////

    auto modelinst  = impl->_char_drawable->_modelinst;
    auto localpose = modelinst->_localPose;
    localpose->bindPose();
    localpose->blendPoses();
    localpose->concatenate();

    ///////////////////////////////////////////////////////////

    float time = impl->_timer.SecsSinceStart();
    float rot_time = time * gpurec->_animspeed;
    float sca_time = time * gpurec->_controller3;
    float radius   = 8 + sinf(sca_time * 3) * 4;

    ///////////////////////////////////////////////////////////

    impl->_target = fvec3(sinf(rot_time), 1, -cosf(rot_time)) * radius;
    impl->_ikchain->compute(localpose,impl->_target);

  };

  /////////////////////////////////////////////////////////////////////////////

  return test;
}
