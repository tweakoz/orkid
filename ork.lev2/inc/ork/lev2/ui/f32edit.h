#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// F32Edit - Float numeric edit widget
//  Displays as "label: value" inside a single box
//  Only accepts numeric input, supports min/max clamping
////////////////////////////////////////////////////////////////////

struct F32Edit final : public Widget {
public:
  F32Edit(const std::string& name,
      const std::string& label,
      float value = 0.0f,
      float minval = -1e30f,
      float maxval = 1e30f,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setValue(float val);
  float getValue() const { return _value; }

  void setMin(float m) { _min = m; }
  void setMax(float m) { _max = m; }
  float getMin() const { return _min; }
  float getMax() const { return _max; }

  void setLabel(const std::string& l) { _label = l; }
  const std::string& getLabel() const { return _label; }

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _input_color;
  bool _input_color_set = false;

  std::string _label;
  float _value;
  float _original_value;
  float _min;
  float _max;
  int _precision = 3;  // decimal places to display

  std::string _edit_buffer;  // text buffer while editing
  bool _editing = false;
  bool _highlight = false;

  // Callbacks
  std::function<void(float)> _onValueChanged;
  std::function<void(float)> _onValueCommitted;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  std::string formatValue() const;
  bool parseValue(const std::string& str, float& out) const;
  float clampValue(float v) const;
  bool isValidChar(char c) const;
};

using f32edit_ptr_t = std::shared_ptr<F32Edit>;

} // namespace ork::ui
