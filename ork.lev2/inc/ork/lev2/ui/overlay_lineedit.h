////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// OverlayLineEdit: A text input widget designed for overlay use.
// Always in edit mode (no click-to-activate).
// ENTER commits, ESC cancels, auto-dismisses via popOverlay().
////////////////////////////////////////////////////////////////////

struct OverlayLineEdit : public Widget {
  OverlayLineEdit(const std::string& name, const std::string& initial_value);

  std::function<void(const std::string&)> _onCommit;  // Called on Enter
  std::function<void()> _onCancel;                     // Called on Escape

  std::string _value;
  std::string _original_value;

  fvec4 _bg_color = fvec4(0.15f, 0.15f, 0.2f, 1.0f);
  fvec4 _fg_color = fvec4(1.0f, 1.0f, 1.0f, 1.0f);
  fvec4 _input_bg_color = fvec4(0.08f, 0.08f, 0.12f, 1.0f);

  void DoDraw(drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using overlay_lineedit_ptr_t = std::shared_ptr<OverlayLineEdit>;

} // namespace ork::ui
