////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// HorizontalSplit: Splits space horizontally between two children
//  - Left/Right split based on _split_ratio (0.0 to 1.0)
//  - Default 0.5 (50/50 split)
////////////////////////////////////////////////////////////////////

struct HorizontalSplit : public Group {
  HorizontalSplit(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~HorizontalSplit();

  // Child management - 0th child = left, 1st child = right
  template <typename T, typename... Args>
  std::shared_ptr<T> makeChild(Args&&... args) {
    auto child = std::make_shared<T>(std::forward<Args>(args)...);
    addChild(child);
    return child;
  }

  float _split_ratio = 0.5f;  // 0.0 = all left, 1.0 = all right

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using hsplit_ptr_t = std::shared_ptr<HorizontalSplit>;

////////////////////////////////////////////////////////////////////
// VerticalSplit: Splits space vertically between two children
//  - Top/Bottom split based on _split_ratio (0.0 to 1.0)
//  - Default 0.5 (50/50 split)
////////////////////////////////////////////////////////////////////

struct VerticalSplit : public Group {
  VerticalSplit(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~VerticalSplit();

  // Child management - 0th child = top, 1st child = bottom
  template <typename T, typename... Args>
  std::shared_ptr<T> makeChild(Args&&... args) {
    auto child = std::make_shared<T>(std::forward<Args>(args)...);
    addChild(child);
    return child;
  }

  float _split_ratio = 0.5f;  // 0.0 = all top, 1.0 = all bottom

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using vsplit_ptr_t = std::shared_ptr<VerticalSplit>;

} // namespace ork::ui
