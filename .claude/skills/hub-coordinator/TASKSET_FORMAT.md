# TASKSET FORMAT v1 (canonical — owner + hub contract)

A taskset is ONE FOLDER of PERSISTED, HUMAN-OWNED planning state. It lives
WHEREVER THE OWNER PUTS IT (any path, any repo the owner git-tracks) and is
switchable by path ("hub, use taskset ~/TODAYSTASKS"). Scope: typically a
large multi-week set of workstreams — epics whose tasks may take multiple
coordinators more than a day; never date-batched. ~/coordination/ is RUNTIME
working state only (message inbox, bus plumbing, the ACTIVE_TASKSET pointer)
— persisted planning state never lives there.

The taskset records TASKS, not everything that happened (owner, 2026-07-30):
work executed from direct owner instruction (fleet bringups, restores, ops)
gets NO retroactive task file — its evidence lives where it landed (fleet
manifest, commits, filed queue items). Never fabricate records for history.

    <taskset>/
      TASKSET.md        title, intent, created — the cover page
      ROSTER.md         RETIRED 2026-07-31 — FLEET.json (machine truth) +
                        each epic's seats: frontmatter supersede it; do not
                        create one in new tasksets.
      epics/<slug>.md   one file per epic
      tasks/<epic-chain>/<slug>.md   BACKLOG — legacy flat filing; epics migrate
                        to planned wave folders (see WAVES).
                        Folders NEST along epic parent chains
                        (program/child/...), so the tree mirrors the hierarchy
                        (generic->specific; filenames stay short — the path
                        carries the epic; task ids remain globally unique)
      tasks/<epic-chain>/wave<K>/s<N>-<slug>.md
                        DISPATCHED — one hub brief = one wave; slices numbered
                        in the brief
      tasks/<epic-chain>/wave<K>/completed/s<N>-<slug>.md
                        slice DONE while the wave is still open — completed/
                        nests INSIDE the wave it belongs to
      completed/<epic-chain>/wave<K>/s<N>-<slug>.md
                        WAVE done — the whole folder moved and the inner
                        completed/ FLATTENED away
      BOARD.md          GENERATED rollup (hub-owned; human edits get regenerated away)
      AWAITING_OWNER.md GENERATED decision queue
      decisions/        owner rulings, one file each (audit trail)
      issues/           discovered problems, one file each — frontmatter:
                        id, seat, task (link), severity, status (open|fixed|parked)
      deviations/       departures from plan/spec/expectation, one file each —
                        what was planned vs what happened vs why; owner-visible
      notes/            working/lane notes, nested by epic chain — durable
                        knowledge that is neither a task nor a decision
      completed/        DONE work, mirroring the same <epic>/ hierarchy. It
                        receives WHOLE UNITS ONLY: an unwaved task file, or an
                        entire wave<K>/ folder once its last slice lands —
                        never a loose slice out of a live wave. The wave's
                        inner completed/ flattens on that move. Each file
                        carries status: done + completion date + one-line
                        evidence. tasks/ holds only work whose WAVE is still
                        open — a landed slice of a live wave stays under
                        tasks/…/wave<K>/completed/. Git history preserves the
                        moves

Epic frontmatter:  id, title, priority (P1..), status (open|active|done|parked),
                   seats [..], interacts [epic-ids],
                   parent [epic-id, optional — epics nest: program -> workstreams],
                   requires [epic-ids, optional — hard prerequisites]
                   — body: outcome, notes.
Task frontmatter:  id, epic, seat, status (todo|active|blocked|review|done|parked),
                   blockers [task-ids], gates (the <=6min proof list)
                   — body: what / done-when / evidence.

CONCISION LAW (owner, 2026-07-29 — binds EVERY file and every write):
- A file speaks ONLY about its own subject: no cross-lane commentary, no
  fleet/seat/machine state outside the generated board, no restated
  fleet laws (a bare pointer at most).
- Repeated text lives ONCE at the highest owning scope: parking noted on the
  epic (never per task), shared contracts in the epic; tasks point, never copy.
- Task body: WHAT (1-3 lines) / DONE-WHEN / load-bearing TRAPS only.
- Completed: date + <=4-line evidence + landing shas. The taskset is
  git-tracked — compressed history stays recoverable — so cut hard.
- Epics: outcome + bar + one-line rung index + pending rulings + doc pointers.
- The test: a human AND an llm can digest any file in one read and find any
  fact from filename + frontmatter alone.

WAVES (dispatch batches — owner directive + hub ratification, 2026-07-30):
- A WAVE is ONE hub brief to ONE seat: a batch of slices cut and numbered at
  briefing time. Only the HUB creates waves, only at brief time. A seat never
  invents a wave or a slice number — it quotes the codes its brief assigned.
- NAMING LAW: report code W<K>-S<N>[<x>] == path
  <epic-chain>/wave<K>/s<N>[<x>]-<slug>.md, exactly. Numbers are assigned once,
  never renumbered, never reused. A letter suffix ANCHORS new work to an
  existing number so nothing ever renumbers (owner jul30): a sibling split of
  an already-briefed slice (S2 -> S2/S2b), or owner/implementor work INSERTED
  mid-wave (insertion after S2 = S2b, next S2c...; the roster diff declares
  it). Merge LANES are not slices and get no S-code — they keep their lane
  names. A dropped slice is RETIRED as a roster entry ("s3 RETIRED") with no
  file.
- Milestone ids (SF2, A1, ...) stay in a slug ONLY when removing them leaves
  the name ambiguous (owner jul30: s3-a0-remainder keeps a0 because "remainder"
  alone says nothing; s2-sound-dsl drops a1 because "sound-dsl" is complete —
  the wave already carries the ladder order). Legal in prose and in `id:`
  fields always; NEVER in place of the wave/slice code. Cross-references cite
  the immutable `id:`, so slug refinements never break citations. A file
  carries no number until it is placed in a wave, planned or briefed.
