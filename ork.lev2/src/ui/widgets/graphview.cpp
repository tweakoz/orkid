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
      auto clicked_series = _findSeriesAtPoint(ilocx, ilocy);

      if (clicked_series) {
        // Toggle selection
        if (_selected_series == clicked_series) {
          _selected_series = nullptr;
          printf("series<%s> deselected\n", clicked_series->_name.c_str());
        } else {
          _selected_series = clicked_series;
          printf("series<%s> selected (scale=%.3f, offset=%.3f)\n",
                 _selected_series->_name.c_str(), _selected_series->_vertical_scale,
                 _selected_series->_vertical_offset);
        }
      } else {
        // Start drag (only for series offset adjustment if selected)
        _downPixelY = ilocy;
        _downSeriesOffset = _selected_series ? _selected_series->_vertical_offset : 0.0f;
        _dragging = true;
        if (_selected_series) {
          printf("DRAG START: series<%s> offset=%.3f\n",
                 _selected_series->_name.c_str(), _downSeriesOffset);
        }
      }
      mNeedsSurfaceRepaint = true;
      break;
    }
    case ui::EventCode::MOVE: {
      _hovered_series = _findSeriesAtPoint(ilocx, ilocy);
      break;
    }
    case ui::EventCode::DRAG: {
      if (_dragging && _selected_series) {
        int pixel_delta_y = ilocy - _downPixelY;
        _adjustSeriesOffset(pixel_delta_y);
        mNeedsSurfaceRepaint = true;
      }
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
            _selected_series->_vertical_offset = 0.0f;
            _selected_series->_freeze_auto_range = false;
            printf("series<%s> RESET\n", _selected_series->_name.c_str());
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

        case 'M':
          _vscale_mode = (_vscale_mode == VerticalScaleMode::AUTO)
                         ? VerticalScaleMode::MANUAL
                         : VerticalScaleMode::AUTO;
          printf("Vertical scale mode: %s\n",
                 _vscale_mode == VerticalScaleMode::AUTO ? "AUTO" : "MANUAL");
          mNeedsSurfaceRepaint = true;
          return HandlerResult(this);

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

          // Pulse selected series label
          fvec3 label_color = series->_color;
          if (_selected_series == series) {
            // Pulse between 0.75 and 1.0 brightness at 2 Hz
            float time = _uicontext->_uitimer.SecsSinceStart();
            float pulse = 0.875f + 0.125f * sinf(time * 2.0f * 3.14159f * 2.0f);
            label_color = label_color * pulse;
            mNeedsSurfaceRepaint = true;  // Keep repainting for animation
          }

          tgt->RefModColor() = label_color;
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
            // Check if any visible series has normalization enabled
            bool any_normalized = false;
            for (auto& series : channel->_series) {
              if (series->_visible && series->_normalize_for_display) {
                any_normalized = true;
                break;
              }
            }

            if (any_normalized) {
              // If normalization is enabled, show [0,1] range
              vrange = fvec2(0.0f, 1.0f);
            } else {
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
            }
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
        ///////////////////////////////////////////////////
        // Render series-based data - each in its own track
        ///////////////////////////////////////////////////
        if (has_series) {
          // Count visible series to calculate track height
          size_t num_visible_series = 0;
          for (auto& series : channel->_series) {
            if (series->_visible && series->sampleCount() > 0) {
              num_visible_series++;
            }
          }

          if (num_visible_series > 0) {
            float track_height = float(h) / float(num_visible_series);
            size_t track_index = 0;

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

              // Calculate current min/max from samples
              float current_min = std::numeric_limits<float>::max();
              float current_max = std::numeric_limits<float>::lowest();
              for (size_t i = 0; i < series_count; i++) {
                float val = series->getSample(i);
                current_min = std::min(current_min, val);
                current_max = std::max(current_max, val);
              }

              // Blend min/max smoothly: approach new value at 1% per frame
              series->_min_value = series->_min_value * 0.99f + current_min * 0.01f;
              series->_max_value = series->_max_value * 0.99f + current_max * 0.01f;

              float series_min = series->_min_value;
              float series_max = series->_max_value;
              float series_range = series_max - series_min;

              // If range is too small (constant value), add padding around center
              const float min_range = 0.1f;
              if (series_range < min_range) {
                float center = (series_min + series_max) / 2.0f;
                series_min = center - min_range / 2.0f;
                series_max = center + min_range / 2.0f;
                series_range = min_range;
              }

              // Calculate this series' vertical pixel range (from top, since screen Y goes down)
              float track_y_top_pixel = float(track_index) * track_height;
              float track_y_bottom_pixel = track_y_top_pixel + track_height;

              // Create a composite transformation:
              // 1. Data X range -> screen X
              // 2. Data Y (series_min to series_max) -> track pixels (track_y_top to track_y_bottom)
              // The Ortho call maps: (left, right, bottom, top) in data space to screen
              // For Y: we want series_max at top of track, series_min at bottom of track

              // We need to use a viewport + ortho approach
              // Set viewport to this track's pixel region
              tgt->FBI()->pushViewport(0, int(track_y_top_pixel), w, int(track_height));

              // Now create ortho that maps data range to normalized viewport
              auto track_ortho = mtxi->Ortho(
                hrange.x, hrange.y,           // X: shared horizontal range
                series_min, series_max,       // Y: this series' data range (min at bottom, max at top)
                0.0f, 1.0f);

              mtxi->PushPMatrix(track_ortho);
              mtxi->PushVMatrix(fmtx4::Identity());
              mtxi->PushMMatrix(fmtx4::Identity());

              lev2::VtxWriter<vtx_t> vw;
              vw.Lock(tgt, vbuf.get(), display_count * 2);

              for (size_t i = 0; i < display_count; i++) {
                size_t sample_index = start_index + i;
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

              // Draw series with additive blending
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

              mtxi->PopPMatrix();
              mtxi->PopVMatrix();
              mtxi->PopMMatrix();

              // Restore viewport
              tgt->FBI()->popViewport();

              track_index++;
            }
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
  int total_series = 0;
  for (auto channel : _channelmap) {
    if (!channel->_series.empty()) {
      total_series += channel->_series.size();
    } else {
      total_series += 1;
    }
  }

  if (total_series == 0) return nullptr;

  int row_height = 16 + _label_spacing;
  int maxy = total_series * row_height + _kbasechanlaby + 16;

  if (x <= (width() - 150) || y >= maxy) return nullptr;

  int iseries_index = (y - 16) / row_height;
  int current_index = 0;

  for (auto channel : _channelmap) {
    if (!channel->_series.empty()) {
      for (auto& series : channel->_series) {
        if (current_index == iseries_index) return series;
        current_index++;
      }
    } else {
      current_index++;
    }
  }
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
void GraphView::_freezeSeriesAutoRange(graphseries_ptr_t series) {
  if (_vscale_mode != VerticalScaleMode::AUTO) return;
  if (series->_freeze_auto_range) return;

  series->_freeze_auto_range = true;
  series->_frozen_min = series->_min_value;
  series->_frozen_max = series->_max_value;
  printf("series<%s> FROZE auto-range: min=%.3f max=%.3f\n",
         series->_name.c_str(), series->_frozen_min, series->_frozen_max);
}
/////////////////////////////////////////////////////////////////////////
float GraphView::_getSeriesVerticalRange(graphseries_ptr_t series) {
  if (_vscale_mode == VerticalScaleMode::AUTO) {
    float vrange;
    if (series->_freeze_auto_range) {
      vrange = series->_frozen_max - series->_frozen_min;
    } else {
      vrange = series->_max_value - series->_min_value;
    }
    return (vrange < 0.001f) ? 1.0f : vrange;
  }
  return _grid._extent * _grid._aspect / _grid._zoomY;
}
/////////////////////////////////////////////////////////////////////////
void GraphView::_adjustSeriesOffset(int pixel_delta_y) {
  _freezeSeriesAutoRange(_selected_series);

  // For perfect 1:1 pixel tracking, we need to use the GLOBAL vrange
  // (same as rendering uses - min/max across ALL visible series)
  // not just the selected series' range
  float vrange_height;

  if (_vscale_mode == VerticalScaleMode::AUTO) {
    // Calculate global range across all visible series (same as rendering)
    float global_min = std::numeric_limits<float>::max();
    float global_max = std::numeric_limits<float>::lowest();

    for (auto channel : _channelmap) {
      for (auto& series : channel->_series) {
        if (series->_visible && series->sampleCount() > 0) {
          if (series->_freeze_auto_range) {
            global_min = std::min(global_min, series->_frozen_min);
            global_max = std::max(global_max, series->_frozen_max);
          } else {
            global_min = std::min(global_min, series->_min_value);
            global_max = std::max(global_max, series->_max_value);
          }
        }
      }
    }
    vrange_height = global_max - global_min;
    if (vrange_height < 0.001f) vrange_height = 1.0f;
  } else {
    // MANUAL mode
    vrange_height = _grid._extent * _grid._aspect / _grid._zoomY;
  }

  // Now calculate offset for 1:1 pixel tracking
  float delta = -(float(pixel_delta_y) / float(height())) * vrange_height / _selected_series->_vertical_scale;
  _selected_series->_vertical_offset = _downSeriesOffset + delta;

  printf("series<%s> offset=%.6f (pixel_delta=%d, global_vrange=%.6f, scale=%.3f)\n",
         _selected_series->_name.c_str(), _selected_series->_vertical_offset,
         pixel_delta_y, vrange_height, _selected_series->_vertical_scale);
}
/////////////////////////////////////////////////////////////////////////
void GraphView::_adjustSeriesScale(int wheel_delta) {
  _freezeSeriesAutoRange(_selected_series);

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
  const float zoom_rate = 1.01f;

  if (_v_key_down) {
    // V key down: vertical zoom only
    if (!_lockYZOOM) {
      if (wheel_delta > 0) {
        _grid._zoomY *= zoom_rate;
      } else {
        _grid._zoomY /= zoom_rate;
      }
      _grid._zoomY = clamp(_grid._zoomY, 0.02f, 10.0f);
    }
  } else {
    // V key up: horizontal zoom only
    if (wheel_delta > 0) {
      _grid._zoomX *= zoom_rate;
    } else {
      _grid._zoomX /= zoom_rate;
    }
    _grid._zoomX = clamp(_grid._zoomX, 0.02f, 10.0f);
  }
}
///////////////////////////////////////////////////////////////////////////////
void GraphPanel::setRect(int iX, int iY, int iW, int iH, bool snap) {
  _uipanel->SetRect(iX, iY, iW, iH);
  if (snap)
    _uipanel->snap();
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
