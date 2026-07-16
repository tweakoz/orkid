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
obt.net.py diff <node> <local_dir> <remote_dir>           # +/-/M/L classified; rc0=equal
obt.net.py watch [--node N] [--grep P]                    # live fleet event tail (ctrl-c)
```
Nodes run inside OBT shells, so staged tools (`ork.build.py`, `obt.dep.*`, `ork.cpp.db.*`)
work directly: `obt.net.py run @linux,gpu -- ork.cpp.db.search.py MySymbol --porcelain`.

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
