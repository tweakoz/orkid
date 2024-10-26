
#pragma once
///////////////////////////////////////////////////////////////////////////////
#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/timer.h>
#include <ork/math/math_types.inl>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
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
#include <ork/lev2/gfx/ikchain.h>
///////////////////////////////////////////////////////////////////////////////
using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets
struct GpuResources;
///////////////////////////////////////////////////////////////////////////////

struct SkinningTest {

  using update_lambda_t = std::function<void(ui::updatedata_ptr_t)>;

  SkinningTest(GpuResources* gpurec) {
    _gpurec = gpurec;
  }

  std::unordered_map<std::string, lev2::xgmanimassetptr_t> _animassets; // retain anims
  lev2::xgmmodelassetptr_t _char_modelasset;                            // retain model
  svar64_t _impl;
  GpuResources* _gpurec = nullptr;

  void_lambda_t onDraw         = [] {};
  void_lambda_t onActivate     = [] {};
  void_lambda_t onDeactivate   = [] {};
  update_lambda_t onUpdate     = [](ui::updatedata_ptr_t) {};
};

using skinning_test_ptr_t = std::shared_ptr<SkinningTest>;

///////////////////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(appinitdata_ptr_t init_data, Context* ctx);

  void onUpdate(ui::updatedata_ptr_t);

  void drawTarget(const RenderContextInstData& RCID,
                  fvec3 target ) const {

      auto RCFD    = RCID.rcfd();
      auto context = RCFD->_target;
      auto fxcache = RCID._pipeline_cache;

      auto pipeline = fxcache->findPipeline(RCID);
      OrkAssert(pipeline);

      using vertex_t = SVtxV12N12B12T8C4;

      VtxWriter<vertex_t> vw;
      auto add_vertex = [&](const fvec3 pos, const fvec3& col) {
        vertex_t hvtx;
        hvtx._position = pos;
        hvtx._color    = (col * 5).ABGRU32();
        hvtx._uv      = fvec2(0, 0);
        hvtx._normal   = fvec3(0, 0, 0);
        hvtx._binormal = fvec3(1, 1, 0);
        vw.AddVertex(hvtx);
      };

      auto& vtxbuf   = GfxEnv::GetSharedDynamicVB2();
      int inumpoints = 6;
      vw.Lock(context, &vtxbuf, inumpoints);
      // printf( "target<%g %g %g>\n", target.x, target.y, target.z );
      add_vertex(target - fvec3(-1, 0, 0), fvec3(1, 1, 1));
      add_vertex(target + fvec3(-1, 0, 0), fvec3(1, 1, 1));
      add_vertex(target - fvec3(0, -1, 0), fvec3(1, 1, 1));
      add_vertex(target + fvec3(0, -1, 0), fvec3(1, 1, 1));
      add_vertex(target - fvec3(0, 0, -1), fvec3(1, 1, 1));
      add_vertex(target + fvec3(0, 0, -1), fvec3(1, 1, 1));
      vw.UnLock(context);

      context->PushModColor(fvec4::White());
      context->MTXI()->PushMMatrix(fmtx4::Identity());
      pipeline->wrappedDrawCall(RCID, [&]() { context->GBI()->DrawPrimitiveEML(vw, PrimitiveType::LINES); });
      context->MTXI()->PopMMatrix();
      context->PopModColor();
    };

  lev2::rtbuffer_ptr_t _rtbuffer;
  lev2::rtgroup_ptr_t _rtgroup;
  lev2::ezuicam_ptr_t _uicamera;
  varmap::varmap_ptr_t _sg_params;
  scenegraph::scene_ptr_t _sg_scene;
  scenegraph::layer_ptr_t _sg_layer;
  lev2::pbr::commonstuff_ptr_t _pbrcommon;

  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;

  skinning_test_ptr_t _sktests[8];
  skinning_test_ptr_t _active_test;
  float _animspeed = 1.0f;
  float _controller1 = 1.0f;
  float _controller2 = 1.0f;
  float _controller3 = 1.0f;
  float _controller4 = 1.0f;


};

