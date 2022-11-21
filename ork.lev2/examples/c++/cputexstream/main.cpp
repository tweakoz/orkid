////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

constexpr int DIM           = 2048;
constexpr float finv        = 1.0f / 256.0f;
constexpr float finvdim     = 1.0f / float(DIM);

using float_vect_t = std::vector<float>;


struct Resources {

  Resources(Context* ctx){
    _texturedata = std::make_shared<float_vect_t>();
    _texture = std::make_shared<Texture>();
    _texturedata->resize(DIM * DIM * 4);
    _texture->_debugName = "cpugeneratedtexture";

    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _fxtechnique        = _material->technique("texcolor");
    _fxparameterMVP     = _material->param("MatMVP");
    _fxparameterTexture = _material->param("ColorMap");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, _fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", _fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", _fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterTexture<%p>\n", _fxparameterTexture);

    //////////////////////////////////////////////////////////
    // update texels on CPU (in parallel)
    //////////////////////////////////////////////////////////
    _texupdthread = std::make_shared<ork::Thread>("cpugenthread");
    _texupdthread->start([this](anyp data) {
      float fi = 0.0f;
      this->_appstate = "THREAD_RUNNING"_crcu;
      while ("THREAD_RUNNING"_crcu == this->_appstate) {
        for (int y = 0; y < DIM; y++) {
          float fy = float(y) * finvdim;
          opq::concurrentQueue()->enqueue([=]() {
            int index      = y * DIM * 4;
            float* ptexels = this->_texturedata->data();
            for (int x = 0; x < DIM; x++) {
              float fx           = float(x) * finvdim;
              ptexels[index + 0] = sinf(fx * PI2 * 2.1f + fi) * 0.5 + 0.5f;
              ptexels[index + 1] = cosf(fx * PI2 * 3.13f + fi) * 0.5 + 0.5f;
              ptexels[index + 2] = sinf(fy * PI2 * 4.17f + fi + fx * 3.19f) * 0.5 + 0.5f;
              ptexels[index + 3] = 1.0f;
              index += 4;
            }
          });
        }
        opq::concurrentQueue()->drain();
        fi += 0.01f;
      }
      this->_appstate = "THREAD_DONE"_crcu;
    });   
  }

  ~Resources(){
      _appstate = "KILL_THREAD"_crcu;
      while(_appstate!="THREAD_DONE"_crcu){
        usleep(0);
      }
  }

  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _fxtechnique    = nullptr;
  const FxShaderParam* _fxparameterMVP     = nullptr;
  const FxShaderParam* _fxparameterTexture = nullptr;
  texture_ptr_t _texture;
  std::shared_ptr<float_vect_t> _texturedata;
  uint32_t _appstate = "INIT_THREAD"_crcu;
  thread_ptr_t _texupdthread;


};

using resources_ptr_t = std::shared_ptr<Resources>;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto qtapp  = OrkEzApp::create(init_data);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  Timer timer;
  timer.Start();
  resources_ptr_t resources;
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx);
  });
  //////////////////////////////////////////////////////////
  int framecounter = 0;
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context        = drwev->GetTarget();
    auto fbi            = context->FBI(); // FrameBufferInterface
    auto fxi            = context->FXI(); // FX Interface
    auto txi            = context->TXI(); // Texture Interface
    float r             = float(rand() % 256) / 255.0f;
    float g             = float(rand() % 256) / 255.0f;
    float b             = float(rand() % 256) / 255.0f;
    int TARGW           = context->mainSurfaceWidth();
    int TARGH           = context->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);

    fbi->SetClearColor(fvec4(r, g, b, 1));
    context->beginFrame();

    TextureInitData tid;
    tid._w           = DIM;
    tid._h           = DIM;
    tid._src_format      = EBufferFormat::RGBA32F;
    tid._dst_format      = EBufferFormat::RGBA32F;
    tid._autogenmips = false;
    tid._data        = resources->_texturedata->data();

    txi->initTextureFromData(resources->_texture.get(), tid);

    RenderContextFrameData RCFD(context);
    resources->_material->begin(resources->_fxtechnique, RCFD);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, fmtx4::Identity());
    resources->_material->bindParamCTex(resources->_fxparameterTexture, resources->_texture.get());
    gfxwin->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
    resources->_material->end(RCFD);
    context->endFrame();

    if (timer.SecsSinceStart() > 5.0f) {
      float FPS    = float(framecounter) / timer.SecsSinceStart();
      float MPPS   = FPS * float(DIM * DIM) / 1e6;
      float MiBPPS = FPS * float(DIM * DIM * 16) / float(1 << 20);
      deco::printf(fvec3::White(), "Frames/Sec<%g> ", FPS);
      deco::printf(fvec3::Magenta(), "MPIX/Sec<%g> ", MPPS);
      deco::printf(fvec3::Yellow(), "MiB/Sec<%g>\n", MiBPPS);
      timer.Start();
      framecounter = 0;
    }
    framecounter++;
  });

  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
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
  qtapp->onGpuExit([&](Context* ctx) {
    resources = nullptr;
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  int rval = qtapp->mainThreadLoop();
  opq::concurrentQueue()->drain();
}
