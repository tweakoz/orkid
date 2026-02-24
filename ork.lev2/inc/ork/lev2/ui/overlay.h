////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/ui.h>
#include <functional>

namespace ork::ui {

struct OverlayEntry {
  widget_ptr_t _widget;
  bool _dismiss_on_click_outside = true;
  bool _modal = false;
  std::function<void()> _onDismissed;
};

} // namespace ork::ui
