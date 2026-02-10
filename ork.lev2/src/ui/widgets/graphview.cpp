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
    _max_samples = 4000;

}
/////////////////////////////////////////////////////////////////////////
void GraphSeries::addSample(float value) {
  _samples.push_back(value);

  // Maintain ring buffer size
  while (_samples.size() > _max_samples) {
    _samples.pop_front();
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
      std::remove_if(_series.begin(), _series.end(), [&name](const graphseries_ptr_t& s) { return s->_name == name; }),
      _series.end());
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
void GraphChannel::setSeriesOrder(const std::vector<std::string>& names) {
  std::vector<graphseries_ptr_t> reordered;
  for (auto& name : names) {
    for (auto& series : _series) {
      if (series->_name == name) {
        reordered.push_back(series);
        break;
      }
    }
  }
  // Append any series not mentioned in names (preserve them at the end)
  for (auto& series : _series) {
    bool found = false;
    for (auto& r : reordered) {
      if (r == series) { found = true; break; }
    }
    if (!found) reordered.push_back(series);
  }
  _series = reordered;
}
/////////////////////////////////////////////////////////////////////////
// GraphView Implementation
/////////////////////////////////////////////////////////////////////////
GraphView::GraphView()
    : Surface("GraphView", 0, 0, 32, 32, fvec4(0, 0, 0, 1), 1.0)
    , _lockX(false)
    , _lockY(false)
    , _lockYZOOM(false)
    , _dragging(false) {

  _grid._baseColor   = fvec3(0.2, 0, 0.2);
  _grid._hiliteColor = fvec3(0.3, 0, 0.3);

  // Default zoom all the way out
  _grid._zoomX = 0.1f;
  _grid._zoomY = 0.1f;

  // Position horizontal pan so x=0 (current sample) is on RIGHT side of viewport
  // hrange = [center - extent/zoom/2, center + extent/zoom/2]
  // To have right edge at 0: center + extent/zoom/2 = 0 → center = -extent/zoom/2
  float hextent = _grid._extent / _grid._zoomX;
  _grid._center.x = -hextent / 2.0f;  // Negative center puts x=0 on right edge
  _grid._center.y = 0.0f;
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
      auto clicked_series = _findSeriesAtPoint(ilocx, ilocy);

      if (clicked_series) {
        // Toggle selection
        if (_selected_series == clicked_series) {
          _selected_series = nullptr;
          printf("series<%s> deselected\n", clicked_series->_name.c_str());
        } else {
          _selected_series = clicked_series;
          printf("series<%s> selected (scale=%.3f)\n",
                 _selected_series->_name.c_str(), _selected_series->_vertical_scale);
        }
      }
      mNeedsSurfaceRepaint = true;
      break;
    }
    case ui::EventCode::MOVE: {
      _hovered_series = _findSeriesAtPoint(ilocx, ilocy);
      break;
    }
    case EventCode::MOUSEWHEEL: {
      int wheel_delta = ev->miMWY;

      if (_selected_series) {
        _adjustSeriesScale(wheel_delta);
      } else {
        _adjustGlobalZoom(wheel_delta);
      }

      mNeedsSurfaceRepaint = true;
      return HandlerResult(this);
    }
    case ui::EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;

      if (key == 'V') {
        _v_key_down = true;
      }

      switch (key) {
        case ' ':
          if (_hovered_series) {
            _hovered_series->_visible = !_hovered_series->_visible;
            printf("series<%s> visible<%d>\n", _hovered_series->_name.c_str(),
                   _hovered_series->_visible);
            mNeedsSurfaceRepaint = true;
            return HandlerResult(this);
          }
          break;

        case 'R':
          if (_selected_series) {
            _selected_series->_vertical_scale = 1.0f;
            printf("series<%s> RESET scale\n", _selected_series->_name.c_str());
            mNeedsSurfaceRepaint = true;
            return HandlerResult(this);
          }
          break;

        case 'N':
          if (_selected_series) {
            _selected_series->_normalize_for_display = !_selected_series->_normalize_for_display;
            printf("series<%s> normalize=%d\n", _selected_series->_name.c_str(),
                   _selected_series->_normalize_for_display);
            mNeedsSurfaceRepaint = true;
            return HandlerResult(this);
          }
          break;

        case ',':
          for (auto channel : _channelmap) {
            for (auto& series : channel->_series) {
              series->setMaxSamples(series->sampleCount() / 2);
            }
          }
          mNeedsSurfaceRepaint = true;
          return HandlerResult(this);

        case '.':
          for (auto channel : _channelmap) {
            for (auto& series : channel->_series) {
              series->setMaxSamples(series->sampleCount() * 2);
            }
          }
          mNeedsSurfaceRepaint = true;
          return HandlerResult(this);
      }
      break;
    }
    case ui::EventCode::KEY_UP: {
      int key = ev->miKeyCode;
      if (key == 'V') {
        _v_key_down = false;
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
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();
  auto vbuf   = get_vertexbuffer(tgt);

  _grid.updateMatrices(tgt, _geometry._w, _geometry._h);

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

  ork::lev2::FontMan::PushFont("i14");
  {
    ///////////////////////////////
    // render channels
    ///////////////////////////////
    auto mtl     = hud_material(tgt);
    auto tek     = mtl->technique("vtxcolor");
    auto RCFD    = std::make_shared<lev2::RenderContextFrameData>(tgt);
    auto par_mvp = mtl->param("MatMVP");

    // Calculate maximum label width for alignment
    int max_label_width = 0;
    for (auto channel : _channelmap) {
      if (!channel->_series.empty()) {
        for (auto& series : channel->_series) {
          int sw          = lev2::FontMan::stringWidth(series->_name.length());
          max_label_width = std::max(max_label_width, sw);
        }
      } else {
        int sw          = lev2::FontMan::stringWidth(channel->_name.length());
        max_label_width = std::max(max_label_width, sw);
      }
    }

    // Count total series across ALL channels for global lane layout
    // Stacked channels count as 1 lane (all series share it)
    size_t global_total_series = 0;
    for (auto channel : _channelmap) {
      if (channel->_stacked) {
        global_total_series += 1;
      } else {
        global_total_series += channel->_series.size();
      }
    }
    float global_lane_height = global_total_series > 0 ? float(height()) / float(global_total_series) : float(height());
    size_t global_lane_index = 0;

    int ichanlaby = _kbasechanlaby;
    for (auto channel : _channelmap) {
      const std::string& name = channel->_name;

      // Check if using series-based system
      bool has_series = !channel->_series.empty();

      size_t numpoints = 0;
      if (has_series && !channel->_series.empty()) {
        numpoints = channel->_series[0]->sampleCount();
      }

      ///////////////////////////////////////////////////
      // draw series labels/toggleboxes (one per series)
      ///////////////////////////////////////////////////

      size_t channel_lane_start = global_lane_index;

      ///////////////////////////////////////////////////
      // Lane background/outline (drawn before labels/data)
      ///////////////////////////////////////////////////
      if (has_series && channel->_lane_bgcolor.w > 0.0f) {
        size_t num_lanes = channel->_stacked ? 1 : channel->_series.size();
        float bg_y_top = float(channel_lane_start) * global_lane_height;
        float bg_y_bottom = bg_y_top + float(num_lanes) * global_lane_height;

        fvec3 bg_rgb(channel->_lane_bgcolor.x, channel->_lane_bgcolor.y, channel->_lane_bgcolor.z);

        lev2::VtxWriter<vtx_t> vw_bg;
        vw_bg.Lock(tgt, vbuf.get(), 6);
        vw_bg.AddVertex(vtx_t(fvec3(0, bg_y_top, 0), fvec4(), bg_rgb));
        vw_bg.AddVertex(vtx_t(fvec3(width(), bg_y_top, 0), fvec4(), bg_rgb));
        vw_bg.AddVertex(vtx_t(fvec3(width(), bg_y_bottom, 0), fvec4(), bg_rgb));
        vw_bg.AddVertex(vtx_t(fvec3(0, bg_y_top, 0), fvec4(), bg_rgb));
        vw_bg.AddVertex(vtx_t(fvec3(width(), bg_y_bottom, 0), fvec4(), bg_rgb));
        vw_bg.AddVertex(vtx_t(fvec3(0, bg_y_bottom, 0), fvec4(), bg_rgb));
        vw_bg.UnLock(tgt);

        mtxi->PushUIMatrix(width(), height());
        auto rs = mtl->_rasterstate;
        auto omacro = rs->_blendingMacro;
        rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
        rs->setDepthTest(lev2::EDepthTest::OFF);
        rs->setWriteMaskZ(false);
        auto fxi = tgt->FXI();
        fxi->pushRasterState(rs);
        mtl->begin(tek, RCFD);
        mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
        gbi->DrawPrimitiveEML(vw_bg, lev2::PrimitiveType::TRIANGLES);
        mtl->end(RCFD);
        fxi->popRasterState();
        rs->_blendingMacro = omacro;
        mtxi->PopUIMatrix();
      }

      if (has_series && channel->_lane_outline) {
        size_t num_lanes = channel->_stacked ? 1 : channel->_series.size();
        float ol_y_top = float(channel_lane_start) * global_lane_height;
        float ol_y_bottom = ol_y_top + float(num_lanes) * global_lane_height;
        fvec3 ol_color = channel->_lane_outline_color;

        lev2::VtxWriter<vtx_t> vw_ol;
        vw_ol.Lock(tgt, vbuf.get(), 8);
        vw_ol.AddVertex(vtx_t(fvec3(0, ol_y_top, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(width(), ol_y_top, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(width(), ol_y_top, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(width(), ol_y_bottom, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(width(), ol_y_bottom, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(0, ol_y_bottom, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(0, ol_y_bottom, 0), fvec4(), ol_color));
        vw_ol.AddVertex(vtx_t(fvec3(0, ol_y_top, 0), fvec4(), ol_color));
        vw_ol.UnLock(tgt);

        mtxi->PushUIMatrix(width(), height());
        mtl->begin(tek, RCFD);
        mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
        mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
        gbi->DrawPrimitiveEML(vw_ol, lev2::PrimitiveType::LINES);
        mtl->end(RCFD);
        mtxi->PopUIMatrix();
      }

      ///////////////////////////////////////////////////
      // draw series labels/toggleboxes
      ///////////////////////////////////////////////////

      if (has_series && channel->_stacked) {
        //////////////////////////////////////////////////////////
        // Stacked channel: compact legend within 1 lane
        //////////////////////////////////////////////////////////
        float lane_y_top = float(global_lane_index) * global_lane_height;
        int legend_y = int(lane_y_top) + 4;  // 4px padding from top of lane
        int legend_x = width() - (max_label_width + 80);  // leave room for value

        for (auto& series : channel->_series) {
          // Color swatch (small filled square)
          fvec3 label_color = series->_visible ? series->_color : series->_color * 0.3f;
          int swatch_x1 = legend_x - 14;
          int swatch_x2 = legend_x - 2;
          int swatch_y1 = legend_y;
          int swatch_y2 = legend_y + 12;

          {
            lev2::VtxWriter<vtx_t> vw_sw;
            vw_sw.Lock(tgt, vbuf.get(), 6);
            vw_sw.AddVertex(vtx_t(fvec3(swatch_x1, swatch_y1, 0), fvec4(), label_color));
            vw_sw.AddVertex(vtx_t(fvec3(swatch_x2, swatch_y1, 0), fvec4(), label_color));
            vw_sw.AddVertex(vtx_t(fvec3(swatch_x2, swatch_y2, 0), fvec4(), label_color));
            vw_sw.AddVertex(vtx_t(fvec3(swatch_x1, swatch_y1, 0), fvec4(), label_color));
            vw_sw.AddVertex(vtx_t(fvec3(swatch_x2, swatch_y2, 0), fvec4(), label_color));
            vw_sw.AddVertex(vtx_t(fvec3(swatch_x1, swatch_y2, 0), fvec4(), label_color));
            vw_sw.UnLock(tgt);

            mtxi->PushUIMatrix(width(), height());
            mtl->begin(tek, RCFD);
            mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
            mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
            gbi->DrawPrimitiveEML(vw_sw, lev2::PrimitiveType::TRIANGLES);
            mtl->end(RCFD);
            mtxi->PopUIMatrix();
          }

          // Series name
          tgt->RefModColor() = label_color;
          mtxi->PushUIMatrix(width(), height());
          lev2::FontMan::beginTextBlock(tgt, 128);
          lev2::FontMan::DrawText(tgt, legend_x, legend_y, series->_name.c_str());
          lev2::FontMan::endTextBlock(tgt);
          mtxi->PopUIMatrix();

          // Current value
          if (series->_visible && series->sampleCount() > 0) {
            float value = series->getSample(series->sampleCount() - 1);
            auto valstr = FormatString("%0.5g", value);
            tgt->RefModColor() = series->_color;
            mtxi->PushUIMatrix(width(), height());
            lev2::FontMan::beginTextBlock(tgt, 128);
            lev2::FontMan::DrawText(tgt, width() - 70, legend_y, valstr.c_str());
            lev2::FontMan::endTextBlock(tgt);
            mtxi->PopUIMatrix();
          }

          legend_y += 16;  // advance to next legend row
        }

        global_lane_index += 1;  // stacked channel = 1 lane
      } else if (has_series) {
        //////////////////////////////////////////////////////////
        // Non-stacked: original per-series label rendering
        //////////////////////////////////////////////////////////
        for (auto& series : channel->_series) {
          // Calculate vertical center using global lane index
          float label_center_y = (float(global_lane_index) + 0.5f) * global_lane_height;
          int label_y = int(label_center_y) - 8;  // Center the 16-pixel label box

          int sw = lev2::FontMan::stringWidth(series->_name.length());

          // Pulse selected series label
          fvec3 label_color = series->_color;
          if (_selected_series == series) {
            float time = _uicontext->_uitimer.SecsSinceStart();
            float pulse = 0.875f + 0.125f * sinf(time * 2.0f * 3.14159f * 2.0f);
            label_color = label_color * pulse;
            mNeedsSurfaceRepaint = true;
          }

          if (!series->_visible) {
            label_color = label_color * 0.3f;
          }

          tgt->RefModColor() = label_color;
          mtxi->PushUIMatrix(width(), height());
          lev2::FontMan::beginTextBlock(tgt, 128);
          lev2::FontMan::DrawText(tgt, width() - (max_label_width + 16), label_y, series->_name.c_str());
          lev2::FontMan::endTextBlock(tgt);
          mtxi->PopUIMatrix();

          if (series->_visible) {
            size_t series_count = series->sampleCount();
            if (series_count > 0) {
              float value        = series->getSample(series_count - 1);
              auto valstr        = FormatString("%0.5g", value);
              int sw2            = lev2::FontMan::stringWidth(valstr.length());
              tgt->RefModColor() = series->_color;
              mtxi->PushUIMatrix(width(), height());
              lev2::FontMan::beginTextBlock(tgt, 128);
              lev2::FontMan::DrawText(tgt, width() - (max_label_width + 16) - (sw2 + 16), label_y, valstr.c_str());
              lev2::FontMan::endTextBlock(tgt);
              mtxi->PopUIMatrix();
            }
          }

          // Toggle box outline
          int x1 = width() - (max_label_width + 28);
          int x2 = width() - 16;
          int y1 = label_y;
          int y2 = label_y + 16;

          fvec3 box_color = series->_visible ? series->_color : series->_color * 0.3f;

          lev2::VtxWriter<vtx_t> vw;
          vw.Lock(tgt, vbuf.get(), 8);
          vw.AddVertex(vtx_t(fvec3(x1, y1, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x2, y1, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x2, y1, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x2, y2, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x2, y2, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x1, y2, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x1, y2, 0), fvec4(), box_color));
          vw.AddVertex(vtx_t(fvec3(x1, y1, 0), fvec4(), box_color));
          vw.UnLock(tgt);

          mtxi->PushUIMatrix(width(), height());
          mtl->begin(tek, RCFD);
          mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
          mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
          gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
          mtl->end(RCFD);
          mtxi->PopUIMatrix();

          global_lane_index++;
        }
      }

      ///////////////////////////////////////////////////

      // Calculate range (series-based only)
      fvec2 hrange, vrange;
      bool has_visible_series = false;

      // Calculate Y-axis pixel position (FIXED position, independent of changing value strings)
      const int value_text_reserved_width = 80;
      int y_axis_pixel_x = width() - (max_label_width + 16) - value_text_reserved_width - 32;

      if (has_series && !channel->_series.empty()) {
        for (auto& series : channel->_series) {
          if (series->_visible && series->sampleCount() > 0) {
            has_visible_series = true;
            break;
          }
        }

        if (has_visible_series) {
          float hextent = _grid._extent / _grid._zoomX;
          float pixels_left_of_axis = y_axis_pixel_x;
          float pixels_total = width();
          float data_left = hextent * (pixels_left_of_axis / pixels_total);
          hrange = fvec2(-data_left, hextent - data_left);
        } else {
          hrange = fvec2(-50, 50);
          vrange = fvec2(0, 1);
        }
      } else {
        hrange = fvec2(0, 100);
        vrange = fvec2(0, 1);
      }

      int w = this->width();
      int h = this->height();

      if (numpoints && has_visible_series) {
        if (has_series && channel->_stacked) {
          ///////////////////////////////////////////////////
          // Render stacked area chart
          ///////////////////////////////////////////////////
          float lane_y_top = float(channel_lane_start) * global_lane_height;
          float lane_y_bottom = lane_y_top + global_lane_height;

          // Determine Y range from first series' fixed range (or auto-range)
          float stack_min = 0.0f;
          float stack_max = 1.0f;
          auto& first_series = channel->_series[0];
          if (first_series->_use_fixed_range) {
            stack_min = first_series->_fixed_min;
            stack_max = first_series->_fixed_max;
          } else {
            // Auto-range from max cumulative sum across all samples
            size_t sc = first_series->sampleCount();
            float max_sum = 0.0f;
            for (size_t i = 0; i < sc; i++) {
              float sum = 0.0f;
              for (auto& s : channel->_series) {
                if (s->_visible && i < s->sampleCount())
                  sum += s->getSample(i);
              }
              max_sum = std::max(max_sum, sum);
            }
            stack_max = max_sum > 0.0f ? max_sum * 1.1f : 1.0f;
          }

          float x_scale = float(w) / (hrange.y - hrange.x);
          float y_range = stack_max - stack_min;
          if (y_range < 0.001f) y_range = 1.0f;

          size_t sample_count = first_series->sampleCount();

          // For each series, draw filled area between cumulative bottom and top
          // Render bottom-to-top so later series stack visually on top
          for (size_t si = 0; si < channel->_series.size(); si++) {
            auto& series = channel->_series[si];
            if (!series->_visible) continue;

            size_t sc = series->sampleCount();
            if (sc < 2) continue;

            // Calculate display window
            size_t display_count = sc;
            size_t start_index = 0;
            if (series->_window_size > 0 && series->_window_size < sc) {
              display_count = series->_window_size;
              start_index = sc - display_count;
            }

            // Each pair of adjacent samples produces 2 triangles (6 vertices)
            size_t num_quads = display_count > 1 ? display_count - 1 : 0;
            if (num_quads == 0) continue;

            mtxi->PushUIMatrix(w, h);

            lev2::VtxWriter<vtx_t> vw;
            vw.Lock(tgt, vbuf.get(), num_quads * 6);

            for (size_t i = 1; i < display_count; i++) {
              size_t idx_prev = start_index + i - 1;
              size_t idx_curr = start_index + i;

              // Calculate cumulative bottom (sum of all series below this one)
              float bottom_prev = 0.0f, bottom_curr = 0.0f;
              for (size_t sj = 0; sj < si; sj++) {
                auto& s_below = channel->_series[sj];
                if (!s_below->_visible) continue;
                if (idx_prev < s_below->sampleCount()) bottom_prev += s_below->getSample(idx_prev);
                if (idx_curr < s_below->sampleCount()) bottom_curr += s_below->getSample(idx_curr);
              }

              // Top = bottom + this series' value
              float top_prev = bottom_prev + (idx_prev < sc ? series->getSample(idx_prev) : 0.0f);
              float top_curr = bottom_curr + (idx_curr < sc ? series->getSample(idx_curr) : 0.0f);

              // Map data X to screen X
              float data_x_prev = float(idx_prev) - float(sample_count - 1);
              float data_x_curr = float(idx_curr) - float(sample_count - 1);
              float sx_prev = (data_x_prev - hrange.x) * x_scale;
              float sx_curr = (data_x_curr - hrange.x) * x_scale;

              // Map data Y to screen Y (bottom of lane = stack_min, top of lane = stack_max)
              // Screen Y is inverted (top=0), so higher values go UP (smaller screen Y)
              auto mapY = [&](float val) -> float {
                float normalized = (val - stack_min) / y_range;
                return lane_y_bottom - normalized * global_lane_height;
              };

              float sy_top_prev = mapY(top_prev);
              float sy_bot_prev = mapY(bottom_prev);
              float sy_top_curr = mapY(top_curr);
              float sy_bot_curr = mapY(bottom_curr);

              // Clamp minimum band height (screen Y inverted: bot > top)
              float min_h = channel->_min_series_height;
              if (sy_bot_prev - sy_top_prev < min_h)
                sy_top_prev = sy_bot_prev - min_h;
              if (sy_bot_curr - sy_top_curr < min_h)
                sy_top_curr = sy_bot_curr - min_h;

              // Two triangles forming a quad: (bot_prev, bot_curr, top_curr), (bot_prev, top_curr, top_prev)
              vw.AddVertex(vtx_t(fvec3(sx_prev, sy_bot_prev, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(sx_curr, sy_bot_curr, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(sx_curr, sy_top_curr, 0), fvec4(), series->_color));

              vw.AddVertex(vtx_t(fvec3(sx_prev, sy_bot_prev, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(sx_curr, sy_top_curr, 0), fvec4(), series->_color));
              vw.AddVertex(vtx_t(fvec3(sx_prev, sy_top_prev, 0), fvec4(), series->_color));
            }
            vw.UnLock(tgt);

            auto rs = mtl->_rasterstate;
            auto omacro = rs->_blendingMacro;
            int prev_pri = rs->_priority;
            rs->setBlendingMacro(lev2::BlendingMacro::OFF);
            rs->setDepthTest(lev2::EDepthTest::OFF);
            rs->setWriteMaskZ(false);
            rs->_priority = 1 << 16;
            auto fxi = tgt->FXI();
            fxi->pushRasterState(rs);

            mtl->begin(tek, RCFD);
            mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
            gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
            mtl->end(RCFD);

            fxi->popRasterState();
            rs->_priority = prev_pri;
            rs->_blendingMacro = omacro;

            mtxi->PopUIMatrix();
          }

        } else if (has_series) {
          ///////////////////////////////////////////////////
          // Render non-stacked line plots (original)
          ///////////////////////////////////////////////////
          size_t lane_index = channel_lane_start;

          for (auto& series : channel->_series) {
            size_t series_count = series->sampleCount();

            if (series->_visible && series_count > 0) {
              size_t display_count = series_count;
              size_t start_index   = 0;

              if (series->_window_size > 0 && series->_window_size < series_count) {
                display_count = series->_window_size;
                start_index   = series_count - display_count;
              }

              float series_min, series_max;

              if (series->_use_fixed_range) {
                series_min = series->_fixed_min;
                series_max = series->_fixed_max;
              } else {
                float current_min = std::numeric_limits<float>::max();
                float current_max = std::numeric_limits<float>::lowest();
                for (size_t i = 0; i < series_count; i++) {
                  float val = series->getSample(i);
                  current_min = std::min(current_min, val);
                  current_max = std::max(current_max, val);
                }

                float b = series->_blend_rate;
                float inb = 1.0f - b;
                series->_min_value = series->_min_value * inb + current_min * b;
                series->_max_value = series->_max_value * inb + current_max * b;
                series->_blend_rate = std::max(0.01f, series->_blend_rate - 0.001f);

                series_min = series->_min_value;
                series_max = series->_max_value;
              }

              float series_range = series_max - series_min;
              const float min_range = 0.1f;
              if (series_range < min_range) {
                float center = (series_min + series_max) / 2.0f;
                series_min = center - min_range / 2.0f;
                series_max = center + min_range / 2.0f;
                series_range = min_range;
              }

              float lane_y_top_pixel = float(lane_index) * global_lane_height;

              mtxi->PushUIMatrix(w, h);

              lev2::VtxWriter<vtx_t> vw;
              vw.Lock(tgt, vbuf.get(), display_count * 2);

              float x_scale = float(w) / (hrange.y - hrange.x);
              float y_scale = global_lane_height / (series_max - series_min) * series->_vertical_scale;
              float data_center_y = (series_min + series_max) / 2.0f;
              float lane_center_y = lane_y_top_pixel + global_lane_height / 2.0f;

              for (size_t i = 0; i < display_count; i++) {
                size_t sample_index = start_index + i;
                float data_x = float(sample_index) - float(series_count - 1);
                float data_y = series->getSample(sample_index);

                float screen_x = (data_x - hrange.x) * x_scale;
                float screen_y = lane_center_y - (data_y - data_center_y) * y_scale;

                if (i > 0) {
                  size_t prev_sample_index = start_index + i - 1;
                  float prev_data_x = float(prev_sample_index) - float(series_count - 1);
                  float prev_data_y = series->getSample(prev_sample_index);

                  float prev_screen_x = (prev_data_x - hrange.x) * x_scale;
                  float prev_screen_y = lane_center_y - (prev_data_y - data_center_y) * y_scale;

                  vw.AddVertex(vtx_t(fvec3(prev_screen_x, prev_screen_y, 0), fvec4(), series->_color));
                  vw.AddVertex(vtx_t(fvec3(screen_x, screen_y, 0), fvec4(), series->_color));
                }
              }
              vw.UnLock(tgt);

              auto rs = mtl->_rasterstate;
              auto omacro = rs->_blendingMacro;
              int prev_pri = rs->_priority;
              rs->setBlendingMacro(lev2::BlendingMacro::ADDITIVE);
              rs->setDepthTest(lev2::EDepthTest::OFF);
              rs->setWriteMaskZ(false);
              rs->_priority = 1<<16;
              auto fxi = tgt->FXI();
              fxi->pushRasterState(rs);

              mtl->begin(tek, RCFD);
              mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
              gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
              mtl->end(RCFD);

              fxi->popRasterState();
              rs->_priority = prev_pri;
              rs->_blendingMacro = omacro;

              mtxi->PopUIMatrix();
            }

            lane_index++;
          }
        }
        ///////////////////////////////////////////////////
      }
    }
    ///////////////////////////////
    // draw misc labels in UI pixel space
    ///////////////////////////////


    mtxi->PushUIMatrix(width(), height());
    tgt->RefModColor() = fvec3(1, 1, 1);
    if(_show_stats){
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
        lev2::FontMan::DrawText(
            tgt, //
            16,  // Top-left with 16px left margin
            16,  // Top with 16px top margin
            _name.c_str());
        lev2::FontMan::endTextBlock(tgt);
      }
    }
    ///////////////////////////////
    mtxi->PopUIMatrix(); // Pop UI matrix for text rendering
  }
  ork::lev2::FontMan::PopFont();
}
///////////////////////////////////////////////////////////////////////////////
// GraphView Helper Functions
///////////////////////////////////////////////////////////////////////////////
graphseries_ptr_t GraphView::_findSeriesAtPoint(int x, int y) {
  // Check if click is in label/toggle region (right side of screen)
  if (x <= (width() - 150)) return nullptr;

  // Count total lanes (stacked channels = 1 lane)
  size_t total_lanes = 0;
  for (auto channel : _channelmap) {
    if (channel->_stacked) {
      total_lanes += 1;
    } else {
      total_lanes += channel->_series.size();
    }
  }

  if (total_lanes == 0) return nullptr;

  float lane_height = float(height()) / float(total_lanes);
  int lane_index = int(float(y) / lane_height);

  if (lane_index < 0 || lane_index >= int(total_lanes)) return nullptr;

  // Find the series at this lane index
  int current_lane = 0;
  for (auto channel : _channelmap) {
    if (channel->_stacked) {
      if (current_lane == lane_index) {
        // For stacked channels, find which legend row was clicked
        float lane_top = float(current_lane) * lane_height;
        int legend_row = int(float(y - lane_top - 4) / 16.0f);
        if (legend_row >= 0 && legend_row < int(channel->_series.size())) {
          return channel->_series[legend_row];
        }
        return nullptr;
      }
      current_lane++;
    } else {
      for (auto& series : channel->_series) {
        if (current_lane == lane_index) return series;
        current_lane++;
      }
    }
  }

  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
void GraphView::_adjustSeriesScale(int wheel_delta) {
  const float zoom_rate = 1.01f;

  // Apply zoom
  if (wheel_delta > 0) {
    _selected_series->_vertical_scale *= zoom_rate;
  } else {
    _selected_series->_vertical_scale /= zoom_rate;
  }
  _selected_series->_vertical_scale = clamp(_selected_series->_vertical_scale, 0.001f, 1000.0f);

  printf("series<%s> scale=%.6f (wheel_delta=%d)\n",
         _selected_series->_name.c_str(),
         _selected_series->_vertical_scale,
         wheel_delta);
}
/////////////////////////////////////////////////////////////////////////
void GraphView::_adjustGlobalZoom(int wheel_delta) {
  const float zoom_rate = 1.001f;
  if (wheel_delta > 0) {
    _grid._zoomX *= zoom_rate;
  } else {
    _grid._zoomX /= zoom_rate;
  }
  _grid._zoomX = clamp(_grid._zoomX, 0.02f, 10.0f);
}
///////////////////////////////////////////////////////////////////////////////
void GraphPanel::setRect(int iX, int iY, int iW, int iH, bool snap) {
  _uipanel->SetRect(iX, iY, iW, iH);
  if (snap)
    _uipanel->snap();
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
