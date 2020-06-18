#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Label Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct Label final : public Widget {
public:
  Label(
      const std::string& name, //
      fvec4 color,
      std::string label);
  fvec4 _color;
  fvec4 _textcolor;
  std::string _label;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

} // namespace ork::ui
