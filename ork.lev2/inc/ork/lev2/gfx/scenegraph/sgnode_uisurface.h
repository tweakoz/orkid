////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/ui/layoutsurface.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/material_freestyle.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct UISurfacePrimitiveData;
using uisurfaceprimitivedata_ptr_t = std::shared_ptr<UISurfacePrimitiveData>;

///////////////////////////////////////////////////////////////////////////////
// UISurfaceRenderImpl - handles rendering and hit testing for 3D UI surfaces
///////////////////////////////////////////////////////////////////////////////

struct UISurfaceRenderImpl {

  UISurfaceRenderImpl(const UISurfacePrimitiveData* data);
  ~UISurfaceRenderImpl();

  //////////////////////////////////////////////////////////////
  // GPU Initialization
  //////////////////////////////////////////////////////////////

  void gpuInit(Context* ctx);

  //////////////////////////////////////////////////////////////
  // Billboard Computation
  //////////////////////////////////////////////////////////////

  static void computeBillboardAxes(
      const CameraMatrices& camMtx,
      const fvec3& center,
      fvec3& right_out,
      fvec3& up_out,
      fvec3& normal_out);

  void computeQuadCorners(
      const CameraMatrices& camMtx,
      fvec3& corner00_out,
      fvec3& corner10_out,
      fvec3& corner11_out,
      fvec3& corner01_out) const;

  fmtx4 computeWorldToSurface(const CameraMatrices& camMtx) const;

  //////////////////////////////////////////////////////////////
  // Hit Testing
  //////////////////////////////////////////////////////////////

  bool rayIntersect(
      const fray3& worldRay,
      const CameraMatrices& camMtx,
      fvec2& uv_out,
      fvec3& worldHitPos_out) const;

  //////////////////////////////////////////////////////////////
  // Event Routing
  //////////////////////////////////////////////////////////////

  // Route a UI event to the embedded surface
  // Returns handler result indicating if event was consumed
  ui::HandlerResult routeUiEvent(
      int viewportWidth,
      int viewportHeight,
      const CameraMatrices& camMtx,
      ui::event_constptr_t ev);

  //////////////////////////////////////////////////////////////
  // Rendering
  //////////////////////////////////////////////////////////////

  void render(const RenderContextInstData& RCID);
  static void renderCallback(RenderContextInstData& RCID);

  //////////////////////////////////////////////////////////////
  // Data
  //////////////////////////////////////////////////////////////

  const UISurfacePrimitiveData* _data = nullptr;
  bool _initted = false;
  std::shared_ptr<FreestyleMaterial> _material;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _param_mvp = nullptr;
  const FxShaderParam* _param_colormap = nullptr;
  const FxShaderParam* _param_texdim = nullptr;
  const FxShaderParam* _param_maxsamples = nullptr;
  ui::context_ptr_t _uiContext;

  // Mouse tracking for enter/leave events
  bool _mouseInside = false;
};

using uisurface_renderimpl_ptr_t = std::shared_ptr<UISurfaceRenderImpl>;

///////////////////////////////////////////////////////////////////////////////
// UISurfacePrimitiveData - configuration for 3D embedded UI surfaces
///////////////////////////////////////////////////////////////////////////////

struct UISurfacePrimitiveData final : public DrawableData {

  DeclareConcreteX(UISurfacePrimitiveData, DrawableData);

public:
  UISurfacePrimitiveData();
  ~UISurfacePrimitiveData();

  drawable_ptr_t createDrawable() const final;

  //////////////////////////////////////////////////////////////
  // Configuration
  //////////////////////////////////////////////////////////////

  ui::layoutsurface_ptr_t _layoutSurface;  // The UI surface to render
  fvec3 _center;                            // Position in 3D world
  float _size = 1.0f;                       // Height of quad in world units
                                            // Width = _size * aspectRatio

  //////////////////////////////////////////////////////////////
  // Rendering Options
  //////////////////////////////////////////////////////////////

  BlendingMacro _blendMode = BlendingMacro::ALPHA;
  bool _doubleSided = false;

  //////////////////////////////////////////////////////////////
  // Anti-aliasing for minification (no mipmaps)
  // Adaptive supersampling up to NxN grid based on texel footprint
  // Default 4.0 = up to 4x4 (16 samples) for 16:1 minification
  // Set to 8.0 for up to 8x8 (64 samples) for 64:1 minification
  //////////////////////////////////////////////////////////////

  float _maxSamplesPerAxis = 4.0f;
};

///////////////////////////////////////////////////////////////////////////////
// Helper to get render impl from drawable
///////////////////////////////////////////////////////////////////////////////

uisurface_renderimpl_ptr_t getUISurfaceRenderImpl(drawable_ptr_t drawable);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
