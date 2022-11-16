////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
///////////////////////////////////////////////////////////////////
// Signed Distance Field Test Scene
///////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/material_freestyle.h>

#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/asset/DynamicAssetLoader.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/imgui_ged.inl>

#include <iostream>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;

const int KSUPERSAMPLE = 1;

///////////////////////////////////////////////////////////////////

struct SdfSubBase : public Object {
  DeclareAbstractX(SdfSubBase, Object);

  virtual ~SdfSubBase() {}

  void gpuInit(Context* ctx ) {

    if( not _needsGpuInit ){
      return;
    }

    _sdf_material.gpuInit(ctx, _shaderpath);

    _tekSDF = _sdf_material.technique("tek_sdfscene");

    _parMatIVP      = _sdf_material.param("IVP");
    _parInvViewSize = _sdf_material.param("InvViewportSize");
    _parMapDepth    = _sdf_material.param("MapDepth");
    _parMapVolTexA  = _sdf_material.param("MapVolTexA");

    _parTime         = _sdf_material.param("Time");
    _parNear         = _sdf_material.param("Near");
    _parFar          = _sdf_material.param("Far");
    _parDisplacement = _sdf_material.param("Displacement");
    _parRepeatPeriod = _sdf_material.param("RepeatPeriod");
    _parNoisePowerA  = _sdf_material.param("NoisePowerA");
    _parNoisePowerB  = _sdf_material.param("NoisePowerB");

    _parColorA     = _sdf_material.param("ColorA");
    _parColorB     = _sdf_material.param("ColorB");
    _parIntensityA = _sdf_material.param("IntensityA");
    _parIntensityB = _sdf_material.param("IntensityB");
    _needsGpuInit = false;
  }

  FreestyleMaterial _sdf_material;

  const FxShaderTechnique* _tekSDF     = nullptr;
  const FxShaderParam* _parMatIVP      = nullptr;
  const FxShaderParam* _parInvViewSize = nullptr;

  const FxShaderParam* _parTime         = nullptr;
  const FxShaderParam* _parNear         = nullptr;
  const FxShaderParam* _parFar          = nullptr;
  const FxShaderParam* _parDisplacement = nullptr;
  const FxShaderParam* _parRepeatPeriod = nullptr;
  const FxShaderParam* _parNoisePowerA  = nullptr;
  const FxShaderParam* _parNoisePowerB  = nullptr;

  const FxShaderParam* _parColorA     = nullptr;
  const FxShaderParam* _parColorB     = nullptr;
  const FxShaderParam* _parIntensityA = nullptr;
  const FxShaderParam* _parIntensityB = nullptr;

  const FxShaderParam* _parMapDepth   = nullptr;
  const FxShaderParam* _parMapVolTexA = nullptr;

  std::string _teststring;
  AssetPath _shaderpath;
  bool _needsGpuInit = true;

};

///////////////////////////////////////////////////////////////////

struct SdfSubOne : public SdfSubBase {
  DeclareConcreteX(SdfSubOne, SdfSubBase);

  SdfSubOne() {
    static int counter = 0;
    _teststring = FormatString("SubOne<%d>", counter++ );
    _shaderpath = "demo://sdf1.glfx";
  }

};

///////////////////////////////////////////////////////////////////

struct SdfSubTwo : public SdfSubBase {
  DeclareConcreteX(SdfSubTwo, SdfSubBase);
  SdfSubTwo() {
    static int counter = 0;
    _teststring = FormatString("SubTwo<%d>", counter++ );
    _shaderpath = "demo://sdf2.glfx";
  }
};

///////////////////////////////////////////////////////////////////

struct SdfSceneData : public Object {

  DeclareConcreteX(SdfSceneData, Object);

  SdfSceneData(){
    _colorA = fvec3(0,1,0);
    _colorB = fvec3(1,0,0);

    _subobject = std::make_shared<SdfSubOne>();
  }
  float _displacement = 0.1;
  float _repeatperiod = 5;
  float _noisepowera = 2;
  float _noisepowerb = 2.5;
  float _intensitya = 2.4;
  float _intensityb = 2;

