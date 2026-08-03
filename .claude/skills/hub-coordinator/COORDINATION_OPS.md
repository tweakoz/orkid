# Coordinator seat operations (restorable from any checkout)

Canonical docs ship IN THIS REPO: this file + TASKSET_FORMAT.md + BRINGUP.md (hub) +
`.claude/skills/sub-coordinator/BRINGUP.md` (sub) + the agent defs in .claude/agents/
(hub-coordinator.md and sub-coordinator.md carry the role doctrine). A coordinator
session on ANY machine restores a seat from these files alone.

DAILY bringup is NOT this file: it is the two BRINGUP.md procedures, triggered by one
owner sentence per seat ("start up as a hub-coord" / "start up as a sub-coord with the
hub-coord at <ip>"). This file is the architecture reference behind them.
Terminology: the integration seat is the HUB (older docs/tools say "master" — same
role; `obt.coord.seat.py` still spells the role flag `--role master`).

## First-time provisioning (new or wiped machine — beyond the daily bringup)
1. obt tooling: ork.build with msg verbs + class routing + `obt.coord.seat.py`
   (>= the jul27 class-routing rev) installed in the seat's venv.
2. `obt.coord.seat.py init` (see BRINGUP step 1 for the role-correct line) scaffolds
   ~/coordination/{inbox/acked,bin,tasksets,decisions}, installs the ssh-path deposit
   tool (venv-independent, system python3), and writes seat identity into
   ~/.obt-global/obtnet.json.
3. On the HUB additionally: "sshhosts": { "<seat>": "<ssh host>", ... } in
   obtnet.json — one entry per sub seat (used for hub→seat push/pull and for repo
   updates when a seat is off-bus). On a SUB: the hub seat must NOT be in sshhosts.
4. LLM wake: a persistent monitor tailing ~/coordination/inbox.stream (metadata lines
   only: ts/from/subject/path — read bodies selectively) plus a coarse scheduled
   fallback check. Re-arm after every processing round.

## Message routing (class-based, deterministic — obt/net/client.py classify())
Every msg/push/pull target derives exactly ONE route from declared config + live
rosters; collisions are a hard RouteError, never a silent pick. `obt.net.py route
<target>` prints the derived class, the decision trail, and liveness without sending.
Classes, in evaluation order:
- coord-seat: `coord-<target>` is registered on YOUR OWN controller → hub→sub mail.
- work-node: `<target>` is a plain worker on your own controller — REFUSED for msg
  verbs (workers execute; they are not mail endpoints).
- coord-master: your obtnet.json declares `master: <hub addr>` and the target matches
  (by `master_coordid` if declared, else by ROLE: any name that is not a local node
  and not an sshhost routes UP) → sub→hub mail. Delivery requires the hub to run
  `coord-<hub-seat>` on its OWN controller — the deposit is written by that node.
- ssh: `<target>` is a declared sshhosts entry. A first-class declared route (hub→seat
  repo/file ops when the seat is off-bus) — NEVER a fallback for a down bus link; a
  down link fails loudly naming its owner.
Config contract: `coordid` on every seat; `master` ONLY on subs (`controller` stays
local — never point it at the hub); `sshhosts` ONLY on the hub. `msg send` takes SEAT
names, never `coord-*` node names, and never needs `--controller` (read-only roster
verification is the one sanctioned `--controller` use).

## Traffic
- obt.net.py msg send <seat|@coords> --subject S --body-file F   (route auto-derived)
- obt.net.py msg list / msg ack <msg>       (ack moves to inbox/acked/ — audit)
- obt.net.py push <seat> <local> <remote> / pull <seat> <remote> <local>   (note the argument order flips; bundles/artifacts use
  sync/gitsync/fetch as ever)
- Payloads are documents (briefs, manifests, taskset excerpts) — never chat.
- Briefs are SELF-CONTAINED: inline every ruling/spec excerpt the seat needs.
  Never cite hub-only paths (the taskset) as if a sub can fetch them — it can't,
  by design (field-hit 2026-07-29).

## Tasksets
Format: TASKSET_FORMAT.md (this dir). Tasksets are HUMAN-OWNED folders at
owner-chosen paths (multi-week workstreams; tasks span seats and days) —
the owner git-tracks them. The hub only holds a POINTER
(~/coordination/ACTIVE_TASKSET); the owner switches by naming a folder.
~/coordination/ itself is runtime-only state. A taskset folder restored onto
any hub checkout is fully operational. ONLY the hub loads tasksets; subs work
from inbox briefs.

## Two node classes — never conflate (owner, 2026-07-26)
- WORKER nodes: execute work (build/run/test/scene, unrestricted verbs) in a
  staging env, registered to whichever controller drives that fleet. Named for
  the machine/lane. Machine-lane exclusivity and gate laws apply to these.
  STAGING DISJOINTNESS (owner jul29): a worker NEVER shares a staging with a
  coordinator session on the same box — the session's live env is nobody's
  build target.
- COORDINATOR (mailbox) nodes (`coord-<seat>`, always `--restrict msg,sync`): pure
  communication endpoints. They never execute work; sending work to one is an
  error the node refuses. Placement rule: the HUB's mailbox joins the hub's OWN
  controller (it receives every sub→hub deposit); a SUB's mailbox joins the
  HUB's controller (it receives hub→sub mail). A seat may host BOTH classes
  (its workers on its own controller, its mailbox per the placement rule) —
  different processes, different purposes, different rules.

## Division of control (owner, 2026-07-26; refined 2026-07-27/29; superseded 2026-07-30)
- OWNER speaks ONE sentence; the HUB starts EVERYTHING from it. "bring up the
  fleet" (= "restore the fleet"; bringup and restore are the same procedure)
  → hub plumbing, then per FLEET.json: sub-coordinator claude sessions in
  screen over ssh, and all `start=true` worker nodes. Console visibility
  survives as `screen -r` — screen IS the watched console; the owner attaches,
  never launches. (This supersedes the jul26/27 owner-starts-workers rule.)
- COORDINATORS (hub AND sub) start/restart their OWN plumbing UNPROMPTED:
  controller and mailbox node. Restarting one needs nobody's permission. A
  sub's dead workers are the hub's to relaunch (fleet bringup step); its dead
  mailbox/controller are its own.
- SUBAGENTS (spawned via the Agent tool) never start nodes, controllers, or
  sessions ON THEIR OWN INITIATIVE — they PRINT launch lines for the
  coordinator that briefed them.

## Authority (never transported)
The hub merges; the owner owns outcomes + distribution gates. STAGING
OWNERSHIP (owner jul29): seat-box stagings are SEAT-OWNED — the hub never
writes into ~/.staging-* on a sub's box; hub-briefed work executes in the
SEAT staging unless the owner says otherwise. Seats are
hardware-anchored (mixer=audio, headset=VR, owner's chair=integration);
roles move when hardware moves.
