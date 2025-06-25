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
#include <ork/kernel/opq.h>
#include <atomic>
#include <mutex>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

struct SliceAnimationData {
  int slice_index;
  int tex_size;
  fvec2 scroll_dir;
  image_ptr_t current_image;
  image_ptr_t base_image;
  std::atomic<bool> needs_update{false};
  std::mutex update_mutex;
  fvec2 accumulated_scroll;
};

using sliceanimdata_ptr_t = std::shared_ptr<SliceAnimationData>;

struct Resources {
  Resources(Context* ctx)
      : _ctx(ctx) {
    auto txi = ctx->TXI();

    // Create test images with patterns that are visible when scrolling
    _tex_size        = 256;
    int stripe_width = 32;

    // Initialize slice animation data
    _slice_animations.resize(4);

    // Red slice with horizontal stripes (scroll up)
    auto img_red = std::make_shared<Image>();
#if defined(__APPLE__)
    img_red->_format = EBufferFormat::RGBA8;
    img_red->init(_tex_size, _tex_size, 4, 1);
#else
    img_red->_format = EBufferFormat::RGB8;
    img_red->init(_tex_size, _tex_size, 3, 1);
#endif
    for (int y = 0; y < _tex_size; y++) {
      for (int x = 0; x < _tex_size; x++) {
        float stripe = (y % stripe_width < (stripe_width >> 1)) ? 1.0f : 0.3f;
        auto pixel   = img_red->pixel8(x, y);
        pixel[0]     = uint8_t(stripe * 255); // R
        pixel[1]     = 0;                     // G
        pixel[2]     = 0;                     // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }

    _slice_animations[0]                = std::make_shared<SliceAnimationData>();
    _slice_animations[0]->slice_index   = 0;
    _slice_animations[0]->tex_size      = _tex_size;
    _slice_animations[0]->scroll_dir    = fvec2(0, 0.4); // scroll up
    _slice_animations[0]->base_image    = img_red;
    _slice_animations[0]->current_image = std::make_shared<Image>(*img_red);

    // Green slice with vertical stripes (scroll right)
    auto img_green = std::make_shared<Image>();
#if defined(__APPLE__)
    img_green->_format = EBufferFormat::RGBA8;
    img_green->init(_tex_size, _tex_size, 4, 1);
#else
    img_green->_format = EBufferFormat::RGB8;
    img_green->init(_tex_size, _tex_size, 3, 1);
#endif
    for (int y = 0; y < _tex_size; y++) {
      for (int x = 0; x < _tex_size; x++) {
        float stripe = (x % stripe_width < (stripe_width >> 1)) ? 1.0f : 0.3f;
        auto pixel   = img_green->pixel8(x, y);
        pixel[0]     = 0;                     // R
        pixel[1]     = uint8_t(stripe * 255); // G
        pixel[2]     = 0;                     // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }

    _slice_animations[1]                = std::make_shared<SliceAnimationData>();
    _slice_animations[1]->slice_index   = 1;
    _slice_animations[1]->tex_size      = _tex_size;
    _slice_animations[1]->scroll_dir    = fvec2(0.5, 0); // scroll right
    _slice_animations[1]->base_image    = img_green;
    _slice_animations[1]->current_image = std::make_shared<Image>(*img_green);

    // Blue slice with diagonal stripes (scroll diagonally)
    auto img_blue = std::make_shared<Image>();
#if defined(__APPLE__)
    img_blue->_format = EBufferFormat::RGBA8;
    img_blue->init(_tex_size, _tex_size, 4, 1);
#else
    img_blue->_format = EBufferFormat::RGB8;
    img_blue->init(_tex_size, _tex_size, 3, 1);
#endif
    int DDB = stripe_width * 2;
    for (int y = 0; y < _tex_size; y++) {
      for (int x = 0; x < _tex_size; x++) {
        float stripe = ((x + y) % DDB < (DDB >> 1)) ? 1.0f : 0.3f;
        auto pixel   = img_blue->pixel8(x, y);
        pixel[0]     = 0;                     // R
        pixel[1]     = 0;                     // G
        pixel[2]     = uint8_t(stripe * 255); // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }

    _slice_animations[2]                = std::make_shared<SliceAnimationData>();
    _slice_animations[2]->slice_index   = 2;
    _slice_animations[2]->tex_size      = _tex_size;
    _slice_animations[2]->scroll_dir    = fvec2(0.3,0.3); // scroll diagonally
    _slice_animations[2]->base_image    = img_blue;
    _slice_animations[2]->current_image = std::make_shared<Image>(*img_blue);

    // White slice with checkerboard pattern (scroll left and down)
    auto img_white = std::make_shared<Image>();
#if defined(__APPLE__)
    img_white->_format = EBufferFormat::RGBA8;
    img_white->init(_tex_size, _tex_size, 4, 1);
#else
    img_white->_format = EBufferFormat::RGB8;
    img_white->init(_tex_size, _tex_size, 3, 1);
#endif
    int DDW = stripe_width >> 3; // Same as arraytex1
    for (int y = 0; y < _tex_size; y++) {
      for (int x = 0; x < _tex_size; x++) {
        float checker = ((x / DDW) + (y / DDW)) % 2 ? 1.0f : 0.1f;
        auto pixel    = img_white->pixel8(x, y);
        pixel[0]      = uint8_t(checker * 255); // R
        pixel[1]      = uint8_t(checker * 255); // G
        pixel[2]      = uint8_t(checker * 255); // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }

    _slice_animations[3]                = std::make_shared<SliceAnimationData>();
    _slice_animations[3]->slice_index   = 3;
    _slice_animations[3]->tex_size      = _tex_size;
    _slice_animations[3]->scroll_dir    = fvec2(0.01, 0.025f); // scroll left and down
    _slice_animations[3]->base_image    = img_white;
    _slice_animations[3]->current_image = std::make_shared<Image>(*img_white);

    // Create texture array with initial images
    TextureArrayInitData TID;
    TID._slices.resize(4);
    TID._slices[0] = TextureArrayInitSubItem{"red"_crcu, _slice_animations[0]->current_image};
    TID._slices[1] = TextureArrayInitSubItem{"green"_crcu, _slice_animations[1]->current_image};
    TID._slices[2] = TextureArrayInitSubItem{"blue"_crcu, _slice_animations[2]->current_image};
    TID._slices[3] = TextureArrayInitSubItem{"white"_crcu, _slice_animations[3]->current_image};

    _texArray                   = std::make_shared<TextureArray>();
    _texArray->_tex->_debugName = "animated_texarray";
    txi->initTextureArray2DFromData(_texArray.get(), TID);

    // Create shader that can sample from texture arrays - same as arraytex1
    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "demo://shader.fxv2");

    _technique            = _material->technique("tek_x");
    _fxparameterMVP       = _material->param("MatMVP");
    _fxparameterTexture   = _material->param("ColorMap");
    _fxparameterQuadIndex = _material->param("QuadIndex");

    // Start animation threads
    startAnimationThreads();

    deco::printf(fvec3::White(), "Animated Texture Array Test Initialized\n");
    deco::printf(fvec3::Yellow(), "  Texture array created with %d slices\n", 4);
    deco::printf(fvec3::Yellow(), "  Each slice is %dx%d RGB8 with animated scrolling\n", _tex_size, _tex_size);
  }

  ~Resources() {
    stopAnimationThreads();
  }

  void startAnimationThreads() {
    _running = true;
    _animation_threads.resize(4);

    for (int i = 0; i < 4; ++i) {
      _animation_threads[i] = std::make_shared<ork::Thread>(FormatString("texanim_%d", i));
      _animation_threads[i]->start([this, i](anyp data) { animateSlice(i); });
    }
  }

  void stopAnimationThreads() {
    _running = false;
    for (auto& thread : _animation_threads) {
      if (thread) {
        thread->join();
      }
    }
  }

  void animateSlice(int slice_idx) {
    auto anim_data = _slice_animations[slice_idx];

    while (_running) {

      anim_data->accumulated_scroll += anim_data->scroll_dir * 0.01;
      if(0)printf(" Animating slice %d: accumulated_scroll = (%f, %f)\n",
             slice_idx,
             anim_data->accumulated_scroll.x,
             anim_data->accumulated_scroll.y);
      for (int y = 0; y < anim_data->tex_size; y++) {
        for (int x = 0; x < anim_data->tex_size; x++) {

          auto dst_pixel = anim_data->current_image->pixel8(x, y);

          // Calculate source position with proper wrapping
          int scroll_x = int(anim_data->accumulated_scroll.x);
          int scroll_y = int(anim_data->accumulated_scroll.y);

          int src_x = (x + scroll_x);
          int src_y = (y + scroll_y);
          while (src_x >= anim_data->tex_size)
            src_x -= anim_data->tex_size;
          while (src_y >= anim_data->tex_size)
            src_y -= anim_data->tex_size;
          while (src_x < 0)
            src_x += anim_data->tex_size;
          while (src_y < 0)
            src_y += anim_data->tex_size;

          auto src_pixel = anim_data->base_image->pixel8(src_x, src_y);

          dst_pixel[0] = src_pixel[0];
          dst_pixel[1] = src_pixel[1];
          dst_pixel[2] = src_pixel[2];
#if defined(__APPLE__)
          if (anim_data->current_image->_format == EBufferFormat::RGBA8) {
            dst_pixel[3] = src_pixel[3];
          }
#endif
        }
      }

      anim_data->needs_update.store(true);
      ::usleep(16<<10); // Sleep for 16ms to control animation speed
    } // while(_running)
  }

  void updateTextureSlices() {
    auto txi = _ctx->TXI();
    TextureArraySliceRef slice_ref(nullptr, 0);
    for (int i = 0; i < 4; ++i) {
      // animateSlice(i);
      auto anim_data = _slice_animations[i];
      if (anim_data->needs_update.exchange(false)) {
        // std::lock_guard<std::mutex> lock(anim_data->update_mutex);
        slice_ref._array = _texArray.get();
        slice_ref._slice = anim_data->slice_index;
        // Update texture array slice from image
        txi->updateTextureArraySlice(&slice_ref, anim_data->current_image);
      }
    }
  }

  Context* _ctx = nullptr;
  int _tex_size = 256;
  texturearray_ptr_t _texArray;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _technique        = nullptr;
  const FxShaderParam* _fxparameterMVP       = nullptr;
  const FxShaderParam* _fxparameterTexture   = nullptr;
  const FxShaderParam* _fxparameterQuadIndex = nullptr;

  std::vector<sliceanimdata_ptr_t> _slice_animations;
  std::vector<thread_ptr_t> _animation_threads;
  std::atomic<bool> _running{false};
};

using resources_ptr_t = std::shared_ptr<Resources>;

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);
  auto ezapp     = OrkEzApp::create(init_data);
  auto this_dir  = ezapp->_orkidWorkspaceDir //
                  / "ork.lev2"               //
                  / "examples"               //
                  / "c++"                    //
                  / "arraytex";
  auto filecontext = FileEnv::createContextForUriBase("demo://", this_dir);
  filecontext->SetFilesystemBaseEnable(true);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;

  Timer timer;
  timer.Start();
  resources_ptr_t resources;
  float abstime = 0.0f;

  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { resources = std::make_shared<Resources>(ctx); });

  //////////////////////////////////////////////////////////
  ezapp->onGpuUpdate([&](Context* ctx) {
    if (resources) {
      resources->updateTextureSlices();
    }
  });

  //////////////////////////////////////////////////////////
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) { abstime = updata->_abstime; });

  //////////////////////////////////////////////////////////
  int framecounter  = 0;
  int total_updates = 0;
  Timer perf_timer;
  perf_timer.Start();

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context = drwev->GetTarget();
    auto fbi     = context->FBI();

    // Clear to dark gray
    float w               = context->mainSurfaceWidth();
    float h               = context->mainSurfaceHeight();
    float aspect          = w / h;
    auto main_rtg         = fbi->_main_rtg;
    auto main_rtb         = main_rtg->buffer(0);
    main_rtb->_clearColor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);

    // Render quads showing different slices
    auto RCFD = std::make_shared<RenderContextFrameData>(context);
    resources->_material->begin(resources->_technique, RCFD);

    // Setup projection and view
    fmtx4 P, V, M;
    P.perspective(45.0f * DTOR, aspect, 0.01f, 10.0f);
    V.lookAt(fvec3(0, 0, 3), fvec3(0, 0, 0), fvec3(0, 1, 0));

    // Bind the texture array's texture
    auto tex = resources->_texArray->_tex;
    resources->_material->bindParamTexture(resources->_fxparameterTexture, tex.get());

    // Draw 4 corner quads showing animated slices, plus center quad showing average
    for (int i = 0; i < 4; ++i) {
      float x = (i % 2) * 2.0f - 1.0f;
      float y = (i / 2) * 1.5f - 0.75f;

      M.setTranslation(x, y, 0);
      resources->_material->bindParamMatrix(resources->_fxparameterMVP, P * V * M);
      resources->_material->bindParamFloat(resources->_fxparameterQuadIndex, float(i));

      // Draw quad
      appwin->Render2dQuadEML(
          fvec4(-0.5f, -0.5f, 1.0f, 1.0f), // quad in NDC
          fvec4(0, 0, 1, 1),               // uv0rect
          fvec4(0, 0, 1, 1));              // uv1rect
    }

    // Draw 5th quad in center showing average of all slices
    M.setTranslation(0, 0, 0);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P * V * M);
    resources->_material->bindParamFloat(resources->_fxparameterQuadIndex, 4.0f);

