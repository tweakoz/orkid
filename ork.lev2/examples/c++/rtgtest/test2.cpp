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

struct Resources {

  Resources(Context* ctx){

    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _tekTexColor        = _material->technique("texcolor");
    _tekDebugUv         = _material->technique("debuguv");
    _tekVtxColor        = _material->technique("vtxcolor");
    _tekLib1X           = _material->technique("testlib1x");
    
    _fxparameterMVP     = _material->param("MatMVP");
    _fxparameterMODC    = _material->param("modcolor");
    _fxparameterTexture = _material->param("ColorMap");
    OrkAssert(_fxparameterTexture != nullptr);
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx);
    deco::printf(fvec3::Yellow(), "  _tekTexColor<%p>\n", _tekTexColor);
    deco::printf(fvec3::Yellow(), "  _tekDebugUv<%p>\n", _tekDebugUv);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", _fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterTexture<%p>\n", _fxparameterTexture);

    // Create 4 offscreen render targets
    for(int i = 0; i < 4; i++) {
      auto rtg = std::make_shared<RtGroup>(ctx, 128, 128, MsaaSamples::MSAA_1X, "user"_crcu);
      auto color = rtg->createRenderTarget(EBufferFormat::RGBA8, "color"_crcu, true);
      auto depth = rtg->createDepthBuffer(EBufferFormat::Z32F, false);
      
      rtg->_name = FormatString("offscreen_rtg_%d", i);
      color->_debugName = FormatString("offscreen_color_%d", i);
      depth->_debugName = FormatString("offscreen_depth_%d", i);
      rtg->_autoclear = true;
      
      // Different clear colors for each RTG
      switch(i) {
        case 0: color->_clearColor = fvec4(0.3f, 0.1f, 0.1f, 1.0f); break; // Red-ish
        case 1: color->_clearColor = fvec4(0.1f, 0.3f, 0.1f, 1.0f); break; // Green-ish
        case 2: color->_clearColor = fvec4(0.1f, 0.1f, 0.3f, 1.0f); break; // Blue-ish
        case 3: color->_clearColor = fvec4(0.3f, 0.3f, 0.1f, 1.0f); break; // Yellow-ish
      }
      
      _offscreen_rtgs.push_back(rtg);
      _offscreen_colors.push_back(color);
      _offscreen_depths.push_back(depth);
    }
  }

  ~Resources(){
  }

  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _tekTexColor    = nullptr;
  const FxShaderTechnique* _tekDebugUv     = nullptr;
  const FxShaderTechnique* _tekVtxColor     = nullptr;
  const FxShaderTechnique* _tekLib1X        = nullptr;
  const FxShaderParam* _fxparameterMVP     = nullptr;
  const FxShaderParam* _fxparameterMODC    = nullptr;
  const FxShaderParam* _fxparameterTexture = nullptr;
  
  std::vector<rtgroup_ptr_t> _offscreen_rtgs;
  std::vector<rtbuffer_ptr_t> _offscreen_colors;
  std::vector<rtbuffer_ptr_t> _offscreen_depths;
  
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
    auto context = drwev->GetTarget();
    auto fbi = context->FBI(); // FrameBufferInterface

    ///////////////////////////////////////////////////
    // Render to 4 offscreen rtgroups
    ///////////////////////////////////////////////////

    // Calculate the aspect ratio of the screen quarters
    float w = context->mainSurfaceWidth();
    float h = context->mainSurfaceHeight();
    float screen_aspect = w / h;
    
    for(int i = 0; i < 4; i++) {
      fbi->PushRtGroup(resources->_offscreen_rtgs[i].get());
      auto RCFD = std::make_shared<RenderContextFrameData>(context);
      
      // Use different techniques for variety
      const FxShaderTechnique* technique = nullptr;
      switch(i) {
        case 0: technique = resources->_tekDebugUv; break; // Debug UV
        case 1: technique = resources->_tekDebugUv; break; // Debug UV
        case 2: technique = resources->_tekVtxColor; break; // Vertex color
        case 3: technique = resources->_tekLib1X; break; // Library technique
      }
      resources->_material->begin(technique, RCFD);
      
      fmtx4 P, V, M;
      P.perspective(65.0f*DTOR, screen_aspect, 0.01f, 10.0f);
      
      // Different camera positions for each RTG
      float angle = (i * 90.0f) * DTOR;
      float radius = 2.0f + float(i) * 0.3f;
      V.lookAt(fvec3(sinf(angle) * radius, 0, cosf(angle) * radius),  // eye
               fvec3(0, 0, 0),  // target
               fvec3(0, 1, 0)); // up
      
      // Different rotation speeds
      M.rotateOnY(abstime * (0.25f + i * 0.1f));
      M.rotateOnZ(abstime * (0.15f + i * 0.05f));
      
      resources->_material->bindParamMatrix(resources->_fxparameterMVP, P*V*M);
      
      // Render a cube or quad
      appwin->Render2dQuadEML(fvec4(-.75, -.75, 1.5, 1.5), // quad in NDC
                              fvec4(0, 0, 1, 1),   // uv0rect
                              fvec4(0, 0, 1, 1));  // uv1rect
      
      resources->_material->end(RCFD);
      fbi->PopRtGroup();
    }

    ///////////////////////////////////////////////////
    // Render 4 offscreen textures to main surface quarters
    ///////////////////////////////////////////////////

    float fi = abstime * 0.33f;
    float r = sinf(fi * 2.1f) * 0.25f + 0.5f;
    float g = cosf(fi * 3.13f) * 0.25f + 0.5f;
    float b = sinf(fi * 4.17f) * 0.25f + 0.5f;
    
    auto main_rtg = fbi->_main_rtg;
    auto main_rtb = main_rtg->buffer(0);
    main_rtb->_clearColor = fvec4(r * 0.3f, g * 0.3f, b * 0.3f, 1);

    // Setup orthographic projection for screen-space rendering
    auto RCFD_main = std::make_shared<RenderContextFrameData>(context);
    resources->_material->begin(resources->_tekTexColor, RCFD_main);
    
    fmtx4 P_ortho, V_ortho, M_ortho;
    P_ortho = fmtx4::Identity();
    V_ortho = fmtx4::Identity();
    M_ortho = fmtx4::Identity();
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P_ortho * V_ortho * M_ortho);

    // Render each RTT to a quarter of the screen
    fvec4 quarters[4] = {
      fvec4(-1.0f,  0.0f, 1.0f, 1.0f), // Top-left
      fvec4( 0.0f,  0.0f, 1.0f, 1.0f), // Top-right
      fvec4(-1.0f, -1.0f, 1.0f, 1.0f), // Bottom-left
      fvec4( 0.0f, -1.0f, 1.0f, 1.0f)  // Bottom-right
    };
    
    for(int i = 0; i < 4; i++) {
      resources->_material->bindParamTexture(resources->_fxparameterTexture, 
                                             resources->_offscreen_colors[i]->_texture.get());
      
      // Add a small gap between quarters
      float gap = 0.02f;
      fvec4 q = quarters[i];
      q.x += gap;
      q.y += gap;
      q.z -= gap * 2.0f;
      q.w -= gap * 2.0f;
      
      appwin->Render2dQuadEML(q,                    // quad in NDC
                              fvec4(0, 0, 1, 1),    // uv0rect
                              fvec4(0, 0, 1, 1));   // uv1rect
    }
    
    resources->_material->end(RCFD_main);

    //::usleep(1<<20); // sleep 1ms to avoid hogging the CPU
    if (timer.SecsSinceStart() > 1.0f) {
      float FPS = float(framecounter) / timer.SecsSinceStart();
      deco::printf(fvec3::White(), "Frames/Sec<%g>\n", FPS);
      timer.Start();
      framecounter = 0;
    }
    framecounter++;
  });

  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { 
    printf("GOTRESIZE<%d %d>\n", w, h); 
  });
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