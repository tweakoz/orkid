# Daily bringup — HUB seat (plumbing + fleet, one procedure)

Trigger: the owner says **"start up as a hub-coord"** or **"bring up the fleet"**
(synonyms: "restore the fleet", "restore <seat>" — restore and bringup are the SAME
procedure, owner ruling jul30). That sentence is the entire input. Run every step
below immediately, in order, without asking permission for any of them. YOU start
everything: hub plumbing, sub-coordinator claude sessions (screen, over ssh), and
worker nodes — from the active taskset's FLEET.json. Nothing in the fleet is
owner-started by hand. Every step is idempotent: "already running/registered" is a
PASS, not a reason to restart anything.

Context: run from this seat's OBT shell. `obt.*` tools are invoked BARE (on PATH).
`<seat>` below = this seat's name (its `coordid`); the canonical hub port is 7461.

## Part 1 — hub plumbing

0. **Version gate**: `obt.net.py route --help` must print usage. `invalid choice:
   'route'` = this venv predates jul27 class routing — STOP and report; the bus
   doctrine cannot run on it. OBT-PURE LAW (owner, jul29): every bringup command
   is an `obt.*`/`ork.*` invocation — the logs scaffold is `init`'s job, never a
   hand-mkdir.

1. **Seat wiring** (config + scaffold, idempotent):

       obt.coord.seat.py init --seat <seat> --role master

   (The tool still spells the hub role `master` — historical flag name; the role is HUB.)
   A hub declares NO master address — if `~/.obt-global/obtnet.json` has a `master` key
   on this seat, a previous mis-setup left it; remove it.
   CHECK: verdict line starts `[obtcoord] ok`.
   If this seat has no name yet, ask the owner for one — do not invent it.

2. **Controller**:

       obt.net.py list

   - Answers (any roster, even empty) → controller already up. Use it. Do NOT start a
     second one.
   - Times out → start it, backgrounded, from the OBT shell:

         obt.net.controller.py

   CHECK: `obt.net.py list` answers. Nodes retry and re-latch on their own (observed
   2026-07-29/30: waiting nodes registered within seconds of controller start) — a
   controller restart never requires touching any node.

