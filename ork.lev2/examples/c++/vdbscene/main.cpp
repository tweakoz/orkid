////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
///////////////////////////////////////////////////////////////////
// Signed Distance Field Test Scene
///////////////////////////////////////////////////////////////////

#include <iostream>

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
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.h>

#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/asset/DynamicAssetLoader.h>

#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/imgui_ged.inl>

#include <openvdb/openvdb.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/Interpolation.h>

#include <ork/lev2/gfx/util/movie.inl>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;

const int KSUPERSAMPLE = 0;
const int width = 512; //1920;
const int height = 360; //1080; //;
const int left = 1500;
const bool do_movie = false;
bool do_rescale = true;

struct Instance {
  fvec3 _curpos;
  fvec3 _curaxis;
  float _curangle = 0.0f;
  fvec3 _target;
  fvec3 _targetaxis;
  float _targetangle = 0.0f;
  float _timeout     = 0.0f;
};

using instances_t       = std::vector<Instance>;

texture_ptr_t vdb_main(AssetPath path, Context* ctx)
{
  using namespace openvdb;

  using gridtype_t = FloatGrid;
  using samplertype_t = tools::BoxSampler;

  auto abspath = path.ToAbsolute();
  io::File file(abspath.c_str());
  file.open();
  GridBase::Ptr baseGrid = file.readGrid("density");
  file.close();

  auto sample_grid = gridPtrCast<gridtype_t>(baseGrid);

  tools::GridSampler<gridtype_t, samplertype_t> density_sampler(*sample_grid);

  float worldValue = density_sampler.isSample(Vec3R(0,0,0));

  const auto& bbMinI = sample_grid->metaValue<Vec3i>(GridBase::META_FILE_BBOX_MIN);
  const auto& bbMaxI = sample_grid->metaValue<Vec3i>(GridBase::META_FILE_BBOX_MAX);

  int dim_x = bbMaxI[0]-bbMinI[0];
  int dim_y = bbMaxI[1]-bbMinI[1];
  int dim_z = bbMaxI[2]-bbMinI[2];

  size_t voxel_count = dim_x*dim_y*dim_z;

  printf("bbmin<%d %d %d>\n", bbMinI[0], bbMinI[1], bbMinI[2] );
  printf("bbmax<%d %d %d>\n", bbMaxI[0], bbMaxI[1], bbMaxI[2] );
  printf("bbdim<%d %d %d>\n", dim_x, dim_y, dim_z );
  printf("voxel_count<%zu>\n", voxel_count );
  printf("voxel_memsize<%zu>\n", voxel_count*4 );

  auto VOLTEX_BASE = new float[voxel_count];

  printf( "VOLTEX_BASE<%p>\n", (void*) VOLTEX_BASE );

  std::set<float> uniquevalues;

  for( int z=0; z<dim_z; z++ ){
    int z_base = z*(dim_x*dim_y);
    for( int y=0; y<dim_y; y++ ){
      int y_base = z_base+(y*dim_x);
      for( int x=0; x<dim_x; x++ ){

          int sample_x = bbMinI[0]+x;
          int sample_y = bbMinI[1]+y;
          int sample_z = bbMinI[2]+z;
          float sample = density_sampler.isSample(Vec3R(sample_x,sample_y,sample_z));
          uniquevalues.insert(sample);


          float fi = float(y_base+x)/float(dim_x*dim_y*dim_z);

          size_t index = y_base+x;

          OrkAssert(index<voxel_count);
          OrkAssert(index>=0);

          VOLTEX_BASE[index] = sample;
      
      }
    }
  }

  int numu = uniquevalues.size();
  printf( "numunique<%d>\n", int(numu));

  TextureInitData tid;
  tid._w = dim_x;
  tid._h = dim_y;
  tid._d = dim_z;
  tid._format = EBufferFormat::R32F;
  tid._data = VOLTEX_BASE;

  auto texture = std::make_shared<Texture>();

  ctx->TXI()->initTextureFromData(texture.get(),tid);

  delete[] VOLTEX_BASE;

  return texture;
}

///////////////////////////////////////////////////////////////////

struct SdfSceneData : public Object {

  DeclareConcreteX(SdfSceneData, Object);

  SdfSceneData(){
    _colorA = fvec3(0,1,0);
    _colorB = fvec3(1,0,0);
  }
  float _displacement = 2.814;
  float _repeatperiod = .837;
  float _noisepowera = 1.747;
  float _noisepowerb = 0.281;
  float _intensitya = 7.274;
  float _intensityb = 4.6;
  float _timescale = 4.03;
  float _featurescale = 0.373;
  fvec3 _colorA;
  fvec3 _colorB;

};

///////////////////////////////////////////////////////////////////

ImplementReflectionX(SdfSceneData, "SdfSceneData");

