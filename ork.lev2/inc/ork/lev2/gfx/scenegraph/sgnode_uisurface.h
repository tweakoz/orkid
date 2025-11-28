////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/ui/layoutsurface.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
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
};

using uisurfaceprimitivedata_ptr_t = std::shared_ptr<UISurfacePrimitiveData>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
