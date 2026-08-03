# Daily bringup — SUB seat

Trigger: the owner says **"start up as a sub-coord with the hub-coord at <hub-ip>."**
`<hub-ip>` is the ONE parameter; the canonical hub address is `tcp://<hub-ip>:7461`.
Run every step below immediately, in order, without asking permission for any of them.
(Your seat's WORKER nodes are normally started by the HUB during fleet bringup from
FLEET.json — owner jul30; restarting your own dead plumbing, mailbox included, is
always yours.) Every step is idempotent: "already running/registered" is a PASS, not
a reason to restart anything.

Context: run from this seat's OBT shell. `obt.*` tools are invoked BARE (on PATH).
`<seat>` below = this seat's name (its `coordid`).

## Procedure

0. **Version gate** (fail HERE, not at step 5 — both sub seats hit this on 2026-07-29):

       obt.net.py route --help

   Must print usage. `invalid choice: 'route'` = this seat's deployed obt predates the
   jul27 class-routing revision — the reply path this doctrine mandates DOES NOT EXIST
   in the installed client. STOP; report to the owner/hub: "seat needs the
   class-routing ork.build delivered to its venv". No config change can fix it and
   every workaround is outlawed.

   OBT-PURE LAW (owner, jul29): every bringup command is an `obt.*`/`ork.*`
   invocation — shell primitives (mkdir/cat/tail) are not in seat allowlists
   and stall unattended sessions at permission prompts. The logs scaffold is
   `init`'s job (step 1); if an older init omitted `~/coordination/logs`,
   rerun init after the ork.build fix lands — do not hand-mkdir.

1. **Seat wiring** (config + scaffold, idempotent — this step is what makes the reply
   path derive correctly; most historical bringup failures were this step skipped):

       obt.coord.seat.py init --seat <seat> --master tcp://<hub-ip>:7461 --role sub

   This writes `coordid` + `master` into `~/.obt-global/obtnet.json`. The `controller`
   key stays LOCAL (or absent) — the hub's address lives ONLY under `master`. The hub
   seat must NOT appear in this machine's `sshhosts`.
   CHECK: verdict line starts `[obtcoord] ok`.
   If this seat has no name yet, ask the owner for one — do not invent it.

2. **Own controller** (yours to start, always — it drives THIS seat's local workers;
   it is unrelated to reaching the hub):

       obt.net.py list

   - Answers → already up, use it. Times out → start backgrounded: `obt.net.controller.py`
   CHECK: `obt.net.py list` answers. (Local workers: the hub starts them at fleet
   bringup; they register here on their own — nodes retry until the controller is up.)

3. **Mailbox node** (yours to start, always — joins the HUB's controller; this is how
   hub→sub mail reaches you):

       obt.net.node.py --controller tcp://<hub-ip>:7461 --name coord-<seat> --restrict msg,sync

   (backgrounded, FROM THE OBT SHELL; supervision optional)
   (outside an OBT shell, use the tool's printed variant that wraps this same command in
   obt.env.launch.py)
   PATH LAW (field-proven jul30, this exact seat class): the mailbox node EXECS
   `obt.net.msg.deposit.py` BY NAME from its own PATH when hub mail arrives. A node
   launched by absolute venv path from a default-login-PATH shell registers and your
   outbound sends work — but every HUB→YOU deposit is refused with `exec_failed`, and
   your own step 6 proof cannot see it. The node's env must carry the venv bin
   (and/or `~/coordination/bin`) on PATH.
   CHECK — read-only roster query against the hub controller:

       obt.net.py --controller tcp://<hub-ip>:7461 list

   shows `coord-<seat>`. (`--controller` is legitimate for VERIFICATION reads like this
   one. It is never part of sending messages.)

4. **Validate the seat**:

       obt.coord.seat.py check

   CHECK: final line `[obtcoord] ok check seat<...> ... fail=0`.
   NOTE (expected on every healthy sub): the `registration` line reads
   `INFO registration      coord-<seat> not registered (controller reachable)`.
   That check queries YOUR LOCAL controller by design, while your mailbox lives on the
   HUB's — step 3 already proved it registered there. Only `fail=0` matters; do not
   "fix" this by pointing `controller` at the hub (that breaks your local fleet).

