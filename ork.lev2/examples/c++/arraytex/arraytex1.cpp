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
#include <ork/lev2/gfx/image.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;


struct Resources {

Resources(Context* ctx){
    _ctx = ctx;
    auto txi = ctx->TXI();

    // Create test images with patterns that are visible when averaged
    int tex_size = 256;
    int stripe_width = 64;
    // Red slice with horizontal stripes
    auto img_red = std::make_shared<Image>();
    img_red->_format = EBufferFormat::RGB8;
    img_red->init(tex_size, tex_size, 3, 1); // 3 components, 1 byte per channel
    for (int y = 0; y < tex_size; y++) {
      for (int x = 0; x < tex_size; x++) {
        float stripe = (y % stripe_width < (stripe_width>>1)) ? 0.65f : 0.0f;
        auto pixel = img_red->pixel8(x, y);
        pixel[0] = uint8_t(stripe * 255);  // R
        pixel[1] = 0;                      // G
        pixel[2] = 0;                      // B
      }
    }
    
    // Green slice with vertical stripes
    auto img_green = std::make_shared<Image>();
    img_green->_format = EBufferFormat::RGB8;
    img_green->init(tex_size, tex_size, 3, 1);
    for (int y = 0; y < tex_size; y++) {
      for (int x = 0; x < tex_size; x++) {
        float stripe = (x % stripe_width < (stripe_width>>1)) ? 0.65f : 0.0f;
        auto pixel = img_green->pixel8(x, y);
        pixel[0] = 0;                      // R
        pixel[1] = uint8_t(stripe * 255);  // G
        pixel[2] = 0;                      // B
      }
    }
    
    // Blue slice with diagonal stripes
    auto img_blue = std::make_shared<Image>();
    img_blue->_format = EBufferFormat::RGB8;
    img_blue->init(tex_size, tex_size, 3, 1);
    int DDB = stripe_width<<2;
    for (int y = 0; y < tex_size; y++) {
      for (int x = 0; x < tex_size; x++) {
        float stripe = ((x + y) % DDB < (DDB>>1)) ? 0.65f : 0.0f;
        auto pixel = img_blue->pixel8(x, y);
        pixel[0] = 0;                      // R
        pixel[1] = 0;                      // G
        pixel[2] = uint8_t(stripe * 255);  // B
      }
    }
    
    // White slice with checkerboard pattern
    auto img_white = std::make_shared<Image>();
    img_white->_format = EBufferFormat::RGB8;
    img_white->init(tex_size, tex_size, 3, 1);
    int DDW = stripe_width>>3;
    for (int y = 0; y < tex_size; y++) {
      for (int x = 0; x < tex_size; x++) {
        float checker = ((x / DDW) + (y / DDW)) % 2 ? 0.1f : 0.0f;
        auto pixel = img_white->pixel8(x, y);
        pixel[0] = uint8_t(checker * 255);  // R
        pixel[1] = uint8_t(checker * 255);  // G
        pixel[2] = uint8_t(checker * 255);  // B
      }
    }

    // Rest of the code remains the same...
    // Create texture array
    TextureArrayInitData TID;
    TID._slices.resize(4);
    TID._slices[0] = TextureArrayInitSubItem{"red"_crcu, img_red};
    TID._slices[1] = TextureArrayInitSubItem{"green"_crcu, img_green};
    TID._slices[2] = TextureArrayInitSubItem{"blue"_crcu, img_blue};
    TID._slices[3] = TextureArrayInitSubItem{"white"_crcu, img_white};
    
    _texArray = std::make_shared<TextureArray>();
    _texArray->_tex->_debugName = "test_texarray";
    txi->initTextureArray2DFromData(_texArray.get(), TID);

    // Create shader that can sample from texture arrays
    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "demo://shader.fxv2");
    
    _technique = _material->technique("tek_x");
    _fxparameterMVP = _material->param("MatMVP");
    _fxparameterTexture = _material->param("ColorMap");
    _fxparameterQuadIndex = _material->param("QuadIndex");
    
    deco::printf(fvec3::White(), "Texture Array Test Initialized\n");
    deco::printf(fvec3::Yellow(), "  Texture array created with %d slices\n", 4);
    deco::printf(fvec3::Yellow(), "  Each slice is %dx%d RGB8 with patterns\n", tex_size, tex_size);
  }

  Context* _ctx = nullptr;
  texturearray_ptr_t _texArray;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _fxparameterMVP = nullptr;
  const FxShaderParam* _fxparameterTexture = nullptr;
  const FxShaderParam* _fxparameterQuadIndex = nullptr;
};

