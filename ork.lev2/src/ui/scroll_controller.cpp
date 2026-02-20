#include <ork/pch.h>
#include <ork/lev2/ui/scroll_controller.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

int ScrollController::maxScroll() const {
  return std::max(0, _content_size - _viewport_size);
}

///////////////////////////////////////////////////////////////////////////////

void ScrollController::clamp() {
  _scroll_offset = std::clamp(_scroll_offset, 0, maxScroll());
}

///////////////////////////////////////////////////////////////////////////////

void ScrollController::applyMouseWheel(int delta, float current_time) {
  _scroll_offset -= delta * _scroll_speed;
  clamp();
  _last_scroll_time = current_time;
}

///////////////////////////////////////////////////////////////////////////////

bool ScrollController::needsIndicator() const {
  return _content_size > _viewport_size;
}

///////////////////////////////////////////////////////////////////////////////

void ScrollController::drawIndicator(
    drawevent_constptr_t drwev,
    ui::Context* uictx,
    int area_x, int area_y, int area_w, int area_h,
    bool horizontal) {

  if (!needsIndicator()) return;
  if (!uictx || !uictx->_theme_engine) return;

  // Calculate fade alpha based on time since last scroll
  float current_time = uictx->_uitimer.SecsSinceStart();
  float time_since_scroll = current_time - _last_scroll_time;

  if (time_since_scroll > _fade_delay + _fade_duration) {
    return;  // Fully faded out
  }

  float alpha = _indicator_color.w;
  if (time_since_scroll > _fade_delay) {
    float fade_progress = (time_since_scroll - _fade_delay) / _fade_duration;
    alpha *= (1.0f - fade_progress);
  }

  int track_length = horizontal ? area_w : area_h;
  float visible_ratio = float(track_length) / float(_content_size);
  int ms = maxScroll();
  float scroll_ratio = (ms > 0) ? float(_scroll_offset) / float(ms) : 0.0f;

  int indicator_len = std::max(_indicator_min_size, int(visible_ratio * track_length));
  int indicator_travel = track_length - indicator_len;
  int indicator_pos = int(scroll_ratio * indicator_travel);

  Style style;
  style._bg_color = fvec4(_indicator_color.x, _indicator_color.y, _indicator_color.z, alpha);
  style._border_color = fvec4(0.0f, 0.0f, 0.0f, 0.0f);
  style._corner_radius = _indicator_corner_radius;
  style._border_width = 0;
  style._blend_mode = lev2::BlendingMacro::ALPHA;

  fvec4 radii(style._corner_radius, style._corner_radius, style._corner_radius, style._corner_radius);

  if (horizontal) {
    int ind_x = area_x + indicator_pos;
    int ind_y = area_y + area_h - _indicator_width - _indicator_margin;
    uictx->_theme_engine->drawBoxPerCorner(ind_x, ind_y, indicator_len, _indicator_width, drwev, &style, radii);
  } else {
    int ind_x = area_x + area_w - _indicator_width - _indicator_margin;
    int ind_y = area_y + indicator_pos;
    uictx->_theme_engine->drawBoxPerCorner(ind_x, ind_y, _indicator_width, indicator_len, drwev, &style, radii);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
