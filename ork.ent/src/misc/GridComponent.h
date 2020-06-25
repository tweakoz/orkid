#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
class GridControllerData : public ent::ComponentData {
  DeclareConcreteX(GridControllerData, ent::ComponentData);

  float _spinrate;

  void GetTextureAccessor(ork::rtti::ICastable*& model) const;
  void SetTextureAccessor(ork::rtti::ICastable* const& model);
  lev2::TextureAsset* _colorTexture;
  float _extent;
  float _tiledim;

public:
  lev2::Texture* GetTexture() const;
  float extent() const {
    return _extent;
  }
  float tileDim() const {
    return _tiledim;
  }
  ent::ComponentInst* createComponent(ent::Entity* pent) const final;

  GridControllerData();
  float GetSpinRate() const {
    return _spinrate;
  }
};

///////////////////////////////////////////////////////////////////////////////

class GridControllerInst : public ent::ComponentInst {
  RttiDeclareAbstract(GridControllerInst, ent::ComponentInst);

  const GridControllerData& mCD;
  float mPhase;

  void DoUpdate(ent::Simulation* sinst) final;

public:
  const GridControllerData& GetCD() const {
    return mCD;
  }

  GridControllerInst(const GridControllerData& cd, ork::ent::Entity* pent);
  float GetPhase() const {
    return mPhase;
  }
};

///////////////////////////////////////////////////////////////////////////////

class GridArchetype : public Archetype {
  RttiDeclareConcrete(GridArchetype, Archetype);

  void DoLinkEntity(Simulation* psi, Entity* pent) const final;
  void DoStartEntity(Simulation* psi, const fmtx4& world, Entity* pent) const final {
  }
  void DoCompose(ork::ent::ArchComposer& composer) final;

public:
  GridArchetype();
};
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
