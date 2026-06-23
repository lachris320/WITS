# Security Hygiene

These rules apply to anything committed to this repo — source under `qt-app/`, config, and anything under `.claude/` or referenced from it.

## No secrets in committed files

- No admin keys, API tokens, database URLs/credentials, or other secrets in source (`*.cpp/*.h`), `.ui` files, `CMakeLists.txt`, `QSettings`-seed files, test fixtures, or anything under `.claude/`.
- The app talks to a PHP backend — **no backend DB credentials, connection strings, or the production admin key** belong in the client repo.
- Keep runtime secrets out of source: load them from a config file the user provides (and that is `.gitignore`d) or from the OS, not from a string literal. Note that `QSettings` is **persistence, not secure storage** — don't treat values written there as protected.
- Use placeholder values in examples and docs: `<ADMIN_KEY>`, `http://example.com/endpoint.php`, `REPLACE_ME`.

## No personal paths or machine-local references

- Don't hardcode `C:\Users\<name>\...` or `/Users/<username>/...` paths in source, skills, rules, or commit messages. Use relative paths, `QStandardPaths`, or a configured base directory.
- The one allowed exception: skills/docs that reference the **auto-memory directory** (`.../.claude/projects/...`) necessarily name the current machine path. Document such paths as variables (e.g. `MEMORY_DIR`) so users can substitute their own.

## No internal URLs or PII in committed files

- Anything under `.claude/` ships with the repo — treat it as semi-public. Don't paste live backend URLs, tokens, or anything that grants access on its own.
- **No real student PII** (names, school IDs, photos, visit logs) in fixtures, screenshots, sample data, or commit messages. Use synthetic data.

## Pre-commit awareness

- If the project adds pre-commit hooks or CI checks (a git hook, a `cmake`/format/lint gate), don't bypass them with `--no-verify` unless explicitly authorized.
- If a hook flags a secret, the commit should fail — investigate, don't override.

## Why this matters

This repo is checked in and visible to everyone with read access — including past contributors whose access may not be revoked. A single committed admin key, backend credential, or real student record becomes a long-lived leak of credentials or personal data.
