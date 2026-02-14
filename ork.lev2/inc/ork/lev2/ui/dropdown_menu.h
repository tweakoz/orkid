////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/kernel/slashnode.h>

namespace ork::ui {

struct DropdownMenu : public Widget {
  using selection_cb_t = std::function<void(std::string value)>;
  using dismissed_cb_t = std::function<void()>;

  DropdownMenu(const std::string& name, slashnode_constptr_t node);

  // Configuration
  selection_cb_t _onSelected;
  dismissed_cb_t _onDismissed;

  // State
  slashnode_constptr_t _node;
  int _hover_index = -1;
  int _scroll_offset = 0;
  double _hover_start_time = 0.0;
  int _submenu_open_index = -1;

  // Items (built from _node children)
  struct MenuItem {
    std::string _label;
    std::string _value;
    slashnode_constptr_t _node;
    bool _is_leaf;
  };
  std::vector<MenuItem> _items;

  // Animated highlight color
  fvec4 _hl_color;

  // Constants
  static constexpr int ITEM_HEIGHT = 28;
  static constexpr int MAX_VISIBLE = 12;
  static constexpr int PADDING_X = 12;
  static constexpr int ARROW_WIDTH = 20;
  static constexpr double SUBMENU_DELAY = 0.2;

  // Overrides
  void DoDraw(drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
  void _doOnPreDestroy() override;

  // Internal
  void _openSubmenu(int index);
  void _closeSubmenu();
  void _selectItem(int index);
  fvec2 computeSize() const;

  // Static convenience: build a SlashTree from a list of slash-delimited paths
  static slashtree_ptr_t buildTreeFromPaths(const std::vector<std::string>& paths);
};

using dropdown_menu_ptr_t = std::shared_ptr<DropdownMenu>;

} // namespace ork::ui