- Wave folders are the ONE non-epic directory level and are always LEAF: they
  sit directly under their owning epic's folder in tasks/ and completed/, never
  contain a child epic, and never nest (a slice never becomes a folder).
  Numbering is PER EPIC and monotonic — two epics' wave1 are unrelated, so a
  report names the epic when its seat holds more than one.
- The WAVE IS THE ONLY SUBDIVISION PRIMITIVE. A task needing subtasks becomes a
  wave; there is no second folder kind and no per-task subfolder.
- Frontmatter: every file under a wave folder carries `slice: W<K>-S<N>`. The
  `id:` NEVER changes when a file moves into or with a wave, so citations by id
  survive every move.
- PLANNED WAVES (owner directive, 2026-07-30): the hub MAY pre-declare future
  waves. Their slices are numbered AT DECLARATION and never renumbered; the
  epic's roster line marks them `(planned)` until brief time flips it to
  `(active, brief <date>)`. Moving a slice between planned waves is a hub edit
  that diffs BOTH roster lines in ONE commit. BOARD shows a planned wave as
  `wave<K> (planned): 0/N`.
- WAVE LIFECYCLE: "Assignment happens at creation, is fluid until brief, and
  freezes at brief — the brief converts plan into commitment."
  CREATE: a new task goes straight into a planned wave (flat is not a legal
  state; unschedulable/owner-blocked work goes to the epic's LAST planned wave).
  PLAN: the hub groups by dependency + theme and numbers slices at declaration.
  UNTIL BRIEF: reassignment is cheap — one commit, both rosters; numbers travel
  with the slice, never reused. BRIEF: the wave ACTIVATES, its roster FREEZES —
  thereafter only a sibling split (s2 -> s2a/s2b) or RETIRING a number; a
  half-landed slice's remainder respawns as a NEW slice in a later wave.
  LAND: slice -> inner completed/; the last landing promotes the folder.
- MIGRATION: a flat <epic-chain>/<slug>.md is a not-yet-restructured remnant,
  not a legal resting state; an epic clears it when the hub waves it.
- NO SPANNING. A wave lives in exactly ONE place at a time. Four positions, one
  per state:
      planned      tasks/<chain>/wave<K>/s<N>-<slug>.md   roster: (planned)
      dispatched   tasks/<chain>/wave<K>/s<N>-<slug>.md   roster: (active, …)
      slice done   tasks/<chain>/wave<K>/completed/s<N>-<slug>.md
      wave done    completed/<chain>/wave<K>/s<N>-<slug>.md
  A finished slice moves DOWN into its own wave's completed/, never out to the
  top-level mirror. Only when the LAST slice lands does the whole wave<K>/
  folder move to completed/<chain>/wave<K>/, and the inner completed/ FLATTENS
  on that move. An inner completed/ therefore exists ONLY under tasks/ and only
  inside a wave<K>/ folder: done-ness is POSITIONAL here, and a path must never
  claim it twice.
- PROGRESS = declared roster vs tree. The roster is DECLARED on the epic's
  rung-index wave line (at declaration for a planned wave, at brief otherwise) (the same line that records retired numbers)
  and is the DENOMINATOR; the tree supplies the numerator (DONE =
  wave<K>/completed/*.md, OPEN = slice files at wave<K>/ level). A file absent
  from the roster is UNDECLARED; a roster entry with no file is UNBRIEFED; both
  are FLAGGED on the BOARD row, never silently counted. Growing a wave = editing
  the roster line, a reviewable one-line diff. The whole-folder move fires iff
  OPEN is empty AND DONE equals the roster minus retired numbers — nothing else
  authorises it.
- A WAVE CLOSES ON ITS OWN ROSTER. A slice whose home task belongs to ANOTHER
  epic gets a <=4-line POINTER stub under this wave while the home task stays
  put and records the remainder — that remainder never holds the wave open, the
  slice the brief cut is what closes. A slice born from an issue gets a slice
  file with `issue: <path>`; the issue itself stays in issues/. Every live slice
  code has exactly one file.
- issues/, decisions/, deviations/, notes/ do NOT mirror waves — only tasks/ and
  completed/. They point at a slice by id or path.
- BOARD lists an epic's live waves as ONE ROW PER WAVE, progress first:
  `wave2: 3/4 — s1 done · s2 done · s2b done · s3 active`. The denominator is
  the declared roster, never a file count; an UNDECLARED/UNBRIEFED mismatch
  PRINTS on the row. A wave that moved to completed/ drops off the live rows and
  is named once in the round's completed line.
- CONCISION: a wave folder holds slice files plus ONE optional completed/
  subfolder — nothing else. No WAVE.md, no README, no per-wave notes. Wave
  narrative AND the declared roster = one line in the epic's rung index; the
  per-slice rollup is GENERATED into BOARD.

Rules:
- ALL content folders (epics/tasks/completed/issues/deviations/decisions) NEST
  along epic parent chains — the directory tree mirrors the program hierarchy
  (a child epic's file sits under its parent's folder; a parent epic's own
  items sit at its folder level). Taskset-scope items sit at folder root.
  Filenames stay short: the path carries the context.
- HUMAN edits anything except BOARD/AWAITING_OWNER (or just tells the hub).
- HUB validates on load (dangling blockers, seat/anchor mismatches,
  cross-epic interactions), regenerates BOARD on every land/gate/decision,
  decomposes tasks to seats over the message bus.
- Slugs are stable ids; status flips are the workflow; git commit whenever.
- Switching tasksets mid-flight: hub reconciles in-flight lanes first,
  parks what doesn't carry over, then loads the new folder.
