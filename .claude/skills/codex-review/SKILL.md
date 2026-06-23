---
name: codex-review
description: Use when the user asks for a Codex review, a second-opinion review, or uses /codex-review — for either CODE (a commit, a phase, or a whole branch) or a DESIGN SPEC / planning doc before implementation. Sends the material to Codex GPT-5.5 for multi-round review, fixes findings, and resubmits until APPROVE or the round cap. Trigger it for code review AND whenever the user wants a spec/plan/design/RFC stress-tested — phrases like "review this spec", "is this plan ready to build", "any gaps before we implement", "validate this design against the codebase", or after brainstorming produces a design doc. Auto-detects design-spec / commit / phase / branch mode and scales the prompt accordingly.
---

# Codex Review

Multi-round review via Codex GPT-5.5 — of **code** (a commit, a phase, or a whole branch) or of a **design spec / planning doc** before any code is written. Sends the material, gets findings, fixes them, resubmits until APPROVE or the round cap. Codex is a second opinion from a different model family — useful as a cross-check on top of whatever review process you already run.

**One command, four modes.** This command auto-detects what you're reviewing and scales the prompt accordingly:

| Mode            | Triggered when                                                              | Prompt depth                                                                  |
| --------------- | --------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| **Design-spec** | Target is a planning doc (spec/RFC/design `.md`), or the user asks to review a spec/plan/design — see Step 1 | Pre-implementation gate — feasibility against the real codebase, gaps, internal consistency, scope |
| **Commit**      | 1 commit in scope, or a single *code* file passed as argument               | Light — correctness, obvious bugs, security baseline, tests                   |
| **Phase**       | 2–9 commits on the current branch                                           | Medium — adds design compliance, prior-fix verification, security baseline, next-step readiness |
| **Branch**      | 10+ commits, or whole-branch review explicitly requested                    | Deep — holistic architecture, cross-cutting concerns, merge readiness         |

You always invoke it the same way: `/codex-review`. The mode picks itself.

Design-spec mode is special in two ways: the "fixes" are **edits to the spec document**, not code (so no tests/quality-gates apply), and its round cap is **5** (spec edits are cheap and carry no execution risk) rather than 3. Everything else — the resume mechanics, the sandbox rules, never force-approving — is shared.

---

## Variables

- `TARGET`: optional `$ARGUMENTS` — file path, branch name, or empty
- `MAX_ROUNDS`: 3 for code modes (COMMIT / PHASE / BRANCH); **5 for DESIGN-SPEC mode**
- `CODEX_MODEL`: `gpt-5.5`
- `CODEX_BIN`: `codex` if on `$PATH`, otherwise the platform-specific install path

---

## Codex CLI Reference

Codex is invoked via CLI (the MCP server is unreliable):

```bash
cat <<'PROMPT' | $CODEX_BIN exec -m gpt-5.5 --sandbox read-only -C "$(pwd)" -
[review prompt]
PROMPT
```

Resume the same session for follow-up rounds (preserves context, avoids re-uploading the diff/spec):

```bash
echo "fixes applied: ..." | $CODEX_BIN exec resume --last -m gpt-5.5 -
```

**`resume` does NOT accept `--sandbox` or `-C` — and must not be given them.** Sandbox policy and working directory are session-scoped: they're locked in by the Round 1 `exec` call and inherited on resume. Passing `--sandbox` errors out (`unexpected argument '--sandbox'`). Resume only takes session/prompt flags (`--last`, `-m`, the `-` stdin marker).

**Never pass `--full-auto`.** It is deprecated, and it silently **overrides `--sandbox read-only` with `workspace-write`** — so the Round 1 command would grant Codex write access to the tree despite this skill's read-only rule (observed: the invocation prints `sandbox: workspace-write`). `codex exec` is non-interactive by default, so `--full-auto` buys nothing; omit it entirely. `--sandbox read-only` alone gives review-only access with no approval prompts.

**Flag breakdown:**

| Flag                  | Purpose                                                    |
| --------------------- | ---------------------------------------------------------- |
| `-m gpt-5.5`          | Always use GPT-5.5 for reviews                             |
| `--sandbox read-only` | Review-only; Codex MUST NOT modify files. Non-interactive by default — no approval prompts |
| `-C <repo>`           | Working directory                                          |
| `-`                   | Read prompt body from stdin                                |

