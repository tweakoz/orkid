#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GridDrawableData final : public DrawableData {

  DeclareConcreteX(GridDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  GridDrawableData();
  ~GridDrawableData();

  image_ptr_t _colorImage;
  image_ptr_t _normalImage;
  image_ptr_t _mtlrufImage;

  std::string _colortexpath;
  std::string _normaltexpath;
  std::string _mtlruftexpath;
  
  fvec3 _modcolor = fvec3(1, 1, 1);
  float _intensityA = 1.0;
  float _intensityB = 1.0;
  float _intensityC = 1.0;
  float _intensityD = 1.0;
  float _lineWidth = 0.05;
  float _extent = 100.0f;
  float _majorTileDim = 1.0f;
  float _minorTileDim = 0.1f;
  std::string _shader_suffix = "";
};

struct GridDrawableImpl {

  GridDrawableImpl(const GridDrawableData* grid);
  ~GridDrawableImpl();
  void gpuInit(lev2::Context* ctx);
  void _render(const RenderContextInstData& RCID);
  static void renderGrid(RenderContextInstData& RCID);

  const GridDrawableData* _griddata = nullptr;
  pbrmaterial_ptr_t _pbrmaterial = nullptr;

  image_ptr_t _color_image;
  image_ptr_t _normal_image;
  image_ptr_t _mtlruf_image;
  fxpipelinecache_constptr_t _fxcache;
  fxparam_constptr_t _paramAuxA;
  fxparam_constptr_t _paramAuxB;
  bool _initted = false;

};

using griddrawableimpl_ptr_t = std::shared_ptr<GridDrawableImpl>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
