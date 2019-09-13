#pragma once
#include "heightmap.h"
#include <pkg/ent/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<HeightMap> hfptr_t;

struct HeightFieldDrawable {

  CallbackDrawable* create();
  ~HeightFieldDrawable();

  file::Path _hfpath;
  fvec3 _visualOffset;
  float _worldHeight = 0.0f;
  float _worldSizeXZ = 0.0f;
  ork::lev2::textureassetptr_t _sphericalenvmap;
  CallbackDrawable* _rawdrawable = nullptr;
  ork::svar16_t _impl;
};

typedef std::shared_ptr<HeightFieldDrawable> hfdrawableptr_t;

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////
