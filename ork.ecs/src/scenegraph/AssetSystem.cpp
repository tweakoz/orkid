////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectObjectVector.inl>
#include <ork/ecs/AssetSystem.h>
#include <ork/ecs/scene.h>                      // D.5: materializeAndWireScene walks the SceneData
#include <ork/ecs/SceneGraphComponent.h>        // D.5: node-item drawable/envmap patching
#include <ork/ecs/ParticlesComponent.h>         // D.5: particles_asset_name patching
#include <ork/ecs/archetype.h>
#include <ork/lev2/gfx/loadjoinset.h> // WS1: parallel channel-texture decodes
#include <ork/lev2/gfx/image.h>
#include <filesystem>
#include <ork/lev2/gfx/material_pbr.inl> // D.1 stage 3: the C++ wire step materializes materials
#include <ork/lev2/gfx/hypermesh/hmdflow.h> // D.3: hypermesh gens materialize to LiveHypermesh
#include <ork/lev2/gfx/asset_gen_vdb.h>     // D.5: ImplicitSdf / VdbGridToDrawable / ParticleSystem
#include <ork/lev2/gfx/terrain/terrain_chunk_drawable.h> // D.5 +terrain: by-name resolution
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h> // v1b: eager skybox radiance warm
#include <ork/file/path.h>

ImplementReflectionX(ork::ecs::AssetSystemData, "AssetSystemData");
ImplementReflectionX(ork::ecs::AssetSystem,     "AssetSystem");

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

AssetSystemData::AssetSystemData() {}

void AssetSystemData::describeX(SystemDataClass* clazz) {
  // Ordered list of AssetGenData. directObjectVectorProperty handles
  // shared_ptr-of-reflected-Object containers via the existing
  // DirectObjectVector codec — vector of polymorphic Object*-likes
  // round-trips through JSON automatically.
  clazz->directObjectVectorProperty("gens", &AssetSystemData::_gens);
}

void AssetSystemData::declareAssetGen(lev2::assetgendata_ptr_t gen) {
  _gens.push_back(gen);
}

System* AssetSystemData::createSystem(ecs::Simulation* psim) const {
  return new AssetSystem(this, psim);
}

