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

struct RenderData {
  RenderData(int dim) : _DIM(dim) {
    _texturedata = std::make_shared<float_vect_t>();
    _texturedata->resize(_DIM * _DIM * 4);
  }
  float invdim() const {
    return 1.0f / float(_DIM);
  }
  void update(compgroup_ptr_t cgroup, float fi){
    //printf("update rdata<%p> fi<%f>\n", this, fi);
    float finvdim = this->invdim();
    float* ptexels = _texturedata->data();
    for (int y = 0; y < _DIM; y++) {
      float fy = float(y) * finvdim;
      cgroup->enqueue([=]() {
        int index      = y * _DIM * 4;
        for (int x = 0; x < _DIM; x++) {
          float fx           = float(x) * finvdim;
          ptexels[index + 0] = sinf(fx * PI2 * 2.1f + fi) * 0.5 + 0.5f;
          ptexels[index + 1] = cosf(fx * PI2 * 3.13f + fi) * 0.5 + 0.5f;
          ptexels[index + 2] = sinf(fy * PI2 * 4.17f + fi + fx * 3.19f) * 0.5 + 0.5f;
          ptexels[index + 3] = 1.0f;
          index += 4;
        }
      });
    }
    cgroup->join();
  }
  int _DIM = 2048;
  std::shared_ptr<float_vect_t> _texturedata;
};

using renderdata_ptr_t = std::shared_ptr<RenderData>;

struct Resources {

  Resources(Context* ctx){
    _renderdata = std::make_shared<RenderData>(1024);
    _texture = std::make_shared<Texture>();
    _texture->_debugName = "cpugeneratedtexture";

    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _fxtechnique        = _material->technique("texcolor");
    OrkAssert(_fxtechnique != nullptr);
    _fxparameterMVP     = _material->param("MatMVP");
    OrkAssert(_fxparameterMVP != nullptr);
    _fxparameterTexture = _material->param("ColorMap");
    OrkAssert(_fxparameterTexture != nullptr);
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, _fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", _fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", _fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterTexture<%p>\n", _fxparameterTexture);


    //_renderpass = std::make_shared<RenderPass>();
    //////////////////////////////////////////////////////////
    // update texels on CPU (in parallel)
    //////////////////////////////////////////////////////////
    _texupdthread = std::make_shared<ork::Thread>("cpugenthread");
    _texupdthread->start([this](anyp data) {
      float fi = 0.0f;
      this->_appstate = "THREAD_RUNNING"_crcu;
      auto cgroup = opq::createCompletionGroup(
          opq::concurrentQueue(), //
          "cpugen");

      _timer.Start();
      while ("THREAD_RUNNING"_crcu == this->_appstate) {
        if(_timer.SecsSinceStart() > 2.0f) {
          int dim = _renderdata->_DIM;
          dim <<= 1; // double the size
          if(dim > 4096) {
            dim = 128; // reset to 128
          }
          printf("texture dimensions changed to %d x %d\n", dim, dim);
          _renderdata = std::make_shared<RenderData>(dim);
          _timer.Start();
        }
        _renderdata->update(cgroup.get(),fi);
        fi += 0.07f;
        _framecounter++;
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
  renderdata_ptr_t _renderdata;
  uint32_t _appstate = "INIT_THREAD"_crcu;
  thread_ptr_t _texupdthread;
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
  renderdata_ptr_t _current_renderdata;
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx);
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuUpdate([&](Context* ctx) {
    _current_renderdata = resources->_renderdata;
    //printf("_current_renderdata<%p>\n", _current_renderdata.get());
    const int DIM = _current_renderdata->_DIM;
    TextureInitData tid;
    auto txi         = ctx->TXI(); // Texture Interface
    tid._w           = DIM;
    tid._h           = DIM;
    tid._src_format  = EBufferFormat::RGBA32F;
    tid._dst_format  = EBufferFormat::RGBA32F;
    tid._autogenmips = false;
    tid._data        = _current_renderdata->_texturedata->data();
    txi->initTextureFromData(resources->_texture.get(), tid);
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
    auto RCFD = std::make_shared<RenderContextFrameData>(context);
    resources->_material->begin(resources->_fxtechnique, RCFD);
    fmtx4 P, V, M;
    P.perspective(45.0f, aspect, 0.01f, 10.0f);
    V.lookAt( fvec3(0, 0, 2),  // eye
              fvec3(0, 0, 0),  // target
              fvec3(0, 1, 0)); // up
    M.rotateOnZ(abstime*0.25f);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P*V*M);
    resources->_material->bindParamTexture(resources->_fxparameterTexture, resources->_texture.get());
    appwin->Render2dQuadEML( fvec4(-.75, -.75, 1.5, 1.5), // quad in NDC
                             fvec4(0, 0, 1, 1),   // uv0rect
                             fvec4(0, 0, 1, 1));  // uv1rect
    resources->_material->end(RCFD);    

    //::usleep(1<<20); // sleep 1ms to avoid hogging the CPU
    const int DIM = _current_renderdata->_DIM;
    if (timer.SecsSinceStart() > 1.0f) {
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
