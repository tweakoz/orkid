#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/particle/modular_particles2.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ParticlesDrawableData final : public DrawableData {

  DeclareConcreteX(ParticlesDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ParticlesDrawableData();
  ~ParticlesDrawableData();

  dataflow::graphdata_ptr_t _graphdata;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