    appwin->Render2dQuadEML(
        fvec4(-0.5f, -0.5f, 1.0f, 1.0f), // quad in NDC
        fvec4(0, 0, 1, 1),               // uv0rect
        fvec4(0, 0, 1, 1));              // uv1rect

    resources->_material->end(RCFD);

    // Count texture updates
    int slice_updates = 0;
    for (auto& anim : resources->_slice_animations) {
      if (anim->needs_update) {
        slice_updates++;
      }
    }
    total_updates += slice_updates;

    // Performance stats
    if (perf_timer.SecsSinceStart() > 1.0f) {
      float elapsed         = perf_timer.SecsSinceStart();
      float FPS             = float(framecounter) / elapsed;
      float updates_per_sec = float(total_updates) / elapsed;
      float tex_size        = resources->_tex_size;
      float mpix_per_slice  = (tex_size * tex_size) / 1e6f;
      float mpix_per_sec    = mpix_per_slice * updates_per_sec;
      float mb_per_sec      = mpix_per_sec * 3; // RGB8 = 3 bytes per pixel

      deco::printf(fvec3::White(), "FPS: %.1f | ", FPS);
      deco::printf(fvec3::Yellow(), "Array Slice Updates/sec: %.1f | ", updates_per_sec);
      deco::printf(fvec3::Cyan(), "MPix/sec: %.1f | ", mpix_per_sec);
      deco::printf(fvec3::Magenta(), "MB/sec: %.1f | ", mb_per_sec);
      deco::printf(fvec3::Green(), "(Testing updateTextureArraySlice)\n");

      perf_timer.Start();
      framecounter  = 0;
      total_updates = 0;
    }
    framecounter++;
  });

  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { deco::printf(fvec3::Cyan(), "Window resized to %dx%d\n", w, h); });

  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->_eventcode) {
      case ui::EventCode::KEY_DOWN:
        switch (ev->miKeyCode) {
          case 'R':
            deco::printf(fvec3::Green(), "Resetting animation positions\n");
            for (auto& anim : resources->_slice_animations) {
              anim->accumulated_scroll = fvec2(0, 0);
            }
            break;
          case '+':
          case '=':
            resources->_tex_size = std::min(resources->_tex_size * 2, 2048);
            deco::printf(fvec3::Yellow(), "Texture size increased to %dx%d\n", resources->_tex_size, resources->_tex_size);
            // Would need to recreate texture array with new size
            break;
          case '-':
          case '_':
            resources->_tex_size = std::max(resources->_tex_size / 2, 64);
            deco::printf(fvec3::Yellow(), "Texture size decreased to %dx%d\n", resources->_tex_size, resources->_tex_size);
            // Would need to recreate texture array with new size
            break;
        }
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