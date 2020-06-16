#pragma once

#include <ork/lev2/ui/group.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////

struct Panel : public Group {
  Panel(const std::string& name, int x, int y, int w, int h);
  ~Panel();

  void setChild(widget_ptr_t w);

  void snap();
  void unsnap();

  inline void setTitle(std::string t) {
    _title = t;
  }

private:
  HandlerResult DoOnUiEvent(event_constptr_t Ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout(void) override;
  void DoOnEnter() override;
  void DoOnExit() override;
  Widget* doRouteUiEvent(event_constptr_t Ev) override;

  widget_ptr_t _child;
  int mPanelUiState;
  bool mDockedAtTop;
  int mCloseX, mCloseY;
  std::string _title;

  int _downx  = 0;
  int _downy  = 0;
  int _prevpx = 0;
  int _prevpy = 0;
  int _prevpw = 0;
  int _prevph = 0;
};

}} // namespace ork::ui
