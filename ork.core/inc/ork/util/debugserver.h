////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

// ork::debugserver — an in-process zmq REQ/REP debug console (ipc), the C++ peer of
// ork.debug.lldbserve.py. Lets a non-interactive caller query a LIVE orkid process
// (thread metrics, uptime, app-registered probes) without a debugger attached:
//
//   ORKID_DEBUG_SERVER=mysid ork.ecs.player.exe scene.ecs        # env opt-in (ezapp auto-start)
//   ork.debug.lldbclient.py --debugserver --sessionid mysid threads
//   ork.debug.lldbclient.py --debugserver --sessionid mysid help
//
// Apps/subsystems can add their own probes:
//   debugserver::registerHandler("hmperf", [](const std::string&){ return HmPerf::instance().statsText(); },
//                                "last hypermesh perf window");
//
// Built-ins: help, pid, uptime, threads (per-thread name/cpu/state — macOS).
// The server thread is detached + poll-driven; start() is idempotent.

#include <string>
#include <functional>

namespace ork::debugserver {

using handler_t = std::function<std::string(const std::string& args)>;

// register (or replace) a named probe. Thread-safe; allowed before or after start().
void registerHandler(const std::string& name, handler_t fn, const std::string& help = "");

// bind ipc:///tmp/ork.debugserve.<sessionid>.ipc and serve. Idempotent.
void start(const std::string& sessionid);

// start iff ORKID_DEBUG_SERVER=<sessionid> is set in the environment. Returns true if started.
bool startFromEnv();

void stop();

} // namespace ork::debugserver
