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

struct GridDrawableData final : public DrawableData {

  DeclareConcreteX(GridDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  GridDrawableData();
  ~GridDrawableData();

  std::string _colortexpath;
  float _extent = 100.0f;
  float _majorTileDim = 1.0f;
  float _minorTileDim = 0.1f;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
