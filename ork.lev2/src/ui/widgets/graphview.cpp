#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/graphview.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/material_freestyle.h>
///////////////////////////////////////////////////////////
namespace ork::ui {
static constexpr int _kbasechanlaby = 16;
/////////////////////////////////////////////////////////////////////////
GraphView::GraphView()
    : Surface("GraphView", 0, 0, 32, 32, fvec3(1, 0, 0), 1.0)
    , _lockX(false)
    , _lockY(false)
    , _lockYZOOM(false)
    , _dragging(false) {

  _grid._baseColor   = fvec3(0.2, 0, 0.2);
  _grid._hiliteColor = fvec3(0.3, 0, 0.3);
}
/////////////////////////////////////////////////////////////////////////
void GraphView::DoInit(lev2::Context* pTARG) {
}
/////////////////////////////////////////////////////////////////////////
HandlerResult GraphView::DoOnUiEvent(event_constptr_t ev) {
  const auto& filtev = ev->mFilteredEvent;
  int ix             = ev->miX;
  int iy             = ev->miY;
  int ilocx, ilocy;
  RootToLocal(ix, iy, ilocx, ilocy);

  float gscaleX = _grid._extent;
  float gscaleY = gscaleX * _grid._aspect;

  switch (filtev._eventcode) {
    case ui::EventCode::PUSH:
    case ui::EventCode::DOUBLECLICK: {
      _dragging       = false;
      int numchannels = _channelmap.size();
      if (numchannels) {
        int maxy = numchannels * 16 + _kbasechanlaby + 16;
        if (ilocx > (width() - 64) and ilocy < maxy) {
          int ichannel = (ilocy - 16) >> 4;
          printf("ilocy<%d> ichannel<%d> numchannels<%d>\n", ilocy, ichannel, numchannels);
          if (ichannel < numchannels) {
            graphchannel_ptr_t channel = _channelmap[ichannel];
            channel->_visible          = not channel->_visible;
            printf("channel<%s>\n", channel->_name.c_str());
          }
        }
      } else {
        float fx    = float(ilocx) * gscaleX / float(width());
        float fy    = float(ilocy) * gscaleY / float(height());
        _downPos    = fvec2(fx, fy);
        _downCenter = _grid._center;
        _dragging   = true;
      }
      mNeedsSurfaceRepaint = true;
      break;
    }
    case ui::EventCode::DRAG: {
      if (_dragging) {
        float fx       = float(ilocx) * gscaleX / float(width());
        float fy       = float(ilocy) * gscaleY / float(height());
        fvec2 delta    = fvec2(fx, fy) - _downPos;
        float newctr_x = _downCenter.x - (delta.x / _grid._zoomX);
        float newctr_y = _downCenter.y + (delta.y / _grid._zoomY);

        if (not _lockX)
          _grid._center.x = newctr_x;
        if (not _lockY)
          _grid._center.y = newctr_y;

        mNeedsSurfaceRepaint = true;
      }
      break;
    }
    case EventCode::MOUSEWHEEL: {
      int idelta = ev->miMWY;
      if (idelta > 0) {
        _grid._zoomX *= 1.1f;
        if (not _lockYZOOM)
          _grid._zoomY *= 1.1f;
      } else if (idelta < 0) {
        _grid._zoomX *= 1.0f / 1.1f;
        if (not _lockYZOOM)
          _grid._zoomY *= 1.0f / 1.1f;
      }
      _grid._zoomX         = clamp(_grid._zoomX, 0.1f, 10.0f);
      _grid._zoomY         = clamp(_grid._zoomY, 0.1f, 10.0f);
      mNeedsSurfaceRepaint = true;
      return HandlerResult(this);
      break;
    }
    default:
      break;
  }
  return HandlerResult();
}
/////////////////////////////////////////////////////////////////////////
graphchannel_ptr_t GraphView::channel(std::string named) {
  graphchannel_ptr_t channel = nullptr;
  size_t numchannels         = _channelmap.size();
  for (size_t i = 0; i < numchannels; i++) {
    channel = _channelmap[i];
    if (channel->_name == named) {
      return channel;
    }
  }
  channel        = std::make_shared<ui::GraphChannel>();
  channel->_name = named;
  _channelmap.push_back(channel);
  return channel;
}
///////////////////////////////////////////////////////////////////////////////
using vtx_t        = lev2::SVtxV16T16C16;
using vtxbuf_t     = lev2::DynamicVertexBuffer<vtx_t>;
using vtxbuf_ptr_t = std::shared_ptr<vtxbuf_t>;
static vtxbuf_ptr_t create_vertexbuffer(lev2::Context* context) {
  auto vb = std::make_shared<vtxbuf_t>(16 << 20, 0, lev2::PrimitiveType::NONE); // ~800 MB
  vb->SetRingLock(true);
  return vb;
}
static vtxbuf_ptr_t get_vertexbuffer(lev2::Context* context) {
  static auto vb = create_vertexbuffer(context);
  return vb;
}
///////////////////////////////////////////////////////////////////////////////
static lev2::freestyle_mtl_ptr_t create_hud_material(lev2::Context* context) {
  auto mtl = std::make_shared<lev2::FreestyleMaterial>();
  mtl->gpuInit(context, "orkshader://solid");
  return mtl;
}
static lev2::freestyle_mtl_ptr_t hud_material(lev2::Context* context) {
  static auto mtl = create_hud_material(context);
  return mtl;
}
/////////////////////////////////////////////////////////////////////////
void GraphView::DoRePaintSurface(drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto gbi    = tgt->GBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();
  auto vbuf   = get_vertexbuffer(tgt);

  _grid.updateMatrices(tgt, _geometry._w, _geometry._h);

  _grid.Render(tgt, _geometry._w, _geometry._h);

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
          "drawbox<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          _name.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    fvec4 color(0.2, 0, 0.2, 1);

    if (not hasMouseFocus())
      color *= 0.9f;

    if (0) { // alphabg
      defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
      defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
      ///////////////////////////////
      tgt->PushModColor(color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi.RenderQuadAtZ(
          defmtl.get(),
          tgt,
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
      tgt->PopModColor();
    }
    ork::lev2::FontMan::PushFont("i14");
    {
      ///////////////////////////////
      // render channels
      ///////////////////////////////
      auto mtl = hud_material(tgt);
      auto tek = mtl->technique("vtxcolor");
      lev2::RenderContextFrameData RCFD(tgt);
      auto par_mvp  = mtl->param("MatMVP");
      int ichanlaby = _kbasechanlaby;
      for (auto channel : _channelmap) {
        const std::string& name = channel->_name;
        size_t numpoints        = channel->_getCount();

        ///////////////////////////////////////////////////
        // draw channel name labels/toggleboxes
        ///////////////////////////////////////////////////

        int sw             = lev2::FontMan::stringWidth(channel->_name.length());
        tgt->RefModColor() = channel->_color;
        lev2::FontMan::beginTextBlock(tgt, 128);
        lev2::FontMan::DrawText(
            tgt, //
            ix2 - (sw + 16),
            ichanlaby,
            channel->_name.c_str());
        lev2::FontMan::endTextBlock(tgt);

        if (channel->_visible) {

          ///////////////////////////////////////////////////
          // draw current value
          ///////////////////////////////////////////////////

          if (numpoints) {
            float value        = channel->_getPoint(numpoints - 1).y;
            auto valstr        = FormatString("%0.5g", value);
            int sw2            = lev2::FontMan::stringWidth(valstr.length());
            tgt->RefModColor() = channel->_color;
            lev2::FontMan::beginTextBlock(tgt, 128);
            lev2::FontMan::DrawText(
                tgt, //
                ix2 - (sw + 16) - (sw2 + 16),
                ichanlaby,
                valstr.c_str());
            lev2::FontMan::endTextBlock(tgt);
          }

          ///////////////////////////////////////////////////
          // draw toggle box
          ///////////////////////////////////////////////////

          int x1 = ix2 - (sw + 16);
          int x2 = x1 + sw;
          int y1 = ichanlaby;
          int y2 = ichanlaby + 16;

          lev2::VtxWriter<vtx_t> vw;
          vw.Lock(tgt, vbuf.get(), 8);
          vw.AddVertex(vtx_t(fvec3(x1, y1, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x2, y1, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x2, y1, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x2, y2, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x2, y2, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x1, y2, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x1, y2, 0), fvec4(), channel->_color));
          vw.AddVertex(vtx_t(fvec3(x1, y1, 0), fvec4(), channel->_color));
          vw.UnLock(tgt);

          mtxi->PushUIMatrix(width(), height());
          mtl->begin(tek, RCFD);
          mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
          mtl->_rasterstate.SetBlending(lev2::Blending::OFF);
          gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
          mtl->end(RCFD);
          mtxi->PopUIMatrix();
        }

        ichanlaby += 16;

        ///////////////////////////////////////////////////

        if (not channel->_visible) {
          continue;
        }

        fvec2 hrange = channel->_getHorizontalRange();
        fvec2 vrange = channel->_getVerticalRange();

        int w = this->width();
        int h = this->height();

        if (numpoints) {
          if (channel->_visible) {
            ///////////////////////////////////////////////////
            // points -> vertex buffer
            ///////////////////////////////////////////////////
            lev2::VtxWriter<vtx_t> vw;
            vw.Lock(tgt, vbuf.get(), numpoints * 2);
            auto prev_point = channel->_getPoint(0);
            for (size_t i = 0; i < numpoints; i++) {
              auto next_point = channel->_getPoint(i);
              vw.AddVertex(vtx_t(fvec3(prev_point), fvec4(), channel->_color));
              vw.AddVertex(vtx_t(fvec3(next_point), fvec4(), channel->_color));

              prev_point = next_point;
            }
            vw.UnLock(tgt);
            ///////////////////////////////////////////////////
            // draw vertex buffer
            ///////////////////////////////////////////////////
            mtxi->PushPMatrix(_grid._mtxOrtho);
            mtxi->PushVMatrix(fmtx4::Identity());
            mtxi->PushMMatrix(fmtx4::Identity());
            mtl->begin(tek, RCFD);
            mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
            mtl->_rasterstate.SetBlending(lev2::Blending::OFF);
            gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
            mtl->end(RCFD);
            mtxi->PopPMatrix();
            mtxi->PopVMatrix();
            mtxi->PopMMatrix();
          }
          ///////////////////////////////////////////////////
        }
      }
      ///////////////////////////////
      // draw misc labels
      ///////////////////////////////
      tgt->RefModColor() = fvec3(1, 1, 1);
      lev2::FontMan::beginTextBlock(tgt, 48);
      int iy = 16;
      lev2::FontMan::DrawText(
          tgt, //
          16,
          iy += 16,
          "pan: left-drag");
      lev2::FontMan::DrawText(
          tgt, //
          16,
          iy += 16,
          "zoom: mouse-wheel");
      lev2::FontMan::endTextBlock(tgt);
      ///////////////////////////////
      tgt->RefModColor() = fvec3(0, 1, 0);
      lev2::FontMan::beginTextBlock(tgt, 128);
      lev2::FontMan::DrawText(
          tgt, //
          16,
          iy += 16,
          "center<%g %g>",
          _grid._center.x,
          _grid._center.y);
      lev2::FontMan::DrawText(
          tgt, //
          16,
          iy += 16,
          "zoomfactorX<%g>",
          _grid._zoomX);
      lev2::FontMan::DrawText(
          tgt, //
          16,
          iy += 16,
          "zoomfactorY<%g>",
          _grid._zoomY);
      lev2::FontMan::endTextBlock(tgt);
      ///////////////////////////////
      if (_name.length()) {
        tgt->RefModColor() = fvec3(1, 0.5, 0);
        lev2::FontMan::beginTextBlock(tgt, 32);
        int sw = lev2::FontMan::stringWidth(_name.length());
        lev2::FontMan::DrawText(
            tgt, //
            ixc - (sw >> 1),
            16,
            _name.c_str());
        lev2::FontMan::endTextBlock(tgt);
      }
      ///////////////////////////////
    }
    ork::lev2::FontMan::PopFont();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
void GraphPanel::setRect(int iX, int iY, int iW, int iH, bool snap) {
  _uipanel->SetRect(iX, iY, iW, iH);
  if (snap)
    _uipanel->snap();
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