---

## Workflow

### Step 1: Detect the mode

**First, check for DESIGN-SPEC mode — it takes priority over the commit-count logic.** Use it when *either*:

- **The target is a planning document.** `$ARGUMENTS` is a single `.md`/`.txt`/`.mdx` file and it looks like a spec rather than code prose — it lives under a planning location (`docs/`, `specs/`, `rfcs/`, `design/`, `plans/`, `**/specs/`) *or* its filename/headings read like a spec/design/RFC/plan (e.g. `*-design.md`, `*-spec.md`, `*-rfc.md`, or headings like "Goal", "Architecture", "Non-goals", "Phasing"). A `README.md` or a `.md` that is just code docs is **not** a spec — fall through to code mode.
- **The user asks for a spec review in words** — "review this spec/plan/design/RFC", "is this plan ready to build", "stress-test this design", "any gaps before we implement", even without naming a file. If no file is named, ask which document.

If neither holds, it's a code review — pick depth by commit count:

```bash
# Resolve the base branch (try main, then master, then origin/HEAD)
BASE=$(git rev-parse --verify main 2>/dev/null && echo main \
    || git rev-parse --verify master 2>/dev/null && echo master \
    || git symbolic-ref refs/remotes/origin/HEAD | sed 's@^refs/remotes/origin/@@')

# Count commits in scope
COMMITS=$(git log "$BASE"..HEAD --oneline | wc -l)

# Decide mode:
# - 1 commit               → COMMIT mode
# - 2–9 commits            → PHASE mode
# - 10+ commits            → BRANCH mode
```

If `$ARGUMENTS` is a single *code* file path, treat it as COMMIT mode regardless of branch state. If `$ARGUMENTS` is a branch name, diff that branch against `$BASE` and use BRANCH mode.

**Commit the document/code first.** Like code modes, run the spec review against a committed file so the rounds diff cleanly; if the spec has uncommitted edits, commit them before Round 1.

### Step 2: Gather context

**For DESIGN-SPEC mode, collect instead:**

1. **The spec file path** — Codex reads it itself; don't paste the whole thing into the prompt.
2. **The real files the spec claims to touch.** Skim the spec for the modules/functions/paths it names and list the concrete files (with the symbols/line refs you can find). This is what makes the review valuable: Codex validates the spec's claims against the actual codebase rather than reviewing it in the abstract. If the spec names few files, name the surfaces it *should* touch so Codex can check coverage.
3. **Conventions files** — `CLAUDE.md` / `AGENTS.md` / `CONTRIBUTING.md`, plus any locked decisions the spec records.

Then skip the code-context items below and go to Step 3 (DESIGN-SPEC template).

**For code modes, always collect:**

1. **Commit list** — `git log <base>..HEAD --oneline`
2. **Diff stat** — `git diff <base>..HEAD --stat`
3. **Test status** — run the project's test command (check `package.json`, `Makefile`, `pyproject.toml`, etc.) and note pass/fail count. If you can't determine it, note "test command unknown".
4. **Primary files changed** — `git diff --name-only <base>..HEAD`
5. **Secrets pre-scan** — deterministically scan the *added* lines of the diff for credential patterns. This is a floor under the reviewer's judgment: a model can skim past a leaked key in a large diff; grep won't. Dependency-free (no gitleaks/trufflehog needed):

   ```bash
   git diff "$BASE"..HEAD -U0 | grep -E '^\+[^+]' | grep -inE -e \
     '-----BEGIN [A-Z ]*PRIVATE KEY-----|AKIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{20,}|xox[baprs]-[A-Za-z0-9-]{10,}|eyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}|(api[_-]?key|secret|token|password|passwd|client[_-]?secret|access[_-]?key)[[:space:]]*[:=][[:space:]]*[^[:space:]]{8,}' \
     || echo "secrets pre-scan: clean (no obvious patterns in added lines)"
   ```

   Paste the result into the prompt's `Secrets pre-scan:` line so the reviewer cross-checks instead of hunting blind. The grep is a *candidate finder*, not a verdict — triage every hit (test fixtures and placeholders are common false positives). Any **confirmed** real secret is Critical and must be **rotated**: deleting it from the diff does not unleak it from git history.

