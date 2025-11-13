////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// SdfShape: Renders SDF primitives via ThemeEngine
//
// Supported shape types (CrcEnum):
//   - "box"             : Standard rounded box (uses style.corner_radius)
//   - "tab"             : Tab shape (round top, sharp bottom)
//   - "box_per_corner"  : Box with per-corner radii (_corner_radii)
//   - "circle"          : Perfect circle (_shape_param = radius)
//   - "capsule"         : Pill/capsule shape (_horizontal = orientation)
//   - "ring"            : Donut/ring (_shape_param = inner_radius)
//
// Usage:
//   auto shape = std::make_shared<SdfShape>("my_shape");
//   shape->_shape_type = "circle"_crcu;
//   shape->_shape_param = 50.0f;  // radius
//   shape->_theme_tag = "my_style"_crcu;
///////////////////////////////////////////////////////////////////////////////

struct SdfShape : public Widget {
  SdfShape(const std::string& name, int x=0, int y=0, int w=0, int h=0);

  // Shape type (CrcEnum)
  uint64_t _shape_type = "box"_crcu;

  // Per-corner radii for "box_per_corner" (x=TL, y=TR, z=BR, w=BL)
  fvec4 _corner_radii = fvec4(8.0f, 8.0f, 8.0f, 8.0f);

  // Shape-specific parameter:
  //   "circle": radius (0.0 = use min(w,h)/2)
  //   "ring": inner_radius
  float _shape_param = 0.0f;

  // Capsule orientation (true=horizontal, false=vertical)
  bool _horizontal = true;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};

using sdfshape_ptr_t = std::shared_ptr<SdfShape>;

} // namespace ork::ui