5. **Resolve the hub's seat name** (you need it as the msg target — the owner's
   sentence only carried an IP): in the step-3 roster listing, the `coord-*` node whose
   address is `<hub-ip>` itself is the hub's own mailbox; its suffix is the hub seat
   name. Example: roster shows `coord-alpha` at `tcp://<hub-ip>:...` → the hub seat is
   `alpha`. (Address-based discovery is a convenience and can mis-read if a mailbox
   ever advertises a loopback address — hub-local workers legitimately show
   `127.0.0.1`. The route proof below is the AUTHORITY; discovery only nominates the
   name to prove.)
   CHECK — prove the route (sends nothing):

       obt.net.py route <hub-seat>

   prints `class=coord-master`, node `coord-<hub-seat>`, live. Any other class → the
   failure table, do not proceed.
   If the roster does not make it obvious, ASK THE OWNER for the hub seat name — never
   guess one; a wrong name fails at step 6 with the "registers neither" error.

6. **Reply-path proof** (the ONLY sanctioned reply path — the bus, never ssh):

       obt.net.py msg send <hub-seat> --subject bringup --body-file <small-md-noting-seat-and-date>

   CHECK: `[obtnet] ok` verdict. LAW: no `--controller` on msg verbs, no `coord-*`
   target names, no ssh to the hub — if any of those feel necessary, config is wrong;
   fix via the table below, never work around.

7. **Arm the inbox monitor** — a persistent background watcher on the metadata stream:

       obt.net.py msg watch

   (obt-pure; until that verb ships in ork.build, the interim is a harness
   monitor wrapping `tail -n0 -f ~/coordination/inbox.stream` — accept its
   one-time permission prompt or pre-allow it.) Each line is
   `<utc-ts>  <from-seat>  <subject>  <path-to-body>` (tab-separated; the body
   file always exists before its line does). Read bodies selectively;
   `obt.net.py msg ack <ts>` when handled. Re-arm after every processing
   round; a periodic `obt.net.py msg list` covers a dead watcher.

8. **Await the hub's ack/reply.** The seat is UP when the round-trip has been seen in
   BOTH directions — not before. Then work from briefs in `~/coordination/inbox/` and
   owner instruction only.

## Failures

| symptom | cause | fix |
|---|---|---|
| `route` is not a verb / `msg send <hub>` errors "unknown target ... known live nodes=[], sshhosts=[]" | deployed obt predates jul27 class routing | step 0's stop — deliver new ork.build to this seat's venv, then restart controller + mailbox node (they import at start) and rerun from step 0 |
| `route <hub-seat>` prints `class=ssh` | `obtnet.json` lacks `master`, or the hub seat is listed in your `sshhosts` | rerun step 1; remove the hub from `sshhosts` (older `init` does not evict it) |
| `... registers neither <target> nor coord-<target> ...` | EITHER you addressed the wrong hub SEAT name, OR the hub's mailbox node is down | re-check the seat name from the step-5 roster first; if it is right, tell the owner/hub — the fix is hub-side (its bringup step 3), nothing to change here |
| "AMBIGUOUS target" RouteError | a local worker node or sshhost shares the hub's name | rename per the error text — routing refuses to guess by design |
| step-3 check: `coord-<seat>` absent from hub roster | wrong `<hub-ip>`, or hub controller not up yet | confirm the address with the owner; hub goes first |
| `msg send` link error naming YOUR coord node | your step-3 node died | restart it (your plumbing, no permission needed) |
| hub reports its sends to you REFUSED: `exec_failed ... obt.net.msg.deposit.py` | your mailbox node's PATH lacks the venv bin (step-3 PATH LAW) — typically a post-outage relaunch from a bare login shell | relaunch YOUR mailbox node from the OBT shell, verify PATH in the node's environ, send a fresh ping |

Role doc: `.claude/agents/sub-coordinator.md`. Bus/routing reference:
`.claude/skills/hub-coordinator/COORDINATION_OPS.md`. Hub-side procedure:
`.claude/skills/hub-coordinator/BRINGUP.md`.
