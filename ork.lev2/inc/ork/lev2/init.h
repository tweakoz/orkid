////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/file/file.h>
#include <ork/lev2/lev2_types.h>

namespace ork::lev2 {

extern appinitdata_ptr_t _ginitdata;  // Global init data accessible during initialization

void initModule(appinitdata_ptr_t init_data);

// Create loader context if not already created (for deferred GPU init in subsystem mode)
// Returns the loader context, or nullptr if graphics disabled
context_ptr_t ensureLoaderContext();

} // namespace ork::lev2
