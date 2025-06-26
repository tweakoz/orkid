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
#include <ork/kernel/memcpy.inl>
#include <atomic>
#include <mutex>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

constexpr bool DO_ASYNC_SLICE_UPDATE = true;
constexpr int TEX_SIZE               = 2048;
constexpr int ROWS_PER_CHUNK         = TEX_SIZE >> 5;

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

    // Initialize slice animation data
    _slice_animations.resize(4);

    auto create_image = [this](const std::string& name) {
      auto img = std::make_shared<Image>();
#if defined(__APPLE__)
      img->_format = EBufferFormat::RGBA8;
      img->init(TEX_SIZE, TEX_SIZE, 4, 1);
#else
      img->_format = EBufferFormat::RGB8;
      img->init(TEX_SIZE, TEX_SIZE, 3, 1);
#endif
      img->_debugName = name;
      return img;
    };

    // Red slice with horizontal stripes (scroll up)
    auto img_red = create_image("red_slice");
    auto img_grn = create_image("grn_slice");
    auto img_blu = create_image("blu_slice");
    auto img_whi = create_image("whi_slice");

    int stripe_width = 256;
    for (int y = 0; y < TEX_SIZE; y++) {
      for (int x = 0; x < TEX_SIZE; x++) {
        float stripe = (y % stripe_width < (stripe_width >> 1)) ? 0.5f : 0.0f;
        auto pixel   = img_red->pixel8(x, y);
        pixel[0]     = uint8_t(stripe * 255); // R
        pixel[1]     = 0;                     // G
        pixel[2]     = 0;                     // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }
    stripe_width = 256;
    for (int y = 0; y < TEX_SIZE; y++) {
      for (int x = 0; x < TEX_SIZE; x++) {
        float stripe = (x % stripe_width < (stripe_width >> 1)) ? 0.5f : 0.0f;
        auto pixel   = img_grn->pixel8(x, y);
        pixel[0]     = 0;                     // R
        pixel[1]     = uint8_t(stripe * 255); // G
        pixel[2]     = 0;                     // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }
    stripe_width = 512;
    int DDB      = stripe_width * 2;
    for (int y = 0; y < TEX_SIZE; y++) {
      for (int x = 0; x < TEX_SIZE; x++) {
        float stripe = ((x + y) % DDB < (DDB >> 1)) ? 0.75f : 0.0f;
        auto pixel   = img_blu->pixel8(x, y);
        pixel[0]     = 0;                     // R
        pixel[1]     = 0;                     // G
        pixel[2]     = uint8_t(stripe * 255); // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }
    stripe_width = 16;
    int DDW      = stripe_width; // Same as arraytex1
    for (int y = 0; y < TEX_SIZE; y++) {
      for (int x = 0; x < TEX_SIZE; x++) {
        float checker = ((x / DDW) + (y / DDW)) % 2 ? 0.25f : 0.0f;
        auto pixel    = img_whi->pixel8(x, y);
        pixel[0]      = uint8_t(checker * 255); // R
        pixel[1]      = uint8_t(checker * 255); // G
        pixel[2]      = uint8_t(checker * 255); // B
#if defined(__APPLE__)
        pixel[3] = 255; // A
#endif
      }
    }

    _slice_animations[0]                = std::make_shared<SliceAnimationData>();
    _slice_animations[0]->slice_index   = 0;
    _slice_animations[0]->tex_size      = TEX_SIZE;
    _slice_animations[0]->scroll_dir    = fvec2(0.12, 0.4); // scroll up
    _slice_animations[0]->base_image    = img_red;
    _slice_animations[0]->current_image = create_image("red_slice_current");

    _slice_animations[1]                = std::make_shared<SliceAnimationData>();
    _slice_animations[1]->slice_index   = 1;
    _slice_animations[1]->tex_size      = TEX_SIZE;
    _slice_animations[1]->scroll_dir    = fvec2(1.5, 0.6); // scroll right
    _slice_animations[1]->base_image    = img_grn;
    _slice_animations[1]->current_image = create_image("rgn_slice_current");

    // Blue slice with diagonal stripes (scroll diagonally)
    auto img_blue = std::make_shared<Image>();

    _slice_animations[2]                = std::make_shared<SliceAnimationData>();
    _slice_animations[2]->slice_index   = 2;
    _slice_animations[2]->tex_size      = TEX_SIZE;
    _slice_animations[2]->scroll_dir    = fvec2(0.6, 0); // scroll diagonally
    _slice_animations[2]->base_image    = img_blu;
    _slice_animations[2]->current_image = create_image("blu_slice_current");

    // White slice with checkerboard pattern (scroll left and down)

    _slice_animations[3]                = std::make_shared<SliceAnimationData>();
    _slice_animations[3]->slice_index   = 3;
    _slice_animations[3]->tex_size      = TEX_SIZE;
    _slice_animations[3]->scroll_dir    = fvec2(0.01, 0.025f) * 0.0f; // scroll left and down
    _slice_animations[3]->base_image    = img_whi;
    _slice_animations[3]->current_image = create_image("whi_slice_current");

    // Create texture array with initial images
    TextureArrayInitData TID;
    TID._slices.resize(4);
    TID._slices[0] = TextureArrayInitSubItem{"red"_crcu, _slice_animations[0]->current_image};
    TID._slices[1] = TextureArrayInitSubItem{"green"_crcu, _slice_animations[1]->current_image};
    TID._slices[2] = TextureArrayInitSubItem{"blue"_crcu, _slice_animations[2]->current_image};
    TID._slices[3] = TextureArrayInitSubItem{"white"_crcu, img_whi};

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
    deco::printf(fvec3::Yellow(), "  Each slice is %dx%d RGB8 with animated scrolling\n", TEX_SIZE, TEX_SIZE);
  }

  ~Resources() {
    stopAnimationThreads();
  }

  void startAnimationThreads() {
    _running = true;
    _animation_threads.resize(4);

    for (int i = 0; i < 3; ++i) {
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
    auto wrap_val  = [](int inp) -> int {
      while (inp < 0)
        inp += TEX_SIZE;
      inp = inp % TEX_SIZE;
      return inp;
    };
    auto txi = _ctx->TXI();

    auto task = opq::createCompletionGroup( //
      opq::concurrentQueue(), //
      "AnimationTask_" + std::to_string(slice_idx) //
    );
    task->dontReportToUI();

    while (_running) {

      anim_data->accumulated_scroll += anim_data->scroll_dir; // Update scroll based on direction and time step

      // Handle negative values from fmod

      int scroll_x = wrap_val(anim_data->accumulated_scroll.x);
      int scroll_y = wrap_val(anim_data->accumulated_scroll.y);
      auto image   = anim_data->current_image;

      // Determine bytes per pixel
      int bytes_per_pixel = 3;
#if defined(__APPLE__)
      if (image->_format == EBufferFormat::RGBA8) {
        bytes_per_pixel = 4;
      }
#endif

      for (int y = 0; y < TEX_SIZE; y += ROWS_PER_CHUNK) {
        int chunk_start = y;
        int chunk_end   = std::min(y + ROWS_PER_CHUNK, TEX_SIZE);

        task->enqueue([=]() {
          for (int row = chunk_start; row < chunk_end; row++) {
            int src_y = (row + scroll_y) & (TEX_SIZE - 1);

            // Handle wrapped row copy
            // Complex case: horizontal wrapping required
            auto dst_row = image->pixel8(0, row);

            // Copy the wrapped portion from scroll_x to end
            int pixels_from_scroll = TEX_SIZE - scroll_x;
            auto src_start         = anim_data->base_image->pixel8(scroll_x, src_y);
            memcpy(dst_row, src_start, pixels_from_scroll * bytes_per_pixel);

            // Copy the wrapped portion from beginning to scroll_x
            auto src_wrap = anim_data->base_image->pixel8(0, src_y);
            memcpy(dst_row + (pixels_from_scroll * bytes_per_pixel), src_wrap, scroll_x * bytes_per_pixel);
          }
        });
      }
      task->join(false);
      if (DO_ASYNC_SLICE_UPDATE) {
        TextureArraySliceRef slice_ref(_texArray.get(), slice_idx);
        txi->updateTextureArraySlice(&slice_ref, anim_data->current_image);
      } else {
        anim_data->needs_update.store(true);
      }
      _slice_updates.fetch_add(1);
      // usleep(4 << 10);

    } // while(_running)
  }

  Context* _ctx = nullptr;
  texturearray_ptr_t _texArray;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _technique        = nullptr;
  const FxShaderParam* _fxparameterMVP       = nullptr;
  const FxShaderParam* _fxparameterTexture   = nullptr;
  const FxShaderParam* _fxparameterQuadIndex = nullptr;

  std::vector<sliceanimdata_ptr_t> _slice_animations;
  std::vector<thread_ptr_t> _animation_threads;
  std::atomic<bool> _running{false};
  std::atomic<int> _slice_updates{0}; // Count of texture updates per frame
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
  ezapp->onGpuUpdate([&](Context* ctx) { //
    if (not DO_ASYNC_SLICE_UPDATE) {
      // update slices synchronously (on GPU thread)
      auto txi = ctx->TXI();
      TextureArraySliceRef slice_ref(nullptr, 0);
      for (int i = 0; i < 4; ++i) {
        auto anim_data = resources->_slice_animations[i];
        if (anim_data->needs_update.exchange(false)) {
          slice_ref._array = resources->_texArray.get();
          slice_ref._slice = anim_data->slice_index;
          txi->updateTextureArraySlice(&slice_ref, anim_data->current_image);
        }
      }
    } // if(not DO_ASYNC_SLICE_UPDATE)
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

    // Performance stats
    if (perf_timer.SecsSinceStart() > 1.0f) {
      int total_updates     = resources->_slice_updates.exchange(0);
      float elapsed         = perf_timer.SecsSinceStart();
      float FPS             = float(framecounter) / elapsed;
      float updates_per_sec = float(total_updates) / elapsed;
      float tex_size        = TEX_SIZE;
      float mpix_per_slice  = (tex_size * tex_size) / 1e6f;
      float mpix_per_sec    = mpix_per_slice * updates_per_sec;
      float mb_per_sec      = mpix_per_sec * 4; // RGBA8 = 4 bytes per pixel

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
      case ui::EventCode::DOUBLECLICK:
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