For PHASE and BRANCH modes, also collect:

6. **Design / spec references** — look in `docs/`, `specs/`, `rfcs/`, `README.md`, or wherever this repo keeps planning docs. Include any that match the feature scope.
7. **Reference files** — existing codebase files that show the conventions this work should follow (e.g., a sibling component for a new UI piece).

### Step 3: Build the prompt (depth depends on mode)

Use the appropriate template from "Prompt Templates by Mode" below. Fill in the bracketed placeholders with the context from Step 2.

### Step 4: Invoke Codex (Round 1)

```bash
cat <<'PROMPT' | $CODEX_BIN exec -m gpt-5.5 --sandbox read-only -C "$(pwd)" -
[constructed prompt from Step 3]
PROMPT
```

Codex may print a large transcript; if the shell truncates it, the full output is saved to a file path shown in the result — read the tail of that file to find the `Verdict:` and findings.

Parse the response and extract:

- **Verdict:** APPROVE / REQUEST CHANGES / REJECT
- **Findings:** by severity (Critical / Important / Low) with file:line references

Report to the user after each round.

### Step 5: Triage and fix (if REQUEST CHANGES)

**In DESIGN-SPEC mode, "fixing" means editing the spec document** — there is no code to change, no tests, and no quality gates. But apply the same rigor you'd want before implementing: **verify each load-bearing finding against the real codebase before editing** (Codex cites file:line — confirm the claim holds; it usually does, but you're allowed to disagree with evidence). Then revise the spec so the gap is closed: tighten the ambiguous requirement, correct the wrong technical claim, add the missing consideration, or record it explicitly as a non-goal/forward-note if it's genuinely out of scope. Don't paper over a real gap with vague wording — the point of the gate is to remove rework before code exists. Commit the spec edits (`docs: address codex review round N`) and go to Step 6. Skip the code-quality-gate guidance below.

**In code modes**, for each finding:

1. **Critical** — fix immediately. Where it makes sense, add a regression test that fails against the buggy code first (TDD discipline).
2. **Important** — fix now unless there's a clear reason to defer. Document any deferral.
3. **Low** — fix if cheap, otherwise capture as a follow-up.

For every test added, verify it was meaningful: temporarily revert the fix and confirm the test fails, then re-apply the fix.

Run the project's standard quality gates after fixes. Typical commands (use whatever the repo actually defines):

- type checker (e.g. `tsc --noEmit`, `mypy`, `pyright`)
- linter (e.g. `eslint`, `ruff`, `golangci-lint`)
- formatter check (e.g. `prettier --check`, `black --check`)
- test suite (e.g. `npm test`, `pytest`, `go test ./...`)

Commit the fixes following the repo's commit convention. A reasonable default subject: `fix: address codex review round N findings`.

### Step 6: Resume (Round 2+)

```bash
echo "Round 2 — fixes applied.

Fixed:
- [Critical] <description> — regression test added at <file>:<line>, verified RED before fix
- [Important] <description> — <what changed>

Deferred:
- [Low] <description> — tracked as follow-up

Please re-review and issue a new verdict." | $CODEX_BIN exec resume --last -m gpt-5.5 -
```

`resume --last` picks up the most recent Codex session — no session ID needed. Context is preserved, no re-upload cost.

**Do not add `--sandbox`, `-C`, or `--full-auto` to the resume command.** The read-only sandbox set in Round 1 carries over automatically; `resume` rejects `--sandbox`/`-C` outright. Never use `--full-auto` anywhere (see the Codex CLI Reference — it forces workspace-write). If you feel the urge to re-assert read-only here, don't — it's already inherited.

Loop until **APPROVE** or the round cap is hit — **3** rounds for code modes, **5** for DESIGN-SPEC mode.

### Step 7: Completion

