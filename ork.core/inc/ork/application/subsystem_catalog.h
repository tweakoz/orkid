////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/application/subsystem.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// Catalog Subsystem Factory
//
// Creates a subsystem that manages the AssetCatalog lifecycle.
// Optional - only created if _std_asset_catalog is true in AppInitData.
//
// Dependencies: OPQ (for async asset operations)
//
// Usage in Application:
//   if (_initdata->_std_asset_catalog) {
//     auto catalog_subsystem = createCatalogSubsystem();
//     catalog_subsystem->addDependency(opq_subsystem);
//     registerSubsystem(catalog_subsystem);
//   }
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createCatalogSubsystem();

} // namespace ork
