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
