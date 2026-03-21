////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/ecs/scene_import_data.h>

ImplementReflectionX(ork::ecs::SceneImportData, "EcsSceneImportData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

void SceneImportData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("SourcePath", &SceneImportData::_sourcePath);
  clazz->directProperty("Namespace", &SceneImportData::_namespace);
  clazz->directVectorProperty("SelectedArchetypes", &SceneImportData::_selectedArchetypes);
  clazz->directVectorProperty("SelectedSpawners", &SceneImportData::_selectedSpawners);
  clazz->directVectorProperty("SelectedSystems", &SceneImportData::_selectedSystems);
}

///////////////////////////////////////////////////////////////////////////////

SceneImportData::SceneImportData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
