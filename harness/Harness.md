---
title: Harness
type: index
project: Desktop gadget
skill_version: 0.3.0
created: 2026-09-13
updated: 2026-09-13
gardener_last_run:
librarian_last_run:
---

# Harness — Desktop gadget

This folder is the project's agent harness, managed by the `harness` skill
(v0.3.0). Start a session with `/harness`; it reads this folder and
knows where it left off.

| Note | What it is |
|---|---|
| [[brief]] | what the project is: graph, wiki and interview |
| [[architecture]] | the roster, routing, escalation, background agents |
| `agents/` | one note per agent: charter link, project context, project memory |
| `decisions/` | H-numbered decision records, including automated merges |
| [[ledger]] | every delegated task, its outcome and cost |
| [[evolution]] | lessons for the skill itself |
| `wiki/` | the project's llm-wiki; schema in `wiki/SCHEMA.md` |

Compiled agents live in each tool's own folder at the project root
(`.claude/agents/`, `.codex/agents/`, `.gemini/agents/`, `.cursor/agents/`,
`.opencode/agents/`) and as generic prompts in `compiled/`. They are
generated; edit the notes here and run `harness compile`.

## Decisions

| Decision | Claim |
|---|---|
