#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple Dial Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

using updatefn_t = std::function<void(float)>;

struct Dial final : public Widget {
  Dial(const std::string& name, fvec4 color);

  fvec4 _bgcolor;
  fvec4 _fgcolor;
  fvec4 _indcolor;
  fvec4 _textcolor;
  std::string _label;
  std::string _font = "i14";

  void setParams(int numsteps, float curval, float minval, float maxval, float power);
  void selValFromStep(int step);
  int _numsteps   = 1001;
  int _cursteps   = 0;
  float _minval   = 0.0f;
  float _ctrval   = 0.5f;
  float _maxval   = 1.0f;
  float _range    = 1.0f;
  float _curvalue = 0.0f;
  float _power    = 1.0f;
  bool _isbipolar = false;
  updatefn_t _onupdate;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
};

} // namespace ork::ui
