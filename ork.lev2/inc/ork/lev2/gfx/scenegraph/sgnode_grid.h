#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GridDrawableInst;
typedef std::shared_ptr<GridDrawableInst> griddrawableinstptr_t;

///////////////////////////////////////////////////////////////////////////////

class GridDrawableData final : public ork::Object {

  DeclareConcreteX(GridDrawableData, ork::Object);

public:
  griddrawableinstptr_t createInstance() const;
  GridDrawableData();
  ~GridDrawableData();

  textureassetptr_t _colorTexture;
  float _extent = 100.0f;
  float _majorTileDim = 1.0f;
  float _minorTileDim = 0.1f;
};

///////////////////////////////////////////////////////////////////////////////

struct GridDrawableInst {

  GridDrawableInst(const GridDrawableData& data);
  ~GridDrawableInst();
  callback_drawable_ptr_t createCallbackDrawable();

  const GridDrawableData& _data;
  callback_drawable_ptr_t _rawdrawable = nullptr;
  ork::svar16_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
