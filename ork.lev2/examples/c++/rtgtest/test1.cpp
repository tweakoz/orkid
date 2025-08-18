////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;


using float_vect_t = std::vector<float>;
using compgroup_ptr_t = opq::CompletionGroup*;

struct Resources {

  Resources(Context* ctx){

    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _tekTexColor        = _material->technique("texcolor");
    _tekDebugUv         = _material->technique("debuguv");
    _fxparameterMVP     = _material->param("MatMVP");
    _fxparameterTexture = _material->param("ColorMap");
    OrkAssert(_fxparameterTexture != nullptr);
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx);
    deco::printf(fvec3::Yellow(), "  _tekTexColor<%p>\n", _tekTexColor);
    deco::printf(fvec3::Yellow(), "  _tekDebugUv<%p>\n", _tekDebugUv);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", _fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterTexture<%p>\n", _fxparameterTexture);

    _offscreen_rtg = std::make_shared<RtGroup>(ctx, 64, 64, MsaaSamples::MSAA_1X, "user"_crcu);
    _offscreen_color = _offscreen_rtg->createRenderTarget(EBufferFormat::RGBA32F, "color"_crcu,true);
    _offscreen_depth = _offscreen_rtg->createDepthBuffer(EBufferFormat::Z32F, true);
    _offscreen_rtg->_name = "offscreen_rtg";
    _offscreen_color->_debugName = "offscreen_color";
    _offscreen_depth->_debugName = "offscreen_depth";
    _offscreen_rtg->_autoclear = true;
    _offscreen_color->_clearColor = fvec4(0.3f, 1, 0.3f, 1.0f);
  }

  ~Resources(){
  }

  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _tekTexColor    = nullptr;
  const FxShaderTechnique* _tekDebugUv     = nullptr;
  const FxShaderParam* _fxparameterMVP     = nullptr;
  const FxShaderParam* _fxparameterTexture = nullptr;
  rtgroup_ptr_t _offscreen_rtg;
  rtbuffer_ptr_t _offscreen_color;
  rtbuffer_ptr_t _offscreen_depth;
  int _framecounter = 0;
  Timer _timer;

};

using resources_ptr_t = std::shared_ptr<Resources>;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;
  Timer timer;
  timer.Start();
  resources_ptr_t resources;
  float abstime = 0.0f;
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx);
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuUpdate([&](Context* ctx) {
  });
  //////////////////////////////////////////////////////////
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    abstime = updata->_abstime;
  });
  //////////////////////////////////////////////////////////
  int framecounter = 0;
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context        = drwev->GetTarget();
    auto fbi            = context->FBI(); // FrameBufferInterface

    ///////////////////////////////////////////////////
    // render to offscreen rtgroup
    ///////////////////////////////////////////////////

    fbi->PushRtGroup(resources->_offscreen_rtg.get());
    auto RCFD1 = std::make_shared<RenderContextFrameData>(context);
    resources->_material->begin(resources->_tekDebugUv, RCFD1);
    fmtx4 P1, V1, M1;
    P1.perspective(55.0f*DTOR, 1.0, 0.01f, 10.0f);
    V1.lookAt( fvec3(0, 0, 1.5),  // eye
              fvec3(0, 0, 0),  // target
              fvec3(0, 1, 0)); // up
    M1.rotateOnZ(abstime*0.25f);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P1*V1*M1);
    //resources->_material->bindParamTexture(resources->_fxparameterTexture, resources->_offscreen_color->_texture.get());

    appwin->Render2dQuadEML( fvec4(-.75, -.75, 1.5, 1.5), // quad in NDC
                             fvec4(0, 0, 1, 1),   // uv0rect
                             fvec4(0, 0, 1, 1));  // uv1rect
    resources->_material->end(RCFD1);    

    fbi->PopRtGroup();

    ///////////////////////////////////////////////////
    // render offscreen texture to main surface
    ///////////////////////////////////////////////////

    float fi = abstime * 0.33f;
    float r             = sinf(fi * 2.1f) * 0.25f + 0.5f;
    float g             = cosf(fi * 3.13f) * 0.25f + 0.5f;
    float b             = sinf(fi * 4.17f) * 0.25f + 0.5f;
    float w = context->mainSurfaceWidth();
    float h = context->mainSurfaceHeight();
    float aspect = w / h;
    auto main_rtg = fbi->_main_rtg;
    auto main_rtb = main_rtg->buffer(0);
    main_rtb->_clearColor = fvec4(r, g, b, 1);
    auto RCFD2 = std::make_shared<RenderContextFrameData>(context);
    resources->_material->begin(resources->_tekTexColor, RCFD2);
    fmtx4 P2, V2, M2;
    P2.perspective(55.0f*DTOR, aspect, 0.01f, 10.0f);
    V2.lookAt( fvec3(0, 0, 2),  // eye
              fvec3(0, 0, 0),  // target
              fvec3(0, 1, 0)); // up
    M2.rotateOnZ(abstime*0.25f);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P2*V2*M2);
    resources->_material->bindParamTexture(resources->_fxparameterTexture, resources->_offscreen_color->_texture.get());

    appwin->Render2dQuadEML( fvec4(-.75, -.75, 1.5, 1.5), // quad in NDC
                             fvec4(0, 0, 1, 1),   // uv0rect
                             fvec4(0, 0, 1, 1));  // uv1rect
    resources->_material->end(RCFD2);    

    //::usleep(1<<20); // sleep 1ms to avoid hogging the CPU
    if (timer.SecsSinceStart() > 1.0f) {
      float FPS    = float(framecounter) / timer.SecsSinceStart();
      deco::printf(fvec3::White(), "Frames/Sec<%g> ", FPS);
      timer.Start();
      framecounter = 0;
    }
    framecounter++;
  });

  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->_eventcode) {
      case ui::EventCode::DOUBLECLICK:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuExit([&](Context* ctx) {
    resources = nullptr;
  });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  int rval = ezapp->mainThreadLoop();
  opq::concurrentQueue()->drain();
}
