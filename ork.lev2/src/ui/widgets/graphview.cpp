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
    _historical_min = 0.0f;
    _historical_max = 1.0f;
    return;
  }

  // Calculate current buffer min/max
  float current_min = _samples[0];
  float current_max = _samples[0];

  for (float val : _samples) {
    current_min = std::min(current_min, val);
    current_max = std::max(current_max, val);
  }

  // Add 10% padding to current range
  float range = current_max - current_min;
  if (range < 0.001f)
    range = 0.001f; // Avoid div by zero
  current_min -= range * 0.1f;
  current_max += range * 0.1f;

  // Initialize historical values on first update
  if (_range_update_counter == 0) {
    _historical_min = current_min;
    _historical_max = current_max;
  }

  // Apply inertia/momentum:
  // - Expand immediately when current range exceeds historical range
  // - Decay slowly toward current range (max 1% change per second at 60fps)
  _historical_min = std::min(_historical_min, current_min);  // Expand down immediately
  _historical_max = std::max(_historical_max, current_max);  // Expand up immediately

  // Decay toward current range slowly (lerp)
  _historical_min = _historical_min * _range_decay_rate + current_min * (1.0f - _range_decay_rate);
  _historical_max = _historical_max * _range_decay_rate + current_max * (1.0f - _range_decay_rate);

  // Use historical range as display range
  _min_value = _historical_min;
  _max_value = _historical_max;

  _range_update_counter++;
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
      _dragging          = false;
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
        int maxy       = total_series * row_height + _kbasechanlaby + 16;
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
        // Removed horizontal panning - X axis is fixed at Y-axis position
        // float newctr_x = _downCenter.x - (delta.x / _grid._zoomX);
        float newctr_y = _downCenter.y + (delta.y / _grid._zoomY);

        // Horizontal panning disabled (Y-axis is fixed)
        // if (not _lockX)
        //   _grid._center.x = newctr_x;
        if (not _lockY)
          _grid._center.y = newctr_y;

        mNeedsSurfaceRepaint = true;
      }
      break;
    }
    case EventCode::MOUSEWHEEL: {
      int idelta = ev->miMWY;
      // Very fine zoom rate for precise control
      // (1.01 = 1% change per tick)
      const float zoom_rate = 1.01f;

      if (idelta > 0) {
        _grid._zoomX *= zoom_rate;
        if (not _lockYZOOM)
          _grid._zoomY *= zoom_rate;
      } else if (idelta < 0) {
        _grid._zoomX *= 1.0f / zoom_rate;
        if (not _lockYZOOM)
          _grid._zoomY *= 1.0f / zoom_rate;
      }
      _grid._zoomX         = clamp(_grid._zoomX, 0.02f, 10.0f);  // 0.02 = 5x more zoom out than 0.1
      _grid._zoomY         = clamp(_grid._zoomY, 0.02f, 10.0f);
      mNeedsSurfaceRepaint = true;
      return HandlerResult(this);
      break;
    }
    case ui::EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;
      printf("GraphView<%s> keydown<%c>\n", _name.c_str(), key);

      // 'm' key toggles between AUTO and MANUAL vertical scale modes
      switch (key) {
        case 'M': {
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
        case ',': {
          // increase num of samples
          for (auto channel : _channelmap) {
            for (auto& series : channel->_series) {
              size_t curr_max = series->sampleCount();
              curr_max /= 2;
              series->setMaxSamples(curr_max);
              printf("  Series<%s> max samples increased to %zu\n", series->_name.c_str(), curr_max);
            } 
          }
          mNeedsSurfaceRepaint = true;
          SetDirty();
          return HandlerResult(this);
        }
        break;
        case '.': {
          // increase num of samples
          for (auto channel : _channelmap) {
            for (auto& series : channel->_series) {
              size_t curr_max = series->sampleCount();
              curr_max *= 2;
              series->setMaxSamples(curr_max);
              printf("  Series<%s> max samples increased to %zu\n", series->_name.c_str(), curr_max);
            } 
          }
          mNeedsSurfaceRepaint = true;
          SetDirty();
          return HandlerResult(this);
        }
        break;
      }
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

      if (has_series) {
        // Draw a label/button for each series
        size_t num_series = channel->_series.size();
        for (auto& series : channel->_series) {
          int sw             = lev2::FontMan::stringWidth(series->_name.length());
          tgt->RefModColor() = series->_color;
          mtxi->PushUIMatrix(width(), height());
          lev2::FontMan::beginTextBlock(tgt, 128);
          lev2::FontMan::DrawText(tgt, width() - (max_label_width + 16), ichanlaby, series->_name.c_str());
          lev2::FontMan::endTextBlock(tgt);
          mtxi->PopUIMatrix();

          if (series->_visible) {
            ///////////////////////////////////////////////////
            // draw current value
            ///////////////////////////////////////////////////
            size_t series_count = series->sampleCount();
            if (series_count > 0) {
              float value        = series->getSample(series_count - 1);
              auto valstr        = FormatString("%0.5g", value);
              int sw2            = lev2::FontMan::stringWidth(valstr.length());
              tgt->RefModColor() = series->_color;
              mtxi->PushUIMatrix(width(), height());
              lev2::FontMan::beginTextBlock(tgt, 128);
              lev2::FontMan::DrawText(tgt, width() - (max_label_width + 16) - (sw2 + 16), ichanlaby, valstr.c_str());
              lev2::FontMan::endTextBlock(tgt);
              mtxi->PopUIMatrix();
            }

            ///////////////////////////////////////////////////
            // draw toggle box
            ///////////////////////////////////////////////////
            int x1 = width() - (max_label_width + 28); // ~12 pixels left margin (1 char width)
            int x2 = width() - 16;                     // 16 pixels right margin
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
      }

      ///////////////////////////////////////////////////

      // Calculate range (series-based only)
      fvec2 hrange, vrange;
      bool has_visible_series = false;

      // Calculate Y-axis pixel position (FIXED position, independent of changing value strings)
      // Reserve a fixed width for value text (e.g., 80 pixels for up to ~10 characters)
      const int value_text_reserved_width = 80;
      int y_axis_pixel_x = width() - (max_label_width + 16) - value_text_reserved_width - 32;

      if (has_series && !channel->_series.empty()) {
        // Check if any visible series exist
        for (auto& series : channel->_series) {
          if (series->_visible && series->sampleCount() > 0) {
            has_visible_series = true;
            break;
          }
        }

        if (has_visible_series) {
          // Horizontal range: Fixed Y-axis at X=0, positioned at y_axis_pixel_x
          // The horizontal range is calculated so that X=0 maps to y_axis_pixel_x in screen space
          // and the range extends from X=0 leftward (negative X values)
          float hextent = _grid._extent / _grid._zoomX;
          float pixels_left_of_axis = y_axis_pixel_x;  // Pixels from left edge to Y-axis
          float pixels_total = width();

          // Calculate how much of the data space is to the left of X=0 (the Y-axis)
          // Proportion: pixels_left_of_axis / pixels_total = data_left / hextent
          float data_left = hextent * (pixels_left_of_axis / pixels_total);

          hrange = fvec2(-data_left, hextent - data_left);

          // Vertical range depends on mode
          if (_vscale_mode == VerticalScaleMode::AUTO) {
            // AUTO mode: find min/max across ALL visible series
            float global_min = std::numeric_limits<float>::max();
            float global_max = std::numeric_limits<float>::lowest();

            for (auto& series : channel->_series) {
              if (series->_visible && series->sampleCount() > 0) {
                global_min = std::min(global_min, series->_min_value);
                global_max = std::max(global_max, series->_max_value);
              }
            }
            vrange = fvec2(global_min, global_max);
          } else {
            // MANUAL mode: use grid zoom/center
            float vcenter = _grid._center.y;
            float vextent = _grid._extent * _grid._aspect / _grid._zoomY;
            vrange        = fvec2(vcenter - vextent / 2, vcenter + vextent / 2);
          }
        } else {
          hrange = fvec2(-50, 50); // Centered on origin
          vrange = fvec2(0, 1);
        }
      } else {
        // No valid series data
        hrange = fvec2(0, 100);
        vrange = fvec2(0, 1);
      }

      int w = this->width();
      int h = this->height();

      if (numpoints && has_visible_series) {
        // Create ortho matrix using hrange/vrange (computed once, shared for axes and all series)
        auto custom_ortho = mtxi->Ortho(hrange.x, hrange.y, vrange.y, vrange.x, 0.0f, 1.0f);

        // Push matrix stack once for all rendering
        mtxi->PushPMatrix(custom_ortho);
        mtxi->PushVMatrix(fmtx4::Identity());
        mtxi->PushMMatrix(fmtx4::Identity());

        ///////////////////////////////////////////////////
        // Draw X and Y axis lines in plot coordinates
        // Only render axes if there are visible series
        ///////////////////////////////////////////////////
        {
          lev2::VtxWriter<vtx_t> vw;
          vw.Lock(tgt, vbuf.get(), 4);

          U32 axis_color = 0xff4d4d4d; // gray color

          // X axis (horizontal line at Y=0)
          vw.AddVertex(vtx_t(fvec3(hrange.x, 0.0f, 0), fvec4(), axis_color));
          vw.AddVertex(vtx_t(fvec3(hrange.y, 0.0f, 0), fvec4(), axis_color));

          // Y axis (vertical line at X=0)
          vw.AddVertex(vtx_t(fvec3(0.0f, vrange.x, 0), fvec4(), axis_color));
          vw.AddVertex(vtx_t(fvec3(0.0f, vrange.y, 0), fvec4(), axis_color));

          vw.UnLock(tgt);

          mtl->begin(tek, RCFD);
          mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
          mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
          gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
          mtl->end(RCFD);
        }
        ///////////////////////////////////////////////////
        // Render series-based data
        ///////////////////////////////////////////////////
        if (has_series) {
          size_t num_series = channel->_series.size();
          for (auto& series : channel->_series) {
            if (!series->_visible)
              continue;

            size_t series_count = series->sampleCount();
            if (series_count == 0)
              continue;

            // Calculate moving window - which samples to display
            size_t display_count = series_count;
            size_t start_index   = 0;

            if (series->_window_size > 0 && series->_window_size < series_count) {
              display_count = series->_window_size;
              start_index   = series_count - display_count; // Show most recent N samples
            }

            lev2::VtxWriter<vtx_t> vw;
            vw.Lock(tgt, vbuf.get(), display_count * 2);

            for (size_t i = 0; i < display_count; i++) {
              size_t sample_index = start_index + i;
              // Current sample (most recent) at x=0, older samples at negative X
              float x = float(sample_index) - float(series_count - 1);
              float y = series->getSample(sample_index);

              if (i > 0) {
                size_t prev_sample_index = start_index + i - 1;
                float prev_x = float(prev_sample_index) - float(series_count - 1);
                float prev_y = series->getSample(prev_sample_index);

                vw.AddVertex(vtx_t(fvec3(prev_x, prev_y, 0), fvec4(), series->_color));
                vw.AddVertex(vtx_t(fvec3(x, y, 0), fvec4(), series->_color));
              }
            }
            vw.UnLock(tgt);

            // Draw series (reusing matrix stack) with additive blending
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
          }
        }

        // Pop matrix stack once at the end
        mtxi->PopPMatrix();
        mtxi->PopVMatrix();
        mtxi->PopMMatrix();
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
void GraphPanel::setRect(int iX, int iY, int iW, int iH, bool snap) {
  _uipanel->SetRect(iX, iY, iW, iH);
  if (snap)
    _uipanel->snap();
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
