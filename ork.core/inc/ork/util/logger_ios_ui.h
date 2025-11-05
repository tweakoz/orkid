////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#if defined(ORK_IOS)

#ifdef __OBJC__
@class UIViewController;
#else
typedef struct objc_object UIViewController;
#endif

namespace ork {

struct LoggerBackend;

// Install the iOS UI logger backend
void installIOSUIToBackend(LoggerBackend* backend);

// Set the main view controller (call after creating MainViewController)
void setIOSUIMainViewController(void* mainVC);

} // namespace ork

#endif // ORK_IOS
