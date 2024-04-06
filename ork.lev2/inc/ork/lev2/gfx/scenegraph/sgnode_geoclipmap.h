#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ClipMapDrawableData final : public DrawableData {

  DeclareConcreteX(ClipMapDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ClipMapDrawableData();
  ~ClipMapDrawableData();

  pbrmaterial_ptr_t _material;
  int _levels = 4;
  int _ringSize = 512;
  float _baseQuadSize = 0.5f;
  bool _circle = false;
};

using clipmapdrawabledata_ptr_t = std::shared_ptr<ClipMapDrawableData>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
