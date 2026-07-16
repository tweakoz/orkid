# VK deferred buffer updates — postmortem (2026-07-02)

**Status: the optimization is REMOVED from the tree** after a correctness bug that three
fix attempts failed to resolve. `VkFxInterface::unmapStorageBuffer` (`vulkan_fxi_buffer.cpp`)
is back to the always-synchronous staged copy. This document exists so the next attempt
starts from the disproof evidence instead of re-deriving it. The enqueue/flush machinery
(`VkComputeInterface::enqueueDeferredBufferUpdate` / `_flushDeferredBufferUpdates` /
`applyPendingUpdatesFor`) lives only in git history now — recover it with
`git log -S enqueueDeferredBufferUpdate` (introduced on the toz-2026-jul01-drm branch).

## What the optimization was

`unmapStorageBuffer` on a DEVICE-local buffer with a WRITE_ONLY mapping ≤64 KB (4-byte
aligned) skipped the synchronous staged copy (one `_syncTransfer` submit + fence wait PER
WRITE) and instead stashed the bytes on the compute interface; `beginDispatchPhase()`
recorded them all as `vkCmdUpdateBuffer` calls followed by one transfer→compute barrier.
Host reads of a buffer with pending entries applied them synchronously first
(read-your-writes). Measured win: hypermesh graph eval 6–9 ms → ~0.9 ms, racer 65–86 →
200–226 fps (Linux, discrete GPU, validation-clean on racer + uvsphere).

## The bug

With deferral enabled, **warm-cook-cached static hypermesh scenes render no hypermeshes**
(scn_forest trees, scn_scatter instances, scn_kush) on macOS/MoltenVK. Terrain
(HOST-resident buffers) unaffected. Racer unaffected — it dispatches every frame.
`BufferResidency::DEVICE` is used only by hypermesh dflow (`hmdflow.cpp` MeshPool channels +
module scratch), so the blast radius is exactly "hypermesh".

**Verified facts (owner-run windowed A/B):**
- Deferral OFF (synchronous path) → correct. Deferral ON → broken. This is the ONLY
  verified causal fact; everything below is disproof of specific repair theories.

## Fix attempts — all three DISPROVEN by windowed A/B (do not retry as-is)

1. **Graphics read-your-writes at draw funnels.** `applyPendingUpdatesFor(buf)` at
   `VkFxInterface::bindStorageBuffer` (descriptor draws) and in `_ssbo_vkbuffer()`
   (`vulkan_gbi.cpp` — all 3 indirect-draw variants + pulled index buffers).
   → no visible change.
2. **Frame-begin flush-all.** `applyAllPendingUpdates()` (synchronous drain of the whole
   queue) at the top of `VkContext::_doBeginFrame`, before any draw records.
   → no visible change. IMPLICATION: this should have delivered every pending byte before
   every draw of every frame — its failure means either the bytes are not what's missing,
   or they are consumed/snapshotted before any flush point, or the queue being drained is
   not the queue holding the writes (see hypothesis H1).
3. **Host program-order rules.** (a) enqueue dedupe changed from update-in-place to
   erase+re-append (newest write flushes last); (b) direct writes drained pending first
   (`unmapStorageBuffer` non-deferred branch + `copyBufferIntoStorageBuffer`); (c) the
   phase-begin barrier's dstAccessMask gained TRANSFER_WRITE (the vkCmdUpdateBuffer flush
   vs prior phases' compute writes was an unordered WAW).
   → no visible change.

All three are REVERTED (tree is back to the merge state + the disable). A cancellation
approach (drop pending entries when MeshPool recycles a buffer) was designed and REJECTED
before testing: dflow's contract is that issued writes execute — no cancellation semantics
exist, and none should be invented (owner decision).

## Hypotheses the next attempt should check FIRST

- **H1 — per-context pending queues vs multi-context execution.** The pending vector lives
  on `VkComputeInterface`, one per `VkContext`. Scene loading / hm graph eval may run on a
  DIFFERENT context (loading context) than the rendering context. A write enqueued on
  context A's CI is invisible to context B's phase-begin/frame-begin flushes — and if
  context A never begins another dispatch phase after cook-load, its entries never flush
  anywhere. Attempt 2 drained the RENDER context's queue only. Check: log which context's
  CI receives the enqueues vs which flushes. If H1 holds, the fix direction is to hang
  pending updates on the **VulkanBuffer itself** (device-wide), not on a per-context CI.
- **H2 — `vkCmdUpdateBuffer` on MoltenVK inside this CB pattern** (many updates + barrier
  at CB start, ONE_TIME_SUBMIT, reset each phase). Validate with a minimal repro writing a
  known pattern and reading it back.
- **H3 — something consumes the buffer between unmap and every flush point** that is
  neither a descriptor-bound draw, an indirect-args read, a host read, nor an in-phase
  CI copy (those were all covered by attempts 1–3). Enumerate consumption paths again
  with H1 in mind (secondary command buffers? per-view cull fan-out? texture-side copies?).

## Constraints a reimplementation must satisfy

- Correct on: warm-cook-cached static hm scenes (forest/scatter/kush — the coverage gap
  that shipped the bug; racer/uvsphere validate NOTHING about deferral because they
  dispatch every frame), multi-context loading, MoltenVK + Linux.
- Host program order per buffer must hold across BOTH write paths (deferred + direct) and
  across buffer reuse (MeshPool free-list recycling) — with writes always executing,
  never cancelled.
- No new host fences on the write path (that's the whole point). Single graphics queue is
  confirmed engine-wide — submission order + barriers can own all ordering.
- Verification protocol: windowed A/B with the disable flag as control, on a WARM cache.
  Offscreen self-verification is currently impossible (offscreen composite renders black
  for every scene — separate bug; `--snapshot` in ork.ecs.player is ready once fixed).

## Postscript (2026-07-02): the bug outlived the disable via the cook cache

After the disable (and later full removal of the machinery), scn_scatter still rendered its
gid-1 bucket wrong (shard tips lost their gold material). Root cause: cooks that ran while
deferral was enabled wrote WRONG results into `<staging>/dflowcache` — the assign-gid/select
dispatches ran without their param-SSBO writes landing, so the cooked gid channel had every
face in bucket 0. Content-addressed caching then re-served the poisoned artifact forever:
the graph content (hash) never changed, so no re-cook, and no runtime fix could help.
Confirmed by moving the cache aside and re-cooking under the clean build (gold tips returned).

LESSON: a GPU-correctness bug's blast radius includes every artifact COOKED while it was
live, not just what it renders. Recovery protocol for any such incident: after the code fix,
invalidate (move aside) every cook cache written during the bug window. The "deferral OFF →
correct" A/B verified mesh PRESENCE on warm caches — it could not catch stale-but-loadable
poisoned artifacts.

## How to reattempt

The machinery is no longer in-tree. Recover the last state of it (enqueue/dedupe,
phase-begin flush, read-your-writes apply) from git history:
`git log -S enqueueDeferredBufferUpdate` — it entered on the toz-2026-jul01-drm branch and
was removed after the disable commit (`015ecc0f2`, "sychro fix (but slower)"). Start from
hypothesis H1 (hang pending updates on the VulkanBuffer, not a per-context CI) rather than
re-porting the old per-context design.
