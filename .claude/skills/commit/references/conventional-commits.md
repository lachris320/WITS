# Conventional Commits Reference

Quick reference for the type prefixes used in commit subjects. The format is:

```
<type>(<optional-scope>): <subject>
```

Append `!` after the type/scope (e.g., `feat(api)!:`) to signal a breaking change. Pair with a `BREAKING CHANGE:` footer.

---

## Type prefixes

| Type | When to use |
|------|-------------|
| `feat` | A new user-visible feature or capability. Anything that adds behavior the user/API consumer can observe. |
| `fix` | A bug fix. The commit corrects incorrect behavior. |
| `refactor` | A code change that neither adds a feature nor fixes a bug. Internal restructuring with no behavior change. |
| `perf` | A change that improves performance without changing behavior. Distinct from `refactor` because it has a measurable goal. |
| `docs` | Documentation only. README, code comments, JSDoc, API docs. No production code touched. |
| `style` | Formatting, whitespace, semicolons, lint auto-fixes. No semantic change. (Note: not "visual styling" — that's `feat` or `fix`.) |
| `test` | Adding or updating tests without changing the code under test. If the test change is part of a feature, fold it into the `feat` commit. |
| `build` | Build system, package manager, or dependency changes. `package.json`, `pnpm-lock.yaml`, `tsconfig.json`, bundler config, Dockerfile. |
| `ci` | CI/CD pipeline changes. GitHub Actions, CircleCI, deployment scripts. |
| `chore` | Maintenance work that doesn't fit elsewhere. Use sparingly — most "chore" commits actually fit `build`, `ci`, `docs`, or `refactor`. |
| `revert` | Reverts a previous commit. Body should include `This reverts commit <sha>.` |

---

## Choosing a type — common ambiguities

**"I refactored to add a feature."** → `feat`. The feature is the customer-visible outcome. Mention the refactor in the body.

**"I added a test that revealed a bug, then fixed it."** → Two commits: `fix` for the fix, `test` for the test (or fold the test into the `fix` commit since they prove each other).

**"I bumped a dependency to get a security fix."** → `build` (or `fix(deps)` if you want to surface that it's a security-driven bump). Mention the CVE or vuln in the body.

**"I renamed a variable for clarity."** → `refactor`. If the rename is part of a `feat` you're working on, fold it in; don't make a noise commit.

**"I changed the colors in the design system."** → `feat` (or `fix` if you're correcting a wrong color). `style` is for code formatting, not visual styling.

**"I deleted dead code."** → `refactor`. Body should briefly note why it's dead (e.g., "FeatureFlagX was removed in #482, this was its last caller").

---

## Scopes

Scope is optional. Use one when it adds clarity:

- A package or module name in a monorepo: `feat(api): ...`, `fix(web): ...`.
- A feature area: `feat(auth): ...`, `fix(export): ...`.
- A file path's most distinctive segment: `refactor(api/users): ...`.

Skip the scope when:

- The change touches many areas evenly.
- The scope would be vague (`misc`, `general`, `core`).
- The repo is small enough that scope is just noise.

Stay consistent within a repo. If the project uses `feat(auth):` for one auth change, don't write `feat(authentication):` next time.

---

## Breaking changes

Two signals — use both:

1. `!` in the subject line: `feat(api)!: replace /users/:id/posts with /users/:id/content`
2. A `BREAKING CHANGE:` footer that describes what breaks and how to migrate.

Without the footer, downstream tooling (changelogs, semantic-release) can miss the breaking change.

---

## Length and casing

- Subject: ≤ 72 chars total (including the type prefix). Aim for ≤ 50 if you can.
- Subject body (after the colon): lowercase first letter, no trailing period, imperative mood.
- Body lines: wrap at 72 chars.

---

## When NOT to use Conventional Commits

If the repo has its own established commit style (visible in `git log`), follow that instead. Mixing conventions in one history is worse than picking either consistently. The skill checks `git log -n 10 --oneline` to detect this.
