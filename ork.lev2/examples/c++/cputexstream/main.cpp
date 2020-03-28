#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.inl>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
int main(int argc, char** argv) {
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  const FxShaderParam* fxparameterTexture = nullptr;
  auto texture = new Texture;
  constexpr int DIM = 2048;
  auto texturedata = std::make_shared<std::vector<float>>();
  texturedata->resize(DIM*DIM*4);
  texture->_debugName = "cpugeneratedtexture";
  int appstate = 0;
  Timer timer;
  timer.Start();
  //////////////////////////////////////////////////////////
  // update texels on CPU (in parallel)
  //////////////////////////////////////////////////////////
  float finv = 1.0f/256.0f;
  float finvdim = 1.0f/float(DIM);
  auto cpugenthread = std::make_shared<ork::Thread>("cpugenthread");
  cpugenthread->start([=,&appstate](anyp data){
    float fi = 0.0f;
    while(0==appstate){
      for( int y=0; y<DIM; y++ ){
        float fy = float(y)*finvdim;
        opq::concurrentQueue().enqueue([=](){
          int index = y*DIM*4;
          float* ptexels = texturedata->data();
          for( int x=0; x<DIM; x++ ){
            float fx = float(x)*finvdim;
            ptexels[index+0] = sinf(fx*PI2*2.1f+fi)*0.5+0.5f;
            ptexels[index+1] = cosf(fx*PI2*3.13f+fi)*0.5+0.5f;
            ptexels[index+2] = sinf(fy*PI2*4.17f+fi+fx*3.19f)*0.5+0.5f;
            ptexels[index+3] = 1.0f;
            index+=4;
          }
        });
      }
      opq::concurrentQueue().drain();
      fi += 0.01f;
    }
    appstate=2;
  });
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxtechnique     = material.technique("texcolor");
    fxparameterMVP  = material.param("MatMVP");
    fxparameterTexture = material.param("ColorMap");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterTexture<%p>\n", fxparameterTexture);
  });
  //////////////////////////////////////////////////////////
  int framecounter = 0;
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context        = drwev.GetTarget();
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
    tid._w = DIM;
    tid._h = DIM;
    tid._format = EBufferFormat::RGBA32F;
    tid._autogenmips = false;
    tid._data =  texturedata->data();

    txi->initTextureFromData(texture,tid);

    RenderContextFrameData RCFD(context);
    material.bindTechnique(fxtechnique);
    material.begin(RCFD);
    material.bindParamMatrix(fxparameterMVP, fmtx4::Identity);
    material.bindParamCTex(fxparameterTexture, texture);
    gfxwin->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
    material.end(RCFD);
    context->endFrame();

    if( timer.SecsSinceStart()>5.0f ){
      float FPS = float(framecounter)/timer.SecsSinceStart();
      float MPPS = FPS*float(DIM*DIM)/1e6;
      float MiBPPS = FPS*float(DIM*DIM*16)/float(1<<20);
      deco::printf( fvec3::White(), "Frames/Sec<%g> ", FPS);
      deco::printf( fvec3::Magenta(), "MPIX/Sec<%g> ", MPPS);
      deco::printf( fvec3::Yellow(), "MiB/Sec<%g>\n", MiBPPS);
      timer.Start();
      framecounter = 0;
    }
    framecounter++;

  });

  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](const ui::Event& ev) -> ui::HandlerResult {
    switch (ev.mEventCode) {
      case ui::UIEV_DOUBLECLICK:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST,-1});
  int rval = qtapp->runloop();
  appstate=1;
  while(1==appstate) usleep(1);
  opq::concurrentQueue().drain();
}
