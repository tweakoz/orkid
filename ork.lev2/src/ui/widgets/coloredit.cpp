#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/coloredit.h>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
ColorEdit::ColorEdit(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _originalColor(color)
    , _currentColor(color) {

}
///////////////////////////////////////////////////////////////////////////////
HandlerResult ColorEdit::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;
  switch (cev->_eventcode) {
    case EventCode::KEY_DOWN: {
      int key = cev->miKeyCode;
      printf("key<%d>\n", key);
      switch (key) {
        case 256: // esc
          _currentColor = _originalColor;
          rval._widget_finished = true;
         
          break;
        case 257: // enter
          rval._widget_finished = true;
          break;
        default:
          break;
      }
      rval.setHandled(this);
      break;
    }
    default:
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void ColorEdit::DoDraw(drawevent_constptr_t drwev) {

  auto context    = drwev->GetTarget();
  auto fbi    = context->FBI();
  auto gbi    = context->GBI();
  auto mtxi   = context->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  if(nullptr==_material){
    _material = std::make_shared<lev2::FreestyleMaterial>();
    _material->gpuInit(context, "orkshader://ui2");
    _tekvtxcolor = _material->technique("ui_vtxcolor");
    _tekmodcolor = _material->technique("ui_modcolor");
    _parmvp      = _material->param("mvp");
    _parmodcolor = _material->param("modcolor");
    _material->dump();
  }

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    if (0)
      printf(
          "drawColorEdit<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          _name.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDepthTest::OFF);
    context->PushModColor(_currentColor);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi.RenderQuadAtZ(
        defmtl.get(),
        context,
        ix1,  // x0
        ix2,  // x1
        iy1,  // y0
        iy2,  // y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    context->PopModColor();

    /////////////////////////////////////////////////////////////////
    // draw radial color picker
    /////////////////////////////////////////////////////////////////

    using vtx_t = lev2::SVtxV16T16C16;
    auto& VB = lev2::GfxEnv::GetSharedDynamicV16T16C16();
    lev2::VtxWriter<vtx_t> vw;
    int numquads = 90;
    vw.Lock(context, &VB, 6 * numquads);

    float radiusOuter = 0.5f * float(_geometry._w);
    float radiusInner = radiusOuter *0.5;

    fvec3 CTR(float(ixc), float(iyc), 0.0f);

    for( int i=0; i<numquads; i++ ){
        float A = float(i*4)*DTOR;
        float B = float((i+1)*4)*DTOR;

        fvec3 XYAI( radiusInner*cosf(A), radiusInner*sinf(A), 0.0f );
        fvec3 XYBI( radiusInner*cosf(B), radiusInner*sinf(B), 0.0f );
        fvec3 XYAO( radiusOuter*cosf(A), radiusOuter*sinf(A), 0.0f );
        fvec3 XYBO( radiusOuter*cosf(B), radiusOuter*sinf(B), 0.0f );

        fvec4 RGBAI, RGBAO;;
        RGBAI.setHSV( float(i)/float(numquads), 1.0f, 0.0f );
        RGBAO.setHSV( float(i+1)/float(numquads), 1.0f, 1.0f );

        vtx_t v0(CTR+XYAI, fvec4(), RGBAI);
        vtx_t v1(CTR+XYBI, fvec4(), RGBAI);
        vtx_t v2(CTR+XYAO, fvec4(), RGBAO);
        vtx_t v3(CTR+XYBO, fvec4(), RGBAO);

        vw.AddVertex(v0);
        vw.AddVertex(v1);
        vw.AddVertex(v2);

        vw.AddVertex(v1);
        vw.AddVertex(v3);
        vw.AddVertex(v2);
    }

    vw.UnLock(context);

    auto uiMatrix = mtxi->uiMatrix(_geometry._w, _geometry._h);
    ///////////////////////////////
    lev2::RenderContextFrameData RCFD(context);
    _material->_rasterstate.SetRGBAWriteMask(true, true);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
     gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
    ///////////////////////////////

  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
