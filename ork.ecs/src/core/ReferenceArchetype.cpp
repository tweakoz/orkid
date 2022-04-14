////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//module;

#if 0
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/Asset.inl>
#include <ork/asset/AssetManager.hpp>
#include <ork/lev2/ui/event.h>

#include <ork/ecs/scene.h>
#include <ork/ecs/ReferenceArchetype.h>

ImplementReflectionX(ork::ecs::ArchetypeAsset, "ArchetypeAsset");
ImplementReflectionX(ork::ecs::ReferenceArchetype, "ReferenceArchetype");

template struct ork::asset::AssetManager<ork::ecs::ArchetypeAsset>;

namespace ork::ecs {

using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;

class ArchetypeAssetLoader final : public asset::FileAssetLoader {
public:
  ArchetypeAssetLoader();

private:
  asset::asset_ptr_t _doLoadAsset(AssetPath assetpath, asset::vars_constptr_t vars) final;
  void destroy(asset::asset_ptr_t asset) final;
};

////////////////////////////////////////////////////////////////////////////////

ArchetypeAssetLoader::ArchetypeAssetLoader()
    : FileAssetLoader(ArchetypeAsset::GetClassStatic()) {
  // addLocation("data://archetypes", ".mox");
}

////////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t ArchetypeAssetLoader::_doLoadAsset(AssetPath assetpath, asset::vars_constptr_t vars) {
  auto archasset = std::make_shared<ArchetypeAsset>();

  object_ptr_t obj = loadObjectFromFile(assetpath.ToAbsolute().c_str());

  if (obj) {
    auto as_arch = std::dynamic_pointer_cast<Archetype>(obj);
    if (as_arch) {
      archasset->SetArchetype(as_arch);
      as_arch->SetName(AddPooledLiteral("ReferencedArchetype"));
    } else {
      archasset->SetArchetype(nullptr);
    }
  }

  // ork::Object* pobj = rtti::autocast(DeserializeObject(asset_name.c_str()));
  // archasset->SetArchetype(rtti::autocast(pobj));

  return archasset;
}

void ArchetypeAssetLoader::destroy(asset::asset_ptr_t asset) {
}

////////////////////////////////////////////////////////////////////////////////

void ArchetypeAsset::describeX(ObjectClass* clazz) {
  auto loader = std::make_shared<ArchetypeAssetLoader>();
  asset::registerLoader<ArchetypeAsset>(loader);
  // clazz->SetAssetNamer("archetypes/");
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::describeX(SceneObjectClass* clazz) {
  clazz->directProperty("asset", &ReferenceArchetype::_asset)
      ->annotate("editor.class", "ged.factory.assetlist")
      ->annotate("editor.assettype", "refarch")
      ->annotate("editor.assetclass", "ArchetypeAsset");
  // reflect::annotateClassForEditor<ReferenceArchetype>( "editor.instantiable", false );
  // reflect::annotatePropertyForEditor<ReferenceArchetype>( "Components", "editor.visible", "false" );
}

} // namespace ork::ecs

//export module ecs.ReferenceArchetype;

namespace ork::ecs {

using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;

////////////////////////////////////////////////////////////////////////////////

ArchetypeAsset::ArchetypeAsset() {
}

ArchetypeAsset::~ArchetypeAsset() {
}


////////////////////////////////////////////////////////////////////////////////

ReferenceArchetype::ReferenceArchetype() {
}

////////////////////////////////////////////////////////////////////////////////

/*void ReferenceArchetype::DoCompose(ork::ecs::ArchComposer& composer) {
  if (_asset)
    if (_asset->GetArchetype())
      _asset->GetArchetype()->compose(composer.mSceneComposer);
}*/

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoInitializeEntity(Simulation* psi, const DecompTransform& world, Entity* pent) const {
  if (_asset)
    if (_asset->GetArchetype())
      _asset->GetArchetype()->initializeEntity(psi, world, pent);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoUninitializeEntity(Simulation* psi, Entity* pent) const {
  if (_asset)
    if (_asset->GetArchetype())
      _asset->GetArchetype()->uninitializeEntity(psi, pent);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoComposeEntity(Simulation* psi, Entity* pent) const {
  /////////////////////////////////////////////////
  if (_asset) {
    // const ent::ComponentDataTable::LutType& clut = GetComponentDataTable().GetComponents();
    // for( ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it!= clut.end(); it++ )
    //{	ent::ComponentData* pcompdata = it->second;
    //	if( pcompdata )
    //	{	ent::Component* pinst = pcompdata->createComponent(pent);
    //		if( pinst )
    //		{	pent->GetComponents().AddComponent(pinst);
    //		}
    //	}
    //}
    if (auto parch = _asset->GetArchetype()) {
      parch->composeEntity(psi,pent);
      // const ent::ComponentDataTable::LutType& clut = parch->GetComponentDataTable().GetComponents();
      // for( ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it!= clut.end(); it++ )
      //{	ent::ComponentData* pcompdata = it->second;
      //	if(pcompdata)
      //	{	ent::Component* pinst = pcompdata->createComponent(pent);
      //		if( pinst )
      //		{	pent->GetComponents().AddComponent(pinst);
      //		}
      //	}
      //}
    }
  }
  /////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoLinkEntity(Simulation* inst, Entity* pent) const {
  if (_asset)
    if (_asset->GetArchetype())
      _asset->GetArchetype()->linkEntity(inst, pent);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoActivateEntity(Simulation* inst, Entity* pent) const {
  if (_asset)
    if (_asset->GetArchetype())
      _asset->GetArchetype()->activateEntity(inst, pent);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoDeactivateEntity(Simulation* psi, Entity* pent) const {
  if (_asset)
    if (_asset->GetArchetype())
      _asset->GetArchetype()->deactivateEntity(psi, pent);
}

} // namespace ork::ecs

#endif