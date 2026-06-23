---
name: commit
description: Use whenever the user wants to commit changes — phrases like "/commit", "commit this", "commit these changes", "make a commit", "stage and commit", "save this to git", "wrap this up", "write a commit message", or any time they want to checkpoint work in git history. Inspects working tree state, groups related files into one or more Conventional Commits with detailed bodies explaining the *why*, shows the full plan, and waits for approval before running `git add` and `git commit`. Use this skill even when the user gives a vague directive like "commit it" without specifying scope or message — that's exactly when the skill earns its keep.
---

# commit

Turn a messy working tree into a clean, debuggable git history. The reader of `git log` six months from now is the customer; the commit you write today is the answer they need.

## Why this skill exists

`git blame` and `git log` are the durable record of *why* a change happened. When a commit smashes ten unrelated edits together under "wip", or says "fix bug" without naming the bug, that record becomes useless. This skill exists to make every commit a useful answer to a future debugger's question.

So the goal isn't ceremony — it's to **group changes by concern** and **explain the why** in language that survives the loss of conversation context.

## The workflow

Follow these phases in order. Don't skip the plan/approve step — the user explicitly wants a confirmation gate.

### Phase 1 — Read the working tree

Run these in parallel via Bash (they are independent and safe):

- `git status` — see all changed and untracked files. **Never use `-uall`** (memory issues on large repos).
- `git diff` — unstaged changes.
- `git diff --staged` — already-staged changes.
- `git log -n 10 --oneline` — learn the repo's commit message style (prefix conventions, capitalization, length).
- `git branch --show-current` — confirm which branch you're on.

If a CLAUDE.md or AGENTS.md exists in the repo root, read it — the project may have specific commit conventions that override the defaults here.

### Phase 2 — Group changes by concern

Cluster the changed files into commit candidates. Each cluster should answer one question: *what single change is this?*

Heuristics:

- **Feature + its tests** → one commit (tests prove the feature; they belong together).
- **Refactor + behavior change** → split. A pure refactor commit is reviewable; mix them and reviewers can't tell what's the refactor vs. the new behavior.
- **Unrelated bugfix discovered mid-feature** → its own commit. Future-you searching for that fix will thank you.
- **Docs-only changes** → usually their own commit, unless they document the very feature being added in the same commit.
- **Formatting / lint-only sweeps** → always separate. They drown out real changes in diffs.
- **Dependency / build-config bumps** (`CMakeLists.txt` version or `find_package` changes, vendored-lib updates under `libs/`, git submodule bumps) → separate, unless directly required by another commit in this batch (then commit the dep first, then the code that uses it).
- **Generated files** (build output, compiled assets) → check `.gitignore`. If they should be ignored, fix `.gitignore` instead of committing them.
- **Config tweaks unrelated to the feature** → separate.

If everything is genuinely one cohesive change (e.g., a single feature touching component + test + style + docs for that component), one commit is correct. Don't over-split for the sake of splitting.

**Files to never auto-stage** (warn the user instead):

- `.env`, `.env.*`, `*.pem`, `id_rsa*`, `credentials.json`, anything in `secrets/` — likely secrets.
- Large binaries (>5 MB) — likely shouldn't be in git.
- Files matching `.gitignore` patterns that somehow weren't ignored — investigate first.

### Phase 3 — Draft the commit message(s)

For each commit candidate, write a message in Conventional Commits format with a detailed body.

**Format:**

```
<type>(<scope>): <subject>

<body — explain the why, not the what>

<optional footer — BREAKING CHANGE, Refs, Closes>
```

**Subject line rules:**

- Type from: `feat`, `fix`, `refactor`, `perf`, `docs`, `style`, `test`, `build`, `ci`, `chore`, `revert`.
- Scope is optional but useful — derive from the most-touched directory or feature area (e.g., `auth`, `api/users`, `export`). Skip if it would be vague.
- Subject ≤ 72 chars, imperative mood ("add" not "added" or "adds"), no trailing period.
- Lowercase after the colon.

**Body rules:**

- Wrap at 72 chars per line.
- Lead with **why** — what problem does this solve, what was the motivation? The diff already shows *what*.
- If the change is non-obvious, briefly explain the approach and why alternatives were rejected.
- Mention follow-ups, known limitations, or things deliberately left out.
- Don't restate the subject. Don't narrate ("this commit does X") — just say what the change accomplishes.
- Skip the body only for trivial commits where the subject is fully self-explanatory (typo fixes, version bumps, simple renames).

**Footer (when applicable):**

- `BREAKING CHANGE: <description>` if the change is breaking.
- `Refs: #123` or `Closes: #123` if there's a linked issue.

