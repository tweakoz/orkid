////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>
#include <map>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// VerticalPack: A container that packs children vertically
// - Layouts children vertically with configurable item height and margin
// - Inherits width from parent, sets height per item
////////////////////////////////////////////////////////////////////

struct VerticalPack : public Group {
  VerticalPack(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~VerticalPack();

  // Child management
  template <typename T, typename... Args>
  std::shared_ptr<T> makeChild(Args&&... args) {
    auto child = std::make_shared<T>(std::forward<Args>(args)...);
    addChild(child);
    return child;
  }

  int _margin = 0;
  int _item_height = 32;
  bool _fill = false;
  fvec4 _bgcolor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);

protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

};

using vpack_ptr_t = std::shared_ptr<VerticalPack>;

////////////////////////////////////////////////////////////////////
// HorizontalPack: A container that packs children horizontally
// - Layouts children horizontally with configurable item width and margin
// - Inherits height from parent, sets width per item
////////////////////////////////////////////////////////////////////

struct HorizontalPack : public Group {
  HorizontalPack(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~HorizontalPack();

  // Child management
  template <typename T, typename... Args>
  std::shared_ptr<T> makeChild(Args&&... args) {
    auto child = std::make_shared<T>(std::forward<Args>(args)...);
    addChild(child);
    return child;
  }

  int _margin = 0;
  int _item_width = 32;
  bool _fill = false;
  bool _uniform = false;  // Distribute children uniformly across width
  fvec4 _bgcolor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);

protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

};

using hpack_ptr_t = std::shared_ptr<HorizontalPack>;

} // namespace ork::ui