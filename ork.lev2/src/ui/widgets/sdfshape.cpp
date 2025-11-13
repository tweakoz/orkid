////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/sdfshape.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/style.h>
#include <ork/util/crc.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

SdfShape::SdfShape(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}

///////////////////////////////////////////////////////////////////////////////

void SdfShape::DoDraw(ui::drawevent_constptr_t drwev) {
  // Require themed rendering
  if (_theme_tag == 0 || !_uicontext || !_uicontext->_theme_engine)
    return;

  auto style = _uicontext->_theme_engine->_styledb->getStyle(_theme_tag);
  if (!style)
    return;

  auto theme = _uicontext->_theme_engine;
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int w = width();
  int h = height();

  // Dispatch based on shape type (CrcEnum)
  switch (_shape_type) {
    case "box"_crcu:
      theme->drawBox(this, drwev, style.get());
      break;

    case "tab"_crcu:
      theme->drawTab(ix1, iy1, w, h, drwev, style.get());
      break;

    case "box_per_corner"_crcu:
      theme->drawBoxPerCorner(ix1, iy1, w, h, drwev, style.get(), _corner_radii);
      break;

    case "circle"_crcu: {
      float radius = _shape_param;
      if (radius <= 0.0f) {
        // Auto-calculate: largest circle that fits
        radius = std::min(w, h) / 2.0f;
      }
      theme->drawCircle(ix1, iy1, w, h, drwev, style.get(), radius);
      break;
    }

    case "capsule"_crcu:
      theme->drawCapsule(ix1, iy1, w, h, drwev, style.get(), _horizontal);
      break;

    case "ring"_crcu: {
      float inner_radius = _shape_param;
      if (inner_radius <= 0.0f) {
        // Default: inner radius = 60% of outer
        inner_radius = std::min(w, h) * 0.3f;
      }
      theme->drawRing(ix1, iy1, w, h, drwev, style.get(), inner_radius);
      break;
    }

    default:
      // Unknown shape type - fall back to box
      theme->drawBox(this, drwev, style.get());
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