**Never include:**

- `Co-Authored-By: Claude ...` or any Claude/Anthropic attribution. The user has a standing global rule against this — commits are authored solely by the user.
- "Generated with Claude Code" footers.
- The current conversation, task numbers, or anything that won't make sense six months later.

See `references/examples.md` for concrete good/bad message examples.

### Phase 4 — Show the plan, wait for approval

Present the plan to the user as a compact preview. For each proposed commit, show:

1. The files that will be staged (full paths).
2. The full commit message (subject + body + footer).
3. A one-line rationale if the grouping isn't obvious.

Format:

```
Proposed commits (N):

[1/N] feat(auth): add JWT refresh flow
      Files:
        src/auth/refresh.ts
        src/auth/refresh.test.ts
      Message:
        <full multi-line message>

[2/N] chore(deps): bump jose to 5.2.0
      Files:
        package.json
        pnpm-lock.yaml
      Message:
        <full multi-line message>

Files NOT being committed: <list any leftover unstaged/untracked files>

Proceed? (y / edit / skip <N> / abort)
```

Then **stop and wait**. Don't run `git add` yet. Common responses:

- `y` / `yes` / `go` → proceed.
- `edit 2` → rewrite commit 2's message before proceeding.
- `skip 3` → drop commit 3 from this batch; leave those files unstaged.
- `merge 1 2` → combine commits 1 and 2 into one.
- `abort` → do nothing.

If the user is silent or gives a one-word "ok"-style reply, treat that as approval.

### Phase 5 — Stage and commit, one group at a time

For each approved commit:

1. Stage **only that group's files** by name: `git add path/to/file1 path/to/file2`. Never `git add -A` or `git add .` — those sweep up files you didn't review.
2. Commit using a HEREDOC for correct multi-line formatting:

```bash
git commit -m "$(cat <<'EOF'
feat(auth): add JWT refresh flow

Sessions were expiring at 1h with no way to extend, forcing users
to re-auth mid-task. Adds a refresh endpoint that rotates the
access token while keeping the refresh token sliding-window valid
for 30 days.

Refresh tokens are stored hashed with a per-user salt; revoking a
user invalidates all their refresh tokens via the existing
revocation list.

Refs: #482
EOF
)"
```

3. After each commit, run `git status` to confirm it landed cleanly.

### Phase 6 — Handle pre-commit hook failures

If a hook fails (lint, typecheck, tests), the commit did **not** happen. Do this:

1. Read the hook output. Identify what failed.
2. Fix the underlying issue in code. Do not bypass with `--no-verify`.
3. Re-stage the fixed files: `git add <files>`.
4. Create a **new** commit (do NOT use `--amend` — the previous commit doesn't exist, so amend would silently modify the wrong commit).
5. If the fix changes the nature of the commit (e.g., you had to refactor to pass typecheck), consider whether the message still describes the change accurately. Update it if not.

If the hook failure reveals a real problem with the change itself (not just a style nit), pause and discuss with the user before forcing the commit through.

## Safety rules

These are non-negotiable. Violating them risks losing the user's work.

- **Never** run `git reset --hard`, `git checkout -- .`, `git clean -f`, `git push --force`, or `git branch -D` from this skill. These belong to other workflows.
- **Never** amend a commit that has been pushed to a shared branch unless the user explicitly says so.
- **Never** skip hooks with `--no-verify`, `--no-gpg-sign`, etc., unless the user explicitly asks.
- **Never** commit files that look like secrets (`.env*`, credentials, keys). Surface them to the user instead.
- **Never** use `git add -A`, `git add .`, or `git add -u` — always name files explicitly.
- **Never** push after committing unless the user asks. Pushing is a separate decision.
- **Never** create empty commits.
- **Never** add Claude / Anthropic co-author trailers (the user has a global rule).

## When the working tree is empty

If `git status` shows nothing to commit, say so directly:

```
Nothing to commit — working tree clean.
```

Don't fabricate changes or create empty commits. If there are only untracked files and nothing staged, ask whether to include them (untracked files often shouldn't be committed — they may be local scratch, build artifacts, or files that need to be `.gitignore`d).

## When in doubt, ask

If the changes are ambiguous (e.g., a giant 2000-line diff with no clear seam, or files that touch concerns you can't classify), stop and ask the user how they want to group them. A 30-second question beats a bad commit that lives forever in `git log`.

## Reference files

- `references/examples.md` — concrete examples of good and bad commit messages, with annotations.
- `references/conventional-commits.md` — the full Conventional Commits type list with when to use each.
