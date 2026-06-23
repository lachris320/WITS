# Workflow

How work flows through this project — **WITS**, a Qt 6 / C++17 desktop app built with CMake — from a task, to implementation, to review, to a merged branch. Read **section 0 first** — it governs everything else.

## 0. Orchestration Discipline (Read This First)

> **Who this section applies to — read before anything else.**
>
> This entire section governs the **top-level (main) agent only** — the one talking directly to the user.
>
> **If you were dispatched as a subagent, this section does NOT apply to you. You are a terminal worker (an IC). Do the work you were assigned directly — write the function bodies, edit the files, run the build/tests yourself. You MUST NOT dispatch further subagents to do your assigned work.** The only depth allowed is one level: main agent → worker. A worker that dispatches another worker creates an infinite nesting chain that wastes time and context. If your task feels too big, do it anyway or report back to the orchestrator — never re-delegate.

**If you find yourself about to write a function body, stop and dispatch a subagent.**

This rule applies to any task that has **3+ steps OR touches 2+ files OR needs review gates**. For those tasks, the main agent is an **orchestrator, not an IC** — its job is classification, dispatch, synthesis, and review.

### The main agent MAY do inline

- Read files to gather context for a subagent prompt
- Run non-destructive bash (`git status`, `ls`, `grep`, and building/running tests for verification)
- Edit a single line in a single file for a trivial fix (e.g. a typo) — **never** multi-line code or function bodies
- Invoke the Skill tool for slash commands (e.g. `/codex-review`)
- Commit (always via the `commit` skill — see section 5), push, create PRs

### The main agent MUST delegate to a subagent

- Writing any function body, slot, dialog, or widget implementation
- Writing or modifying tests
- Editing multiple files as part of one logical change
- Large doc rewrites (> 20 lines of new prose)
- Any TDD cycle (red → green → refactor)

## 1. Multi-Task Implementation (Subagent-Driven Development)

Break feature work into independent tasks and dispatch them to subagents. Each subagent owns one task end-to-end (TDD cycle included). The main agent classifies, dispatches, and synthesizes results — it does not write the implementation itself.

## 2. TDD Workflow (All Feature Work)

All feature work follows test-driven development: **red → green → refactor**, run inside a subagent.

> **Note on the current state of the harness.** As of writing, the repo has no automated test target — `qt-app/CMakeLists.txt` builds only the `WITS` executable. Standing up a Qt Test target is the first prerequisite for real TDD here; until it exists, treat "add the test target" as the red step of the first feature that needs it, and don't claim TDD coverage that doesn't exist.

### Test Types & tooling

**Unit tests** — Qt Test (`QtTest`) cases that cover **pure logic extracted from the UI**: date math, report aggregation, palette selection, RFID-payload parsing/validation, JSON-response decoding. The reason the general-code-reviewer pushes logic *out* of the `QMainWindow`/`QDialog` classes is precisely so it can be unit-tested without spinning up widgets.

- Add a test target in CMake (e.g. `qt_add_executable(wits_tests ...)` linking `Qt::Test`, or a `tests/` subdirectory) and register it with `add_test(...)` so it runs under **ctest**.
- Don't hit the real network in unit tests. Isolate logic from `QNetworkAccessManager`; where a test must exercise request/response handling, feed it a captured/synthetic `QByteArray` payload rather than a live `http://localhost` call.

**Widget / integration tests** — Qt Test with `QTest::mouseClick` / `QSignalSpy` to drive a real widget and assert on emitted signals and resulting state. Use these for flows that genuinely depend on the widget tree (e.g. an admin action emitting `schoolInfoUpdated`).

### Running tests

From the build directory: `ctest --output-on-failure` (configure once with `cmake -S qt-app -B qt-app/build` then `cmake --build qt-app/build`). If no test target exists yet, say so plainly — don't report a passing suite that isn't there.

## 3. Codex Code Review (After Implementation)

After all tasks are implemented and the build + tests are green, run `/codex-review` **before finishing the branch**.

Codex reviews for correctness, security, performance, and maintainability — a second opinion from a different model family. The review runs in rounds until **APPROVE or 3 rounds max**.

### Process

1. Invoke `/codex-review` via the **Skill tool** (it is a slash command). This is the sanctioned way to run Codex review on this project. (For a same-family fresh-context review instead — or in addition, for high-stakes merges — use `/claude-review`.)
   - Invocation: use the Skill tool with `skill: "codex-review"`.
2. Fix **Critical** and **Important** findings.
3. Re-submit until **APPROVE**.
4. Then proceed to QA / manual verification of the app.

**Do not skip review on any non-trivial implementation.**

## 4. Finish Work

When all tasks are done, the build passes, tests are green, and Codex has approved:

- Verify the build: configure + `cmake --build qt-app/build` completes with no new warnings/errors. Smoke-test the `WITS` executable for the feature you touched (this is a desktop GUI app — a clean build is necessary but not sufficient; actually run it).
- Use `superpowers:finishing-a-development-branch`.
- Use the project-scoped `create-pr` skill to open the PR (only meaningful once a remote is configured — see the skill).

## 5. Committing

**Always commit via the `commit` skill** — never run raw `git add` / `git commit` by hand. The skill groups changes by concern into Conventional Commits with bodies that explain the *why*, and waits for approval before writing history. This applies everywhere commits happen: mid-implementation checkpoints, fix-up commits during the review gate, and the final commit before `create-pr`.

Never commit `qt-app/build/`, Qt Creator `*.user` files, or `moc_*`/`ui_*` generated output — they're covered by `.gitignore`; if any slip through, fix `.gitignore` rather than committing them.