- **APPROVED** — report final verdict with round count and the list of all fixes applied across rounds
- **Round cap reached without APPROVE** — stop, surface the remaining issues, and ask the user to either accept the risk, escalate to a senior reviewer, or rework the scope. Never force-approve. (If the last round's findings were clearly correct and you applied fixes for them, you may say so — but be explicit that Codex has not re-reviewed those fixes, and offer one more verification round rather than implying approval.)

---

## Prompt Templates by Mode

All templates start with a short repo context block so Codex knows what stack it's looking at. The depth differs in what review areas are asked for.

### Shared repo context block (prepend to every prompt)

Before invoking Codex, inspect the repo and fill in a short context block. Keep it factual and small — Codex will read files itself when it needs more detail.

```
# Repo context

- Language(s): <detect from file extensions and package manifests>
- Framework(s) / runtime: <detect from package.json, requirements.txt, go.mod, Cargo.toml, etc.>
- Test framework: <detect from devDependencies / dev-dependencies>
- Build / package manager: <npm, pnpm, yarn, uv, poetry, cargo, go modules, etc.>
- Notable conventions (if visible in the repo): <e.g. "C++17", "Qt parent-ownership for QObjects", "#ifndef header guards, no m_ prefix">

If a CLAUDE.md, AGENTS.md, CONTRIBUTING.md, or similar conventions file exists, include the key rules from it here.
```

If you can't detect something confidently, omit that line rather than guessing.

### DESIGN-SPEC mode — pre-implementation gate on a spec / planning doc

Use before code is written, to stress-test a spec against reality. The whole value is that
Codex **reads the spec AND validates its claims against the actual codebase** — so name the
real files. Tell it explicitly this is a design review, not a code review (otherwise it
hunts for line-level bugs that don't exist yet).

```
[shared repo context block]

# Design-spec review (NOT a code review)

The file under review is a DESIGN SPEC, not code:
  <path/to/spec.md>

Goal of the feature: <1–3 sentences from the spec>
Locked decisions (do not relitigate): <bullet the decisions the spec marks as settled>

Please READ the spec file, then VALIDATE it against the actual codebase. Inspect at least
these real files to check the design's claims are feasible and accurate:
- <file:symbol> — <what the spec assumes about it>
- <file:symbol> — <...>
(If the spec names few files, also check the surfaces it SHOULD touch for coverage.)

Review for:
- Feasibility — do the named seams/APIs/modules actually exist where the spec claims, and is
  the proposed change realistic given how those files are written today?
- Correctness of technical claims — call out anything that won't work as described
  (protocol/tooling behavior, concurrency, security boundaries, data model).
- Gaps — missing considerations a design at this stage should cover (security, error/retry,
  migration/back-compat, identity/scoping, platform edge cases, dev-vs-prod/build differences).
- Internal consistency — contradictions between locked decisions and the rest of the spec.
- Scope — is each phase/slice coherent and shippable, or mislabeled?

Be willing to debate. Reserve "Critical" for genuine design flaws that would cause rework or
a broken feature if not addressed before implementation. This is a pre-implementation gate.

# Output format

Verdict: APPROVE / REQUEST CHANGES / REJECT

Findings:
- [Critical]  description — spec section or file:line
- [Important] description — spec section or file:line
- [Low]       description — spec section or file:line

Cross-cutting observations: 1-3 sentences.
```

### COMMIT mode — single commit / single file

Use for bug fixes, small refactors, single-file changes.

```
[shared repo context block]

# Single-commit code review

Commit: <SHA> <subject>
Files changed: <list>
Test status: <N> passing / <M> failing  (or "unknown")
Secrets pre-scan (added lines, automated): <paste hits with line refs, or "clean">

Please review for:
- Correctness — does the change do what it says it does?
- Obvious bugs — null checks, error handling, edge cases, async race conditions
- Security baseline (always, even for a one-line change) — no hardcoded secrets/credentials/API keys introduced (cross-check the secrets pre-scan above); input validation on any user-facing or external input; no injection vectors (SQL, command, XSS, path traversal); auth/authz checks present where the change touches a protected path
- Tests — is the fix covered? Does the test actually catch the bug?
- Style — anything that would fail the repo's linter or stated conventions?

Be strict on correctness and the security baseline, relaxed on stylistic nits. Reserve Critical for real bugs and genuine security holes.

# Output format

Verdict: APPROVE / REQUEST CHANGES / REJECT

Findings:
- [Critical]  description — file:line
- [Important] description — file:line
- [Low]       description — file:line
```

### PHASE mode — multi-commit phase of a larger plan

Use at the end of a phase in a multi-phase plan (e.g., `superpowers:subagent-driven-development`).

```
[shared repo context block]

# End-of-phase review

<N> commits on <branch>:
- <SHA1> <subject>
- <SHA2> <subject>
- ...

Base: <base SHA>
HEAD: <head SHA>

Test status: <N> passing / <M> failing  (or "unknown")
Secrets pre-scan (added lines, automated): <paste hits with line refs, or "clean">

# Files
Primary:
<list of files changed in this phase>

Reference (for patterns/conventions):
<list of existing files for pattern reference>

# Focus areas

## Security baseline (always check, regardless of what the phase touched)
- No hardcoded secrets, credentials, or API keys introduced (cross-check the secrets pre-scan in the header)
- Input validation on every user-facing / external boundary added in this phase
- No injection vectors (SQL, command, template, XSS, path traversal)
- Auth/authz enforced server-side wherever this phase adds or changes a protected path
(If the phase is auth/session or backend work, the work-type block below goes deeper — this baseline is the floor, not the ceiling.)

[Insert one or more focus area blocks from "Focus areas by work type" below — pick based on what the phase touched]

## Design compliance
- Check against <docs/specs path>, if applicable
- Any contract drift from the design?

## Next-step readiness
- Does this phase leave the codebase ready to build on?
- Any TODOs or scaffolds that should be resolved before proceeding?

## Previously caught bugs verification
<list bugs fixed in earlier phases of the same plan — are they still fixed?>

# Output format

Verdict: APPROVE / REQUEST CHANGES / REJECT

Findings:
- [Critical]  description — file:line
- [Important] description — file:line
- [Low]       description — file:line

Cross-cutting observations: 1-3 sentences.

# Important
- Be willing to debate. Reserve Critical for actual bugs / a11y blockers / security issues.
- If you find a subtle async or state bug, REPRODUCE it locally before reporting.
```

### BRANCH mode — whole branch before merge

Use for the final review before merging a branch / PR with a large number of commits.

```
[shared repo context block]

# Full branch review before merge

Base: <base branch>
HEAD: <branch HEAD>
Commits: <N>
Diff: <X> files changed, +<Y>/-<Z> lines

Prior reviews on this branch: <e.g. "2 phase reviews all APPROVE", or "none">
Test status: <N> passing / <M> failing  (or "unknown")
Build: <pass / fail / not run>
Secrets pre-scan (added lines, automated): <paste hits with line refs, or "clean">

# What shipped

<Feature summary from the PR description — what changed and why>

# Review focus

## Architecture coherence
- Do all the pieces fit together as a coherent module?
- Any inconsistencies between layers?
- Design / spec drift?

## Security audit (highest priority)
- Authentication / authorization bypass opportunities
- Input validation on all external boundaries
- Injection risks (SQL, command, template, XSS)
- CSRF on state-changing requests
- Sensitive data in client bundles, logs, or error messages
- Secret / token leakage (cross-check the secrets pre-scan in the header — but go beyond it: the grep only sees added lines and known patterns)

## Previously-caught bugs verification
<list bugs fixed in earlier phases of this branch — verify each is still fixed and not regressed by later commits>

## Test quality across the whole suite
- Do the new tests meaningfully cover the user-facing flows?
- Any flaky tests (timing, async)?
- Any tests that mock too much and don't verify real behavior?

## Accessibility (for UI work)
- Keyboard navigation works on all new interactive components?
- Focus management correct on modals / popovers?
- Screen reader labels present?
- Color contrast meets WCAG AA?

## Merge readiness
- Any TODO/FIXME left in production code that should block merge?
- Known limitations — are they documented in code comments or PR body?
- Any follow-up work that should be tracked separately?

# Output format

Verdict: APPROVE / REQUEST CHANGES / REJECT

Findings (group by severity):
- [Critical]  description — file:line
- [Important] description — file:line
- [Low]       description — file:line

Cross-cutting observations: 1-3 sentences on overall quality + readiness for merge.

# Important
- This is the LAST review before merge. Be strict but scoped — critical issues only, not nitpicks.
- Reserve Critical for actual merge blockers.
- Low priority issues can be noted but shouldn't block merge.
- Acknowledge prior review rounds if any — this is a final pass, not a re-litigation.
```

---

## Focus areas by work type

Use these template blocks when filling in the "Focus areas" section for PHASE or BRANCH mode, depending on what the phase touched. Pick what's relevant — skip what isn't.

### Backend / API work

```
## Correctness
- Input validation on every external boundary (request body, query params, headers)?
- Error handling: do failures return useful error codes / messages without leaking internals?
- Transactions / atomicity: any multi-step writes that should be transactional?
- N+1 queries or other obvious performance traps?
- Idempotency on retryable endpoints (PUT, DELETE, payment-like flows)?
- Backward compatibility: API contract changes flagged?

## Test quality
- Happy path AND error paths (4xx, 5xx, timeout, partial failure)?
- Boundary cases (empty input, max input, unicode, very large payloads)?
- Integration tests for the full request/response cycle where it matters?
```

### Qt / Widgets UI work

```
## Correctness
- UI states covered: loading (busy indicator), empty, and error — not just the happy path?
- Signal/slot wiring: functor syntax, no duplicate connections, lambdas use the 3-arg context form?
- Object ownership: widgets/replies parented or deleted; no leaks, no double-free in slots?
- Long/blocking work kept off the UI thread (no synchronous network/file waits freezing the GUI)?

## Robustness
- Network replies and JSON parsed defensively (QJsonParseError checked, types validated before use)?
- External input (RFID payloads, server filenames, chosen file paths) validated before use?

## Test quality
- Pure logic extracted from the window and unit-tested (QtTest), not buried in the UI class?
- Widget interactions exercised with QTest / QSignalSpy where the flow depends on the widget tree?
```

### Data / persistence work

```
## Correctness
- Schema migration safe under concurrent reads/writes?
- Rollback path exists and was tested?
- Indexes added for new query patterns?
- Defaults / NOT NULL backfill strategy for new columns on large tables?

## Test quality
- Migration tested against representative data?
- Query plans checked for the hot paths?
```

### Auth / admin-gate / transport work (Qt client ↔ PHP backend)

```
## Security
- Admin gate enforced server-side, not just a client-side string compare that anyone with the binary can bypass?
- Admin key / credentials not hardcoded, not logged, not stored in cleartext (QSettings is not secure storage)?
- Transport: is the endpoint HTTPS? Cleartext http:// sends PII / admin key in the clear — flag it.
- TLS errors never ignored (no ignoreSslErrors / VerifyNone)?
- User/external input encoded into requests (QUrlQuery FullyEncoded), and the server-side SQLi exposure noted?
- No secrets or student PII in qDebug/logs, error dialogs, or exported reports?

## Correctness
- Failed/invalid responses handled (a non-2xx or error JSON not treated as success)?
- Register / update / delete flows handle all states (network failure, malformed response, partial upload)?
```

### Build / CI / tooling work

```
## Correctness
- Pinned versions vs floating ranges — intentional?
- New dependencies: license check, maintenance status, transitive bloat?
- CI cache keys still valid after the change?
- Hook scripts don't silently swallow failures?
- Build determinism preserved (no time-based output, no random seeds)?
```

---

## When NOT to use `/codex-review`

- **Prose/documentation changes** (READMEs, guides, comments, changelogs) — skip it. **Exception: design specs and planning docs are exactly what DESIGN-SPEC mode is for** — review those.
- **Trivial refactors, renames, or formatting-only commits** — skip it
- **Build is failing** (code modes) — fix the build first; Codex needs a passing typecheck/test to give useful signal. (Doesn't apply to DESIGN-SPEC mode — there's no build.)
- **The user explicitly says "skip Codex"** — honor the request

---

## Cost awareness

- Each Codex round uses ~100–300k tokens depending on diff/spec size
- DESIGN-SPEC mode: ~100–150k tokens/round; a full spec gate often runs 3–5 rounds (~0.4–0.7M total)
- COMMIT mode: ~50–150k tokens (light prompt, small diff)
- PHASE mode: ~150–300k tokens
- BRANCH mode: ~250–500k tokens
- An 8-phase plan budgets ~2–4M tokens total across all reviews
- Worth it — Codex catches design flaws and bugs that would cost 100x more to fix after implementation.

---

## Report Format

After each round (`<cap>` is 3 for code modes, 5 for DESIGN-SPEC):

```
*Codex Review — Round N/<cap>*

Mode: DESIGN-SPEC / COMMIT / PHASE / BRANCH
Verdict: APPROVE / REQUEST CHANGES / REJECT

Findings:
- [Critical]  description — file:line (or spec section)
- [Important] description — file:line
- [Low]       description — file:line

(if REQUEST CHANGES: summary of what was fixed, what remains)
```

After the final round:

```
*Codex Review Complete*

Mode: <mode>
Result: APPROVED after N rounds / NEEDS MANUAL REVIEW at the round cap

Fixes applied:
- <list>

Remaining issues (if not approved):
- <list>
```

---

## Integration with multi-phase plans

If you're following a multi-phase plan (e.g., `superpowers:subagent-driven-development`), run `/codex-review` at three points:

0. **Before any code** — DESIGN-SPEC mode on the spec/plan itself (e.g. right after `superpowers:brainstorming` produces a design doc, before `writing-plans`). Catching a design flaw here is the cheapest fix you'll ever make.
1. **End of every phase** — PHASE mode auto-activates from the commit count
2. **End of the full branch** — BRANCH mode auto-activates once the branch crosses ~10 commits

Don't run per-task Codex reviews inside a phase — per-phase is the sweet spot for cost vs. coverage.

---

## Rules

- **Never force-approve.** If Codex still says REQUEST CHANGES at the round cap (3 for code, 5 for DESIGN-SPEC), hand it back to the user.
- **Never skip hooks or bypass lint/typecheck** to make a fix pass — fix the underlying issue. (Code modes only; DESIGN-SPEC fixes are document edits.)
- **Don't auto-fix Low-severity** findings unless the user asks — keep the diff focused. (In DESIGN-SPEC mode, Low items that are genuinely implementation-time concerns are best recorded as forward-notes in the spec rather than over-specified now.)
- Follow the repo's existing commit convention for the round-N fix commits.
- Read-only sandbox for Codex — Codex must never modify the working tree; the fixes are applied by Claude in this session. Set `--sandbox read-only` **once** on the Round 1 `exec` call. It is session-scoped and inherited by every `resume`, so never re-pass `--sandbox`/`-C` on resume. **Never pass `--full-auto`** — it is deprecated and silently overrides `--sandbox read-only` with `workspace-write`, granting Codex write access; `codex exec` is non-interactive without it.
- Detect the base branch (`main`, `master`, or `origin/HEAD`); don't assume.

---

## Checklist before declaring a review done

- [ ] All target changes are committed before running the review (the spec doc, for DESIGN-SPEC mode)
- [ ] Code modes: the repo's typecheck and test commands are green before Round 1 (N/A for DESIGN-SPEC)
- [ ] Code modes: the secrets pre-scan ran over the added lines and its result is in the prompt; any confirmed real secret was treated as Critical and flagged for rotation (N/A for DESIGN-SPEC)
- [ ] The mode was auto-detected correctly — DESIGN-SPEC for a spec/plan doc, else code mode by commit count (or manually overridden if needed)
- [ ] Round 1 `exec` was invoked with `--sandbox read-only` and **no `--full-auto`** (confirm Codex didn't print `sandbox: workspace-write`); resume rounds passed no sandbox/cwd flags and inherited it
- [ ] DESIGN-SPEC: the prompt named real files for Codex to validate the spec against (not an abstract review)
- [ ] All Critical findings fixed — code modes with regression tests verified RED where applicable; DESIGN-SPEC with the gap closed in the document (or recorded as an explicit non-goal/forward-note)
- [ ] All Important findings fixed or explicitly deferred with justification
- [ ] Round 2+ used `codex exec resume --last` (not a fresh session)
- [ ] Final verdict is APPROVE (or escalated at the round cap — 3 for code, 5 for DESIGN-SPEC — never force-approved)
- [ ] Fixes committed before moving on
