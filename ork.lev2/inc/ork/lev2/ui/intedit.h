#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// IntEdit - Integer numeric edit widget
//  Displays as "label: value" inside a single box
//  Only accepts numeric input, supports min/max clamping
////////////////////////////////////////////////////////////////////

struct IntEdit final : public Widget {
public:
  IntEdit(const std::string& name,
      const std::string& label,
      int value = 0,
      int minval = INT_MIN,
      int maxval = INT_MAX,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void setValue(int val);
  int getValue() const { return _value; }

  void setMin(int m) { _min = m; }
  void setMax(int m) { _max = m; }
  int getMin() const { return _min; }
  int getMax() const { return _max; }

  void setLabel(const std::string& l) { _label = l; }
  const std::string& getLabel() const { return _label; }

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _input_color;
  bool _input_color_set = false;

  std::string _label;
  int _value;
  int _original_value;
  int _min;
  int _max;

  std::string _edit_buffer;
  bool _editing = false;
  bool _highlight = false;

  // Callbacks
  std::function<void(int)> _onValueChanged;
  std::function<void(int)> _onValueCommitted;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  std::string formatValue() const;
  bool parseValue(const std::string& str, int& out) const;
  int clampValue(int v) const;
};

using intedit_ptr_t = std::shared_ptr<IntEdit>;

} // namespace ork::ui
