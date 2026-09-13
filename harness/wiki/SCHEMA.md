# Desktop gadget harness wiki — LLM wiki

This directory is an **LLM-maintained wiki** (the "llm-wiki" pattern) kept by
the project's agent harness. The project's own files are the primary sources;
`raw/` holds external material the owner hands over; `wiki/` holds pages the
LLM writes and maintains; this file (`SCHEMA.md`) is the schema, and the
`CLAUDE.md`, `AGENTS.md` or `GEMINI.md` stubs beside it only point here so
every tool finds it. The owner curates sources and asks questions; the
harness's librarian does the filing, cross-referencing and bookkeeping.

**Purpose:** compile what the project's code and documents say into pages an
agent can read in a minute, so that no agent re-derives the project from
scratch and the architect can see every domain at a glance.

## Layout

- The project tree above `harness/` — primary sources. Read-only from the
  wiki's point of view; link to files by their repository path.
- `raw/` — external sources the owner adds (clipped docs, standards, papers).
  Read-only. Images, if any, under `raw/assets/`.
- `wiki/sources/` — one summary page per ingested source, including the
  graphify report and each project document ingested.
- `wiki/domains/` — one page per domain from the knowledge graph: what lives
  there, its god nodes, its conventions, who works on it.
- `wiki/concepts/` — cross-cutting ideas, methods, recurring problems.
- `wiki/synthesis/` — overview, open questions, comparisons, and filed query
  answers.
- `wiki/index.md` — catalogue of every page, updated on every change.
- `wiki/log.md` — append-only; entries start `## [YYYY-MM-DD] <op> | <title>`.

## Conventions

- Wikilinks everywhere a page mentions another; link to not-yet-written pages
  freely. Bare `[[wikilinks]]` refer to wiki pages only; project files and raw
  files are linked by path.
- Frontmatter on every page: `type`, `tags`, `created`, `updated`. Bump
  `updated` on every touch.
- Every claim cites a page or a file path. Contradictions are recorded, never
  silently resolved; when the code and a document disagree, say which the
  owner declared authoritative in the brief.
- Pages are short. An agent reads a domain page before working there, so a
  page that takes longer than a minute defeats its purpose.

## Workflows

- **Ingest**: batch, with a one-paragraph report. Read source → takeaways →
  summary page → sweep the domain and concept pages → index → log.
- **Query**: index first, cite pages. File synthesis answers back
  automatically under `wiki/synthesis/`; index and log them too.
- **Lint**: once per session, by the librarian. Fix mechanical drift
  immediately; report judgment calls to the conductor.

## Decisions

- 2026-09-13 — wiki created by the harness skill v0.3.0 with these
  defaults; revise as the project shows what it needs.
