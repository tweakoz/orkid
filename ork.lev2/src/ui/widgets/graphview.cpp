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
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/math/misc_math.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////
namespace ork::ui {
static constexpr int _kbasechanlaby = 16;
/////////////////////////////////////////////////////////////////////////
// GraphSeries Implementation
/////////////////////////////////////////////////////////////////////////
GraphSeries::GraphSeries(const std::string& name, fvec3 color)
    : _name(name)
    , _color(color) {
}
/////////////////////////////////////////////////////////////////////////
void GraphSeries::addSample(float value) {
  _samples.push_back(value);

  // Maintain ring buffer size
  while (_samples.size() > _max_samples) {
    _samples.pop_front();
  }

  // Update range if auto-ranging
  if (_auto_range) {
    _updateRange();
  }
}
/////////////////////////////////////////////////////////////////////////
void GraphSeries::clearSamples() {
  _samples.clear();
  _min_value = 0.0f;
  _max_value = 1.0f;
}
/////////////////////////////////////////////////////////////////////////
void GraphSeries::setMaxSamples(size_t count) {
  _max_samples = count;

  // Trim if needed
  while (_samples.size() > _max_samples) {
    _samples.pop_front();
  }
}
/////////////////////////////////////////////////////////////////////////
float GraphSeries::getSample(size_t index) const {
  if (index < _samples.size()) {
    return _samples[index];
  }
  return 0.0f;
}
/////////////////////////////////////////////////////////////////////////
void GraphSeries::_updateRange() {
  if (_samples.empty()) {
    _min_value = 0.0f;
    _max_value = 1.0f;
    return;
  }

  _min_value = _samples[0];
  _max_value = _samples[0];

  for (float val : _samples) {
    _min_value = std::min(_min_value, val);
    _max_value = std::max(_max_value, val);
  }

  // Add 10% padding to range
  float range = _max_value - _min_value;
  if (range < 0.001f) range = 0.001f;  // Avoid div by zero
  _min_value -= range * 0.1f;
  _max_value += range * 0.1f;
}
/////////////////////////////////////////////////////////////////////////
// GraphChannel Implementation
/////////////////////////////////////////////////////////////////////////
graphseries_ptr_t GraphChannel::addSeries(const std::string& name, fvec3 color) {
  auto series = std::make_shared<GraphSeries>(name, color);
  _series.push_back(series);
  return series;
}
/////////////////////////////////////////////////////////////////////////
void GraphChannel::removeSeries(const std::string& name) {
  _series.erase(
    std::remove_if(_series.begin(), _series.end(),
      [&name](const graphseries_ptr_t& s) { return s->_name == name; }),
    _series.end()
  );
}
/////////////////////////////////////////////////////////////////////////
graphseries_ptr_t GraphChannel::getSeries(const std::string& name) {
  for (auto& series : _series) {
    if (series->_name == name) {
      return series;
    }
  }
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
// GraphView Implementation
/////////////////////////////////////////////////////////////////////////
GraphView::GraphView()
    : Surface("GraphView", 0, 0, 32, 32, fvec4(1, 0, 0, 1), 1.0)
    , _lockX(false)
    , _lockY(false)
    , _lockYZOOM(false)
    , _dragging(false) {

  _grid._baseColor   = fvec3(0.2, 0, 0.2);
  _grid._hiliteColor = fvec3(0.3, 0, 0.3);
}
/////////////////////////////////////////////////////////////////////////
void GraphView::_doGpuInit(lev2::Context* pTARG) {
  Surface::_doGpuInit(pTARG);
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
      bool handled_click = false;

      // Count total number of series across all channels
      int total_series = 0;
      for (auto channel : _channelmap) {
        if (!channel->_series.empty()) {
          total_series += channel->_series.size();
        } else {
          total_series += 1; // Lambda-based channel gets one entry
        }
      }

      if (total_series) {
        int row_height = 16 + _label_spacing;
        int maxy = total_series * row_height + _kbasechanlaby + 16;
        // Check if click is in the label region (right side)
        if (ilocx > (width() - 150) and ilocy < maxy) {
          int iseries_index = (ilocy - 16) / row_height;
          printf("ilocy<%d> iseries_index<%d> total_series<%d>\n", ilocy, iseries_index, total_series);

          // Find which series was clicked
          int current_index = 0;
          for (auto channel : _channelmap) {
            if (!channel->_series.empty()) {
              for (auto& series : channel->_series) {
                if (current_index == iseries_index) {
                  series->_visible = not series->_visible;
                  printf("series<%s> visible<%d>\n", series->_name.c_str(), series->_visible);
                  handled_click = true;
                  goto done;
                }
                current_index++;
              }
            } else {
              if (current_index == iseries_index) {
                channel->_visible = not channel->_visible;
                printf("channel<%s> visible<%d>\n", channel->_name.c_str(), channel->_visible);
                handled_click = true;
                goto done;
              }
              current_index++;
            }
          }
          done:;
        }
      }

      // If click wasn't handled by series toggle, start drag
      if (!handled_click) {
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
    case ui::EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;
      printf("GraphView<%s> keydown<%c>\n", _name.c_str(), key);

      // 'v' key toggles between AUTO and MANUAL vertical scale modes
      if (key == 'v' || key == 'V') {
        if (_vscale_mode == VerticalScaleMode::AUTO) {
          _vscale_mode = VerticalScaleMode::MANUAL;
          printf("  Vertical scale mode: AUTO -> MANUAL\n");
        } else {
          _vscale_mode = VerticalScaleMode::AUTO;
          printf("  Vertical scale mode: MANUAL -> AUTO\n");
        }
        mNeedsSurfaceRepaint = true;
        SetDirty();
        return HandlerResult(this);
      }

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
  auto vb = std::make_shared<vtxbuf_t>(16 << 20, 0); // ~800 MB
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
  auto primi = tgt->PRI();
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
      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
      ///////////////////////////////
      tgt->PushModColor(color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(
          defmtl.get(),
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
      auto RCFD = std::make_shared<lev2::RenderContextFrameData>(tgt);
      auto par_mvp  = mtl->param("MatMVP");

      // Calculate maximum label width for alignment
      int max_label_width = 0;
      for (auto channel : _channelmap) {
        if (!channel->_series.empty()) {
          for (auto& series : channel->_series) {
            int sw = lev2::FontMan::stringWidth(series->_name.length());
            max_label_width = std::max(max_label_width, sw);
          }
        } else {
          int sw = lev2::FontMan::stringWidth(channel->_name.length());
          max_label_width = std::max(max_label_width, sw);
        }
      }

      int ichanlaby = _kbasechanlaby;
      for (auto channel : _channelmap) {
        const std::string& name = channel->_name;

        // Check if using series-based or lambda-based system
        bool has_series = !channel->_series.empty();
        bool has_lambdas = (channel->_getCount != nullptr);

        size_t numpoints = 0;
        if (has_series && !channel->_series.empty()) {
          numpoints = channel->_series[0]->sampleCount();
        } else if (has_lambdas) {
          numpoints = channel->_getCount();
        }

        ///////////////////////////////////////////////////
        // draw series labels/toggleboxes (one per series)
        ///////////////////////////////////////////////////

        if (has_series) {
          // Draw a label/button for each series
          for (auto& series : channel->_series) {
            int sw = lev2::FontMan::stringWidth(series->_name.length());
            tgt->RefModColor() = series->_color;
            lev2::FontMan::beginTextBlock(tgt, 128);
            lev2::FontMan::DrawText(
                tgt,
                ix2 - (max_label_width + 16),
                ichanlaby,
                series->_name.c_str());
            lev2::FontMan::endTextBlock(tgt);

            if (series->_visible) {
              ///////////////////////////////////////////////////
              // draw current value
              ///////////////////////////////////////////////////
              size_t series_count = series->sampleCount();
              if (series_count > 0) {
                float value = series->getSample(series_count - 1);
                auto valstr = FormatString("%0.5g", value);
                int sw2 = lev2::FontMan::stringWidth(valstr.length());
                tgt->RefModColor() = series->_color;
                lev2::FontMan::beginTextBlock(tgt, 128);
                lev2::FontMan::DrawText(
                    tgt,
                    ix2 - (max_label_width + 16) - (sw2 + 16),
                    ichanlaby,
                    valstr.c_str());
                lev2::FontMan::endTextBlock(tgt);
              }

              ///////////////////////////////////////////////////
              // draw toggle box
              ///////////////////////////////////////////////////
              int x1 = ix2 - (max_label_width + 28);  // ~12 pixels left margin (1 char width)
              int x2 = ix2 - 16;  // 16 pixels right margin
              int y1 = ichanlaby;
              int y2 = ichanlaby + 16;

              lev2::VtxWriter<vtx_t> vw;
              vw.Lock(tgt, vbuf.get(), 8);
              vw.AddVertex(vtx_t(fvec3(x1, y1, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x2, y1, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x2, y1, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x2, y2, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x2, y2, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x1, y2, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x1, y2, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(x1, y1, 0), fvec4(), series->_color));
              vw.UnLock(tgt);

              mtxi->PushUIMatrix(width(), height());
              mtl->begin(tek, RCFD);
              mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
              mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
              gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
              mtl->end(RCFD);
              mtxi->PopUIMatrix();
            }

            ichanlaby += 16 + _label_spacing;
          }
        } else {
          // Lambda-based: draw one button for the channel
          int sw = lev2::FontMan::stringWidth(channel->_name.length());
          tgt->RefModColor() = channel->_color;
          lev2::FontMan::beginTextBlock(tgt, 128);
          lev2::FontMan::DrawText(
              tgt,
              ix2 - (max_label_width + 16),
              ichanlaby,
              channel->_name.c_str());
          lev2::FontMan::endTextBlock(tgt);

          if (channel->_visible) {
            ///////////////////////////////////////////////////
            // draw current value
            ///////////////////////////////////////////////////
            if (numpoints) {
              float value = channel->_getPoint(numpoints - 1).y;
              auto valstr = FormatString("%0.5g", value);
              int sw2 = lev2::FontMan::stringWidth(valstr.length());
              tgt->RefModColor() = channel->_color;
              lev2::FontMan::beginTextBlock(tgt, 128);
              lev2::FontMan::DrawText(
                  tgt,
                  ix2 - (max_label_width + 16) - (sw2 + 16),
                  ichanlaby,
                  valstr.c_str());
              lev2::FontMan::endTextBlock(tgt);
            }

            ///////////////////////////////////////////////////
            // draw toggle box
            ///////////////////////////////////////////////////
            int x1 = ix2 - (max_label_width + 28);  // ~12 pixels left margin (1 char width)
            int x2 = ix2 - 16;  // 16 pixels right margin
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
            mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
            gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
            mtl->end(RCFD);
            mtxi->PopUIMatrix();
          }

          ichanlaby += 16 + _label_spacing;
        }

        ///////////////////////////////////////////////////

        // For lambda-based channels, check channel visibility
        if (has_lambdas && not channel->_visible) {
          continue;
        }

        // Calculate range (series-based or lambda-based)
        fvec2 hrange, vrange;
        if (has_series && !channel->_series.empty()) {
          // Use series auto-range - find first visible series
          graphseries_ptr_t first_visible_series = nullptr;
          for (auto& series : channel->_series) {
            if (series->_visible && series->sampleCount() > 0) {
              first_visible_series = series;
              break;
            }
          }
          if (first_visible_series) {
            hrange = fvec2(-50, 50);  // Centered on origin - samples will be scaled to fit
            vrange = fvec2(first_visible_series->_min_value, first_visible_series->_max_value);
          } else {
            hrange = fvec2(-50, 50);  // Centered on origin
            vrange = fvec2(0, 1);
          }
        } else if (has_lambdas && channel->_getHorizontalRange && channel->_getVerticalRange) {
          hrange = channel->_getHorizontalRange();
          vrange = channel->_getVerticalRange();
        } else {
          hrange = fvec2(0, 100);
          vrange = fvec2(0, 1);
        }

        int w = this->width();
        int h = this->height();

        if (numpoints) {
          ///////////////////////////////////////////////////
          // Render series-based data
          ///////////////////////////////////////////////////
          if (has_series) {
              for (auto& series : channel->_series) {
                if (!series->_visible)
                  continue;

                size_t series_count = series->sampleCount();
                if (series_count == 0)
                  continue;

                // Calculate moving window - which samples to display
                size_t display_count = series_count;
                size_t start_index = 0;

                if (series->_window_size > 0 && series->_window_size < series_count) {
                  display_count = series->_window_size;
                  start_index = series_count - display_count;  // Show most recent N samples
                }

                lev2::VtxWriter<vtx_t> vw;
                vw.Lock(tgt, vbuf.get(), display_count * 2);

                float x_scale = (hrange.y - hrange.x) / float(display_count > 1 ? display_count - 1 : 1);
                float y_scale = vrange.y - vrange.x;
                if (y_scale < 0.001f) y_scale = 0.001f;

                for (size_t i = 0; i < display_count; i++) {
                  size_t sample_index = start_index + i;
                  float x = hrange.x + float(i) * x_scale;
                  float y = series->getSample(sample_index);

                  fvec3 point(x, y, 0);

                  if (i > 0) {
                    float prev_x = hrange.x + float(i - 1) * x_scale;
                    float prev_y = series->getSample(start_index + i - 1);
                    fvec3 prev_point(prev_x, prev_y, 0);

                    vw.AddVertex(vtx_t(prev_point, fvec4(), series->_color));
                    vw.AddVertex(vtx_t(point, fvec4(), series->_color));
                  }
                }
                vw.UnLock(tgt);

                // Draw series
                mtxi->PushPMatrix(_grid._mtxOrtho);
                mtxi->PushVMatrix(fmtx4::Identity());
                mtxi->PushMMatrix(fmtx4::Identity());
                mtl->begin(tek, RCFD);
                mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
                mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
                gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
                mtl->end(RCFD);
                mtxi->PopPMatrix();
                mtxi->PopVMatrix();
                mtxi->PopMMatrix();
              }
            }
            ///////////////////////////////////////////////////
            // Render lambda-based data (backward compat)
            ///////////////////////////////////////////////////
            else if (has_lambdas) {
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

              mtxi->PushPMatrix(_grid._mtxOrtho);
              mtxi->PushVMatrix(fmtx4::Identity());
              mtxi->PushMMatrix(fmtx4::Identity());
              mtl->begin(tek, RCFD);
              mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
              mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
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
