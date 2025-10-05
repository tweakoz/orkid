#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// IntSlider Widget
//  Integer slider with drag-to-edit and text entry
////////////////////////////////////////////////////////////////////

struct IntSlider final : public Widget {
public:
  IntSlider(const std::string& name, //
      fvec4 color,
      int min,
      int max,
      int value,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setValue(int val);
  int value() const { return _value; }

  void setRange(int min, int max);

  // Callback for value changes
  void_lambda_t _onValueChanged;

  int _value;
  int _min;
  int _max;
  bool _update_on_drag = false;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _fill_color;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout() final;

  float _valToUnit(int val) const;
  int _unitToVal(float unit) const;
  void _refresh();

  float _indicator_pos = 0.0f;
  float _text_pos = 0.0f;
  std::string _value_str;
  bool _dragging = false;
};

using intslider_ptr_t = std::shared_ptr<IntSlider>;

////////////////////////////////////////////////////////////////////
// FloatSlider Widget
//  Float slider with drag-to-edit, text entry, and log mode
////////////////////////////////////////////////////////////////////

struct FloatSlider final : public Widget {
public:
  FloatSlider(const std::string& name, //
      fvec4 color,
      float min,
      float max,
      float value,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setValue(float val);
  float value() const { return _value; }

  void setRange(float min, float max);
  void setLogMode(bool log);
  bool logMode() const { return _log_mode; }

  // Callback for value changes
  void_lambda_t _onValueChanged;

  float _value;
  float _min;
  float _max;
  bool _log_mode = false;
  bool _update_on_drag = false;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _fill_color;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout() final;

  float _valToUnit(float val) const;
  float _unitToVal(float unit) const;
  void _refresh();

  float _indicator_pos = 0.0f;
  float _text_pos = 0.0f;
  std::string _value_str;
  bool _dragging = false;
};

using floatslider_ptr_t = std::shared_ptr<FloatSlider>;

} // namespace ork::ui
