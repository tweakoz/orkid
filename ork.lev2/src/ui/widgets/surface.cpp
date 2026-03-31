#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/pickbuffer.h>

namespace ork { namespace ui {

constexpr bool DEBUG_BLIT = false;

/////////////////////////////////////////////////////////////////////////
// SSAA support
/////////////////////////////////////////////////////////////////////////

void Surface::_initSsaa(lev2::Context* ctx) {
  if (_ssaa_initialized) return;
  _ssaa_initialized = true;

  _ssaa_blit_mtl.gpuInit(ctx, "orkshader://blit");
  _ssaa_blit_mtl._rasterstate->setCullTest(lev2::ECullTest::OFF);
  _ssaa_tek[0] = _ssaa_blit_mtl.technique("blit");           // 1x1 (passthrough)
  _ssaa_tek[1] = _ssaa_blit_mtl.technique("downsample_2x2");
  _ssaa_tek[2] = _ssaa_blit_mtl.technique("downsample_3x3");
  _ssaa_tek[3] = _ssaa_blit_mtl.technique("downsample_4x4");
  _ssaa_tek[4] = _ssaa_blit_mtl.technique("downsample_5x5");
  _ssaa_tek[5] = _ssaa_blit_mtl.technique("downsample_6x6");
  _ssaa_par_mvp      = _ssaa_blit_mtl.param("MatMVP");
  _ssaa_par_colormap  = _ssaa_blit_mtl.param("ColorMap");

  _ssaa_resolve_rtg = std::make_shared<lev2::RtGroup>(ctx, 8, 8, lev2::MsaaSamples::MSAA_1X);
  _ssaa_resolve_rtg->createRenderTarget(lev2::EBufferFormat::RGBA8);
}

void Surface::_ssaaResolve(lev2::Context* ctx, int dst_w, int dst_h) {
  auto fbi = ctx->FBI();
  auto gbi = ctx->GBI();

  if (_ssaa_resolve_rtg->width() != dst_w || _ssaa_resolve_rtg->height() != dst_h) {
    _ssaa_resolve_rtg->Resize(dst_w, dst_h);
  }

  auto ssaa_tex = _rtgroup->buffer(0)->texture();
  _ssaa_resolve_rtg->_autoclear = false;
  fbi->PushRtGroup(_ssaa_resolve_rtg.get());
  {
    auto rcfd = ctx->topRenderContextFrameData();
    auto& mtl = _ssaa_blit_mtl;
    mtl._rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    mtl._rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    mtl._rasterstate->_force = true;

    int tek_idx = std::clamp(_supersample, 0, 5);
    mtl.begin(_ssaa_tek[tek_idx], rcfd);
    mtl.bindParamTexture(_ssaa_par_colormap, ssaa_tex);
    mtl.bindParamMatrix(_ssaa_par_mvp, fmtx4::Identity());

    lev2::ViewportRect resolve_vp(0, 0, dst_w, dst_h);
    fbi->pushViewport(resolve_vp);
    fbi->pushScissor(resolve_vp);
    gbi->render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
    fbi->popScissor();
    fbi->popViewport();
    mtl.end(rcfd);
  }
  fbi->PopRtGroup();
}

lev2::Texture* Surface::_resolvedTexture() {
  if (_supersample > 0 && _ssaa_resolve_rtg) {
    return _ssaa_resolve_rtg->buffer(0)->texture();
  }
  return _rtgroup->buffer(0)->texture();
}

/////////////////////////////////////////////////////////////////////////

Surface::Surface(const std::string& name, int x, int y, int w, int h, fcolor4 color, F32 depth)
    : Group(name, x, y, w, h)
    , mbClear(true)
    , _clearColor(color)
    , mfClearDepth(depth)
    , mNeedsSurfaceRepaint(true)
    , _pickbuffer(nullptr) {

  _flipY = true; // on vulkan we need to flip the UVs
}

///////////////////////////////////////////////////////////////////////////////

void Surface::GetPixel(int ix, int iy, lev2::PixelFetchContext& pfc) {
  int iW   = width();
  int iH   = height();
  float fx = float(ix) / float(iW);
  float fy = float(iy) / float(iH);
  /////////////////////////////////////////////////////////////
  if (_pickbuffer) {
    auto tgt     = _pickbuffer->context();
    auto fbi     = tgt->FBI();
    pfc._rtgroup = _pickbuffer->_rtgroup;
    /////////////////////////////////////////////////////////////
    fbi->pushViewport(0, 0, iW, iH); // ??
    fbi->pushScissor(0, 0, iW, iH);  // ??
    /////////////////////////////////////////////////////////////
    // repaint this surface into the pickbuffer's rtgroup
    /////////////////////////////////////////////////////////////
    _pickbuffer->Draw(pfc);
    /////////////////////////////////////////////////////////////
    fbi->GetPixel(fvec4(fx, fy, 0.0f), pfc);
    /////////////////////////////////////////////////////////////
    fbi->popViewport();
    fbi->popScissor();
  }
  /////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void Surface::_doOnResized(void) {
  //printf( "Surface<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), x(), y(), width(), height() );
  DoSurfaceResize();
  SetDirty();
}

void Surface::RePaintSurface(ui::drawevent_constptr_t drwev) {
  DoRePaintSurface(drwev);
}

void Surface::_doGpuInit(lev2::Context* context) {
  _rtgroup        = std::make_shared<lev2::RtGroup>(context, 8, 8, lev2::MsaaSamples::MSAA_1X);
  _rtgroup->_name = FormatString("ui::Surface<%p>", (void*)this);
  auto mrt0       = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8);
}

///////////////////////////////////////////////////////////////////////////////

void Surface::decoupleFromUiSize(int w, int h) {
  _decoupled_width       = w;
  _decoupled_height      = h;
  _decouple_from_ui_size = true;
}

void Surface::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto fbi    = tgt->FBI();
  auto fxi    = tgt->FXI();
  auto dwi    = tgt->DWI();
  if(_alwaysRepaint){
    mNeedsSurfaceRepaint = true;
  }
  ///////////////////////////////////////
  // Determine render target size (with optional SSAA upscale)
  int multiplier = _supersample + 1;
  int target_w, target_h;

