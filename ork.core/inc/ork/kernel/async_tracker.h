#pragma once
////////////////////////////////////////////////////////////////
// async_tracker — a process-wide registry of pending asynchronous work.
//
// orkid historically had NO holistic way to know when all in-flight async work
// (GPU bakes, asset streaming, cooks, ...) has completed — each subsystem tracked
// (or didn't track) its own, so "is the scene fully settled?" had no answer and
// callers fell back to fragile frame-count / loader-idle heuristics. This is the
// shared primitive: any subsystem that kicks off async work calls
// asyncWorkBegin(tag) when it starts and asyncWorkEnd(tag) when it finishes; a
// waiter (the offscreen player / materialize, a screenshot grab, a test harness)
// polls asyncWorkPending()==0 to know the work has drained. `tag` is a coarse
// category for diagnostics (asyncWorkSummary()), not a unique id — begin/end need
// not pass the same string, but matching tags keep the summary readable.
//
// Wire-in is incremental: today the terrain proctex texbake registers; other async
// producers (HDRI / impostor bakes, hybrid asset loads, cooks) should register as
// they are touched, growing toward true holistic coverage.
////////////////////////////////////////////////////////////////
#include <string>
namespace ork {
void asyncWorkBegin(const std::string& tag);
void asyncWorkEnd(const std::string& tag);
int  asyncWorkPending();        // total currently-pending (never negative)
std::string asyncWorkSummary(); // "terrain_texbake:1 ..." over the still-pending tags
} // namespace ork
