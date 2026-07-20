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

## qt-app/quick/ conventions (LOAMS 2.0)

- **QML module target is `witsquickmodule`** (URI `LOAMS`), NOT `witsquick` — the
  latter collides on case-insensitive NTFS with the `WITSQuick` executable's
  autogen dir. Runtime `loadFromModule` consumers link `witsquickmoduleplugin`
  + `witsquickmoduleplugin_init`.
- **File naming:** QML types and C++ ViewModel/model classes are `PascalCase`
  (`KioskViewModel.h`, `BrandPanel.qml`); C++ member variables are `m_camelCase`.
- **MVVM:** ViewModels (`quick/viewmodels/`) are the ONLY QML-facing C++; QML
  never calls a `witscore` controller directly. Screens receive their VM as a
  `property var vm` so QuickTests can inject a plain-QML stub.
- **Theming:** `Theme.qml` (pragma Singleton) is the single source of every
  visual token. ZERO raw hex outside `Theme.qml`; opacity variants use
  `Qt.alpha(Theme.<token>, a)`, never a literal color.
- **Tests:** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`);
  add `OFFSCREEN` for any GUI/Quick/painting test.

## Build & run

- Configure: `cmake -S qt-app -B qt-app/build`
- Build: `cmake --build qt-app/build`
- Tests (once a Qt Test target exists): `ctest --test-dir qt-app/build --output-on-failure`
- It's a GUI app — a clean build is necessary but not sufficient; run the `WITS` executable to verify behavior.

## How the pieces fit together

- **Rules** (`.claude/rules/`) — the binding policy. `workflow.md` governs how work flows (orchestration discipline → TDD → review → finish). `security-hygiene.md` governs what may be committed (no secrets, no admin keys, no real student PII).
- **Skills** (`.claude/skills/`) — `commit`, `codex-review`, `claude-review`, `create-pr`. Invoke via the Skill tool / slash commands.
- **Agents** (`.claude/agents/`) — `dry-checker`, `security-reviewer`, `general-code-reviewer`, all adapted for this Qt/C++ codebase. Dispatched in diff-scoped mode by `create-pr`, or manually for full-project sweeps.

## Skill precedence — project + superpowers ONLY

A user-level **kane-claude-standalone** install (`~/.claude/skills/`, `~/.claude/agents/`) ships
same-named, **web-stack-oriented** copies of several skills. They are NOT for this repo and have
shadowed the project copies before — a `/create-pr` run once dispatched a four-agent gate including
`api-checker`, which this project does not define.

**Binding resolution order for this repo:**

1. **Project skills** (`.claude/skills/`) always win for `commit`, `claude-review`, `codex-review`,
   `create-pr`. Before acting on one, confirm you loaded the project copy — the project `create-pr`
   dispatches exactly **three** agents (`dry-checker`, `security-reviewer`, `general-code-reviewer`).
   If a loaded skill names `api-checker` or a fourth agent, you loaded the standalone copy: **stop and
   re-read `.claude/skills/create-pr/SKILL.md` directly**, and follow that instead.
2. **superpowers** provides everything the project does not define — `brainstorming`,
   `systematic-debugging`, `writing-plans`, `using-git-worktrees`, `subagent-driven-development`,
   `finishing-a-development-branch`. Prefer the `superpowers:`-prefixed name so resolution is unambiguous.
3. **Never** use the standalone `~/.claude/skills/` copies of `debug`, `worktrees`, `subagent-build`,
   `writing-plans`, or `brainstorming` here — they assume Next.js/React/TypeScript conventions that do
   not apply to a Qt 6 / C++17 / CMake codebase.

`merge-pr` exists **only** at user level and has no project or superpowers equivalent; it is generic
git plumbing, so it is the one standalone skill that is safe to use as-is.

Agents: dispatch only the three defined in `.claude/agents/`. There is deliberately no `api-checker`
here — the client↔PHP contract is checked by the `general-code-reviewer` and `security-reviewer`.