  if (_decouple_from_ui_size) {
    target_w = _decoupled_width * multiplier;
    target_h = _decoupled_height * multiplier;
  } else {
    target_w = width() * multiplier;
    target_h = height() * multiplier;
  }

  if (target_w < 1 || target_h < 1) return;

  // Initialize SSAA resolve resources if needed
  if (_supersample > 0) {
    _initSsaa(tgt);
  }

  // Resize RTG if needed
  {
    int irtgw = _rtgroup->width();
    int irtgh = _rtgroup->height();
    if (irtgw != target_w || irtgh != target_h) {
      _rtgroup->Resize(target_w, target_h);
      mNeedsSurfaceRepaint = true;
    }
  }

  if (mNeedsSurfaceRepaint || IsDirty()) {
    _rtgroup->_autoclear = true;
    _rtgroup->buffer(0)->_clearColor = _clearColor;
    _rtgroup->buffer(0)->_clearDepth = mfClearDepth;
    fbi->PushRtGroup(_rtgroup.get());
    RePaintSurface(drwev);
    fbi->PopRtGroup();
    mNeedsSurfaceRepaint = false;
    _dirty               = false;
  }

  if (_postRenderCallback) {
    _postRenderCallback();
  }

  // SSAA resolve: downsample from upscaled RTG to widget-size resolve RTG
  if (_supersample > 0) {
    int resolve_w = _decouple_from_ui_size ? _decoupled_width : width();
    int resolve_h = _decouple_from_ui_size ? _decoupled_height : height();
    _ssaaResolve(tgt, resolve_w, resolve_h);
  }

  ///////////////////////////////////
  // pickbuffer debug ?
  ///////////////////////////////////
  if (false) {
    if (_pickbuffer) {
      ork::lev2::PixelFetchContext pfc(2);
      pfc.miMrtMask = (1 << 0);
      pfc._usage[0] = lev2::PixelFetchContext::EPixelUsage::PTR64;
      pfc._usage[1] = lev2::PixelFetchContext::EPixelUsage::FLOAT;
      GetPixel(100, 100, pfc);
    }
  }
  ///////////////////////////////////////

  lev2::material_ptr_t ui_material = lev2::defaultUIMaterial();
  lev2::material_ptr_t material = ui_material;

  if (_rtgroup) {
    static auto texmtl = std::make_shared<lev2::GfxMaterialUITextured>(tgt);
    // Use resolved texture (downsampled) if SSAA, otherwise direct RTG texture
    auto ptex = _resolvedTexture();
    OrkAssert(ptex);
    texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
    material = texmtl;
  }

  bool has_foc = hasMouseFocus();
  //tgt->PushModColor(has_foc ? fcolor4::Green() : fcolor4::Blue());
  tgt->PushModColor(fcolor4::White());
  mtxi->PushUIMatrix();

