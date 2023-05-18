#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Simple (Colored) Box Widget
//  mostly used for testing, but if you need a colored box...
////////////////////////////////////////////////////////////////////

struct ChoiceList final : public Widget {
public:
  ChoiceList(
      const std::string& name, //
      fvec4 color,
      int x            = 0,
      int y            = 0,
      int w            = 0,
      int h            = 0,
      fvec2 dimensions = fvec2());
  ~ChoiceList();
  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;
  void _doOnParentChanged(Group* parent) final;
  void _doOnPreDestroy() final;

    void setValue(const std::string& val);

    void incScroll(int amt);

    fvec4 _bg_color;
    fvec4 _hl_color;
    fvec4 _fg_color;
    fvec2 _dimensions;
    double _hoverTime = 0.0;
    int selection_index() const;
    std::string _value;
    std::vector<std::string> _choices;
    int _mouse_hover_y = 0;
    int _scroll_y      = 0;

    static fvec2 computeDimensions(const std::vector<std::string>& choices);

  private:
    void DoDraw(ui::drawevent_constptr_t drwev) override;
  };

} // namespace ork::ui {
