---
name: hub-coordinator
description: Become the HUB coordinator (formerly "master") — the integration seat for multi-agent orkid engine work. Load when the owner says "start up as a hub-coord", names this session the hub/master/integration coordinator, or says "coordinator" with NO hub address to work under. If the sentence names a hub to work UNDER ("start up as a sub-coord with the hub-coord at <addr>") that is the SUB trigger: load sub-coordinator instead, not this. Holds the taskset, decomposes to seats, merges everything, runs the fleet controller + mailbox node. A seat working UNDER a hub loads sub-coordinator instead.
---

# hub-coordinator — the integration seat

Two files, in this order, both mandatory:

1. **`.claude/agents/hub-coordinator.md`** — Read IN FULL. The canonical role: agent
   graph, per-slice loop, laws, landing checklist, multi-coordinator doctrine, tasksets.
2. **`BRINGUP.md`** (this folder) — if the owner's instruction was to start up or bring
   up/restore the fleet, EXECUTE it now, in full: hub plumbing (controller + mailbox +
   monitor), THEN fleet bringup from the active taskset's FLEET.json — YOU start the
   sub-coordinator claude sessions (screen, over ssh) and the worker nodes; nothing is
   owner-started by hand. Bringup and restore are the same procedure (owner jul30).

Also in this folder: `COORDINATION_OPS.md` (bus architecture, routing classes, node
classes, division of control), `TASKSET_FORMAT.md` (taskset folder contract — hub-only;
subs never load tasksets), `fleet.schema.json` (FLEET.json schema — the instance file
lives in the active taskset, hostnames legal there only).

Wrong role? If this session was told to be a sub-coordinator under some hub, load
`sub-coordinator` instead — this skill is for the ONE integration seat.

Fleet operation: the `obtnet` skill (controller side) and `obtnet-node` (node side).