void AssetSystemData::materializeAll(lev2::Context* ctx, varmap::VarMap& artifacts) const {
  // WS2 pass A — fan the PURE-CPU gens out to workers (LoadJoinSet) before the
  // ordered pass below. Today that is the OpenVDB AX voxelizations (CPU-heavy,
  // per-call-local ax::Compiler; openvdb::ax::initialize ran once at lev2 init).
  // GPU-cook gens (terrain bake, hypermesh, materials) STAY SERIAL on ctx —
  // cross-context GPU cooking is postmortem territory (see the deferred-updates
  // postmortem, ork.dox) and is not attempted here. Results are keyed
  // by GEN INSTANCE so duplicate asset names cannot race a shared slot.
  std::map<const lev2::AssetGenData*, lev2::vdb_floatgrid_ptr_t> sdf_results;
  {
    // create every slot BEFORE any worker runs (stable node addresses)
    for (auto gen : _gens)
      if (auto sdf = std::dynamic_pointer_cast<lev2::ImplicitSdfGenData>(gen))
        sdf_results[sdf.get()] = nullptr;
    lev2::LoadJoinSet ljs("assetsys_cpu_gens");
    for (auto gen : _gens)
      if (auto sdf = std::dynamic_pointer_cast<lev2::ImplicitSdfGenData>(gen)) {
        auto slot = &sdf_results[sdf.get()];
        ljs.spawnOnWorkers([slot, sdf]() { *slot = lev2::materializeImplicitSdf(*sdf); });
      }
    ljs.join(ctx);
  }
  // pass B — the ordered materialize (original semantics; SDF branch consumes pass A)
  for (auto gen : _gens) {
    if (not gen)
      continue;
    const auto& name = gen->_asset_name;
    if (auto pbr = std::dynamic_pointer_cast<lev2::PbrMaterialGenData>(gen)) {
      artifacts.makeValueForKey<lev2::pbrmaterial_ptr_t>(name) = pbr->materialize(ctx);
    } else if (auto hf = std::dynamic_pointer_cast<lev2::HeightFieldGenData>(gen)) {
      artifacts.makeValueForKey<std::string>(name) = hf->materialize(ctx);
    } else if (auto hm = std::dynamic_pointer_cast<lev2::HypermeshGenData>(gen)) {
      // D.3: the live GPU mesh graph — shared artifact (a future shared-mesh instancer or a
      // HypermeshDrawableData referencing the asset can pull the SAME LiveHypermesh).
      artifacts.makeValueForKey<lev2::hypermesh::livehypermesh_ptr_t>(name) = hm->materialize(ctx);
    } else if (auto sdf = std::dynamic_pointer_cast<lev2::ImplicitSdfGenData>(gen)) {
      // D.5: AX voxelize -> FloatGrid (consumed by VdbGridToDrawable + collider sdf refs)
      // WS2: voxelized on workers in pass A above — consume the slot.
      artifacts.makeValueForKey<lev2::vdb_floatgrid_ptr_t>(name) = sdf_results[sdf.get()];
    } else if (auto ptc = std::dynamic_pointer_cast<lev2::ParticleSystemGenData>(gen)) {
      // D.5: embedded graph -> ParticlesDrawableData (+ sdf_asset resolution + probe stamp)
      artifacts.makeValueForKey<lev2::particles_drawable_data_ptr_t>(name) =
          lev2::materializeParticleSystem(*ptc, artifacts);
    } else if (auto v2d = std::dynamic_pointer_cast<lev2::VdbGridToDrawableGenData>(gen)) {
      // D.5: marching-cubes drawable; grid + material resolve BY NAME from earlier artifacts
      lev2::vdb_floatgrid_ptr_t grid;
      if (auto g = artifacts.typedValueForKey<lev2::vdb_floatgrid_ptr_t>(v2d->_grid_asset_name))
        grid = g.value();
      lev2::material_ptr_t mtl;
      if (auto m = artifacts.typedValueForKey<lev2::pbrmaterial_ptr_t>(v2d->_material_asset_name))
        mtl = m.value();
      artifacts.makeValueForKey<lev2::drawabledata_ptr_t>(name) =
          lev2::materializeVdbGridToDrawable(*v2d, ctx, grid, mtl);
    } else if (auto mg = std::dynamic_pointer_cast<lev2::MeshGenData>(gen)) {
      // baked-geometry mesh: .ogeo sidecar -> rigid primitive; material BY NAME
      lev2::material_ptr_t mtl;
      if (auto m = artifacts.typedValueForKey<lev2::pbrmaterial_ptr_t>(mg->_material_asset_name))
        mtl = m.value();
      if (auto dd = lev2::materializeMeshGen(*mg, ctx, mtl))
        artifacts.makeValueForKey<lev2::drawabledata_ptr_t>(name) = dd;
    } else if (auto hdri = std::dynamic_pointer_cast<lev2::HdriToXirGenData>(gen)) {
      // D.5: load-time short-circuit (mirrors the Python wire) — the .xir was baked eagerly
      // at AUTHORING; at load just resolve its deterministic path (no re-bake).
      artifacts.makeValueForKey<std::string>(name) =
          file::Path::expandPathString("<assetcache>/xirtemp/" + name + ".xir");
    } else {
      // transition: this gen type's C++ materializer hasn't landed — the Python wire path
      // (materialize_from_scenedata) still covers it. Loud enough to notice, quiet enough to run.
      printf(
          "AssetSystemData::materializeAll: gen<%s> type<%s> left to the Python wire path\n",
          name.c_str(),
          gen->GetClass()->Name().c_str());
    }
  }
  // E.6/2.20 — the terrain↔material contract POST-PASS: a HeightField asset that
  // declares its shading material + channel→sampler map gets each baked channel
  // auto-bound (deterministic <assetcache> path — single source of truth on the
  // TERRAIN asset, no hand-synced path strings on the material). Post-pass so
  // declaration order is irrelevant; the 2.12 rebind contract makes the binds
  // live even though the material's pipelines may already exist.
  for (auto gen : _gens) {
    auto hf = std::dynamic_pointer_cast<lev2::HeightFieldGenData>(gen);
    if (not hf or hf->_material_asset.empty() or hf->_channel_samplers.empty())
      continue;
    lev2::pbrmaterial_ptr_t mtl;
    if (auto m = artifacts.typedValueForKey<lev2::pbrmaterial_ptr_t>(hf->_material_asset))
      mtl = m.value();
    if (not mtl) {
      printf(
          "AssetSystemData::materializeAll: HeightField<%s> references material<%s> "
          "which did NOT materialize — channel_samplers unbound\n",
          hf->_asset_name.c_str(),
          hf->_material_asset.c_str());
      continue;
    }
    // WS1 (LoadJoinSet proof adoption): the channel EXRs are multi-hundred-MB
    // decodes — fan the DECODES out to workers and join (pumping), then do the
    // GPU uploads + binds serially on ctx. Order of binds preserved.
    struct ChannelBindJob {
      std::string _sampler;
      std::string _path;
      std::string _who;
      lev2::image_ptr_t _img;
    };
    auto jobs = std::make_shared<std::vector<ChannelBindJob>>();
    for (const auto& [channel, sampler] : hf->_channel_samplers)
      jobs->push_back(ChannelBindJob{
          sampler,
          ork::file::Path::expandPathString(hf->channelPath(channel)),
          "HeightField<" + hf->_asset_name + ">↔material<" + hf->_material_asset + ">",
          nullptr});
    lev2::LoadJoinSet ljs("terrain_channel_decode");
    for (size_t ji = 0; ji < jobs->size(); ji++) {
      auto job = &(*jobs)[ji];
      if (not std::filesystem::exists(job->_path)) {
        printf(
            "%s: sampler<%s> texture<%s> MISSING — binding skipped "
            "(is the producing asset declared in the scene?)\n",
            job->_who.c_str(), job->_sampler.c_str(), job->_path.c_str());
        continue;
      }
      ljs.spawnOnWorkers([job]() { job->_img = lev2::Image::createFromFile(job->_path); });
    }
    ljs.join(ctx);
    for (auto& job : *jobs)
      if (job._img)
        lev2::PbrMaterialGenData::bindSamplerImage(mtl, ctx, job._sampler, job._img, job._who);
  }
}

