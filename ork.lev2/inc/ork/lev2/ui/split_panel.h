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

  widget_ptr_t _child1;
  widget_ptr_t _child2;
  int mPanelUiState;
  bool mDockedAtTop;
  float mSplitVal;
  int mCloseX, mCloseY;
  bool mEnableCloseButton;
  bool _moveEnabled = true;
  int _downx        = 0;
  int _downy        = 0;
  int _prevpx       = 0;
  int _prevpy       = 0;
  int _prevpw       = 0;
  int _prevph       = 0;

private:
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout(void) override;
  void DoOnEnter() override;
  void DoOnExit() override;
  Widget* doRouteUiEvent(event_constptr_t Ev) override;
};

}} // namespace ork::ui
