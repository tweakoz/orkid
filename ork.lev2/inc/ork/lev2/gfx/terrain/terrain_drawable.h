#pragma once
#include "heightmap.h"
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<HeightMap> hfptr_t;
struct TerrainDrawableInst;
typedef std::shared_ptr<TerrainDrawableInst> hfdrawableinstptr_t;

class TerrainDrawableData : public ork::Object {

  DeclareConcreteX(TerrainDrawableData, ork::Object);

public:
  hfdrawableinstptr_t createInstance() const;
  TerrainDrawableData();
  ~TerrainDrawableData() final;

  float _testxxx = 0.0f;
  fvec3 _fogcolor;
  fvec3 _grass, _snow, _rock1, _rock2;
  float _gblend_yscale                          = 1.0f;
  float _gblend_ybias                           = 0.0f;
  float _gblend_steplo                          = 0.5f;
  float _gblend_stephi                          = 0.6f;
  ork::lev2::textureassetptr_t _sphericalenvmap = nullptr;

  void _readEnvMap(ork::rtti::ICastable*& model) const;
  void _writeEnvMap(ork::rtti::ICastable* const& model);
  void _writeHmapPath(file::Path const& lmap);
  void _readHmapPath(file::Path& lmap) const;

  file::Path _hfpath;
  fvec3 _visualOffset;
};

struct TerrainDrawableInst {

  TerrainDrawableInst(const TerrainDrawableData& data);
  ~TerrainDrawableInst();
  lev2::CallbackDrawable* createCallbackDrawable();

  const TerrainDrawableData& _data;
  file::Path hfpath() const;
  fvec3 _visualOffset;
  float _worldHeight                   = 0.0f;
  float _worldSizeXZ                   = 0.0f;
  lev2::CallbackDrawable* _rawdrawable = nullptr;
  ork::svar16_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
