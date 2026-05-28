////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <ork/ecs/system.h>
#include <ork/lev2/gfx/asset_gen.h>

// HYPERECS M2b.4 — AssetSystem / AssetSystemData.
//
// AssetSystemData holds the ordered list of reflected AssetGenData
// instances declared by a Scene's `self.asset.<X>(...)` calls. Living
// on the SceneData as a reflected SystemData means the whole asset
// graph round-trips through JSON for free (the Scene's systemDatas
// vector is already reflected).
//
// AssetSystem is the runtime ECS system for the data. M2b leaves it
// as a pure data carrier — no per-frame work. Future milestones can
// hook _onStage to drive materialization from the deserialized side
// (e.g. when scene_data was loaded from JSON and the Python asset
// wrappers need to look up their inputs by grid_asset_name).

namespace ork::ecs {

struct AssetSystem;

///////////////////////////////////////////////////////////////////////////////

struct AssetSystemData : public SystemData {
  DeclareConcreteX(AssetSystemData, SystemData);

public:
  AssetSystemData();

  // Append a gen to the list. Name uniqueness is enforced by the
  // Scene-side wrapper (Scene.asset namespace) — by the time a gen
  // reaches here it already has a unique _asset_name. Multiple
  // entries with the same name would be a Scene-side bug.
  void declareAssetGen(lev2::assetgendata_ptr_t gen);

  std::vector<lev2::assetgendata_ptr_t> _gens;

private:
  System* createSystem(Simulation* psim) const final;
};

using asset_system_data_ptr_t = std::shared_ptr<AssetSystemData>;

///////////////////////////////////////////////////////////////////////////////

struct AssetSystem final : public System {
  DeclareAbstractX(AssetSystem, System);

  static constexpr systemkey_t SystemType = "AssetSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  AssetSystem(const AssetSystemData* data, Simulation* psim);

private:
  const AssetSystemData* _data = nullptr;
};

} // namespace ork::ecs