///////////////////////////////////////////////////////////////////

void SdfSceneData::describeX(ork::object::ObjectClass* clazz) {

  clazz->floatProperty("Displacement", float_range{0.001,1}, &SdfSceneData::_displacement);
  clazz->floatProperty("RepeatPeriod", float_range{0.001,2}, &SdfSceneData::_repeatperiod);
  clazz->floatProperty("NoisePowerA", float_range{.01,2}, &SdfSceneData::_noisepowera);
  clazz->floatProperty("NoisePowerB", float_range{.01,2}, &SdfSceneData::_noisepowerb);
  clazz->floatProperty("IntensityA", float_range{.001,200}, &SdfSceneData::_intensitya);
  clazz->floatProperty("IntensityB", float_range{0,0.5}, &SdfSceneData::_intensityb);
  clazz->floatProperty("TimeScale", float_range{.01,10}, &SdfSceneData::_timescale);
  clazz->floatProperty("FeatureScale", float_range{.01,2}, &SdfSceneData::_featurescale);

  clazz
      ->directProperty("ColorA", &SdfSceneData::_colorA) //
      ->annotate<ConstString>("editor.type", "color");

  clazz
      ->directProperty("ColorB", &SdfSceneData::_colorB) //
      ->annotate<ConstString>("editor.type", "color");

  reflect::group_list_ptr_t gl = std::make_shared<reflect::PropGroupList>(); 

  gl->addGroup("main", "Displacement RepeatPeriod TimeScale FeatureScale");
  gl->addGroup("noise", "NoisePowerA NoisePowerB");
  gl->addGroup("color", "ColorA IntensityA ColorB IntensityB");

  clazz->annotate("editor.groups",gl);
}

///////////////////////////////////////////////////////////////////

    constexpr size_t KNUMINSTANCES = 30;

struct GpuResources {

  GpuResources(Context* ctx, file::Path this_dir) {
    _renderer = std::make_shared<IRenderer>();
    _lightmgr = std::make_shared<LightManager>(_lmd);

    _camlut  = std::make_shared<CameraDataLut>();
    _camdata = std::make_shared<CameraData>();
    _camlut->AddSorted("spawncam", _camdata.get());

    _sdf_drawable = std::make_shared<CallbackDrawable>(nullptr);

    _voltexA = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/voltex_pn2");

    _instanced_drawable = std::make_shared<InstancedModelDrawable>();

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
    _instanced_drawable->bindModel(_modelasset->getSharedModel());


    _instanced_drawable->resize(KNUMINSTANCES);
    _instanced_drawable->gpuInit(ctx);
    _instances          = std::make_shared<instances_t>();
    for (int i = 0; i < KNUMINSTANCES; i++) {
      Instance inst;
      _instances->push_back(inst);
    }

    _renderer->setContext(ctx);

    ctx->debugPopGroup();

    _sdf_material.gpuInit(ctx, "demo://sdf1.glfx");

    _tekSDF = _sdf_material.technique("tek_sdfscene");

    _parMatIVP      = _sdf_material.param("IVP");
    _parInvViewSize = _sdf_material.param("InvViewportSize");
    _parMapDepth    = _sdf_material.param("MapDepth");
    _parMapVolTexA  = _sdf_material.param("MapVolTexA");
    _parMapVolTexB  = _sdf_material.param("MapVolTexB");

    _parTime         = _sdf_material.param("Time");
    _parNear         = _sdf_material.param("Near");
    _parFar          = _sdf_material.param("Far");
    _parFeatureScale = _sdf_material.param("FeatureScale");
    _parDisplacement = _sdf_material.param("Displacement");
    _parRepeatPeriod = _sdf_material.param("RepeatPeriod");
    _parNoisePowerA  = _sdf_material.param("NoisePowerA");
    _parNoisePowerB  = _sdf_material.param("NoisePowerB");

    _parColorA     = _sdf_material.param("ColorA");
    _parColorB     = _sdf_material.param("ColorB");
    _parIntensityA = _sdf_material.param("IntensityA");
    _parIntensityB = _sdf_material.param("IntensityB");

    _vdbtex = vdb_main("demo://cloud1_sdf.vdb", ctx);

  }

  instanced_modeldrawable_ptr_t _instanced_drawable;
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
  texture_ptr_t _vdbtex;
  FreestyleMaterial _sdf_material;
  std::shared_ptr<instances_t> _instances;

  const FxShaderTechnique* _tekSDF     = nullptr;
  const FxShaderParam* _parMatIVP      = nullptr;
  const FxShaderParam* _parInvViewSize = nullptr;

  const FxShaderParam* _parTime         = nullptr;
  const FxShaderParam* _parNear         = nullptr;
  const FxShaderParam* _parFar          = nullptr;
  const FxShaderParam* _parFeatureScale = nullptr;
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
  const FxShaderParam* _parMapVolTexB = nullptr;

