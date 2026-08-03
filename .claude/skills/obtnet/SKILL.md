---
name: obtnet
description: Drive the obtnet fleet (distributed build/run/sync across machines) via obt.net.py — node routing, async jobs, artifact fetch, git-free tree sync. Use whenever work should run on another machine or files must move between machines without git.
---

# obtnet — one controller, many nodes

Every command prints ONE greppable verdict line last: `[obtnet] ok|FAIL <verb> <node> ...`.
**Trust it.** Do not fetch or dump logs on success. `--json` on any verb for tool parsing.

Controller resolution: `--controller` > `$OBTNET_CONTROLLER` > `~/.obt-global/obtnet.json`
(set once with `obt.net.py config tcp://HOST`) > localhost. Bare `HOST` or `HOST:PORT` are
accepted anywhere an address is (port defaults to 7461).

## Node names and selectors
`obt.net.py list` shows the fleet. Any `<node>` argument accepts a literal name or a
`@` selector, routed to the least-busy live match:
`@any` · `@linux` / `@mac` · `@gpu` · `@gpu=5090` (capability substring) · `@5090`
(substring across name/os/arch/gpu) · comma-composed: `@linux,gpu`.
FAN-OUT on run/build/test/scene/sync/gitsync: `@each` (every live node), `@each:linux`
(every match), or `a,b` lists — execution verbs submit to ALL then wait (simultaneous
cross-machine builds/tests), working-copy verbs loop; per-node verdicts + a fleet summary.

## One happy path per task
```bash
obt.net.py list                                           # fleet + load + clock offsets
obt.net.py run <node> [--timeout S] [--cwd D] -- CMD...   # short sync command (<~60s)
obt.net.py submit <node> [--out GLOB] -- CMD...           # long work -> job id
obt.net.py build <node> [--cwd D]                         # ork.build.py (or -- CMD);
                                                          #   verdict: errors=N warnings=N
                                                          #   (error-grep beats lying rc0)
obt.net.py test <node> -- CMD...                          # verdict: passed=N failed=N
obt.net.py scene <node> [--out G] [--env K=V] -- CMD...   # OFFSCREEN forced unless
                                                          #   --windowed (owner consent);
                                                          #   cook/settle/fps lines surfaced
obt.net.py wait <node> j-XXXX                             # verdict; auto stderr-tail on fail
obt.net.py log <node> j-XXXX [--stderr --tail N --grep P] # bounded peek at a LIVE job
obt.net.py jobs                                           # fleet-wide job table
obt.net.py cancel <node> j-XXXX                           # SIGTERM the job's pgid
obt.net.py fetch <node> <sha256> <dest>                   # artifact by sha (from verdicts)
obt.net.py sync <node> <local_dir> <remote_dir>           # make remote == local (git-free)
obt.net.py sync <node> L R --pull                         # make local == remote
obt.net.py gitsync <node> <local_repo> <remote_repo>      # align git base (branch+HEAD)
                                                          #   via bundle, NO push; --tree
                                                          #   chains the uncommitted delta
                                                          #   LFS objects the node lacks
                                                          #   ride along (--no-lfs off,
                                                          #   --lfs-max-mb caps a seed)
obt.net.py diff <node> <local_dir> <remote_dir>           # +/-/M/L classified; rc0=equal
obt.net.py watch [--node N] [--grep P]                    # live fleet event tail (ctrl-c)
```
Nodes run inside OBT shells, so staged tools (`ork.build.py`, `obt.dep.*`, `ork.cpp.db.*`)
work directly: `obt.net.py run @linux,gpu -- ork.cpp.db.search.py MySymbol --porcelain`.

## Coordination traffic (seat mail — the fleet's other half)

The same controller carries COORDINATOR MAIL between seats. Targets are SEAT names
(never `coord-*` node names), and the route is derived from config — msg verbs take no
`--controller`.

```bash
obt.net.py route <seat>                                   # derived class + liveness; sends nothing
obt.net.py msg send <seat|@coords> --subject S --body-file F
obt.net.py msg list                                       # reads YOUR local ~/coordination/inbox/
obt.net.py msg ack <ts-or-path>                           # moves it to inbox/acked/ (audit trail)
obt.net.py push <seat> <local> <remote>                   # order flips between the two:
obt.net.py pull <seat> <remote> <local>                   #   push=local first, pull=remote first
```

Routing classes, in decision order (`route` prints the one it picked):
`coord-seat` (a `coord-<target>` node on YOUR controller → hub-to-sub) · `work-node` (a
worker on your controller — REFUSED for msg: workers execute, they are not mail
endpoints) · `coord-master` (your config's `master` key → sub-to-hub) · `ssh` (a declared
`sshhosts` entry — a first-class file route, NEVER a fallback for a down bus link).
Collisions raise a hard RouteError; routing never guesses.

Config in `~/.obt-global/obtnet.json`: `coordid` (every seat) · `master` (subs only — the
hub's address; `controller` always stays local) · `sshhosts` (hub only).
`@coords` resolves on YOUR OWN controller: every live `coord-*` there plus your `sshhosts`
entries. On a hub that is the sub seats (and its own mailbox); on a sub it does NOT
include the hub — the hub is reached by seat name.

Payloads are documents, not chat. Bringing a seat up is not this skill:
`.claude/skills/hub-coordinator/BRINGUP.md` / `.claude/skills/sub-coordinator/BRINGUP.md`;
architecture and the full config contract in
`.claude/skills/hub-coordinator/COORDINATION_OPS.md`.

## Rules
- Long or unbounded work: ALWAYS `submit` (never `run`) and give an honest `--timeout`.
- BEFORE modifying/building a node's checkout: `gitsync` it (aligned bases = diffs/patches
  line up; it fails loudly on divergence or tracked dirt), then `--tree`/`sync` for the
  uncommitted delta. Never assume the node is current.
- `sync` dry-runs first when the target matters: `--dry` (counts only); `--delete` is opt-in.
  Verdict `tree=MATCH` is cryptographic proof both sides are equal — nothing else to check.
- Kill only YOUR jobs, by job id. Check `jobs` before heavy work on a busy node.
- Anything that opens a window on a node needs the owner's consent first.
- Logs stay remote. On failure the verdict + auto-tail is usually enough; escalate with
  bounded `log --grep`, never full dumps.
- `@each`/`@any` SEE MAILBOX NODES: `coord-*` nodes are live registrations and selectors
  do not skip them. `sync`/`gitsync` fan-outs are harmless there (content verbs only), but
  `build`/`test`/`scene`/`run` with `@each` earns a `restricted node ... refuses` reply
  from every mailbox node, and one refusal flips the whole fan-out's exit code to failure.
  Name worker nodes explicitly (or use a narrower selector) for execution verbs.
- Mail is not work: never send a job to a `coord-*` node, never address mail to a worker.
  Both are refused by design, not by accident.
