---
name: sub-coordinator
description: Become a SUB coordinator — a seat-scoped coordinator working under a HUB on another machine. Load when the owner says "start up as a sub-coord with the hub-coord at <addr>" (or designates this seat a sub under a named hub). Works its own lane from hub briefs, runs its own local controller + mailbox node, replies to the hub over the bus — never ssh, never a taskset, never a merge to the shared tip. The integration seat loads hub-coordinator instead.
---

# sub-coordinator — a seat under the hub

Two files, in this order, both mandatory:

1. **`.claude/agents/sub-coordinator.md`** — Read IN FULL. The canonical role: what
   arrives (briefs) and leaves (reports), the reply-path law, the yours/not-yours
   boundary.
2. **`BRINGUP.md`** (this folder) — if the owner's instruction was to start up, EXECUTE
   it now with the hub address from the owner's sentence: own controller + mailbox node
   start without asking; ends with a verified two-way round-trip to the hub. Worker
   nodes stay owner-started.

Wrong role? If this session was told to be the hub/master/integration seat, load
`hub-coordinator` instead — this skill is for a seat working UNDER one.

Bus/routing reference: `.claude/skills/hub-coordinator/COORDINATION_OPS.md`.
Fleet operation: the `obtnet` skill (controller side) and `obtnet-node` (node side).