  fvec3 _colorA;
  fvec3 _colorB;

  object_ptr_t _subobject;

};

///////////////////////////////////////////////////////////////////

ImplementReflectionX(SdfSceneData, "SdfSceneData");
ImplementReflectionX(SdfSubBase, "SdfSubBase");
ImplementReflectionX(SdfSubOne, "SdfSubOne");
ImplementReflectionX(SdfSubTwo, "SdfSubTwo");

///////////////////////////////////////////////////////////////////

void SdfSubBase::describeX(ork::object::ObjectClass* clazz) {
}
void SdfSubOne::describeX(ork::object::ObjectClass* clazz) {
  clazz
      ->directProperty("TestString", &SdfSubOne::_teststring);
}
void SdfSubTwo::describeX(ork::object::ObjectClass* clazz) {
  clazz
      ->directProperty("TestString", &SdfSubTwo::_teststring);
}

///////////////////////////////////////////////////////////////////

void SdfSceneData::describeX(ork::object::ObjectClass* clazz) {

  clazz->floatProperty("Displacement", float_range{0,1}, &SdfSceneData::_displacement);
  clazz->floatProperty("RepeatPeriod", float_range{3,10}, &SdfSceneData::_repeatperiod);
  clazz->floatProperty("NoisePowerA", float_range{.1,5}, &SdfSceneData::_noisepowera);
  clazz->floatProperty("NoisePowerB", float_range{1.5,5}, &SdfSceneData::_noisepowerb);
  clazz->floatProperty("IntensityA", float_range{.1,10}, &SdfSceneData::_intensitya);
  clazz->floatProperty("IntensityB", float_range{.1,10}, &SdfSceneData::_intensityb);

  clazz
      ->directProperty("ColorA", &SdfSceneData::_colorA) //
      ->annotate<ConstString>("editor.type", "color");

  clazz
      ->directProperty("ColorB", &SdfSceneData::_colorB) //
      ->annotate<ConstString>("editor.type", "color");

  clazz
      ->directProperty("Sub", &SdfSceneData::_subobject)
      ->annotate("editor.factory.classbase", SdfSubBase::objectClassStatic() );

  reflect::group_list_ptr_t gl = std::make_shared<reflect::PropGroupList>(); 

  gl->addGroup("main", "Displacement RepeatPeriod Sub");
  gl->addGroup("noise", "NoisePowerA NoisePowerB");
  gl->addGroup("color", "ColorA IntensityA ColorB IntensityB");

  clazz->annotate("editor.groups",gl);
}

