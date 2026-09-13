---
title: Evolution
type: log
project: Desktop gadget
---

# Evolution — lessons for the harness skill

One line per lesson, dated. What slowed you down, what a playbook got wrong,
what a template lacked, what an agent kept needing that its charter did not
give it. `harness lessons collect` moves these into the skill's library,
where the evolve phase acts on them.

- 2026-09-13 — harness initialised with skill v0.3.0.
- 2026-09-13 — `harness init` reports "created CLAUDE.md" and "created AGENTS.md" when it only appended the pointer block to files that already existed; the word should be "pointer added". Cosmetic, but it made me check the files were not overwritten.
- 2026-09-13 — Running the harness from a session whose working directory is the skill repository means every command needs `--project "<path>"`; the playbooks assume the project is the working directory. Either say so in SKILL.md or have the script remember the project after `init`.
- 2026-09-13 — The graph had a build date older than the last commit; the understand playbook says "run with --update" but not how to judge when an update is worth its cost. A line about comparing the report date with the last commit would have saved the check.
- 2026-09-13 — The librarian paused mid-task to ask whether to spend a subagent on graph extraction, which a subagent cannot ask; the pause surfaced as a completion notice and cost a round trip. The librarian charter should say: decide within scope, or return `ESCALATE:` with the question, never wait.
- 2026-09-13 — `graphify --update` on a documentation-heavy change dispatches an LLM extraction pass (here fifteen changed docs, one of them the 147 KB log). The understand playbook should warn that an update is not free and let the conductor bound it, for example by naming the files worth re-extracting.
