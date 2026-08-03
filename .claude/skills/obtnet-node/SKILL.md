---
name: obtnet-node
description: Operate THIS machine as an obtnet fleet node — launching the node daemon correctly, what the controller may do to this machine (jobs, builds into the staging, checkout gitsync/sync), node-side etiquette and troubleshooting. Use when setting up, running, or debugging the node side of the fleet (the controller-side skill is `obtnet`).
---

# obtnet-node — this machine as a fleet node

A node is a small daemon (`obt.net.node.py`) that registers with the controller, heartbeats,
and executes relayed work. Its terminal console is a first-class UI: one compact line per
event (connect, run→/done rc+duration+bytes, uploads, job state changes) — run it somewhere
you can watch.

## The launch law (non-negotiable)

**Nodes ALWAYS run inside an obt shell.** Jobs inherit the node's environment verbatim —
the obt shell is what makes bare tools (`ork.build.py`, `obt.dep.*`, `ork.cpp.db.*`) resolve
and what points them at THIS machine's staging + project checkout.

```bash
# inside an obt shell (obt.env.launch.py --stagedir <stage> [--project <checkout>]):
obt.net.node.py --controller <controller-host>     # port defaults to 7461
```
Name defaults to the hostname. Never launch a node "naked" (plain ssh session): a partial
environment produces confusing distant failures — `ModuleNotFoundError: obt`, empty
`$OBT_STAGE` — instead of a loud local one. Temp/extra nodes go through
`obt.env.launch.py --stagedir <stage> --command obt.net.node.py` too.

## Two kinds of node on this machine (never conflate)

- **WORKER node** — everything else in this file: executes jobs in this machine's staging
  and checkout. Normally HUB-started from FLEET.json during fleet bringup (owner jul30);
  hand-start only on direct instruction. Agents outside the hub's bringup flow never
  start one — they print the launch line.
- **MAILBOX node** — `obt.net.node.py … --name coord-<seat> --restrict msg,sync`: a
  coordinator seat's mail endpoint. It executes NO work: `--restrict msg,sync` allows only
  the message-deposit tool and `git` (so `sync`/`gitsync` still work), and everything else
  comes back as `restricted node <name> refuses …`. It belongs to that seat's COORDINATOR,
  who starts and restarts it unprompted; the launch line lives in that seat's bringup
  procedure (`.claude/skills/hub-coordinator/BRINGUP.md` /
  `.claude/skills/sub-coordinator/BRINGUP.md`), not here. Placement rule: a HUB's mailbox
  joins the hub's OWN controller; a SUB's mailbox joins the HUB's controller — that is how
  hub-to-sub mail arrives.

Both kinds obey the launch law above (always inside an obt shell). One machine commonly
hosts both — different processes, different owners, different rules.

## What the controller may do to this machine

- **Jobs run in your env**: `run`/`submit`/`build`/`test`/`scene` execute with this node's
  staging and checkout. `build` INSTALLS into this machine's staging.
- **`scene` runs are offscreen by default** (windowed requires explicit consent
  controller-side) — but on linux even offscreen needs the DRM device: a node launched from
  a seat-less session (plain ssh) will fail scene runs with a loud
  `CtxDRM::_runloopBegin` assert. Launch from the local seat (or grant DRM access) if this
  node should render.
- **Your checkout is fleet-managed**: `gitsync` fast-forwards it to the controller's
  branch@sha (refuses if you have local commits — that's divergence, resolved with normal
  git by a human); `sync` pushes the controller's working-tree delta as content.
- **Your hand-edits are protected**: the sync dirt guard refuses to overwrite tracked files
  you changed that the controller doesn't know about — the controller must consciously
  `--force`. Best etiquette on a fleet-managed checkout: keep node-side experiments as
  working-tree state (never local commits), and expect them to be pulled/merged or
  deliberately clobbered.

## Node-side etiquette

- One lane per machine: when a lane (a controller-driven work stream) owns this box, its
  staging and checkout state belong to that lane until it ends. Watch the console.
- A STAGING is the unit of isolation (owner clarification jul29): it houses both
  the build dirs AND the install dirs, so full lane isolation = ONE STAGING PER
  LANE. "Isolated build dirs" inside one staging isolate nothing at install time
  (field-hit 2026-07-29: parallel installs collided in the shared pyvenv). When
  lanes MUST share a staging (e.g. a seat granted a single staging), that is the
  degraded case: serialize runtime-gate windows per staging — coordinator grants
  the slot, agent verifies installed-lib timestamps; compile-only may overlap.
- The node imports its code at start — after an `ork.build` update
  (`pip install -U ork.build`), RESTART the node to serve new verbs/ops.
- Ctrl-C is a clean shutdown; the controller expires the registration within ~2 minutes,
  and a relaunched node re-registers automatically (same name = replaces).

## Troubleshooting (symptom → cause)

| Symptom | Cause |
|---|---|
| `ModuleNotFoundError: obt` from bare tools in jobs | node not in a (full) obt shell — relaunch per the law |
| `$OBT_STAGE` empty in job env | same — partial environment |
| scene jobs die instantly, `CtxDRM ... _target` assert | no DRM master (node launched from a seat-less session) |
| controller `list` doesn't show this node | controller addr wrong (`--controller`, `OBTNET_CONTROLLER`, `~/.obt-global/obtnet.json`) or network |
| node console says "controller forgot us; re-registering" | controller restarted — harmless, self-heals |
| gitsync refuses: DIVERGED | this checkout has local commits — resolve with git, don't fight the tool |
| gitsync FAILs naming LFS pointers/smudge | the asset objects are not in this node's `.git/lfs/objects` and no server has them (lane commits are never pushed) — the controller stages them; do NOT `git lfs pull` |
| build fails `CMake Error: source ... does not match` | a lane repointed the staging build dir — remove `CMakeCache.txt` + `CMakeFiles` under `<stage>/builds/<proj>/.build` |
| a seat's mail fails `... registers neither <seat> nor coord-<seat> ...` | the mailbox node for THAT seat is dead/never started (hub-side, for sub-to-hub mail) — or the sender used the wrong seat name |
| a job sent to a `coord-*` node returns `restricted node ... refuses` | working as designed: mailbox nodes take no work — send it to a worker |
| an `@each` execution fan-out reports one failure per `coord-*` node | same cause; use explicit worker names for `build`/`test`/`scene`/`run` |