  tgt->debugPushGroup("Surface::Draw");
  {
    int ix_root = 0;
    int iy_root = 0;
    LocalToRoot(0, 0, ix_root, iy_root);
    
    // printf( "Surface<%s>::Draw wx<%d> wy<%d> w<%d> h<%d>\n", _name.c_str(), ix_root, iy_root, _geometry._w, _geometry._h );

    if (_decouple_from_ui_size and _aspect_from_rtgroup) {
      tgt->debugPushGroup("Surface::Draw::1");

      // UV coordinates - flip V for Vulkan
      float u0 = 0.0f;
      float u1 = 1.0f;
      float v0 = 1.0f;  // Flipped: start at 1
      float v1 = 0.0f;  // Flipped: end at 0

      tgt->PushModColor(fcolor4::White());

      ui_material->BeginBlock(tgt);
      dwi->quad2D(
          fvec4(ix_root, iy_root, _geometry._w, _geometry._h),  // QuadRect: x, y, width, height
          fvec4(0.0f, 1.0f, 1.0f, -1.0f),  // UvRect - flip V coordinates for Vulkan
          fvec4(0, 0, 1, 1),  // UvRect2
          0.0f  // depth
      );
      ui_material->EndBlock(tgt);

      tgt->PopModColor();

      float inp_aspect = float(_decoupled_width) / float(_decoupled_height);
      float out_aspect = float(_geometry._w) / float(_geometry._h);
      float aspectt    = inp_aspect / out_aspect;

      printf("inp_aspect<%g> out_aspect<%g> aspectt<%g>\n", inp_aspect, out_aspect, aspectt);
  
      if (aspectt > 1.0) { // wider than UI (vertical letterbox)

        int hdiff = _geometry._h - int(float(_geometry._h)/aspectt);
        int oy0 = hdiff/2;
        int oy1 = -hdiff/2;
        
        // Use the already correct Y position
        int final_y = iy_root + oy0;

        material->BeginBlock(tgt);

        fvec4 uvrect = _flipY //
                     ? fvec4(u0, v0, u1 - u0, v1 - v0) //
                     : fvec4(u0, v1, u1 - u0, v0 - v1);
        fvec4 uvrect2 = _flipY //
                      ? fvec4(0, 1, 1, -1) //
                      : fvec4(0, 0, 1, 1);

        dwi->quad2D(
            fvec4(ix_root, final_y, _geometry._w, _geometry._h + oy1 - oy0),  // QuadRect
            uvrect,  // UvRect - using flipped V coordinates
            uvrect2,  // UvRect2
            0.0f  // depth
        );
        material->EndBlock(tgt);

      } else {
        int wdiff = _geometry._w - int(float(_geometry._w)*aspectt);
        int ox0 = wdiff/2;
        int ox1 = -wdiff/2;
        
        // Use the already correct Y position
        int final_y = iy_root;

        material->BeginBlock(tgt);

        fvec4 uvrect = _flipY //
                      ? fvec4(u0, v0, u1 - u0, v1 - v0) //
                      : fvec4(u0, v1, u1 - u0, v0 - v1);
        fvec4 uvrect2 = _flipY //
                      ? fvec4(0, 1, 1, -1) //
                      : fvec4(0, 0, 1, 1);

        dwi->quad2D(
            fvec4(ix_root + ox0, final_y, _geometry._w + ox1 - ox0, _geometry._h),  // QuadRect
            uvrect,  // UvRect - using flipped V coordinates
            uvrect2,  // UvRect2
            0.0f  // depth
        );
        material->EndBlock(tgt);
      }
      tgt->debugPopGroup();

    } else {
      tgt->debugPushGroup("Surface::Draw::2");
      
      material->BeginBlock(tgt);

        fvec4 uvrect = _flipY //
                      ? fvec4(0.0f, 1.0f, 1.0f, -1.0f) //
                      : fvec4(0.0f, 0.0f, 1.0f, 1.0f);
        fvec4 uvrect2 = _flipY //
                      ? fvec4(0, 1, 1, -1) //
                      : fvec4(0, 0, 1, 1);
      dwi->quad2D(
          fvec4(ix_root, iy_root, _geometry._w, _geometry._h),  // QuadRect: x, y, width, height
          uvrect,  // UvRect - flip V coordinates for Vulkan
          uvrect2,  // UvRect2
          0.0f  // depth
      );
      material->EndBlock(tgt);
      
      tgt->debugPopGroup();
    }
  }

  drawChildren(drwev);

  tgt->debugPopGroup();
  mtxi->PopUIMatrix();
  tgt->PopModColor();
}

/////////////////////////////////////////////////////////////////////////

void Surface::SurfaceRender(lev2::RenderContextFrameData& FrameData, const std::function<void()>& render_lambda) {
#if 0
	lev2::Context* pTARG = FrameData.GetTarget();
	lev2::IRenderTarget* pIT = FrameData.GetRenderTarget();

	int vpx = x;
	int vpy = y;
	int vpw = width();
	int vph = height();

	auto fbi = pTARG->FBI();

	//fbi->setScissor( vpx, vpy, vpw, vph );
	//fbi->setViewport( vpx, vpy, vpw, vph );

SRect VPRect( 0, 0, pIT->width(), pIT->height() );
	pTARG->FBI()->pushViewport( VPRect );
	pTARG->FBI()->pushScissor( VPRect );
	{
    printf( "Surface<%s>::Render lambda\n", _name.c_str());
		render_lambda();
	}
	pTARG->FBI()->popScissor();
	pTARG->FBI()->popViewport();
#endif
}

/////////////////////////////////////////////////////////////////////////

void Surface::RenderCached() {
}

/////////////////////////////////////////////////////////////////////////

void Surface::BeginSurface(lev2::Context* pTARG) {
}

/////////////////////////////////////////////////////////////////////////

void Surface::EndSurface(lev2::Context* pTARG) {
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
