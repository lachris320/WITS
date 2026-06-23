# Project Instructions

**WITS** — a Qt 6 / C++17 desktop application (a library monitoring system) built with CMake. The app lives in `qt-app/`; it is a client that talks to a PHP/HTTP backend and reads RFID hardware. The rules below are **binding** — read and follow them.

## Rules

@.claude/rules/workflow.md

@.claude/rules/security-hygiene.md

## Project layout

- `qt-app/` — the Qt application (sources, `*.ui` forms, `CMakeLists.txt`).
- `qt-app/libs/QXlsx/` — vendored third-party library. **Do not modify or review it** as project code.
- `qt-app/build/` — generated build output (gitignored). Never edit or commit.
- `deliverables/` — project deliverables/docs.

## Build & run

- Configure: `cmake -S qt-app -B qt-app/build`
- Build: `cmake --build qt-app/build`
- Tests (once a Qt Test target exists): `ctest --test-dir qt-app/build --output-on-failure`
- It's a GUI app — a clean build is necessary but not sufficient; run the `WITS` executable to verify behavior.

## How the pieces fit together

- **Rules** (`.claude/rules/`) — the binding policy. `workflow.md` governs how work flows (orchestration discipline → TDD → review → finish). `security-hygiene.md` governs what may be committed (no secrets, no admin keys, no real student PII).
- **Skills** (`.claude/skills/`) — `commit`, `codex-review`, `claude-review`, `create-pr`. Invoke via the Skill tool / slash commands.
- **Agents** (`.claude/agents/`) — `dry-checker`, `security-reviewer`, `general-code-reviewer`, all adapted for this Qt/C++ codebase. Dispatched in diff-scoped mode by `create-pr`, or manually for full-project sweeps.
