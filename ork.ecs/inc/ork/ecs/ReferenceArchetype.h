////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#if 0
#include <ork/rtti/RTTI.h>
#include <ork/asset/Asset.h>
#include <ork/asset/AssetManager.h>

#include "archetype.h"

namespace ork { namespace ecs {

struct ArchetypeAsset final : public asset::Asset {
  DeclareConcreteX(ArchetypeAsset, asset::Asset);

public:
  ArchetypeAsset();
  ~ArchetypeAsset();

  archetype_ptr_t GetArchetype() const {
    return _archetype;
  }
  void SetArchetype(archetype_ptr_t archetype) {
    _archetype = archetype;
  }

protected:
  archetype_ptr_t _archetype;
};

using archetype_asset_ptr_t = std::shared_ptr<ArchetypeAsset>;

///////////////////////////////////////////////////////////

struct ReferenceArchetype final : public Archetype {
  DeclareConcreteX(ReferenceArchetype, Archetype);

public:
  ReferenceArchetype();

  archetype_asset_ptr_t asset() const {
    return _asset;
  }
  void SetAsset(archetype_asset_ptr_t passet) {
    _asset = passet;
  }

private:

  //void DoCompose(ArchComposer& composer) override;

  void DoInitializeEntity(Simulation* psi, const DecompTransform& world, Entity* pent) const override;
  void DoUninitializeEntity(Simulation* psi, Entity* pent) const override;
  void DoComposeEntity(Simulation* psi, Entity* pent) const override;
  void DoLinkEntity(Simulation* inst, Entity* pent) const override;
  void DoActivateEntity(Simulation* psi, Entity* pent) const override;
  void DoDeactivateEntity(Simulation* psi, Entity* pent) const override;

  void DoComposePooledEntities(Simulation* inst);
  void DoLinkPooledEntities(Simulation* inst);

  archetype_asset_ptr_t _asset;
};

}} // namespace ork::ecs

#endif