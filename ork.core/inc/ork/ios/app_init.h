////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#if defined(ORK_IOS)

///////////////////////////////////////////////////////////////////////////////
// iOS Core Initialization API
// These functions manage the Orkid core lifecycle on iOS
///////////////////////////////////////////////////////////////////////////////

// Initialize Orkid core (call once on main thread at app startup)
void _coreappinit(int argc, char** argv);

// Shutdown Orkid core (call at app exit)
void _coreappexit();

// Poll Orkid core (call every frame on main thread)
void _coreapppoll();

#endif // ORK_IOS