using resources_ptr_t = std::shared_ptr<Resources>;

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);
  auto ezapp = OrkEzApp::create(init_data);
  auto this_dir = ezapp->_orkidWorkspaceDir //
                  / "ork.lev2"               //
                  / "examples"              //
                  / "c++"                   //
                  / "arraytex";
  auto filecontext = FileEnv::createContextForUriBase("demo://", this_dir);
  filecontext->SetFilesystemBaseEnable(true);
  auto ezwin = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;
  
  Timer timer;
  timer.Start();
  resources_ptr_t resources;
  float abstime = 0.0f;
  int current_slice = 0;
  
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx);
  });
  
  //////////////////////////////////////////////////////////
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    abstime = updata->_abstime;
    // Cycle through texture array slices every second
    current_slice = int(abstime) % 4;
  });
  
  //////////////////////////////////////////////////////////
  int framecounter = 0;
ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto fbi = context->FBI();
  
  // Clear to dark gray
  float w = context->mainSurfaceWidth();
  float h = context->mainSurfaceHeight();
  float aspect = w / h;
  auto main_rtg = fbi->_main_rtg;
  auto main_rtb = main_rtg->buffer(0);
  main_rtb->_clearColor = fvec4(0.2f, 0.2f, 0.2f, 1.0f);
  
  // Render quads showing different slices of the texture array
  auto RCFD = std::make_shared<RenderContextFrameData>(context);
  resources->_material->begin(resources->_technique, RCFD);
  
  // Setup projection and view
  fmtx4 P, V, M;
  P.perspective(45.0f * DTOR, aspect, 0.01f, 10.0f);
  V.lookAt(fvec3(0, 0, 3), fvec3(0, 0, 0), fvec3(0, 1, 0));
  
  // Bind texture array once
  auto tex = resources->_texArray->_tex;
  //resources->_material->bindParamTextureArray(resources->_fxparameterTexture, tex.get());
  resources->_material->bindParamTexture(resources->_fxparameterTexture, tex.get());

  // Draw 4 corner quads, each showing a different slice
  for (int i = 0; i < 4; ++i) {
    float x = (i % 2) * 2.0f - 1.0f;
    float y = (i / 2) * 1.5f - 0.75f;
    
    M.setTranslation(x, y, 0);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P * V * M);
    resources->_material->bindParamFloat(resources->_fxparameterQuadIndex, float(i));
    
    // Draw quad
    appwin->Render2dQuadEML(
      fvec4(-0.5f, -0.5f, 1.0f, 1.0f), // quad in NDC
      fvec4(0, 0, 1, 1),                // uv0rect
      fvec4(0, 0, 1, 1));               // uv1rect
  }
  
  // Draw 5th quad in center showing average of all slices
  M.setTranslation(0, 0, 0);
  resources->_material->bindParamMatrix(resources->_fxparameterMVP, P * V * M);
  resources->_material->bindParamFloat(resources->_fxparameterQuadIndex, 4.0f);
  
  appwin->Render2dQuadEML(
    fvec4(-0.5f, -0.5f, 1.0f, 1.0f), // quad in NDC
    fvec4(0, 0, 1, 1),                // uv0rect
    fvec4(0, 0, 1, 1));               // uv1rect
  
  resources->_material->end(RCFD);
  
  // FPS counter
  if (timer.SecsSinceStart() > 1.0f) {
    float FPS = float(framecounter) / timer.SecsSinceStart();
    deco::printf(fvec3::White(), "FPS: %.1f | Corner quads show individual slices, center shows average\n", FPS);
    timer.Start();
    framecounter = 0;
  }
  framecounter++;
});
  
  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { 
    deco::printf(fvec3::Cyan(), "Window resized to %dx%d\n", w, h); 
  });
  
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->_eventcode) {
      case ui::EventCode::DOUBLECLICK:
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  
  //////////////////////////////////////////////////////////
  ezapp->onGpuExit([&](Context* ctx) {
    deco::printf(fvec3::Yellow(), "GPU Exit - cleaning up resources\n");
    resources = nullptr;
  });
  
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}