  std::string _teststring;
  AssetPath _shaderpath;

};

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);

  init_data->_width = width;
  init_data->_height = height;
  init_data->_left = left;
  init_data->_update_rendersync = true;
  
  SdfSceneData::GetClassStatic();

  lev2::initModule(init_data);
  imgui::initModule(init_data);
  auto qtapp        = OrkEzApp::create(init_data);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  std::shared_ptr<GpuResources> gpurec;

  atomic<int> update_count = 0;

  //////////////////////////////////////////////////////////
  openvdb::initialize();
  //////////////////////////////////////////////////////////
  std::shared_ptr<MovieContext> movie = nullptr;
  if(do_movie){
    movie = std::make_shared<MovieContext>();
    movie->init(init_data->_width,init_data->_height);
  }
  //////////////////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();
  int _width       = init_data->_width;
  int _height      = init_data->_height;
  float _near      = 2;
  float _far       = 100;
  bool _imgui_open = true;

  auto _sdfsceneparams = std::make_shared<SdfSceneData>();
  //////////////////////////////////////////////////////////
  auto this_dir = qtapp->_orkidWorkspaceDir //
                  / "ork.lev2"              //
                  / "examples"              //
                  / "c++"                   //
                  / "vdbscene";             //
  //////////////////////////////////////////////////////////
  auto filecontext = FileEnv::createContextForUriBase("demo://", this_dir);
  filecontext->SetFilesystemBaseEnable(true);
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

      auto RCFD = RCID._RCFD;
      auto targ = RCFD->GetTarget();

      float time      = RCFD->getUserProperty("time"_crc).get<float>();

      //printf( "timeR<%g>\n", time );
      auto& material  = gpurec->_sdf_material;
      auto& TOPCPD    = RCFD->topCPD();
      auto cammtc     = TOPCPD.cameraMatrices();


      auto rtg_gbuffer = RCFD->getUserProperty("rtg_gbuffer"_crc).get<rtgroup_ptr_t>();
      auto gbuffer_depth = rtg_gbuffer->_depthBuffer->_texture.get();

      material.begin(gpurec->_tekSDF, *RCFD);
      material.bindParamMatrix(gpurec->_parMatIVP, cammtc->GetIVPMatrix());

      float ss_w = float(_width)*(KSUPERSAMPLE+1);
      float ss_h = float(_height)*(KSUPERSAMPLE+1);

      material.bindParamVec2(gpurec->_parInvViewSize, fvec2(1.0 / ss_w, 1.0f / ss_h));
      material.bindParamFloat(gpurec->_parTime, time*_sdfsceneparams->_timescale);
      material.bindParamFloat(gpurec->_parNear, _near);
      material.bindParamFloat(gpurec->_parFar, _far);
      material.bindParamFloat(gpurec->_parFeatureScale, _sdfsceneparams->_featurescale);
      material.bindParamFloat(gpurec->_parDisplacement, _sdfsceneparams->_displacement);
      material.bindParamFloat(gpurec->_parRepeatPeriod, _sdfsceneparams->_repeatperiod);
      material.bindParamFloat(gpurec->_parNoisePowerA, _sdfsceneparams->_noisepowera);
      material.bindParamFloat(gpurec->_parNoisePowerB, _sdfsceneparams->_noisepowerb);

      material.bindParamCTex(gpurec->_parMapVolTexA, gpurec->_vdbtex.get());
      material.bindParamCTex(gpurec->_parMapVolTexB, gpurec->_voltexA->_texture.get());
      material.bindParamCTex(gpurec->_parMapDepth, gbuffer_depth);

      material.bindParamVec3(gpurec->_parColorA, _sdfsceneparams->_colorA);
      material.bindParamVec3(gpurec->_parColorB, _sdfsceneparams->_colorB);
      material.bindParamFloat(gpurec->_parIntensityA, _sdfsceneparams->_intensitya);
      material.bindParamFloat(gpurec->_parIntensityB, _sdfsceneparams->_intensityb);

      auto new_mask = RGBAMask{false,false,false,true};
      auto old_mask = targ->RSI()->SetRGBAWriteMask(new_mask);

      gfxwin->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));

      targ->RSI()->SetRGBAWriteMask(old_mask);

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
  double absabstime = 0.0f;
  qtapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;

    if(do_movie){
      dt = 1.0f/60.0f;
      absabstime += dt;
      abstime = absabstime;
    }

    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.015f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), -0.2f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    gpurec->_camdata->Lookat(eye, tgt, up);
    gpurec->_camdata->Persp(_near, _far, 45.0);
    ///////////////////////////////////////
    auto instdata = gpurec->_instanced_drawable->_instancedata;
    int index = 0;
    for (auto& inst : *gpurec->_instances) {

      fvec3 delta   = inst._target - inst._curpos;
      inst._curpos += delta.normalized() * dt * 1.0;

      delta         = inst._targetaxis - inst._curaxis;
      inst._curaxis = (inst._curaxis + delta.normalized() * dt * 0.1).normalized();
      inst._curangle += (inst._targetangle - inst._curangle) * dt * 0.1;

      if (inst._timeout < abstime) {
        inst._timeout  = abstime + float(rand() % 255) / 64.0;
        inst._target.x = (float(rand() % 255) / 2.55) - 50;
        inst._target.y = (float(rand() % 255) / 2.55) - 50;
        inst._target.z = (float(rand() % 255) / 2.55) - 50;
        inst._target *= (4.5f/50.0f);

        fvec3 axis;
        axis.x            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.y            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.z            = (float(rand() % 255) / 255.0f) - 0.5f;
        inst._targetaxis  = axis.normalized();
        inst._targetangle = PI2 * (float(rand() % 255) / 255.0f) - 0.5f;
      }
      fquat q;
      q.fromAxisAngle(fvec4(inst._curaxis, inst._curangle));
      instdata->_worldmatrices[index++].compose(inst._curpos, q, 0.3f);
    }
    ///////////////////////////////////////
    auto DB = qtwin->_tryAcquireUpdateBuffer();
    if(DB){
      DB->copyCameras(*gpurec->_camlut);

      RenderContextFrameData::usermap_t userprops;
      userprops.AddSorted("time"_crc, float(abstime));
      DB->setUserProperty("rcfdprops",userprops);

      //printf( "timeU<%g> DBI<%d>\n", abstime, DB->miBufferIndex );

      auto layer = DB->MergeLayer("Default");
      DrawQueueTransferData ident;
      gpurec->_sdf_drawable->enqueueOnLayer(ident, *layer);
      gpurec->_instanced_drawable->enqueueOnLayer(ident, *layer);
      qtwin->_releaseAcquireUpdateBuffer(DB);
    }
    ///////////////////////////////////////

  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////

  qtapp->onDraw([&](ui::drawevent_constptr_t drawEvent) {
    StandardCompositorFrame sframe(drawEvent);
    sframe._updrendersync = init_data->_update_rendersync;
    auto context = drawEvent->GetTarget();
    context->beginFrame();

    //////////////////////////////////////////////////////////////////
    // lambda which renders IMGUI related stuff
    //  called automatically at the appropriate time in the frame
    //   (typically post-compositor)
    //////////////////////////////////////////////////////////////////

    sframe.onImguiRender = [&](const AcquiredDrawQueueForRendering& rdb) {

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
      if (!ImGui::Begin("Signed Distance Field Scene Settings", &_imgui_open, window_flags)) {
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

    sframe.compositor = gpurec->_compositorimpl;
    sframe.renderer   = gpurec->_renderer;
    sframe.passdata   = gpurec->_TOPCPD;
    sframe._updrendersync = init_data->_update_rendersync;

    //////////////////////////////////////////////////////////////////
    // capture to disk...
    //////////////////////////////////////////////////////////////////
    if(movie){
      sframe.onPostCompositorRender = [&](const AcquiredDrawQueueForRendering& rdb) {

        //auto rtbuf_accum = rdb._RCFD.getUserProperty("rtb_accum"_crc).get<rtbuffer_ptr_t>();
        auto ctx = rdb._RCFD->GetTarget();
        auto fbi = ctx->FBI();
        //fbi->capture(rtbuf_accum.get(),"demo://output.png");
        CaptureBuffer capbuf;
        bool ok = fbi->captureAsFormat(nullptr, &capbuf, EBufferFormat::RGB8);
        movie->writeFrame(capbuf);

        int ircount = qtapp->_render_count.load();
        int iucount = qtapp->_update_count.load();
        printf( "movie write frame<%d> ircount<%d> iucount<%d>\n", movie->_frame, ircount, iucount );
      };
    }
    //////////////////////////////////////////////////////////////////

    sframe.render();

    context->endFrame();

  });
  //////////////////////////////////////////////////////////
  qtapp->onUpdateExit([&]() {
    movie = nullptr;
  });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->_eventcode) {
      case ui::EventCode::KEY_DOWN:
        switch (ev->miKeyCode) {
          case 'X':
            movie = nullptr;
            exit(0);
            break;
        }
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  qtapp->_onAppEarlyTerminated = [&](){
    movie = nullptr;
  };
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
    if(do_rescale){
      lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
      //
      gpurec->_compositorimpl->compositingContext().Resize(w, h);
      _width  = w;
      _height = h;
      lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
      do_rescale = false;
    }
  });
  qtapp->onGpuExit([&](Context* ctx) { gpurec = nullptr; });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->mainThreadLoop();
}
