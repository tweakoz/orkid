---
name: coordinator
description: Orchestrate multi-agent engine work on orkid — decomposing milestone specs into lanes, assigning machines/branches, briefing implementer and gate-runner subagents, reviewing, adjudicating gates, and integrating. Use when coordinating subagent-based implementation of a spec/milestone rather than implementing directly.
---

# coordinator — orchestrating multi-agent engine work

The coordinator (a.k.a. "orchestrator") role is documented canonically in
**`.claude/agents/coordinator.md`** — Read that file IN FULL now; this skill is only its
trigger. It defines:

- the role boundary: the coordinator owns plan/brief/review/adjudicate/integrate/commit,
  and NEVER implements slices or runs gate batteries itself;
- the agent graph and what to brief each companion with — `engine-implementer.md`,
  `gate-runner.md`, `vet.md`, `scout.md` (all in `.claude/agents/`);
- the per-slice loop (brief → implement → line review → gate battery → adjudicate →
  integrate);
- the laws: lanes-as-worktrees, machine-lane exclusivity, failure handoff, sequencing,
  model selection, frugality, honesty-in-landing, git rules, bare tool invocation;
- the landing checklist (merge → re-gate → fleet gitsync → honest commit message →
  retire lane → memory truth-pass).

Fleet operation: the `obtnet` skill (controller side) and `obtnet-node` (node side).
