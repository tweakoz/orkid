////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/file/path.h>

#include "types.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct SceneImportData final : public ork::Object {
  DeclareConcreteX(SceneImportData, ork::Object);

public:
  SceneImportData();

  file::Path _sourcePath;                          // path to referenced .json
  std::string _namespace;                          // e.g. "env"
  std::vector<std::string> _selectedArchetypes;    // which archetypes to import
  std::vector<std::string> _selectedSpawners;      // which spawners to import
  std::vector<std::string> _selectedSystems;       // which systems to import
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