3. **Mailbox node** (the hub's inbox endpoint; every sub→hub message is physically
   written by THIS process; without it every sub's send fails with "registers neither
   <seat> nor coord-<seat>"):

   If `obt.net.py list` does not show `coord-<seat>`:

       obt.net.node.py --controller tcp://127.0.0.1:7461 --name coord-<seat> --restrict msg,sync

   (backgrounded, FROM THE OBT SHELL — see PATH LAW below; supervision via
   launchd/systemd is optional permanence, not required)
   PATH LAW (field-proven jul30, audio seat): a mailbox node EXECS
   `obt.net.msg.deposit.py` BY NAME from its own PATH when mail arrives. Launching
   the node binary by absolute path from a default-login-PATH shell yields a node
   that registers, sends, but REFUSES every deposit (`exec_failed`). The node's env
   must carry the venv bin (and/or `~/coordination/bin`) on PATH.
   CHECK: `obt.net.py list` shows `coord-<seat>`.

4. **Validate the seat**:

       obt.coord.seat.py check

   CHECK: final line `[obtcoord] ok check seat<...> ... fail=0`.

5. **Arm the inbox monitor** (this is how you notice a sub report at all — nothing
   pushes to you): a persistent background watcher on the metadata stream.

       obt.net.py msg watch
       (until that verb ships: a harness monitor wrapping
        tail -n0 -f ~/coordination/inbox.stream — one-time prompt acceptable
        on the HUB, whose owner is present)

   Each line is `<utc-ts>  <from-seat>  <subject>  <path-to-body>` (tab-separated,
   body written and fsynced BEFORE its line appears). Read bodies from
   `~/coordination/inbox/` selectively, and `obt.net.py msg ack <ts>` when handled.
   Re-arm after every processing round; a periodic `obt.net.py msg list` (local read,
   no network) is the coarse fallback if the watcher dies.

6. **Load the work**: read `~/coordination/ACTIVE_TASKSET` and load that taskset folder —
   validate it (dangling blockers, seat/anchor fit), reconcile any in-flight lanes,
   regenerate BOARD and AWAITING_OWNER per `TASKSET_FORMAT.md`. If the pointer is missing
   or empty, ASK THE OWNER which taskset — never guess one. The taskset's FLEET.json
   (schema: `fleet.schema.json` beside this file) is Part 2's source of truth.

## Part 2 — fleet bringup (seats + workers, from FLEET.json)

Hub plumbing first, then seats in FLEET.json order; one dead seat never blocks the
rest. Workers last per seat. `ork.coord.restore.py` supersedes this hand procedure
when built.

Per enabled seat, in order:

0. PROBE: `ssh <host> true` — down box = SKIP seat with reason, move on, return later.
1. PREP (first bringup per box only): `~/.local/bin/claude` exists.
2. REPO: verify base only (`git -C <repo.path> log --oneline -1`). NEVER force-align:
   lanes may be checked out; gitsync refuses divergence by design and the seat's own
   bringup reports staleness to the hub.
3. SEAT SESSION (this starts everything seat-owned — its bringup brings up its own
   controller + mailbox):

       # config_dir ABSENT (default profile + existing login):
       ssh <host> "screen -dmS <claude.screen_session> bash -lc 'cd <claude.cwd> && env -u CLAUDE_CONFIG_DIR ~/.local/bin/claude \"<claude.startup_prompt>\"'"
       # config_dir PRESENT (pinned account profile):
       ssh <host> "screen -dmS <claude.screen_session> bash -lc 'cd <claude.cwd> && CLAUDE_CONFIG_DIR=<claude.config_dir> ~/.local/bin/claude \"<claude.startup_prompt>\"'"

   Any `env.extra_env` entries (e.g. OBT_COORD_HOME for same-box seats) go into the
   launch env too. Same-box seats: no ssh, same line locally.
   CHECK: `ssh <host> screen -ls` shows the session. Owner attaches any time:
   `ssh <host> -t screen -r <session>`. MAC NOTE (owner fix jul29): macOS's bundled
   screen mangles truecolor — set the claude THEME to ANSI colors for sessions living
   under mac screen.
4. WORKERS (`start` not false only): PREFER the seat's launcher scripts when they
   exist (`~/.obt-coord/<seat>/bin/launch-*.sh` — no quoting layers, the jul29
   lesson): `screen -dmS <screen_session> <script>`. Otherwise the canonical line:

       ssh <worker.host> "screen -dmS <screen_session> bash -c \"env -i HOME=\$HOME USER=\$USER TERM=xterm PATH=<venv>/bin:/usr/local/bin:/usr/bin:/bin VIRTUAL_ENV=<venv> <venv>/bin/obt.env.launch.py --stagedir <staging> [--project <worker.repo>] --command '<venv>/bin/obt.net.node.py --name <name>' > <log> 2>&1\""

   `--project <worker.repo>` whenever the worker declares a repo — a repo-less live
   worker is a COLLISION RISK to resolve, not a default (jul30 audit: undeclared
   workers drift toward the seat's own checkout). CWD LAW: `cd` into the worker's
   repo (else $HOME) inside the launch — an inherited cwd sends cwd-relative job
   output into whatever checkout the launcher happened to sit in. `<log>` =
   `~/.obt-coord/<seat>/coordination/logs/<screen_session>.log` where that scaffold
   exists, else `$HOME/obtnet-<name>.log`. TWO LAWS in that line, field-proven
   2026-07-29:
   (a) env -i scrub with VIRTUAL_ENV set — obt.env.launch requires it;
   (b) the node script by ABSOLUTE VENV PATH inside --command, never bare —
       the staging-prepended PATH shadows bare names into the staging pyvenv
       (free-threaded 3.14t), whose interpreter cannot load the venv's
       C extensions (zmq dies at import). General rule: any obt entry point
       with C-ext deps is invoked by absolute venv path under staging PATH.
   Workers register to their OWNING seat's controller; nodes retry, so starting
   before the seat's controller is up is fine. Hub-owned workers on localhost: same
   line, no ssh; hub-owned workers on remote boxes add
   `--controller <hub controller_ext_addr>`; a SEAT's workers omit `--controller`
   (the box-local default resolves to the seat controller per the config contract).
   Render/windowed/DRM needs are declared by SPECIFIC TESTS/GATES, never fleet data
   (owner jul29) — offscreen always works. STAGING DISJOINTNESS LAW: a worker never
   shares a staging with a coordinator session on its box.
5. VERIFY (always): `coord-<seat>` appears on the hub roster; the seat's bringup ping
   arrives, you REPLY over the bus (`obt.net.py msg send <seat> ...`) and ack it. A
   seat is UP only when the round-trip has been seen in BOTH directions. Seat workers
   appear on the seat's roster — the seat's report says so; the hub does not query sub
   controllers for state it can get from reports.
6. RECORD: taskset/manifest seat state updated the moment each seat closes its
   round-trip.

Then report to the owner: one line per component + a fleet table, including the hub
address line `hub live: tcp://<LAN-IP>:7461`. If you cannot determine this machine's
LAN address, ask the owner — do not guess or use a hostname.

## Failures

| symptom | cause | fix |
|---|---|---|
| controller start dies on port bind | one is already running | that IS your controller; Part 1 step 2's check should have caught it |
| a sub reports "registers neither ... nor coord-..." | your Part 1 step 3 mailbox node is absent/dead | rerun step 3; the error is hub-side, not the sub's |
| your `msg send <seat>` REFUSED: `exec_failed ... obt.net.msg.deposit.py` | THE SUB's mailbox node runs with a default-login PATH (venv bin absent) — it execs the deposit tool by name | sub-side: seat relaunches its mailbox node from its OBT shell (PATH LAW, step 3); until then the bus toward that seat is down — relay via its screen console |
| a sub reports its route to hub is `class=ssh` | THE SUB's config lacks `master` (or lists the hub in its `sshhosts`) | sub reruns ITS bringup step 1 — nothing to fix here |
| `obt.coord.seat.py check` FAIL line | the named check tells you exactly what | fix that item, rerun step 4 |
| `coord-<sub>` never appears | sub given wrong address / sub-side controller typo / seat session died in screen | `ssh <host> screen -ls`; attach to read; re-launch per Part 2 step 3 |
| a seat's roster shows a worker FLEET.json doesn't start (e.g. start=false box) | a leftover node daemon on that box re-latched — nodes retry forever | verify provenance via the seat (read-only), leave untasked, record; stopping/repointing happens on that box, owner word |
| two node processes per worker name after a relaunch | `screen -X quit` ORPHANS children on macOS (they reparent under login and live on) — this is how stale twins are born | stopping a worker = kill its PID TREE (node + obt.env.launch + shell parents, PIDs you own), THEN quit the screen; verify by ps start-times + fresh roster ports |

Role doc: `.claude/agents/hub-coordinator.md`. Bus/routing reference:
`COORDINATION_OPS.md` beside this file. Sub-side procedure:
`.claude/skills/sub-coordinator/BRINGUP.md`.
