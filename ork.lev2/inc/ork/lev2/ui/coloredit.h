#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Box Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct ColorEdit final : public Widget {
public:
  ColorEdit(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

    HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  fvec4 _originalColor;
  fvec4 _currentColor;
  ork::lev2::freestyle_mtl_ptr_t _material;
  float _intensity = 1.0f;
  float _saturation = 1.0f;
  float _hue = 0.0f;
  fvec2 _push_pos;
  float _push_radius = 0.0;
  float _push_angle = 0.0f;
  const ork::lev2::FxShaderTechnique* _tekvtxcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tekmodcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tekcolorwheel = nullptr;
  const ork::lev2::FxShaderParam* _parmvp          = nullptr;
  const ork::lev2::FxShaderParam* _parmodcolor     = nullptr;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

} //namespace ork::ui {