///////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(Context* ctx, file::Path this_dir) {
    _renderer = std::make_shared<DefaultRenderer>();
    _renderer->setContext(ctx);
    _lightmgr = std::make_shared<LightManager>(_lmd);

    _camlut  = std::make_shared<CameraDataLut>();
    _camdata = _camlut->create("spawncam");

    auto filecontext = FileEnv::createContextForUriBase("demo://", this_dir);
    filecontext->SetFilesystemBaseEnable(true);

    _sdf_drawable = std::make_shared<CallbackDrawable>(nullptr);

    _voltexA = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/voltex_pn2");

    //////////////////////////////////////////////////////////
    // initialize compositor (necessary for PBR models)
    //  use a deferredPBR compositing node
    //  which does all the gbuffer and lighting passes
    //////////////////////////////////////////////////////////

    _compositordata = std::make_shared<CompositingData>();
    _compositordata->presetDeferredPBR();
    _compositordata->mbEnable = true;
    auto nodetek              = _compositordata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
    auto outpnode             = nodetek->tryOutputNodeAs<ScreenOutputCompositingNode>();
     outpnode->setSuperSample(KSUPERSAMPLE);
    _compositorimpl = _compositordata->createImpl();
    _compositorimpl->bindLighting(_lightmgr.get());

    _TOPCPD = std::make_shared<lev2::CompositingPassData>();
    _TOPCPD->addStandardLayers();

    ctx->debugPushGroup("main.onGpuInit");
    _modelasset = asset::AssetManager<XgmModelAsset>::load("data://tests/pbr1/pbr1");

    ctx->debugPopGroup();
  }

  callback_drawable_ptr_t _sdf_drawable;
  textureassetptr_t _voltexA;
  renderer_ptr_t _renderer;
  LightManagerData _lmd;
  lightmanager_ptr_t _lightmgr;
  compositingpassdata_ptr_t _TOPCPD;
  compositorimpl_ptr_t _compositorimpl;
  compositordata_ptr_t _compositordata;
  lev2::xgmmodelassetptr_t _modelasset; // retain model
  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;
};

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  imgui::initModule(init_data);

  SdfSceneData::GetClassStatic();
  SdfSubOne::GetClassStatic();
  SdfSubTwo::GetClassStatic();

  auto qtapp  = OrkEzApp::create(init_data);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  std::shared_ptr<GpuResources> gpurec;
  //////////////////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();
  int _width       = 0;
  int _height      = 0;
  float _near      = 2;
  float _far       = 100;
  bool _imgui_open = true;

  auto _sdfsceneparams = std::make_shared<SdfSceneData>();
  //////////////////////////////////////////////////////////
  auto this_dir = qtapp->_orkidWorkspaceDir //
                  / "ork.lev2"              //
                  / "examples"              //
                  / "c++"                   //
                  / "sdfscene";             //
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    gpurec = std::make_shared<GpuResources>(ctx, this_dir);
    /////////////////////////////////////
    // our callback renderable just renders a full screen quad
    //  during the gbuffer pass, writing the Signed Distance Function query to the gbuffer
    /////////////////////////////////////
    auto render_sdf = [&](lev2::RenderContextInstData& RCID) {

      const auto RCFD = RCID._RCFD;
      auto targ = RCFD->GetTarget();
      auto sub = std::dynamic_pointer_cast<SdfSubBase>(_sdfsceneparams->_subobject);
      sub->gpuInit(targ);

      float time      = RCFD->getUserProperty("time"_crc).get<float>();
      auto& material  = sub->_sdf_material;
      auto& TOPCPD    = RCFD->topCPD();
      auto cammtc     = TOPCPD.cameraMatrices();


      material.begin(sub->_tekSDF, *RCFD);
      material.bindParamMatrix(sub->_parMatIVP, cammtc->GetIVPMatrix());

      float ss_w = float(_width)*(KSUPERSAMPLE+1);
      float ss_h = float(_height)*(KSUPERSAMPLE+1);

      material.bindParamVec2(sub->_parInvViewSize, fvec2(1.0 / ss_w, 1.0f / ss_h));
      material.bindParamFloat(sub->_parTime, time);
      material.bindParamFloat(sub->_parNear, _near);
      material.bindParamFloat(sub->_parFar, _far);
      material.bindParamFloat(sub->_parDisplacement, _sdfsceneparams->_displacement);
      material.bindParamFloat(sub->_parRepeatPeriod, _sdfsceneparams->_repeatperiod);
      material.bindParamFloat(sub->_parNoisePowerA, _sdfsceneparams->_noisepowera);
      material.bindParamFloat(sub->_parNoisePowerB, _sdfsceneparams->_noisepowerb);
      material.bindParamCTex(sub->_parMapVolTexA, gpurec->_voltexA->_texture.get());

      material.bindParamVec3(sub->_parColorA, _sdfsceneparams->_colorA);
      material.bindParamVec3(sub->_parColorB, _sdfsceneparams->_colorB);
      material.bindParamFloat(sub->_parIntensityA, _sdfsceneparams->_intensitya);
      material.bindParamFloat(sub->_parIntensityB, _sdfsceneparams->_intensityb);

      gfxwin->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
      material.end(*RCFD);
    };
    ///////////////////////////////////////
    gpurec->_sdf_drawable->SetRenderCallback(render_sdf);
    gpurec->_sdf_drawable->SetSortKey(0xffff);
    /////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  auto sframe = std::make_shared<StandardCompositorFrame>();
  qtapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.1f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    gpurec->_camdata->Lookat(eye, tgt, up);
    gpurec->_camdata->Persp(_near, _far, 45.0);
    ///////////////////////////////////////
    auto DB = sframe->_dbufcontext->acquireForWriteLocked();
    DB->Reset();
    DB->copyCameras(*gpurec->_camlut);
    auto layer = DB->MergeLayer("Default");
    DrawQueueXfData ident;
    gpurec->_sdf_drawable->enqueueOnLayer(ident, *layer);
    ////////////////////////////////////////
    sframe->_dbufcontext->releaseFromWriteLocked(DB);
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  qtapp->onDraw([&](ui::drawevent_constptr_t drawEvent) {
    sframe->_drawEvent = drawEvent;

    auto context = drawEvent->GetTarget();
    context->beginFrame();

    //////////////////////////////////////////////////////////////////
    // lambda which renders IMGUI related stuff
    //  called automatically at the appropriate time in the frame
    //   (typically post-compositor)
    //////////////////////////////////////////////////////////////////

    sframe->onImguiRender = [&](const AcquiredRenderDrawBuffer& rdb) {

      ImGuiStyle& style = ImGui::GetStyle();
      style.WindowRounding = 5.3f;
      style.FrameRounding = 2.3f;
      style.ScrollbarRounding = 2.3f;
      style.FrameBorderSize = 1.0f;

      // Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
      // Most ImGui functions would normally just crash if the context is missing.
      IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

      ImGuiWindowFlags window_flags = 0;

      // Main body of the Demo window starts here.
      if (!ImGui::Begin("SDF Scene", &_imgui_open, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
      }

      // e.g. Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

      // ImGui::Text("dear imgui says hello. (%s)", IMGUI_VERSION);
      ImGui::Spacing();

      editor::EditorContext edctx(rdb);
      editor::imgui::ObjectEditor(edctx,_sdfsceneparams);

      ImGui::PopItemWidth();
      ImGui::End();
    };

    //////////////////////////////////////////////////////////////////
    // draw the actual frame
    //////////////////////////////////////////////////////////////////

    sframe->compositor = gpurec->_compositorimpl;
    sframe->renderer   = gpurec->_renderer;
    sframe->passdata   = gpurec->_TOPCPD;
    sframe->_updrendersync = init_data->_update_rendersync;

    sframe->_userprops.AddSorted("time"_crc, float(timer.SecsSinceStart()));

    sframe->render();

    context->endFrame();

  });
  //////////////////////////////////////////////////////////
  opq::updateSerialQueue()->setHook("ged-pre-lockgfx",[](svar64_t data){
    auto obj = data.get<object_ptr_t>();
    printf( "ged-pre-lockgfx invoked on obj<%p>!\n", (void*) obj.get() );
  });
  //////////////////////////////////////////////////////////
  opq::updateSerialQueue()->setHook("ged-post-unlockgfx",[](svar64_t data){
    auto obj = data.get<object_ptr_t>();
    printf( "ged-post-unlockgfx invoked on obj<%p>!\n", (void*) obj.get() );
  });
  //////////////////////////////////////////////////////////
  opq::updateSerialQueue()->setHook("ged-pre-edit",[](svar64_t data){
    auto obj = data.get<object_ptr_t>();
    printf( "ged-pre-edit invoked on obj<%p>!\n", (void*) obj.get() );
  });
  //////////////////////////////////////////////////////////
  opq::updateSerialQueue()->setHook("ged-post-edit",[](svar64_t data){
    auto obj = data.get<object_ptr_t>();
    printf( "ged-post-edit invoked on obj<%p>!\n", (void*) obj.get() );
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
    //
    gpurec->_compositorimpl->compositingContext().Resize(w, h);
    _width  = w;
    _height = h;
    lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  });
  qtapp->onGpuExit([&](Context* ctx) { gpurec = nullptr; });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->mainThreadLoop();
}
