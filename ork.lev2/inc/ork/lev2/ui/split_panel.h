#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////

struct SplitPanel : public Group {
  SplitPanel(const std::string& name, int x, int y, int w, int h);
  ~SplitPanel();

  void setChild1(widget_ptr_t w);
  void setChild2(widget_ptr_t w);

  void enableCloseButton() {
    mEnableCloseButton = true;
  }

  void snap();

private:
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout(void) override;
  void DoOnEnter() override;
  void DoOnExit() override;
  HandlerResult DoRouteUiEvent(event_constptr_t Ev) override;

  widget_ptr_t _child1;
  widget_ptr_t _child2;
  int mPanelUiState;
  bool mDockedAtTop;
  float mSplitVal;
  int mCloseX, mCloseY;
  bool mEnableCloseButton;
};

}} // namespace ork::ui
