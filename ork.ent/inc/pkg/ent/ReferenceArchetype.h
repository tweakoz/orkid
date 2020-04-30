////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTI.h>

#include <ork/asset/Asset.h>
#include <ork/asset/AssetManager.h>

#include <pkg/ent/entity.h>

namespace ork { namespace ent {

class ArchetypeAsset final : public asset::Asset {
  RttiDeclareConcrete(ArchetypeAsset, asset::Asset);

public:
  ArchetypeAsset();
  ~ArchetypeAsset();

  Archetype* GetArchetype() const {
    return mArchetype;
  }
  void SetArchetype(Archetype* archetype) {
    mArchetype = archetype;
  }

protected:
  Archetype* mArchetype;
};

///////////////////////////////////////////////////////////

class ReferenceArchetype final : public Archetype {
  RttiDeclareConcrete(ReferenceArchetype, Archetype);

public:
  ReferenceArchetype();

  ArchetypeAsset* GetAsset() const {
    return mArchetypeAsset;
  }
  void SetAsset(ArchetypeAsset* passet) {
    mArchetypeAsset = passet;
  }

private:
  void DoCompose(ork::ent::ArchComposer& composer) override;
  void DoComposeEntity(Entity* pent) const override;
  void DoLinkEntity(Simulation* inst, Entity* pent) const override;
  void DoStartEntity(Simulation* psi, const fmtx4& world, Entity* pent) const override;
  void DoStopEntity(Simulation* psi, Entity* pent) const override;

  void DoComposePooledEntities(Simulation* inst);
  void DoLinkPooledEntities(Simulation* inst);

  ArchetypeAsset* mArchetypeAsset;
};

}} // namespace ork::ent
