////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/layoutgroup.inl>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// LayoutSurface: Renders a LayoutGroup hierarchy to a texture
// - Texture can be larger than widget bounds (virtual space)
// - Supports scrolling through the virtual space
// - Provides clipping naturally through texture viewport
////////////////////////////////////////////////////////////////////

struct LayoutSurface : public Surface {
  LayoutSurface(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0, int margin = 0);
  ~LayoutSurface();

  // Set the virtual size (size of the texture/rtgroup)
  void setVirtualSize(int w, int h);
  int getVirtualWidth() const { return _virtualWidth; }
  int getVirtualHeight() const { return _virtualHeight; }

  // Scroll control
  void setScrollPosition(int x, int y);
  int getScrollX() const { return _scrollX; }
  int getScrollY() const { return _scrollY; }

  // Access to the internal layout group
  std::shared_ptr<LayoutGroup> layoutGroup() { return _layoutGroup; }
  anchor::layout_ptr_t layout() { return _layoutGroup->_layout; }

  // Proxy to create children in the internal LayoutGroup
  template <typename T, typename... Args>
  LayoutItem<T> makeChild(Args&&... args) {
    return _layoutGroup->makeChild<T>(std::forward<Args>(args)...);
  }

  // Override from Surface
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) final;
  void DoDraw(drawevent_constptr_t drwev) final;
  void _doOnResized() final;
  Widget* doRouteUiEvent(event_constptr_t ev) final;
  HandlerResult DoOnUiEvent(event_constptr_t Ev) final;

  void _updateRenderTarget();

  //////////////////////////////////////////////////////////////
  // 3D Embedding Support
  //////////////////////////////////////////////////////////////

  // Ensure texture is current before 3D rendering
  // Called by external renderers (e.g., UISurfaceRenderImpl)
  void updateTextureIfNeeded(lev2::Context* ctx);

  // Convert surface-local coordinates to pixel coordinates
  // Input: local coords in [-0.5, 0.5] range (center = origin)
  // Output: pixel coords in [0, width] x [0, height]
  fvec2 localToPixel(const fvec2& local) const;

  // Convert pixel coordinates to surface-local coordinates
  // Inverse of localToPixel()
  fvec2 pixelToLocal(const fvec2& pixel) const;

  // Handle input that has been transformed to surface pixel space
  // Creates a 2D Event and routes through normal UI system
  HandlerResult handleTransformedInput(
      const fvec2& surfacePixelCoords,
      EventCode eventCode,
      uint32_t buttonState,
      uint32_t modifierKeys);

  std::shared_ptr<LayoutGroup> _layoutGroup;
  context_ptr_t _ownedContext;  // LayoutSurface owns its own UIContext

  // Virtual dimensions (texture size)
  int _virtualWidth = 0;
  int _virtualHeight = 0;

  // Current scroll position
  int _scrollX = 0;
  int _scrollY = 0;

  // Cached UV coordinates for viewport
  float _u0 = 0.0f, _v0 = 0.0f;
  float _u1 = 1.0f, _v1 = 1.0f;
};

using layoutsurface_ptr_t = std::shared_ptr<LayoutSurface>;

} // namespace ork::ui