///////////////////////////////////////////////////////////////////////////////

void AssetSystem::describeX(object::ObjectClass* clazz) {}

AssetSystem::AssetSystem(const AssetSystemData* data, Simulation* psim)
    : System(data, psim)
    , _data(data) {
}

// D.1 stage 3: the one-shot C++ wire step. gpu-init is the first GPU-capable lifecycle moment
// (materials gpuInit; the terrain bake dispatches compute), mirroring where the Python wire path
// runs (EcsRuntime.load_scene with the loading context). Components resolve from artifacts() BY
// NAME — the same name-reference contract the Python registry uses.
void AssetSystem::_onGpuInit(Simulation* psi, lev2::Context* ctx) {
  System::_onGpuInit(psi, ctx);
  if (_materialized)
    return;
  _materialized = true;
  _data->materializeAll(ctx, _artifacts);
}

///////////////////////////////////////////////////////////////////////////////
// D.5 — materializeAndWireScene: the pure-C++ analogue of the Python wire_scene_data.
// materializeAll over the scene's AssetSystemData, then patch every component data that
// references an artifact BY NAME. Mutates the (just-deserialized) scenedata in place;
// call BEFORE Controller::bindScene.
///////////////////////////////////////////////////////////////////////////////

varmap::varmap_ptr_t materializeAndWireScene(scenedata_ptr_t scenedata, lev2::Context* ctx) {
  auto artifacts = std::make_shared<varmap::VarMap>();

  asset_system_data_ptr_t asys;
  for (const auto& item : scenedata->getSystemDatas()) {
    if (auto as = std::dynamic_pointer_cast<AssetSystemData>(item.second)) {
      asys = as;
      break;
    }
  }
  if (asys)
    asys->materializeAll(ctx, *artifacts);

  /////////////////////////////////////////////////////////////////////////////
  // component patching — archetypes live in the scene-object map. Component datas
  // are stored const in the archetype lut; the wire step is the sanctioned mutation
  // point (the same patch the Python wire performs pre-bind), hence the const_cast.
  /////////////////////////////////////////////////////////////////////////////

  for (const auto& so_item : scenedata->GetSceneObjects()) {
    auto arch = std::dynamic_pointer_cast<Archetype>(so_item.second);
    if (not arch)
      continue;
    for (const auto& comp_item : arch->componentdata()) {
      auto comp = std::const_pointer_cast<ComponentData>(comp_item.second);
      ///////////////////////////////////////////////////////////
      if (auto sgcd = std::dynamic_pointer_cast<SceneGraphComponentData>(comp)) {
        for (auto& nd_item : sgcd->_nodedatas) {
          auto nid = nd_item.second;
          if (not nid)
            continue;
          // terrain chunk drawables serialize INLINE on the node; their asset
          // references (manifest + material) resolve here, by name (D.5 +terrain).
          if (auto tcd = std::dynamic_pointer_cast<lev2::terrain::TerrainChunkDrawableData>(nid->_drawabledata)) {
            if (auto m = artifacts->typedValueForKey<std::string>(tcd->_hf_asset_name))
              tcd->_resolved_manifest = m.value();
            else
              printf(
                  "materializeAndWireScene: terrain hf asset<%s> did not materialize\n",
                  tcd->_hf_asset_name.c_str());
            if (auto mt = artifacts->typedValueForKey<lev2::pbrmaterial_ptr_t>(tcd->_material_asset_name))
              tcd->_resolved_material = mt.value();
            else
              printf(
                  "materializeAndWireScene: terrain material asset<%s> did not materialize\n",
                  tcd->_material_asset_name.c_str());
            // [M] material-override: resolve the scene-declared debug materials GENERICALLY from
            // the reflected debug_material_assets list (data-driven — a new debug look is a Python-
            // only change, no C++ edit). _mode_materials[0] = declared (identity for mode 0); [i+1] =
            // the i-th named material. A named-but-MISSING asset FAILS LOUDLY (ops-self-defend) and
            // its slot stays null so the render falls back to the declared material for that mode.
            tcd->_mode_materials.clear();
            tcd->_mode_materials.reserve(1 + tcd->_debug_material_assets.size());
            tcd->_mode_materials.push_back(tcd->_resolved_material); // mode 0 = declared
            for (const auto& dbgname : tcd->_debug_material_assets) {
              if (auto dm = artifacts->typedValueForKey<lev2::pbrmaterial_ptr_t>(dbgname))
                tcd->_mode_materials.push_back(dm.value());
              else {
                printf(
                    "materializeAndWireScene: terrain debug material asset<%s> did not materialize "
                    "-- [M] mode skipped (falls back to declared)\n",
                    dbgname.c_str());
                tcd->_mode_materials.push_back(nullptr); // keep index alignment with the label list
              }
            }
          }
          // re-attach the materialized drawable by name
          if (not nid->_drawable_asset_name.empty()) {
            if (auto dd = artifacts->typedValueForKey<lev2::drawabledata_ptr_t>(nid->_drawable_asset_name))
              nid->_drawabledata = dd.value();
            else if (auto pd = artifacts->typedValueForKey<lev2::particles_drawable_data_ptr_t>(nid->_drawable_asset_name))
              nid->_drawabledata = pd.value();
          }
          // "asset://<name>" envmap -> the baked .xir path
          const std::string asset_pfx = "asset://";
          if (nid->_envmap_path.rfind(asset_pfx, 0) == 0) {
            auto env_name = nid->_envmap_path.substr(asset_pfx.size());
            if (auto p = artifacts->typedValueForKey<std::string>(env_name))
              nid->_envmap_path = p.value();
            else
              printf(
                  "materializeAndWireScene: node<%s> envmap asset<%s> did not materialize\n",
                  nd_item.first.c_str(),
                  env_name.c_str());
          }
        }
      }
      ///////////////////////////////////////////////////////////
      else if (auto pcd = std::dynamic_pointer_cast<ParticlesComponentData>(comp)) {
        if (pcd->_particles_asset_name.empty())
          continue;
        auto dd = artifacts->typedValueForKey<lev2::particles_drawable_data_ptr_t>(pcd->_particles_asset_name);
        if (dd and dd.value())
          pcd->_drawabledata = dd.value();
        else
          printf(
              "materializeAndWireScene: particles asset<%s> did not materialize\n",
              pcd->_particles_asset_name.c_str());
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // RENDER DEFAULTS (the Python ensure_scenegraph_system analogue): editor-authored
  // scenes commonly carry EMPTY userparams and relied on the host runtime to supply
  // the compositor preset — without one, the ECS-owned scene ctor throws "unknown
  // compositor preset type" (uncaught -> abort in a pure-C++ host). Author values
  // always win; only ABSENT keys are filled.
  /////////////////////////////////////////////////////////////////////////////

  for (const auto& item : scenedata->getSystemDatas()) {
    auto sgsd = std::dynamic_pointer_cast<SceneGraphSystemData>(item.second);
    if (not sgsd)
      continue;
    auto default_if_absent = [&](const std::string& key, auto value) {
      if (not sgsd->hasUserSceneParam(key)) {
        varmap::VarMap::value_type val;
        val.set<decltype(value)>(value);
        sgsd->setUserSceneParam(key, val);
      }
    };
    default_if_absent("preset", std::string("ForwardPBR"));
    default_if_absent("SkyboxIntensity", 1.0f);
    default_if_absent("DiffuseIntensity", 1.0f);
    default_if_absent("SpecularIntensity", 1.0f);
    default_if_absent("AmbientLevel", fvec3(0.0f, 0.0f, 0.0f));
    // LAYERS: a ForwardPBR scene REQUIRES "std_forward" (components stage their nodes
    // onto it; Scene::findLayer hard-asserts on a miss). Editor-authored scenes predate
    // the reflected Layers list — when none are declared, declare the standard set the
    // Python runtime always created.
    if (sgsd->declaredLayers().empty()) {
      sgsd->declareLayer("std_forward");
      sgsd->declareLayer("std_transparent");
      sgsd->declareLayer("hud_overlay");
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // SceneGraphSystemData skybox: "asset://<name>" resolves through the registry;
  // a literal path pushes through unchanged (both land in _userParams where the
  // SG-param generation reads them).
  /////////////////////////////////////////////////////////////////////////////

  for (const auto& item : scenedata->getSystemDatas()) {
    auto sgsd = std::dynamic_pointer_cast<SceneGraphSystemData>(item.second);
    if (not sgsd or sgsd->_skybox_path.empty())
      continue;
    std::string resolved   = sgsd->_skybox_path;
    const std::string apfx = "asset://";
    if (resolved.rfind(apfx, 0) == 0) {
      auto aname = resolved.substr(apfx.size());
      if (auto p = artifacts->typedValueForKey<std::string>(aname))
        resolved = p.value();
      else
        printf("materializeAndWireScene: skybox asset<%s> did not materialize\n", aname.c_str());
    }
    varmap::VarMap::value_type val;
    val.set<std::string>(resolved);
    sgsd->setUserSceneParam("SkyboxTexPathStr", val);

    // v1b Join-narrowing (JUL05 Appendix C step 2): FIRE the skybox radiance
    // load NOW. This runs on the render thread's onGpuInit, well before
    // startSimulation drives the sim's link rendezvous — which is where the
    // compositor LAZILY requests the skybox today. That lazy request lands
    // AFTER the offscreen settle gate exits, so the settled frame it snapshots
    // has a black sky + black IBL (#27). Warming the shared RadianceMapCache
    // here streams the decode during the settle window; the load self-registers
    // as async work (radiancemaps_asset asyncWorkBegin) so the settle gate WAITS
    // for it, and the compositor consume (requestAndRefSkyboxTexture) resolves
    // this same cached, by-then-filled object. NON-BLOCKING: warm and return,
    // never join. Scheduler-independent (helps the legacy drain too).
    if (ctx)
      lev2::pbr::getRadianceMapCache()->get(resolved);
  }

  return artifacts;
}

///////////////////////////////////////////////////////////////////////////////
// [M] the ordered debug-material asset names declared on the scene's (first) terrain chunk
// drawable — the player derives its material-cycle labels + length from this DATA (no hardcoded
// mode table). Walks the same archetype/component/node path as the wire step. Empty when the
// scene has no terrain or declares no debug materials.
///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> terrainDebugMaterialAssets(scenedata_ptr_t scenedata) {
  if (not scenedata)
    return {};
  for (const auto& so_item : scenedata->GetSceneObjects()) {
    auto arch = std::dynamic_pointer_cast<Archetype>(so_item.second);
    if (not arch)
      continue;
    for (const auto& comp_item : arch->componentdata()) {
      auto sgcd = std::dynamic_pointer_cast<SceneGraphComponentData>(
          std::const_pointer_cast<ComponentData>(comp_item.second));
      if (not sgcd)
        continue;
      for (auto& nd_item : sgcd->_nodedatas) {
        auto nid = nd_item.second;
        if (nid)
          if (auto tcd = std::dynamic_pointer_cast<lev2::terrain::TerrainChunkDrawableData>(nid->_drawabledata))
            return tcd->_debug_material_assets; // first terrain wins (the player cycles one set)
      }
    }
  }
  return {};
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
