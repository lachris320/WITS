---
name: create-pr
description: Use when the user wants to open a pull request — phrases like "/create-pr", "create the PR", "let's open the PR", "ship this branch", "raise a PR", or any time a feature branch is ready to be merged. Runs the project's pre-PR review gate (dispatches the dry-checker, security-reviewer, and general-code-reviewer in diff-scoped mode in parallel), drives fixes for Critical/Important findings, verifies the build and tests are green, then writes a structured PR body and opens the PR with `gh`. This is the final step of the project Workflow rule.
---

# Create PR

The last gate before a branch merges. By the time this runs, implementation is done and the build + tests are green — this skill makes sure the diff is **clean, secure, and convention-compliant** before it becomes a PR, then opens it.

## Why this skill exists

A PR is a request for someone else's time and trust. Opening one with duplicated logic, a security hole, or a convention violation wastes a reviewer's attention and erodes that trust. This skill front-loads the automated review the project already knows how to do, so the human reviewer sees a diff that has already passed the bar.

It is the implementation of step 4 ("Finish Work") of the [workflow rule](../../rules/workflow.md), and it follows the [orchestration discipline](../../rules/workflow.md) in section 0: the main agent **orchestrates** — it dispatches reviewers and fixers as subagents and never writes implementation code inline.

## Preconditions (verify before doing anything)

Per the workflow rule, a PR is only created when the work is actually finished. Confirm, in parallel via Bash:

- **A GitHub remote exists**: `git remote -v`. This repo may have been `git init`'d with no remote yet — if there is no remote, **stop and tell the user**: a PR needs a GitHub repo (`gh repo create` or add an `origin`). Everything up to Phase 4 (the review gate) still works without a remote; only the push + `gh pr create` require one.
- On a feature branch, not the default (`master`/`main`): `git branch --show-current`.
- Working tree is committed (or will be committed as part of this flow): `git status`.
- Build passes: `cmake --build qt-app/build` (configure first with `cmake -S qt-app -B qt-app/build` if needed).
- Tests are green: `ctest --test-dir qt-app/build --output-on-failure` — or note "no test target yet" if the project hasn't stood one up.
- Codex/Claude review has approved (the workflow runs `/codex-review` before this step). If it hasn't been run yet, stop and tell the user to run `/codex-review` first — do not open a PR around unreviewed code.

If any precondition fails, **stop and report** — don't open a PR around a broken branch.

## The workflow

### Phase 1 — Establish the diff scope

Determine the branch's base and the changed files. Resolve the base defensively (this repo's default is `master` and may have no remote):

```bash
BASE=$(git merge-base HEAD origin/main 2>/dev/null \
    || git merge-base HEAD origin/master 2>/dev/null \
    || git merge-base HEAD main 2>/dev/null \
    || git merge-base HEAD master)
git diff --name-only "$BASE"...HEAD
git diff "$BASE"...HEAD --stat
```

Read the relevant plan/spec (under `tasks/` or `docs/`) so you can pass plan context to the general-code-reviewer and write an accurate PR description.

### Phase 2 — Dispatch the review gate (parallel, diff-scoped)

Dispatch all three reviewer agents **in a single message** so they run concurrently. Each runs in **diff-scoped** mode against the base computed above. In the prompt to each, state the mode explicitly and pass the base ref.

- **`dry-checker`** — duplication in the diff (literal + semantic + the repeated network/JSON/chart boilerplate), compared against the whole codebase.
- **`security-reviewer`** — vulnerabilities introduced by the diff: cleartext HTTP, admin-key/credential handling, client-side-only auth, untrusted network/RFID input, file/path handling, PII exposure.
- **`general-code-reviewer`** — plan alignment, Qt object-model & memory ownership, signal/slot usage, project naming conventions, UI/logic separation, C++ quality. Pass the plan/spec you read in Phase 1.

Wait for all three to return.

### Phase 3 — Synthesize and fix

Merge the three reports into one list, de-duplicated, sorted by severity:

- **Critical** (any reviewer) — must fix before the PR opens.
- **Important** — should fix before the PR opens.
- **Suggestions / low** — note in the PR description as follow-ups; don't block on them.

Fix Critical and Important findings by **dispatching a subagent per logical fix** (orchestration discipline — the main agent does not write the fix itself). If a fix is a true one-line trivial change (typo), the main agent may do it inline.

Each fixer subagent is a **terminal worker**: it applies its assigned fix directly (writes the code, edits the files, builds/tests) and **must not dispatch further subagents**. Keep dispatch depth at one level — main agent → fixer — to avoid nesting chains that stall the review-fix loop. Scope each subagent to a single logical fix so it stays small enough to do inline.

After fixes, re-verify: `cmake --build qt-app/build` and `ctest --test-dir qt-app/build --output-on-failure`. If a reviewer found something substantive and you changed real logic, **re-dispatch that reviewer** on the updated diff to confirm the finding is resolved. Loop until Critical and Important findings are cleared or the user accepts the remaining risk.

### Phase 4 — Commit anything outstanding

If the fixes produced uncommitted changes, commit them using the `commit` skill (Conventional Commits, grouped by concern). Do not bundle unrelated changes. Never commit `qt-app/build/` or generated/`*.user` files.

### Phase 5 — Push and open the PR

```bash
git push -u origin "$(git branch --show-current)"
```

Then open the PR with `gh`, using the body template below. Show the user the title and body and **get approval before running `gh pr create`** unless they've told you to proceed without asking.

```bash
gh pr create --title "<conventional title>" --body "$(cat <<'EOF'
<body>
EOF
)"
```

## PR body template

```
## Summary
[1-3 sentences: what this PR does and why]

## Changes
- [Key change, grouped by concern]
- [...]

## Plan / Spec
[Link or reference to the plan step this implements; note any justified deviations]

## Review Gate
- DRY check: <pass | findings fixed — summary>
- Security review: <pass | findings fixed — summary>
- General review: <pass | findings fixed — summary>
- Codex/Claude: <APPROVE>
- Build: `cmake --build qt-app/build` ✅  ·  Tests: `ctest` ✅ (or "no test target yet")

## Follow-ups (non-blocking)
- [Any low-severity suggestions deferred to later]

## Test Plan
- [How to verify; what the automated tests cover; manual GUI steps run]
```

## Rules

- **Never** open a PR with unresolved Critical findings or a failing build.
- **Never** `git push --no-verify` or bypass hooks (see the `security-hygiene` rule).
- Run the three reviewers in **diff-scoped** mode here. Full-project mode is for manual sweeps, not the PR gate.
- Orchestrate — dispatch reviewers and fixers as subagents; the main agent classifies, synthesizes, commits, and opens the PR.
- After the PR is open, consider `superpowers:finishing-a-development-branch` for any remaining cleanup/merge decisions.
