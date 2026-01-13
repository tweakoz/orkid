////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/subsystem_catalog.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/util/logger.h>

namespace ork {

static logchannel_ptr_t logchan_CATALOG = logger()->configureChannel("CATALOG", fvec3(0.4, 0.6, 1.0), true);

///////////////////////////////////////////////////////////////////////////////
// Catalog Subsystem Implementation
//
// Wraps AssetCatalog initialization with proper FSM lifecycle.
// The global catalog instance is lazily created on first access.
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createCatalogSubsystem() {
  auto subsystem = std::make_shared<Subsystem>("catalog");

  // INITIALIZING state -> START -> initializes catalog
  subsystem->_state_initializing->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_CATALOG->log("Catalog subsystem initializing...");

    using namespace asset::catalog;

    // Get/create the global catalog instance
    // This loads manifests from $ORKID_ASSET_MANIFEST_DIRS
    auto catalog = AssetCatalog::globalInstance();

    logchan_CATALOG->log("Catalog subsystem initialized");

    // Transition to READY
    instance->sendEvent("READY");
  };

  // SHUTTING_DOWN state -> SHUTDOWN -> cleanup
  subsystem->_state_shutting_down->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_CATALOG->log("Catalog subsystem shutting down...");

    // The global catalog instance will be cleaned up at process exit
    // No explicit cleanup needed here

    logchan_CATALOG->log("Catalog subsystem shutdown complete");

    // Transition to TERMINATED
    instance->sendEvent("TERMINATED");
  };

  return subsystem;
}

} // namespace ork
