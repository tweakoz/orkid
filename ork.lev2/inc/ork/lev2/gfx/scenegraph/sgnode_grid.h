#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GridDrawableData;
struct GridDrawableInst;

using griddrawableinstptr_t = std::shared_ptr<GridDrawableInst> ;
using griddrawabledataptr_t = std::shared_ptr<GridDrawableData> ;

///////////////////////////////////////////////////////////////////////////////

struct GridDrawableData final : public ork::Object {

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
