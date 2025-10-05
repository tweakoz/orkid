#pragma once

#include <ork/lev2/ui/widget.h>
#include <vector>
#include <string>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ComboBox Widget
//  Selection widget that cycles through string options
//  - Click +/− buttons to change selection
//  - Mouse wheel to scroll (when active)
//  - Arrow keys to navigate
////////////////////////////////////////////////////////////////////

struct ComboBox final : public Widget {
public:
  ComboBox(const std::string& name, //
      fvec4 color,
      int x = 0,
      int y = 0,
      int w = 0,
      int h = 0);

  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void addItem(const std::string& item);
  void setItems(const std::vector<std::string>& items);
  void setSelectedIndex(int idx);
  int selectedIndex() const { return _selected_index; }
  std::string selectedItem() const;

  // Callback for selection changes
  void_lambda_t _onSelectionChanged;

  std::vector<std::string> _items;
  int _selected_index = 0;
  bool _active = false;

  fvec4 _bg_color;
  fvec4 _fg_color;
  fvec4 _button_color;
  int _scroll_pos = 0;
  static const int BUTTON_WIDTH = 20;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;

  void _incrementSelection();
  void _decrementSelection();
};

using combobox_ptr_t = std::shared_ptr<ComboBox>;

} // namespace ork::ui
