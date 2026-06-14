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
#include <ork/kernel/varmap.inl>

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

  // D.1 stage 3 — the C++ WIRE STEP. Walk _gens in declaration order, materializing every type
  // whose C++ materializer exists (PbrMaterialGenData -> live PBRMaterial; HeightFieldGenData ->
  // the .terrain.json manifest path) into `artifacts` keyed by _asset_name. Types still on the
  // Python wire path (transition) are SKIPPED with a notice — the Python wire_scene_data layer
  // covers them until their materializers land. Declaration order = dependency order (the same
  // contract the Python materialize_from_scenedata documents).
  void materializeAll(lev2::Context* ctx, varmap::VarMap& artifacts) const;

  std::vector<lev2::assetgendata_ptr_t> _gens;

private:
  System* createSystem(Simulation* psim) const final;
};

using asset_system_data_ptr_t = std::shared_ptr<AssetSystemData>;

///////////////////////////////////////////////////////////////////////////////
// D.5 — the C++ WIRE STEP for a deserialized scene (the pure-C++ analogue of the
// Python wire_scene_data): materializeAll over the scene's AssetSystemData, then
// patch the component datas that reference artifacts BY NAME:
//   - SceneGraphComponentData node items: _drawable_asset_name -> _drawabledata,
//     "asset://" _envmap_path -> the baked .xir path
//   - ParticlesComponentData: _particles_asset_name -> _drawabledata
//   - SceneGraphSystemData: "asset://" _skybox_path -> _userParams SkyboxTexPathStr
//     (literal paths push through unchanged)
// Call AFTER deserializeJson and BEFORE Controller::bindScene; needs a GPU
// context (material gpuInit / bakes). Returns the {asset_name -> artifact} map.
///////////////////////////////////////////////////////////////////////////////

struct SceneData;
using scenedata_ptr_t = std::shared_ptr<SceneData>;
varmap::varmap_ptr_t materializeAndWireScene(scenedata_ptr_t scenedata, lev2::Context* ctx);

///////////////////////////////////////////////////////////////////////////////

struct AssetSystem final : public System {
  DeclareAbstractX(AssetSystem, System);

  static constexpr systemkey_t SystemType = "AssetSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  AssetSystem(const AssetSystemData* data, Simulation* psim);

  // the materialized {asset_name -> artifact} registry (populated once at gpu-init; the
  // future C++ host's components resolve their references here BY NAME)
  const varmap::VarMap& artifacts() const { return _artifacts; }

private:
  void _onGpuInit(Simulation* psi, lev2::Context* ctx) override; // one-shot: materializeAll
  const AssetSystemData* _data = nullptr;
  varmap::VarMap _artifacts;
  bool _materialized = false;
};

} // namespace ork::ecs