///////////////////////////////////////////////////////////////////////////////

#include "test0.inl"
#include "test1.inl"
#include "test1a.inl"
#include "test1b.inl"
#include "test1c.inl"
#include "test2.inl"
#include "test2b.inl"
#include "test3.inl"

///////////////////////////////////////////////////////////////////////////////

inline GpuResources::GpuResources(
    appinitdata_ptr_t init_data, //
    Context* ctx) {              //

  auto& vars = *init_data->_commandline_vars;

  bool use_forward = vars["forward"].as<bool>();

  //////////////////////////////////////////////////////////
  int testnum = vars["testnum"].as<int>();
  //////////////////////////////////////////////////////////

  _camlut                = std::make_shared<CameraDataLut>();
  _camdata               = std::make_shared<CameraData>();
  (*_camlut)["spawncam"] = _camdata;

  //////////////////////////////////////////////
  // create scenegraph
  //////////////////////////////////////////////

  _rtgroup                                                      = std::make_shared<lev2::RtGroup>(ctx, 1, 1, MsaaSamples::MSAA_1X);
  _rtbuffer                                                     = _rtgroup->createRenderTarget(EBufferFormat::RGBA8);
  _sg_params                                                    = std::make_shared<varmap::VarMap>();
  _sg_params->makeValueForKey<std::string>("preset")            = use_forward ? "ForwardPBR" : "DeferredPBR";
  _sg_params->makeValueForKey<lev2::rtgroup_ptr_t>("outputRTG") = _rtgroup;

  _sg_scene        = std::make_shared<scenegraph::Scene>(_sg_params);
  _sg_layer    = _sg_scene->createLayer("default");
  auto sg_compdata = _sg_scene->_compositorData;
  auto nodetek     = sg_compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
  auto rendnode    = nodetek->tryRenderNodeAs<ork::lev2::pbr::deferrednode::DeferredCompositingNodePbr>();
  _pbrcommon   = rendnode->_pbrcommon;

  _pbrcommon->_depthFogDistance = 10000.0f;
  _pbrcommon->_depthFogPower    = 1.0f;
  _pbrcommon->_skyboxLevel      = 1;
  _pbrcommon->_diffuseLevel     = 1;
  _pbrcommon->_specularLevel    = 1;

  auto outpnode = nodetek->tryOutputNodeAs<ScreenOutputCompositingNode>();

  //////////////////////////////////////////////////////////

  ctx->debugPushGroup("main.onGpuInit");

  /*auto mesh_override = vars["mesh"].as<std::string>();
  auto anim_override = vars["anim"].as<std::string>();

  if (mesh_override.length()) {
    model_load_req->_asset_path = mesh_override;
  }
  if (anim_override.length()) {
    anim_load_req->_asset_path = anim_override;
  }*/

  //////////////////////////////////////////////
  // scenegraph nodes
  //////////////////////////////////////////////

  _uicamera                 = std::make_shared<EzUiCam>();
  _uicamera->_constrainZ    = true;
  _uicamera->_base_zmoveamt = 2.0f;
  _uicamera->mfLoc          = 25.0f;
  ctx->debugPopGroup();

  _sktests[0] = createTest0(this);
  _sktests[1] = createTest1(this);
  _sktests[2] = createTest1A(this);
  _sktests[3] = createTest1B(this);
  _sktests[4] = createTest1C(this);
  _sktests[5] = createTest2(this);
  _sktests[6] = createTest2B(this);
  _sktests[7] = createTest3(this);

  _active_test = _sktests[6];

}

///////////////////////////////////////////////////////////////////////////////

inline void GpuResources::onUpdate(ui::updatedata_ptr_t updata){

  for( auto item : _sktests ){
    if(item == _active_test ){
      item->onActivate();
    }
    else {
      item->onDeactivate();
    }
  }
  _active_test->onUpdate(updata);
}
