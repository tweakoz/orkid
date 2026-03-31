////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <functional>
#include <string>
#include <vector>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ChoicelistWidget
// Shows current value text with a dropdown indicator.
// Opens DropdownMenu on click with choices from _getChoices callback.
////////////////////////////////////////////////////////////////////

struct ChoicelistWidget : public Widget {
  ChoicelistWidget(const std::string& name, const std::string& current_value = "");

  std::string _current_value;
  std::function<std::vector<std::string>()> _getChoices;
  std::function<void(const std::string&)> _onChoiceSelected;

  fvec4 _bg_color = fvec4(0.2f, 0.2f, 0.25f, 1.0f);
  fvec4 _fg_color = fvec4(0.8f, 0.8f, 0.8f, 1.0f);

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using choicelist_widget_ptr_t = std::shared_ptr<ChoicelistWidget>;

} // namespace ork::ui
