#pragma once
#include "heightmap.h"
#include <pkg/ent/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<HeightMap> hfptr_t;
struct HeightFieldDrawable;
typedef std::shared_ptr<HeightFieldDrawable> hfdrawableptr_t;

class HeightFieldDrawableData : public ork::Object {

	RttiDeclareConcrete( HeightFieldDrawableData, ork::Object );

 public:
   hfdrawableptr_t createDrawable() const ;
  HeightFieldDrawableData();
  ~HeightFieldDrawableData() final;

  float _testxxx = 0.0f;
  fvec3 _fogcolor;
  fvec3 _grass, _snow, _rock1, _rock2;
  float _gblend_yscale = 1.0f;
  float _gblend_ybias = 0.0f;
  float _gblend_steplo = 0.5f;
  float _gblend_stephi = 0.6f;
  ork::lev2::textureassetptr_t _sphericalenvmap = nullptr;

private:

  friend HeightFieldDrawable;

	void _readEnvMap(ork::rtti::ICastable *&model) const;
	void _writeEnvMap(ork::rtti::ICastable *const &model);
  void _writeHmapPath(file::Path const& lmap);
  void _readHmapPath(file::Path& lmap) const;

  file::Path _hfpath;
  fvec3 _visualOffset;

};

struct HeightFieldDrawable {

  HeightFieldDrawable(const HeightFieldDrawableData& data);
  ~HeightFieldDrawable();
  CallbackDrawable* create();

  const HeightFieldDrawableData& _data;
  file::Path _hfpath;
  fvec3 _visualOffset;
  float _worldHeight = 0.0f;
  float _worldSizeXZ = 0.0f;
  CallbackDrawable* _rawdrawable = nullptr;
  ork::svar16_t _impl;
};


///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////
