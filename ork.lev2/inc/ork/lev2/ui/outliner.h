////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/kernel/varmap.inl>
#include <functional>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// Outliner: A tree view widget for displaying hierarchical data
// - Data stored in VarMap with nested VarMaps for tree structure
// - Supports selection with callback
// - Collapsible tree nodes
////////////////////////////////////////////////////////////////////

struct Outliner : public Widget {
  Outliner(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~Outliner();

  // Data management
  void setData(varmap::varmap_ptr_t data);
  varmap::varmap_ptr_t getData() const { return _data; }

  // Selection
  void setSelectedKey(const std::string& key);
  std::string getSelectedKey() const { return _selected_key; }

  // Expand/collapse
  void setExpanded(const std::string& key, bool expanded);
  bool isExpanded(const std::string& key) const;
  void expandAll();
  void collapseAll();

  // Callbacks
  std::function<void(const std::string& key)> _onSelect;

  // Appearance
  int _item_height = 20;
  int _indent_width = 16;
  fvec4 _bgcolor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  fvec4 _text_color = fvec4(1.0f, 1.0f, 1.0f, 1.0f);
  fvec4 _selected_color = fvec4(0.3f, 0.5f, 0.8f, 1.0f);
  fvec4 _hover_color = fvec4(0.2f, 0.3f, 0.4f, 1.0f);
  bool _draw_background = true;
  lev2::font_ptr_t _font;

protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  // Internal representation of visible items
  struct VisibleItem {
    std::string key;
    std::string display_name;
    int depth;
    bool has_children;
    bool is_expanded;
  };

  void _rebuildVisibleItems();
  void _addItemsRecursive(varmap::varmap_ptr_t node, const std::string& path_prefix, int depth);
  void _clampScrollOffset();
  int _getItemIndexAt(int local_y) const;
  std::string _getItemKeyAt(int local_y) const;

  varmap::varmap_ptr_t _data;
  std::string _selected_key;
  std::string _hovered_key;
  std::vector<VisibleItem> _visible_items;
  std::unordered_set<std::string> _expanded_keys;
  bool _needs_rebuild = true;
  int _scroll_offset = 0;
};

using outliner_ptr_t = std::shared_ptr<Outliner>;

} // namespace ork::ui
