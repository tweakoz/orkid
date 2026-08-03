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
// Wire-in is incremental: today the terrain proctex texbake, radiancemaps, the GPU
// microtask scheduler, and submitLoadingPhase texture uploads ("texture_upload")
// register; other async producers (HDRI / impostor bakes, hybrid asset loads, cooks)
// should register as they are touched, growing toward true holistic coverage.
////////////////////////////////////////////////////////////////
#include <string>
namespace ork {
void asyncWorkBegin(const std::string& tag);
void asyncWorkEnd(const std::string& tag);
int  asyncWorkPending();        // total currently-pending (never negative)
// pending count over EVERY tag except `exclude_tag` (computed under the tag-map lock).
// The offscreen snapshot gate uses this to wait on APPEARANCE async (radiancemaps = the
// sky/IBL, asset streaming) while ignoring a background bake that never completes headless
// (terrain_texbake stalls offscreen) — so a healthy scene is not held hostage by it.
int  asyncWorkPendingExcluding(const std::string& exclude_tag);

// STEADY STATE. Some registered producers are not jobs that finish — they are
// RECURRING feeds that re-arm for as long as the scene runs (the procedural
// sky's IBL refilter re-snapshots and re-filters as the sun moves). Counting
// those as pending work makes "has the scene settled?" unanswerable: an
// offscreen movie's pre-roll drain waits for a census that will never be zero,
// so the run never starts recording. A producer declares its tag steady ONCE
// (idempotent, and never undone — a feed that has recurred once will recur
// again); settle/drain waiters then use asyncWorkPendingOneShot, which still
// waits on every genuinely-pending one-shot job. asyncWorkPending() keeps
// reporting the true total, and asyncWorkSummary() marks steady tags STEADY so
// the distinction is visible in a log line rather than implicit.
void asyncWorkMarkSteady(const std::string& tag);
bool asyncWorkIsSteady(const std::string& tag);
// pending over ONE-SHOT tags only: skips every steady-declared tag, plus
// `exclude_tag` (pass "" to exclude nothing beyond the steady set).
int  asyncWorkPendingOneShot(const std::string& exclude_tag);
std::string asyncWorkSummary(); // "terrain_texbake:1 ..." over the still-pending tags
} // namespace ork
