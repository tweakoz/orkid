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

// Stop the lev2 loader thread (idempotent). The thread is spawned
// automatically whenever gloadercontext is created — see lev2_init.cpp.
// Callers MUST invoke this before gloadercontext is destroyed
// (typically from the gpu subsystem's _onGpuExit, OrkEzApp destructor,
// or any teardown path that releases gloadercontext).
void stopLoaderThread();

///////////////////////////////////////////////////////////////////////////////
// Loader-thread lifecycle hooks. The callbacks run on the loader thread
// with a ThreadGfxContext TLS already attached, so any TXI/GBI/etc. work
// inside them is queued onto gloadercontext.
//
//   init   — fires once on the first loader iteration AFTER the callback
//            has been set. Use for one-time setup (reserve textures, etc.).
//   update — fires each loader iteration between beginFrame and endFrame.
//            Use for per-frame chunked-upload streaming, etc. Today this
//            is ~2kHz worst-case (gated by the loader's 500µs sleep), so
//            the callback must gate its own work (dirty flag, time check,
//            etc.) or it will spin hot.
//   exit   — fires once after the loader loop exits, before the thread
//            joins. Symmetric counterpart to init.
//
// Pass nullptr to clear a hook (used during teardown so the loader stops
// invoking a callback that's referring to torn-down state).
//
// Callbacks are stored behind a mutex; reads/writes are cheap.
///////////////////////////////////////////////////////////////////////////////
using loader_callback_t = std::function<void(context_ptr_t loader_ctx)>;
void setOnLoaderInit(loader_callback_t cb);
void setOnLoaderUpdate(loader_callback_t cb);
void setOnLoaderExit(loader_callback_t cb);

} // namespace ork::lev2
