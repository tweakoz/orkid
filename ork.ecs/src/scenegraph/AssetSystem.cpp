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

///////////////////////////////////////////////////////////////////////////////

void AssetSystem::describeX(object::ObjectClass* clazz) {}

AssetSystem::AssetSystem(const AssetSystemData* data, Simulation* psim)
    : System(data, psim)
    , _data(data) {
  // M2b: data carrier only. No-op runtime. Materialization is driven
  // from Python (Scene asset registry) for now; if/when we want a
  // pure-C++ scene load path, materialize() can live here on _onStage.
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
