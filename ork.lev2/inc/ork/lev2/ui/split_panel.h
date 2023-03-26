////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////

struct SplitPanel : public Group {
  SplitPanel(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~SplitPanel();

  void setChild1(widget_ptr_t w);
  void setChild2(widget_ptr_t w);

  void enableCloseButton() {
    mEnableCloseButton = true;
  }

  void snap();

  widget_ptr_t _child1 = nullptr;
  widget_ptr_t _child2 = nullptr;

  int mPanelUiState = 0;
  int _downx        = 0;
  int _downy        = 0;
  int _prevpx       = 0;
  int _prevpy       = 0;
  int _prevpw       = 0;
  int _prevph       = 0;
  int mCloseX = 0;
  int mCloseY = 0;
  float mSplitVal = 0.5f;

  bool mDockedAtTop = false;
  bool mEnableCloseButton = false;
  bool _moveEnabled = true;

private:
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout(void) override;
  Widget* doRouteUiEvent(event_constptr_t Ev) override;
};

}} // namespace ork::ui
