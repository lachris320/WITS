# LOAMS S1b — Server Stack Manifest & Host Convergence — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Revision 5 (2026-10-06):** Codex plan review round 3 applied — `-Converge` validates the final report location (inside the protected reports folder, allowlist ACL) in preflight before any backup or checkpoint, and a final-report save failure after success is deterministic (`SuccessReportNotSaved`, exit 6); every run-owned DB credential file is tracked from before creation and swept at undo and end of run, with any leftover ⇒ `RecoveryRequired`; the reports folder gets a fresh protected allowlist ACL (Administrators + SYSTEM only) that is verified programmatically and checked again on every write.

**Revision 4 (2026-10-06):** Codex plan review round 2 applied — run-specific MariaDB verification schemas with an idempotent dropping undo (`MariaDbVerification` checkpoint, restore step, fingerprint and post-change check), `reports` folder provisioned in runbook B0 while `-Report` fails closed on a missing folder, a dirty post-change report rolls back (`RolledBack`, report kept as evidence; `RecoveryRequired` if rollback fails) tested through the real `Validate` checkpoint and engine, exact `tools\` tree capture/restore incl. directories, and a child-process success test of the generated restore wrapper.

**Revision 3 (2026-10-06):** Codex plan review round 1 applied — manifest array contract (`Get-LoamsPropList`), contained tool paths, prior-state-preserving retention rollback, a restore that covers every mutation (16-row inventory) and is tested by executing the generated script, database backup readiness (D19), verified `mysqld` shutdown, post-change report gating `Success`, mandatory legacy photo fallback, future-dated report rejection, and mechanically recounted tests.

**Revision 2 (2026-10-05):** owner review "APPROVE WITH CHANGES" applied — MariaDB identity and data-directory hardening moved into S1b (D15/D16), encrypted backups with a verified off-host copy and 30-day retention (D14), staging authorization by approved machine identity plus typed confirmation (D2), a fresh drift re-check before the first change (D9), and read-only post-change validation beyond `get_branding.php` (D13).

**Goal:** Deliver the S1b slice of the approved S1 spec (`docs/superpowers/specs/2026-10-05-loams-s1-transport-security-design.md`): a versioned, schema-checked compatibility manifest (server "LOAMS Server Stack v1.0" + client TLS runtime, plus the pinned OpenSSL tool), the `Test-LoamsServerHost.ps1` host tool (`-Report` read-only by default, `-Converge` recoverable-not-atomic), service identity for Apache (`NT SERVICE\Apache2.4`) **and MariaDB** (`NT SERVICE\mysql`), least-privilege ACLs including the MariaDB data and log directories, `LOAMS-Transport` event-source registration, encrypted verified backups with an off-host copy, and the staging upgrade runbook. S1b ships **tooling and documentation only**; nothing touches a production gate PC before S1f, and production convergence is refused against any manifest that is not `approved`.

**Architecture:** One PowerShell module, `deploy/server/LoamsHost/LoamsHost.psm1`, that dot-sources one focused file per responsibility (`LoamsHost.<Area>.ps1`) so every function lives in a single module scope and Pester can `Mock -ModuleName LoamsHost` any of them. Every external program (`httpd.exe`, `mysqld.exe`, `mysql.exe`, `mysqladmin.exe`, `mysqldump.exe`, `openssl.exe`, `sc.exe`, `icacls.exe`, `reg.exe`) is called only through `Invoke-LoamsExternal` or `Start-LoamsRedirectedProcess`, and every Windows query goes through a small `Get-Loams*` wrapper, so tests never touch the real host. A module-scoped mode flag (`Report` | `Converge`) is checked by `Assert-LoamsMutationAllowed` inside every mutating function, so `-Report` is read-only by construction **and** by test. `Test-LoamsServerHost.ps1` is a thin entry that calls `Invoke-LoamsHostReport` or `Invoke-LoamsHostConverge`. Convergence: read-only gates → typed operator confirmation → encrypted backup (CMS to an offline recovery certificate, streamed so no plaintext lands on disk) → verified off-host copy → live drift re-check → named checkpoints (`Do`/`Verify`/`Undo`) through `Invoke-LoamsCheckpointPlan`, which rolls back in reverse and yields `RECOVERY REQUIRED` (never success) if any undo fails. The stack **upgrade** itself is runbook-driven, bracketed by `-Converge -BackupOnly` and `-Report`.

**Tech Stack:** Windows PowerShell 5.1 (target runtime; no PowerShell 7 dependency), Pester 5.5+ (dev prerequisite, not vendored), XAMPP layout (Apache 2.4 Apache Lounge, PHP 8.x ZTS `apache2handler`, MariaDB `mysqld`/`mysqldump`/`mysql`), OpenSSL `cms` (manifest-pinned `openssl.exe`), CIM (`Win32_Service`, `Win32_Process`), `icacls`/`sc.exe`/`reg.exe`, ScheduledTasks module, JSON (hand-validated schema — no `Test-Json` in 5.1), Markdown runbooks. One PHP CLI file (staging-only identity probe, never in the web root).

## Global Constraints

- Spec is the single source of truth; this plan MUST NOT redefine any S1 security rule (spec §7 "Slices").
- Service identities: virtual accounts `NT SERVICE\Apache2.4` and `NT SERVICE\mysql` (`NT SERVICE\<verified actual service name>` when an existing service has another name — D3).
- `Test-LoamsServerHost.ps1` modes: `-Report` (**default, read-only**) and `-Converge`.
- First run on any gate PC is report-only: `-Converge` requires `-AcknowledgedReport` (same computer, elevated, ≤ 72 h, same `profileHash`) **and** a fresh live drift re-check immediately before the first change; any drift aborts with no change.
- Convergence is **recoverable, not atomic**: preflight → verified encrypted backup + verified off-host copy → named checkpoints → post-change validation → automatic rollback where safe → **RECOVERY REQUIRED** naming the failed checkpoint + manual steps.
- **Success is never reported after partial restoration** (rollback failure ⇒ `RecoveryRequired`, exit code 4, never 0).
- Runtime verification is from the modules actually loaded by the running `httpd` processes (paths, file versions, SHA-256), including PHP's linked OpenSSL — **never** `openssl.exe version`.
- **No new diagnostic HTTP endpoint** (post-change validation uses existing read-only endpoints; the staging probe is a piped logger, never in the web root).
- Manifest `deploy/stack/loams-stack-manifest.json` is the single source of truth for `Test-LoamsServerHost.ps1` now and Installer 2.0 later; schema-checked by the Pester suite (and later by the S1 gate). It stays `status: "draft"` (explicitly unapproved for production) until exact versions, packages and hashes are selected and tested.
- Manifest `status` ∈ `draft` | `staging-validated` | `approved`; `-Converge` refuses every non-`approved` manifest unless `-Staging` is passed **and** the host is an authorized staging host (admin-only marker + approved machine identity — D2). Production convergence against a non-approved manifest is always refused.
- Every `-Converge` run requires the operator to type this computer's exact name.
- Real component versions and hashes are recorded only by the documented human selection procedure (Task 19); agents MUST NOT invent versions or hashes.
- Least-privilege ACL profiles = spec §4 table (Apache) + owner decision D15 (MariaDB data/log directories: Administrators + SYSTEM full, MariaDB virtual account modify, **no Users / Authenticated Users / Apache**); compat log append-only `(AD,S)`; server key Apache + Administrators only.
- The MariaDB ACL and identity checkpoints are a production-convergence gate: a non-staging run refuses a plan without them and cannot report success unless they passed.
- Every backup set is encrypted to an offline recovery certificate with the manifest-pinned OpenSSL; no plaintext dump or file copy is written on the server; the server never holds the decryption key.
- One independently protected off-host copy, hash-verified (or explicitly operator-attested by typed SHA-256), before any production change; the only recovery copy is never the machine being migrated.
- Backup sets: hash manifest (`SHA256SUMS.json`), staging restore drill, 30-day retention by a privileged scheduled task.
- Post-change validation is read-only: no request to `student_login.php`, `rfid_login.php`, `guest_login.php`, `turnstile.php`, `turnstile_pull.php`, `reset_visits.php`; DB checks are read-only queries or the dedicated scratch schemas `loams_s1b_verify*`.
- `icacls` grants use well-known SIDs (`*S-1-5-32-544` Administrators, `*S-1-5-18` SYSTEM); virtual accounts are granted by name.
- DB credentials come from the deployed `config.php` or an interactive/`PSCredential` prompt, travel only in an admin-only `--defaults-extra-file`, and never appear in logs, transcripts, `-Verbose`/`-Debug`, exception messages, child-process arguments or backup manifests.
- Production MUST NOT be upgraded directly without staging validation; no arbitrary mixing of Apache/PHP/OpenSSL binaries (any loaded `libssl*`/`libcrypto*` not listed in the manifest ⇒ mismatch).
- No production deployment before S1f (invariant 8). S1b merges as tooling only.
- No secrets, real IPs, machine GUIDs, personal paths or real student data in committed files — placeholders `<server-LAN-IP>`, `<controller-IP>`, `<ADMIN_KEY>`, `REPLACE_ME`.
- Scripts run on Windows PowerShell 5.1: no `??`, `?:`, `?.`, `&&`/`||`, `ConvertFrom-Json -AsHashtable`, `Test-Json`, or .NET Core-only APIs.
- Manifest arrays are read with `Get-LoamsPropList` (always consumed as `@(...)`); `Get-LoamsProp` returns raw values and is never wrapped in `@()` (PS 5.1 nests the array — verified). Module functions that return collections emit elements; callers wrap in `@()`.
- Paths taken from the manifest (`server.tools[].relativePath`, module paths) must be plain relative paths strictly below the XAMPP root: no `.`/`..`, root or drive (validator), and before execution no escape and no junction/symbolic link (`Resolve-LoamsContainedPath`).
- Every mutation `-Converge` can make is listed in the Task 14 mutation inventory: captured before the change, undone automatically (no error suppression in any rollback path), restorable by the generated restore script, and part of the host fingerprint so drift and restoration are verifiable.
- The database is backed up only while MariaDB is running; a stopped MariaDB that holds data, or a host without the application database, is refused with instructions (D19). A backup is never skipped.
- `Success` requires a clean post-change report: Apache and MariaDB `Converged`, both ACL profiles compliant, event source registered, complete module inventory, no leftover verification schemas, and report outcome `Success` (staging: no runtime finding other than `ManifestNotFinal`). A dirty report fails `Validate` ⇒ rollback ⇒ `RolledBack` with the report kept in `<backup>\logs\post-change-report.json` (or `RecoveryRequired` if rollback fails).
- `-Report` never creates directories and fails closed if its target folder is missing; `C:\ProgramData\LOAMS\reports` is provisioned by the administrative runbook step B0 with a protected allowlist ACL (Administrators + SYSTEM FullControl only), verified by `Test-LoamsProtectedFolderAcl`; writes into that folder and `-Converge` preflight re-check the allowlist.
- `-Converge` validates `-ReportPath` (directly inside the protected reports folder) before any backup or checkpoint; a report save failure after a validated success is `SuccessReportNotSaved` (exit 6), never an unhandled error.
- Every run-owned DB credential file (`mysql-client-*.cnf`) is tracked before creation and must be gone at the end of every run; otherwise the outcome is `RecoveryRequired`.
- The deployed web root must contain `loams_api\uploads\default.jpg`; validation fails without it (D20).
- Acknowledged reports dated more than 5 minutes in the future are refused (clock-skew tolerance).
- Pester mock bodies run in the module scope: fixtures reach them through `$global:LoamsT*` variables (removed in `AfterAll`) or literals, never test-file `$script:` variables.
- Commits via the project `commit` skill (Conventional Commits); **no Claude/Anthropic co-author trailer** (standing owner rule).

### Verified facts on this dev box (2026-10-05, read-only commands)

| Fact | Evidence |
|---|---|
| Windows PowerShell 5.1.26100; `pwsh` not installed | `$PSVersionTable`, `Get-Command pwsh` |
| Only inbox Pester **3.4.0** installed (too old) | `Get-Module -ListAvailable Pester` |
| Apache/2.4.56 (Win64) Apache Lounge VS16; `ssl_module`, `php_module` loaded | `httpd.exe -v` / `-M` |
| Apache's mod_ssl links **OpenSSL 1.1.1t** (`apache\bin\libssl-1_1-x64.dll`); `apache\bin\openssl.exe` = OpenSSL 1.1.1t | file versions, `openssl.exe version` |
| PHP 8.2.4 ZTS VS16 x64 links **OpenSSL 3.0.8** (`php\libssl-3-x64.dll`) — two OpenSSL lineages are expected in one `httpd` process (to confirm elevated) | `php.exe -i`, file versions |
| PHP SAPI in Apache = `apache2handler` | `httpd-xampp.conf` |
| PHP writes: `upload_tmp_dir` / `session.save_path` = `C:\xampp\tmp`, `error_log` = `C:\xampp\php\logs\php_error_log`; API writes `loams_api\uploads\…`, `loams_api\logs\` | `php.ini`, `register_student.php`, `upload_students_zip.php`, `config.php` |
| Service `Apache2.4` runs as **LocalSystem**; MariaDB service `mysql` runs as **LocalSystem** with `--defaults-file=c:\xampp\mysql\bin\my.ini`; a separate `MySQL80` service exists (stopped) | `Win32_Service` |
| `my.ini [mysqld]`: `datadir=C:/xampp/mysql/data`, `tmpdir=C:/xampp/tmp`, `log_error=mysql_error.log` (in datadir), `pid_file=mysql.pid`, port 3306 | `my.ini` |
| `C:\xampp` and `C:\xampp\mysql\data` inherit **`Users:(RX)` and `Authenticated Users:(M)`** — any local user (and the Apache account) can read/modify application code and raw DB files today | `icacls` |
| `mysqldump` = MariaDB 10.4.28 client; `mysql.exe` present | `mysqldump --version` |
| Non-elevated: `Get-Process httpd` returns no modules; `[EventLog]::SourceExists` throws `SecurityException` | observed |
| `LOAMS-Transport` source absent; `C:\ProgramData\LOAMS` absent | `Test-Path` |
| `get_branding.php`, `get_departments.php`, `get_years.php`, `get_courses.php` are unauthenticated `SELECT`-only GETs returning JSON (used by read-only validation) | source inspection |
| The deployed bridge logs to `C:\ProgramData\LOAMS\loams-turnstile-bridge.log` as the interactive user | `loams-turnstile-bridge.ps1:41` |
| Qt kit OpenSSL: only 1.1 DLLs in `C:\Qt\Tools\mingw1310_64\opt\bin`; Qt 6.11.1 `plugins\tls` = `qcertonlybackend`, `qopensslbackend`, `qschannelbackend` | directory listing |

### Design decisions (owner-confirmed 2026-10-05 unless marked revised/new)

- **D1 — Stack upgrade is runbook-driven, not automated in S1b** (approved). The tool supplies `-Converge -BackupOnly`, `-Report` before/after, and the restore script.
- **D2 — Staging authorization (revised).** The admin-only `STAGING-HOST.marker` is necessary but not sufficient: the host's MachineGuid + computer name must be listed in an admin-only `approved-staging-hosts.json` (staging host only, never committed), and every `-Converge` run (staging or production) requires the operator to type the computer name.
- **D3 — Other service names** keep `NT SERVICE\<their name>` (approved; applies to MariaDB too).
- **D4 — Password-based service accounts ⇒ stop and report** (approved).
- **D5 — S1b-provisioned server paths** under `C:\ProgramData\LOAMS` (approved): `server\` (`compat.ini`, controller config), `server\logs\compat-guard.log` (pre-created, append-only), `server\tls\`, `server\tls\private\`, `backups\`, `reports\`, `tools\` (retention task), `backup-recipient\` (recovery certificate).
- **D6 — Inheritance on `C:\xampp` is broken and `Authenticated Users` write removed** (approved); `Users` keeps RX on the tree, **except** the MariaDB data/log directories (D15).
- **D7 — Gate entry**: S1b ships the Pester runner; `Invoke-LoamsSecurityGate.ps1` comes with S1d (approved).
- **D8 — Append-only starts at `(AD,S)`**, widened only by reviewed change after the staging probe (approved).
- **D9 — Report acknowledgement (revised):** `-AcknowledgedReport` (≤ 72 h, same computer, elevated, same `profileHash`) **plus** a live fingerprint re-check (service configuration/accounts, config-file hashes, loaded-module hashes, XAMPP-side ACLs) immediately before the first change; any drift aborts.
- **D10 — Staging identity probe runs as an Apache piped logger** (approved).
- **D11 — Legacy bridge log** stays writable by the bridge task's own account only (approved).
- **D12 — Acknowledged report maximum age 72 h** (approved, as part of D9).
- **D13 — Post-change validation (revised):** `get_branding.php` is only a smoke test; `Validate` also requires Apache and MariaDB running under their virtual accounts, PHP+DB reads through `get_departments.php`/`get_years.php`/`get_courses.php`, the legacy static photo fallback, and a read-only DB query — never an attendance write. Default base URL `http://127.0.0.1/loams_api/`; after S1d pass `-BaseUrl https://localhost/loams_api/`.
- **D14 — Backups (revised):** CMS (`openssl cms -encrypt -binary -stream -aes256`) to an offline recovery certificate using the manifest-pinned `openssl.exe`; plaintext streamed (dump from `mysqldump` stdout; files through the in-process `LOAMSARC1` container) so nothing plaintext is written on the server; `SHA256SUMS.json` integrity manifest; hash-verified export to removable/USB/approved network storage (or typed-SHA-256 operator attestation) required before any change; staging restore drill; 30-day retention via a SYSTEM scheduled task. Rollback metadata (service configuration, per-path ACL saves without `/t`) stays plaintext, admin-only, and contains no secrets or student data so automatic rollback works without the recovery key.
- **D15 — MariaDB hardening in S1b (new, replaces "defer to S4"):** detect and classify the XAMPP `mysqld` like Apache; virtual account `NT SERVICE\mysql`; data and log directories protected (Administrators + SYSTEM full, MariaDB account modify, no Users/Authenticated Users/Apache); tmpdir and tree grants; verification of startup, reads, writes and backup/restore on scratch schemas only; staging validation first; mandatory production-convergence gate.
- **D16 — Control-Panel-launched MariaDB is registered as a service (new):** `mysqld --install mysql --defaults-file=<my.ini>`, stopped gracefully first with `mysqladmin shutdown` — a virtual account exists only for a service, and a Control-Panel `mysqld` would keep the data directory tied to the logged-on user.
- **D17 — `LOAMSARC1` container (new):** a minimal length-prefixed, per-entry-SHA-256 stream format so many files are encrypted as one CMS stream without a plaintext archive; keeps student file names out of every plaintext artefact.
- **D19 — Database backup readiness (new, Codex round 1):** dump only while MariaDB runs; stopped-with-data and no-application-database hosts are refused with instructions; S1b never auto-starts a database before backing it up and never skips the backup (Task 13 table).
- **D20 — Legacy photo fallback (new, Codex round 1):** `uploads\default.jpg` must be deployed and served as an image; its absence fails validation instead of passing silently.
- **D21 — Restore covers every mutation (new, Codex round 1):** 17-row mutation inventory (Task 14); the backup set carries a module copy and prior-state capture (`loams-state.json`, task XML, tools copies); the generated restore script verifies its inputs inside the step machinery and exits 4 naming the failed step; the fingerprint includes LOAMS-owned state (event source, retention task, tools, layout).
- **D24 — Round-3 decisions (new):** final-report save failure after success ⇒ `SuccessReportNotSaved` (exit 6); credential-file leftovers ⇒ `RecoveryRequired` (17th inventory row); the default `-ReportPath` for both modes is `<ProgramDataRoot>\reports\loams-host-report-<computer>-<UTC>.json`.
- **D23 — Round-2 decisions (new):** dirty post-change report ⇒ rollback ⇒ `RolledBack` with the report kept as evidence (`RecoveryRequired` only if rollback fails); verification schema names are per run (`loams_s1b_verify_<set id>`, `…_r`) and only those are ever dropped; the restore requires elevation only when a step touches services, ACLs, the event log, a scheduled task or the database.
- **D22 — Manifest access contract (new, Codex round 1):** `Get-LoamsProp` raw / `Get-LoamsPropList` enumerating, with tests that fail on the PS 5.1 nested-array bug.
- **D18 — Cipher (new):** AES-256-CBC CMS EnvelopedData (`-aes256`) plus external SHA-256 hashes; CMS AuthEnvelopedData (GCM) to be evaluated on the pinned OpenSSL 3 in staging and adopted by reviewed change if supported end-to-end.

---

## File Structure

| File | Responsibility |
|---|---|
| `deploy/stack/loams-stack-manifest.json` | The versioned compatibility manifest (server + client sections + pinned tools). Committed with `status: "draft"` and `REPLACE_ME` values until Task 19 fills it. |
| `deploy/stack/loams-stack-manifest.schema.json` | JSON Schema (draft-07) documenting the fixed shape; mirrored exactly by the PowerShell validator. |
| `deploy/server/Test-LoamsServerHost.ps1` | Thin entry: parameters, module import, dispatch to report/converge, exit code. |
| `deploy/server/LoamsHost/LoamsHost.psm1` | Module root: dot-sources every `LoamsHost.*.ps1`, holds module state, exports `*-Loams*`. |
| `deploy/server/LoamsHost/LoamsHost.Common.ps1` | Mode flag + mutation guard, external-process wrapper, argument quoting, secret redaction, logging, elevation check, exit codes. |
| `deploy/server/LoamsHost/LoamsHost.Manifest.ps1` | Read + validate the manifest (hand-written validator), status gate. |
| `deploy/server/LoamsHost/LoamsHost.ManifestRecord.ps1` | Digests, publisher-checksum comparison, manifest module entries for the human selection procedure. |
| `deploy/server/LoamsHost/LoamsHost.Detection.ps1` | Apache service / process / XAMPP-layout / `httpd -v -M` / PHP SAPI detection. |
| `deploy/server/LoamsHost/LoamsHost.RuntimeModules.ps1` | Loaded-module enumeration of running `httpd` and comparison to the manifest. |
| `deploy/server/LoamsHost/LoamsHost.Acl.ps1` | Apache ACL profile, `icacls` application, compliance against a service token. |
| `deploy/server/LoamsHost/LoamsHost.EventSource.ps1` | `LOAMS-Transport` source registration + static writability check. |
| `deploy/server/LoamsHost/LoamsHost.ServiceIdentity.ps1` | Apache service registration, virtual-account switch, generic service start/stop. |
| `deploy/server/LoamsHost/LoamsHost.Classification.ps1` | Shared service-situation algorithm, Apache classification, host fingerprint + drift comparison. |
| `deploy/server/LoamsHost/LoamsHost.MariaDb.ps1` | MariaDB detection, `my.ini` parsing, classification, ACL profile, service registration, graceful Control-Panel stop. |
| `deploy/server/LoamsHost/LoamsHost.HostState.ps1` | Read-only LOAMS-owned state (event source, retention task, deployed tools, `ProgramData` layout) for the fingerprint. |
| `deploy/server/LoamsHost/LoamsHost.Report.ps1` | Live fingerprint, build/render/save the host report; `Invoke-LoamsHostReport`. |
| `deploy/server/LoamsHost/LoamsHost.Crypto.ps1` | Pinned OpenSSL, recovery-certificate pin, streaming CMS encryption, CMS check, decryption for recovery. |
| `deploy/server/LoamsHost/LoamsHost.BackupArchive.ps1` | `LOAMSARC1` container writer/reader and encrypted-archive expansion. |
| `deploy/server/LoamsHost/LoamsHost.Secrets.ps1` | DB connection info, admin-only client defaults file, encrypted streamed DB dump. |
| `deploy/server/LoamsHost/LoamsHost.Database.ps1` | Read-only queries and scratch-schema verification of MariaDB (reads, writes, backup/restore). |
| `deploy/server/LoamsHost/LoamsHost.Backup.ps1` | Encrypted backup set, integrity manifest, verification, ACL restore, retention + its scheduled task. |
| `deploy/server/LoamsHost/LoamsHost.Restore.ps1` | Full restore of every inventoried mutation (`Invoke-LoamsHostRestore`), start-type restore. |
| `deploy/server/LoamsHost/LoamsHost.OffHost.ps1` | Off-host destination classification, hash-verified export, operator attestation, verification record. |
| `deploy/server/LoamsHost/LoamsHost.Checkpoints.ps1` | Generic checkpoint engine with rollback and RECOVERY REQUIRED. |
| `deploy/server/LoamsHost/LoamsHost.Authorization.ps1` | Staging authorization, typed operator confirmation, acknowledged report, pre-change drift check. |
| `deploy/server/LoamsHost/LoamsHost.Validation.ps1` | Read-only post-change validation suite. |
| `deploy/server/LoamsHost/LoamsHost.Converge.ps1` | Converge preflight, checkpoint plan (Apache + MariaDB), production-plan gate, outcome; `Invoke-LoamsHostConverge`. |
| `deploy/server/LoamsHost/Restore-LoamsHostBackup.template.ps1` | Restore wrapper copied into every backup set (not dot-sourced): verifies rollback data, then runs the module copy stored in the set. |
| `deploy/server/staging/LoamsIdentityProbe.php` | Staging-only piped-logger probe (append-only, Event Log, no access to backups/MariaDB data). Never deployed to `htdocs`. |
| `deploy/tests/Invoke-LoamsPesterSuite.ps1` | Runs all `deploy/tests/*.Tests.ps1` under Pester 5; zero failed/skipped/not-run; expected minimum count. |
| `deploy/tests/expected-test-count.txt` | Committed minimum passing-test count. |
| `deploy/tests/TestHelpers.ps1` | Fake XAMPP tree (incl. MariaDB), sample manifest, report mocks, test OpenSSL + throwaway recovery certificate. |
| `deploy/tests/*.Tests.ps1` | `Common`, `Manifest`, `ManifestRecord`, `Detection`, `RuntimeModules`, `Acl`, `EventSource`, `ServiceIdentity`, `Classification`, `MariaDb`, `Report`, `Crypto`, `Secrets`, `Database`, `Backup`, `Checkpoints`, `Authorization`, `Validation`, `Converge`, `StagingArtifacts`. |
| `docs/security/runbooks/stack-upgrade-staging.md` | Component selection, recovery key, staging upgrade, staging verifications, rollback/restore/off-host/retention, patch approval, production first run. |
| `docs/security/evidence/<date>-stack-v1.0-staging.md` | Staging evidence written by the Task 19 human procedure (synthetic data; placeholders for host identifiers). |
| `deploy/README.md` | Dev prerequisites (Pester 5, an `openssl.exe` for tests) and how to run the suite and the tool. |

Line-ending/encoding: all `.ps1`/`.psm1` files are saved **UTF-8 with BOM** (Windows PowerShell 5.1 reads BOM-less files as the ANSI code page) and use only ASCII in code; `.json`/`.md` are UTF-8 without BOM.

Test prerequisites (dev and gate machines): Pester ≥ 5.5, a PHP 8 CLI (`LOAMS_PHP`, default `C:\xampp\php\php.exe`) and an `openssl.exe` 1.1.1+/3.x (`LOAMS_OPENSSL`, default `C:\xampp\apache\bin\openssl.exe`) for the crypto round-trip tests. A missing prerequisite is a test **failure**, never a skip.

---

### Task 1: Dev prerequisite (Pester 5), module skeleton, common helpers, suite runner

**Files:**
- Create: `deploy/README.md`
- Create: `deploy/server/LoamsHost/LoamsHost.psm1`
- Create: `deploy/server/LoamsHost/LoamsHost.Common.ps1`
- Create: `deploy/tests/TestHelpers.ps1`
- Create: `deploy/tests/Common.Tests.ps1`
- Create: `deploy/tests/Invoke-LoamsPesterSuite.ps1`
- Create: `deploy/tests/expected-test-count.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `Set-LoamsMode -Mode <'Report'|'Converge'>` → `$null`; `Get-LoamsMode` → `[string]`.
  - `Assert-LoamsMutationAllowed -Action <string>` → throws `"LOAMS-READONLY: '<Action>' refused in Report mode"` unless mode is `Converge`.
  - `ConvertTo-LoamsArgumentString -ArgumentList <string[]>` → `[string]` (Windows `CommandLineToArgvW` quoting).
  - `Register-LoamsSecret -Secret <string>`; `Clear-LoamsSecrets`; `Protect-LoamsText -Text <string>` → `[string]` with every registered secret replaced by `<REDACTED>`.
  - `Set-LoamsLogPath -Path <string>`; `Write-LoamsLog -Message <string> [-Level <'Info'|'Warn'|'Error'>]` → appends `"<UTC ISO> [<Level>] <redacted message>"` to the log file (if set) and to the host.
  - `Invoke-LoamsExternal -FilePath <string> [-ArgumentList <string[]>] [-AllowNonZeroExit]` → `[pscustomobject]@{ ExitCode=[int]; StdOut=[string]; StdErr=[string] }`; throws a redacted message on non-zero exit unless `-AllowNonZeroExit`.
  - `Test-LoamsElevated` → `[bool]`.
  - `Get-LoamsExitCode -Outcome <string>` → `[int]` (`Success`=0, `Refused`=1, `StopAndReport`=2, `Incomplete`=3, `RecoveryRequired`=4, `RolledBack`=5, `SuccessReportNotSaved`=6 — host converged and validated, but the final report could not be written to `-ReportPath`).
  - Test helpers (dot-sourced by tests only): `New-LoamsFakeXampp -Root <string>` → `[string]`; `Get-LoamsTestSha256 -Path <string>` → `[string]` (lower-case hex); `New-LoamsTestManifest -XamppRoot <string> [-Status <string>]` → `[hashtable]`; `Save-LoamsTestManifest -Manifest <hashtable> -Path <string>`.

- [ ] **Step 1: Install the dev prerequisite (one-time, per developer machine)**

Windows PowerShell 5.1 ships Pester 3.4.0, which cannot run Pester 5 syntax. Install Pester 5 for the current user (no admin rights needed, not vendored into the repo):

```powershell
Install-Module Pester -MinimumVersion 5.5 -Scope CurrentUser -SkipPublisherCheck -Force
Get-Module -ListAvailable Pester | Sort-Object Version -Descending | Select-Object -First 1 Name, Version
```

Expected: a `5.x` version (≥ 5.5.0) listed first. `-SkipPublisherCheck` is needed because the inbox 3.4.0 is signed by a different publisher. This is a dev-only prerequisite and is never run on a gate PC.

- [ ] **Step 2: Write `deploy/README.md`**

````markdown
# LOAMS deployment tooling

Windows PowerShell 5.1 tooling for the LOAMS server host (spec: `docs/superpowers/specs/2026-10-05-loams-s1-transport-security-design.md`).

## Dev prerequisite: Pester 5

Windows ships Pester 3.4, which cannot run these tests. Once per developer machine:

```powershell
Install-Module Pester -MinimumVersion 5.5 -Scope CurrentUser -SkipPublisherCheck -Force
```

Pester is a dev/test dependency only. It is never installed on a gate PC.

## Run the suite

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\tests\Invoke-LoamsPesterSuite.ps1
```

Fails on any failed, skipped or not-run test, or when fewer tests pass than `deploy/tests/expected-test-count.txt`.

The suite also needs a PHP 8 CLI (`LOAMS_PHP`, default `C:\xampp\php\php.exe`) and an `openssl.exe` 1.1.1+/3.x (`LOAMS_OPENSSL`, default `C:\xampp\apache\bin\openssl.exe`) for the probe lint and the real encryption round trips. A missing prerequisite fails the suite; it is never skipped.

## Host tool

```powershell
# Read-only (default). Run elevated so loaded modules can be verified.
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\server\Test-LoamsServerHost.ps1 -Report
```

`-Converge` is documented in `docs/security/runbooks/stack-upgrade-staging.md`. The first run on any gate PC is report-only.
````

- [ ] **Step 3: Write the failing tests `deploy/tests/Common.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
}

Describe 'Mode guard' {
    It 'refuses mutations in Report mode' {
        Set-LoamsMode -Mode Report
        { Assert-LoamsMutationAllowed -Action 'sc.exe config' } |
            Should -Throw -ExpectedMessage "LOAMS-READONLY: 'sc.exe config' refused in Report mode"
    }
    It 'allows mutations in Converge mode' {
        Set-LoamsMode -Mode Converge
        { Assert-LoamsMutationAllowed -Action 'sc.exe config' } | Should -Not -Throw
        Set-LoamsMode -Mode Report
    }
    It 'defaults to Report mode on import' {
        Set-LoamsMode -Mode Converge
        Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
        Get-LoamsMode | Should -Be 'Report'
    }
}

Describe 'ConvertTo-LoamsArgumentString' {
    It 'leaves simple arguments bare' {
        ConvertTo-LoamsArgumentString -ArgumentList @('-k', 'install') | Should -Be '-k install'
    }
    It 'quotes arguments with spaces' {
        ConvertTo-LoamsArgumentString -ArgumentList @('obj=', 'NT SERVICE\Apache2.4') |
            Should -Be 'obj= "NT SERVICE\Apache2.4"'
    }
    It 'renders empty arguments as a quoted empty string' {
        ConvertTo-LoamsArgumentString -ArgumentList @('password=', '') | Should -Be 'password= ""'
    }
    It 'escapes embedded quotes and trailing backslashes' {
        ConvertTo-LoamsArgumentString -ArgumentList @('a"b', 'C:\dir with space\') |
            Should -Be '"a\"b" "C:\dir with space\\"'
    }
}

Describe 'Secret redaction and logging' {
    BeforeEach { Clear-LoamsSecrets }
    It 'redacts registered secrets' {
        Register-LoamsSecret -Secret 'S1b-test-db-pass'
        Protect-LoamsText -Text 'pw=S1b-test-db-pass end' | Should -Be 'pw=<REDACTED> end'
    }
    It 'ignores empty secrets' {
        Register-LoamsSecret -Secret ''
        Protect-LoamsText -Text 'abc' | Should -Be 'abc'
    }
    It 'writes redacted lines to the log file' {
        $log = Join-Path $TestDrive 'host.log'
        Set-LoamsLogPath -Path $log
        Register-LoamsSecret -Secret 'S1b-test-db-pass'
        Write-LoamsLog -Message 'leak? S1b-test-db-pass' -Level Warn
        $text = Get-Content -Raw -Path $log
        $text | Should -Match '\[Warn\] leak\? <REDACTED>'
        $text | Should -Not -Match 'S1b-test-db-pass'
        Set-LoamsLogPath -Path ''
    }
}

Describe 'Invoke-LoamsExternal' {
    BeforeEach { Clear-LoamsSecrets }
    It 'captures stdout and exit code' {
        $r = Invoke-LoamsExternal -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList @('/c', 'echo', 'hello')
        $r.ExitCode | Should -Be 0
        $r.StdOut.Trim() | Should -Be 'hello'
    }
    It 'throws a redacted message on non-zero exit' {
        Register-LoamsSecret -Secret 'S1b-test-db-pass'
        $err = $null
        try {
            Invoke-LoamsExternal -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList @('/c', 'echo S1b-test-db-pass 1>&2 & exit 3')
        } catch { $err = $_.Exception.Message }
        $err | Should -Match 'exit code 3'
        $err | Should -Not -Match 'S1b-test-db-pass'
    }
    It 'returns non-zero exit codes when allowed' {
        $r = Invoke-LoamsExternal -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList @('/c', 'exit 5') -AllowNonZeroExit
        $r.ExitCode | Should -Be 5
    }
}

Describe 'Exit codes' {
    It 'maps outcomes' {
        Get-LoamsExitCode -Outcome Success | Should -Be 0
        Get-LoamsExitCode -Outcome Refused | Should -Be 1
        Get-LoamsExitCode -Outcome StopAndReport | Should -Be 2
        Get-LoamsExitCode -Outcome Incomplete | Should -Be 3
        Get-LoamsExitCode -Outcome RecoveryRequired | Should -Be 4
        Get-LoamsExitCode -Outcome RolledBack | Should -Be 5
        Get-LoamsExitCode -Outcome SuccessReportNotSaved | Should -Be 6
    }
}
```

- [ ] **Step 4: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Common.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Import-Module` cannot find `LoamsHost.psm1`.

- [ ] **Step 5: Implement `deploy/server/LoamsHost/LoamsHost.psm1`**

```powershell
#Requires -Version 5.1
Set-StrictMode -Version 2.0

$script:LoamsMode = 'Report'
$script:LoamsSecrets = New-Object System.Collections.Generic.List[string]
$script:LoamsLogPath = ''

Get-ChildItem -Path $PSScriptRoot -Filter 'LoamsHost.*.ps1' | Sort-Object Name | ForEach-Object {
    . $_.FullName
}

Export-ModuleMember -Function '*-Loams*'
```

Every later task adds one `LoamsHost.<Area>.ps1`; the root module picks it up automatically, so no task edits this file again.

- [ ] **Step 6: Implement `deploy/server/LoamsHost/LoamsHost.Common.ps1`**

```powershell
# Common helpers: mode guard, external processes, redaction, logging, exit codes.

function Set-LoamsMode {
    [CmdletBinding()]
    param([Parameter(Mandatory)][ValidateSet('Report', 'Converge')][string] $Mode)
    $script:LoamsMode = $Mode
}

function Get-LoamsMode { [CmdletBinding()] param() return $script:LoamsMode }

function Assert-LoamsMutationAllowed {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Action)
    if ($script:LoamsMode -ne 'Converge') {
        throw "LOAMS-READONLY: '$Action' refused in Report mode"
    }
}

function ConvertTo-LoamsArgumentString {
    [CmdletBinding()]
    param([AllowEmptyCollection()][AllowEmptyString()][string[]] $ArgumentList = @())
    $parts = foreach ($arg in $ArgumentList) {
        if ($null -eq $arg -or $arg -eq '') { '""'; continue }
        if ($arg -notmatch '[\s"]') { $arg; continue }
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.Append('"')
        $backslashes = 0
        foreach ($ch in $arg.ToCharArray()) {
            if ($ch -eq '\') { $backslashes++; continue }
            if ($ch -eq '"') {
                [void]$sb.Append('\' * ($backslashes * 2 + 1)); [void]$sb.Append('"')
            } else {
                if ($backslashes -gt 0) { [void]$sb.Append('\' * $backslashes) }
                [void]$sb.Append($ch)
            }
            $backslashes = 0
        }
        if ($backslashes -gt 0) { [void]$sb.Append('\' * ($backslashes * 2)) }
        [void]$sb.Append('"')
        $sb.ToString()
    }
    return ($parts -join ' ')
}

function Register-LoamsSecret {
    [CmdletBinding()]
    param([AllowEmptyString()][AllowNull()][string] $Secret)
    if ([string]::IsNullOrEmpty($Secret)) { return }
    if (-not $script:LoamsSecrets.Contains($Secret)) { $script:LoamsSecrets.Add($Secret) }
}

function Clear-LoamsSecrets { [CmdletBinding()] param() $script:LoamsSecrets.Clear() }

function Protect-LoamsText {
    [CmdletBinding()]
    param([AllowEmptyString()][AllowNull()][string] $Text)
    if ([string]::IsNullOrEmpty($Text)) { return $Text }
    $out = $Text
    foreach ($s in $script:LoamsSecrets) { $out = $out.Replace($s, '<REDACTED>') }
    return $out
}

function Set-LoamsLogPath {
    [CmdletBinding()]
    param([AllowEmptyString()][string] $Path)
    $script:LoamsLogPath = $Path
}

function Write-LoamsLog {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string] $Message,
        [ValidateSet('Info', 'Warn', 'Error')][string] $Level = 'Info'
    )
    $line = '{0} [{1}] {2}' -f ([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')), $Level, (Protect-LoamsText -Text $Message)
    if ($script:LoamsLogPath) {
        Add-Content -Path $script:LoamsLogPath -Value $line -Encoding UTF8
    }
    Write-Host $line
}

function Invoke-LoamsExternal {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $FilePath,
        [AllowEmptyCollection()][AllowEmptyString()][string[]] $ArgumentList = @(),
        [switch] $AllowNonZeroExit
    )
    $argString = ConvertTo-LoamsArgumentString -ArgumentList $ArgumentList
    Write-LoamsLog -Message ("exec: {0} {1}" -f $FilePath, $argString)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = $argString
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    $proc.WaitForExit()
    $result = [pscustomobject]@{
        ExitCode = $proc.ExitCode
        StdOut   = $stdoutTask.Result
        StdErr   = $stderrTask.Result
    }
    if ($result.ExitCode -ne 0 -and -not $AllowNonZeroExit) {
        $msg = "External command '{0}' failed with exit code {1}: {2}" -f (Split-Path -Leaf $FilePath), $result.ExitCode, $result.StdErr.Trim()
        throw (Protect-LoamsText -Text $msg)
    }
    return $result
}

function Test-LoamsElevated {
    [CmdletBinding()] param()
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-LoamsExitCode {
    [CmdletBinding()]
    param([Parameter(Mandatory)][ValidateSet('Success', 'Refused', 'StopAndReport', 'Incomplete', 'RecoveryRequired', 'RolledBack', 'SuccessReportNotSaved')][string] $Outcome)
    $map = @{ Success = 0; Refused = 1; StopAndReport = 2; Incomplete = 3; RecoveryRequired = 4; RolledBack = 5; SuccessReportNotSaved = 6 }
    return $map[$Outcome]
}
```

The external command line is logged **after** redaction; Task 13 additionally guarantees no secret is ever placed in `ArgumentList` (redaction is defence in depth, not the control).

- [ ] **Step 7: Implement `deploy/tests/TestHelpers.ps1`**

```powershell
# Shared Pester fixtures. Synthetic data only (no real hosts, IPs, credentials or student data).

function New-LoamsFakeXampp {
    param([Parameter(Mandatory)][string] $Root)
    $files = @{
        'apache\bin\httpd.exe'                 = 'fake-httpd'
        'apache\bin\libhttpd.dll'              = 'fake-libhttpd'
        'apache\bin\libssl-3-x64.dll'          = 'fake-apache-libssl3'
        'apache\bin\libcrypto-3-x64.dll'       = 'fake-apache-libcrypto3'
        'apache\modules\mod_ssl.so'            = 'fake-mod-ssl'
        'apache\conf\httpd.conf'               = "ServerRoot `"C:/xampp/apache`"`r`nListen 80`r`nLoadModule ssl_module modules/mod_ssl.so`r`nInclude `"conf/extra/httpd-xampp.conf`"`r`n"
        'apache\conf\extra\httpd-xampp.conf'   = "LoadFile `"C:/xampp/php/php8ts.dll`"`r`nLoadModule php_module `"C:/xampp/php/php8apache2_4.dll`"`r`n"
        'apache\conf\extra\httpd-ssl.conf'     = "Listen 443`r`n"
        'apache\logs\error.log'                = ''
        'php\php.exe'                          = 'fake-php'
        'php\php8ts.dll'                       = 'fake-php8ts'
        'php\php8apache2_4.dll'                = 'fake-php-apache'
        'php\libssl-3-x64.dll'                 = 'fake-php-libssl3'
        'php\libcrypto-3-x64.dll'              = 'fake-php-libcrypto3'
        'php\php.ini'                          = "upload_tmp_dir=`"C:\xampp\tmp`"`r`nsession.save_path=`"C:\xampp\tmp`"`r`nauto_prepend_file=`r`n"
        'php\logs\php_error_log'               = ''
        'mysql\bin\mysqldump.exe'              = 'fake-mysqldump'
        'tmp\keep.txt'                         = ''
        'xampp-control.exe'                    = 'fake-control'
        'htdocs\loams_api\config.php'          = "<?php`r`ndefine('DB_HOST', 'localhost');`r`ndefine('DB_USER', 'loams_test');`r`ndefine('DB_PASS', 'S1b-test-db-pass');`r`ndefine('DB_NAME', 'wits_test');`r`n"
        'htdocs\loams_api\get_branding.php'    = '<?php echo "{}";'
        'htdocs\loams_api\uploads\default.jpg' = 'JPEGDATA'
        'htdocs\loams_api\logs\keep.txt'       = ''
        'apache\bin\openssl.exe'               = 'fake-openssl'
        'mysql\bin\mysqld.exe'                 = 'fake-mysqld'
        'mysql\bin\mysql.exe'                  = 'fake-mysql'
        'mysql\bin\mysqladmin.exe'             = 'fake-mysqladmin'
        'mysql\bin\my.ini'                     = ("[client]`r`nport=3306`r`n[mysqld]`r`nport=3306`r`nbasedir=`"{0}/mysql`"`r`ntmpdir=`"{0}/tmp`"`r`ndatadir=`"{0}/mysql/data`"`r`nlog_error=`"mysql_error.log`"`r`n" -f ($Root -replace '\\', '/'))
        'mysql\data\ibdata1'                   = 'fake-innodb'
        'mysql\data\mysql_error.log'           = ''
    }
    foreach ($rel in $files.Keys) {
        $full = Join-Path $Root $rel
        $dir = Split-Path -Parent $full
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        [IO.File]::WriteAllText($full, $files[$rel])
    }
    return $Root
}

function Get-LoamsTestSha256 {
    param([Parameter(Mandatory)][string] $Path)
    return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function New-LoamsTestManifest {
    param(
        [Parameter(Mandatory)][string] $XamppRoot,
        [ValidateSet('draft', 'staging-validated', 'approved')][string] $Status = 'approved'
    )
    $mod = {
        param($rel)
        @{ relativePath = $rel; sha256 = (Get-LoamsTestSha256 -Path (Join-Path $XamppRoot $rel)); fileVersion = '1.0.0' }
    }
    $fakeHash = ('ab' * 32)
    return @{
        schemaVersion   = 1
        manifestVersion = '1.0'
        status          = $Status
        approval        = @{ approvedBy = 'Test Approver'; approvedOn = '2026-10-05'; stagingEvidence = 'docs/security/evidence/test-staging.md' }
        server          = @{
            name                  = 'LOAMS Server Stack v1.0'
            layout                = 'xampp'
            phpSapi               = 'apache2handler'
            apacheVersion         = '2.4.99'
            phpVersion            = '8.3.99'
            requiredApacheModules = @('ssl_module', 'php_module')
            requiredPhpExtensions = @('openssl', 'mysqli', 'zip')
            cryptoModulePatterns  = @('libssl*.dll', 'libcrypto*.dll')
            tools                 = @( @{ name = 'openssl'; relativePath = 'apache\bin\openssl.exe'; sha256 = (Get-LoamsTestSha256 -Path (Join-Path $XamppRoot 'apache\bin\openssl.exe')); fileVersion = '1.0.0' } )
            components            = @(
                @{ id = 'apache'; name = 'Apache HTTP Server'; version = '2.4.99'
                   source = @{ url = 'https://www.apachelounge.com/download/'; verification = 'publisher SHA-256 compared' }
                   archive = @{ fileName = 'httpd-test.zip'; sha256 = $fakeHash }
                   modules = @( (& $mod 'apache\bin\libhttpd.dll'), (& $mod 'apache\bin\libssl-3-x64.dll'), (& $mod 'apache\bin\libcrypto-3-x64.dll'), (& $mod 'apache\modules\mod_ssl.so') ) },
                @{ id = 'php'; name = 'PHP'; version = '8.3.99'
                   source = @{ url = 'https://windows.php.net/download/'; verification = 'publisher SHA-256 compared' }
                   archive = @{ fileName = 'php-test.zip'; sha256 = $fakeHash }
                   modules = @( (& $mod 'php\php8ts.dll'), (& $mod 'php\php8apache2_4.dll'), (& $mod 'php\libssl-3-x64.dll'), (& $mod 'php\libcrypto-3-x64.dll') ) }
            )
        }
        client          = @{
            name            = 'LOAMS Client TLS Runtime v1.0'
            qtVersion       = '6.11.1'
            tlsBackend      = 'openssl'
            excludedPlugins = @('plugins/tls/qschannelbackend.dll', 'plugins/tls/qcertonlybackend.dll')
            components      = @(
                @{ id = 'openssl'; name = 'OpenSSL'; version = '3.0.99'
                   source = @{ url = 'https://download.qt.io/'; verification = 'Qt Maintenance Tool OpenSSL 3 toolkit' }
                   archive = @{ fileName = 'openssl-test.7z'; sha256 = $fakeHash }
                   modules = @( @{ relativePath = 'libssl-3-x64.dll'; sha256 = $fakeHash; fileVersion = '3.0.99' }, @{ relativePath = 'libcrypto-3-x64.dll'; sha256 = $fakeHash; fileVersion = '3.0.99' } ) },
                @{ id = 'qopensslbackend'; name = 'Qt OpenSSL TLS backend'; version = '6.11.1'
                   source = @{ url = 'https://download.qt.io/'; verification = 'Qt online installer' }
                   archive = @{ fileName = 'qt-6.11.1-mingw.7z'; sha256 = $fakeHash }
                   modules = @( @{ relativePath = 'plugins/tls/qopensslbackend.dll'; sha256 = $fakeHash; fileVersion = '6.11.1' } ) }
            )
        }
    }
}

function Save-LoamsTestManifest {
    param([Parameter(Mandatory)][hashtable] $Manifest, [Parameter(Mandatory)][string] $Path)
    [IO.File]::WriteAllText($Path, ($Manifest | ConvertTo-Json -Depth 12))
}
```

- [ ] **Step 8: Implement `deploy/tests/Invoke-LoamsPesterSuite.ps1` and `deploy/tests/expected-test-count.txt`**

```powershell
#Requires -Version 5.1
<#
    Runs the LOAMS deployment Pester suite. Fails (exit 1) on any failed,
    skipped or not-run test, or when fewer tests pass than the committed
    minimum in expected-test-count.txt. Composed later by the S1 gate (S1d).
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Import-Module Pester -MinimumVersion 5.5 -ErrorAction Stop

$config = New-PesterConfiguration
$config.Run.Path = $PSScriptRoot
$config.Run.PassThru = $true
$config.Run.Exit = $false
$config.Output.Verbosity = 'Detailed'
$result = Invoke-Pester -Configuration $config

$expected = [int](Get-Content -Path (Join-Path $PSScriptRoot 'expected-test-count.txt') -TotalCount 1)
$problems = @()
if ($result.FailedCount -gt 0)  { $problems += "$($result.FailedCount) failed" }
if ($result.SkippedCount -gt 0) { $problems += "$($result.SkippedCount) skipped" }
if ($result.NotRunCount -gt 0)  { $problems += "$($result.NotRunCount) not run" }
if ($result.PassedCount -lt $expected) { $problems += "passed $($result.PassedCount) < expected minimum $expected" }
if ($problems.Count -gt 0) {
    Write-Host ("LOAMS Pester suite FAILED: " + ($problems -join '; '))
    exit 1
}
Write-Host ("LOAMS Pester suite PASSED: {0} tests (minimum {1})" -f $result.PassedCount, $expected)
exit 0
```

`deploy/tests/expected-test-count.txt` holds a single integer: the cumulative passing count. Every task's final step raises it to that task's cumulative total (listed in each task). After Task 1:

```
14
```

- [ ] **Step 9: Run to verify it passes**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\tests\Invoke-LoamsPesterSuite.ps1
```

Expected: `LOAMS Pester suite PASSED: 14 tests (minimum 14)`, exit code 0.

- [ ] **Step 10: Commit**

Via the project `commit` skill — subject: `build(deploy): add LoamsHost module skeleton, common helpers and Pester 5 runner`

### Task 2: Manifest, JSON schema, PowerShell 5.1 validator and status gate

**Files:**
- Create: `deploy/stack/loams-stack-manifest.json`
- Create: `deploy/stack/loams-stack-manifest.schema.json`
- Create: `deploy/server/LoamsHost/LoamsHost.Manifest.ps1`
- Modify: `deploy/tests/TestHelpers.ps1` (append `ConvertTo-LoamsManifestObject`)
- Create: `deploy/tests/Manifest.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `42`

**Interfaces:**
- Consumes: Task 1 module skeleton.
- Produces:
  - `Get-LoamsProp -Object <object> -Name <string>` → the **raw** property value or `$null`, emitted without pipeline enumeration (an array stays one object). Contract: assign it to a variable or pass it as an argument; **never wrap it in `@()`** — in Windows PowerShell 5.1 that yields a one-element array containing the array (verified). Used by the validator, which must see whether a JSON value is an array.
  - `Get-LoamsPropList -Object <object> -Name <string>` → the property's elements **enumerated onto the pipeline** (nothing for absent, `$null` or empty). Contract: always consume as `@(Get-LoamsPropList ...)`. Every consumer that iterates manifest arrays (Tasks 5, 12) uses it; functions in this module that return collections follow the same rule (they emit elements; callers wrap in `@()`).
  - `Read-LoamsStackManifest -Path <string>` → `[pscustomobject]` (parsed JSON); throws `"Manifest '<path>' is not valid JSON: ..."` or `"Manifest '<path>' failed schema validation: <errors joined by '; '>"`.
  - `Test-LoamsStackManifest -Manifest <pscustomobject>` → `[pscustomobject]@{ IsValid=[bool]; Errors=[string[]] }`.
  - `Get-LoamsManifestSchemaKeys` → `[pscustomobject]@{ Top=[string[]]; Statuses=[string[]]; Server=[string[]]; Client=[string[]]; Component=[string[]]; Module=[string[]] }` (the validator's constants; used by the schema-mirror test).
  - `Test-LoamsManifestStatusGate -Manifest <pscustomobject> [-Staging] -IsStagingHost <bool>` → `[pscustomobject]@{ Allowed=[bool]; Reason=[string] }`.
  - Test helper `ConvertTo-LoamsManifestObject -Manifest <hashtable>` → `[pscustomobject]` (JSON round-trip, as `Read-LoamsStackManifest` would see it).

**Schema rules (fixed; validator and `.schema.json` MUST agree):**

| Path | Rule |
|---|---|
| top level | exactly `schemaVersion`, `manifestVersion`, `status`, `approval`, `server`, `client` (no unknown keys) |
| `schemaVersion` | integer `1` |
| `manifestVersion` | `^\d+\.\d+$` (e.g. `1.0`, `1.1`) |
| `status` | `draft` \| `staging-validated` \| `approved` |
| `approval` | exactly `approvedBy`, `approvedOn`, `stagingEvidence`; may be `null` when `draft`; when `staging-validated`, `stagingEvidence` non-empty; when `approved`, all three non-empty and `approvedOn` = `^\d{4}-\d{2}-\d{2}$` |
| `server` | exactly `name` (`^LOAMS Server Stack v\d+\.\d+$`), `layout` (`xampp`), `phpSapi` (`apache2handler`\|`fastcgi`), `apacheVersion` (`^2\.4\.\d+$`), `phpVersion` (`^8\.\d+\.\d+$`), `requiredApacheModules` (non-empty, `^[a-z0-9_]+_module$`, must contain `ssl_module`), `requiredPhpExtensions` (non-empty, `^[a-z0-9_]+$`, must contain `openssl`), `cryptoModulePatterns` (non-empty strings), `tools` (non-empty; each exactly `name` (`^[a-z0-9-]+$`), `relativePath`, `sha256`, `fileVersion`; must include `openssl`), `components` (non-empty, unique `id`) |
| `client` | exactly `name` (`^LOAMS Client TLS Runtime v\d+\.\d+$`), `qtVersion` (`^6\.\d+\.\d+$`), `tlsBackend` (`openssl`), `excludedPlugins` (non-empty), `components` (non-empty, unique `id`, must include `openssl` and `qopensslbackend`) |
| component | exactly `id` (`^[a-z0-9-]+$`), `name`, `version`, `source` {exactly `url` (`^https://`), `verification`}, `archive` {exactly `fileName`, `sha256`}, `modules` (non-empty) |
| module | exactly `relativePath` (relative, no `..`, `^[A-Za-z0-9._\-/\\]+$`), `sha256`, `fileVersion` |
| any `sha256` | `^[0-9a-f]{64}$` and not 64 zeros |
| placeholder | the literal `REPLACE_ME` is accepted for `version`, `apacheVersion`, `phpVersion`, `fileVersion`, `fileName`, `sha256` **only** when `status` is `draft` |

- [ ] **Step 1: Write `deploy/stack/loams-stack-manifest.json` (committed draft)**

The draft names the candidate sources and expected module paths; every real value is `REPLACE_ME` until Task 19's human procedure records it.

```json
{
  "schemaVersion": 1,
  "manifestVersion": "1.0",
  "status": "draft",
  "approval": { "approvedBy": null, "approvedOn": null, "stagingEvidence": null },
  "server": {
    "name": "LOAMS Server Stack v1.0",
    "layout": "xampp",
    "phpSapi": "apache2handler",
    "apacheVersion": "REPLACE_ME",
    "phpVersion": "REPLACE_ME",
    "requiredApacheModules": ["ssl_module", "php_module", "authz_core_module", "authz_host_module", "log_config_module", "headers_module"],
    "requiredPhpExtensions": ["openssl", "mysqli", "mysqlnd", "zip", "json", "ctype", "iconv"],
    "cryptoModulePatterns": ["libssl*.dll", "libcrypto*.dll", "ssleay*.dll", "libeay*.dll"],
    "tools": [
      { "name": "openssl", "relativePath": "apache/bin/openssl.exe", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" }
    ],
    "components": [
      {
        "id": "apache",
        "name": "Apache HTTP Server (Apache Lounge Win64 build, OpenSSL 3.x)",
        "version": "REPLACE_ME",
        "source": { "url": "https://www.apachelounge.com/download/", "verification": "REPLACE_ME: publisher checksum/signature compared, see runbook Part A" },
        "archive": { "fileName": "REPLACE_ME", "sha256": "REPLACE_ME" },
        "modules": [
          { "relativePath": "apache/bin/httpd.exe", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" },
          { "relativePath": "apache/bin/libhttpd.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" },
          { "relativePath": "apache/bin/libssl-3-x64.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" },
          { "relativePath": "apache/bin/libcrypto-3-x64.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" },
          { "relativePath": "apache/modules/mod_ssl.so", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" }
        ]
      },
      {
        "id": "php",
        "name": "PHP for Windows (Thread Safe, x64)",
        "version": "REPLACE_ME",
        "source": { "url": "https://windows.php.net/download/", "verification": "REPLACE_ME: publisher SHA-256 compared, see runbook Part A" },
        "archive": { "fileName": "REPLACE_ME", "sha256": "REPLACE_ME" },
        "modules": [
          { "relativePath": "php/php8ts.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" },
          { "relativePath": "php/php8apache2_4.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" }
        ]
      }
    ]
  },
  "client": {
    "name": "LOAMS Client TLS Runtime v1.0",
    "qtVersion": "6.11.1",
    "tlsBackend": "openssl",
    "excludedPlugins": ["plugins/tls/qschannelbackend.dll", "plugins/tls/qcertonlybackend.dll"],
    "components": [
      {
        "id": "openssl",
        "name": "OpenSSL 3 runtime DLLs (candidate: Qt Maintenance Tool OpenSSL 3 toolkit)",
        "version": "REPLACE_ME",
        "source": { "url": "https://download.qt.io/", "verification": "REPLACE_ME: Qt Maintenance Tool package + SHA-256, see runbook Part A3" },
        "archive": { "fileName": "REPLACE_ME", "sha256": "REPLACE_ME" },
        "modules": [
          { "relativePath": "libssl-3-x64.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" },
          { "relativePath": "libcrypto-3-x64.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" }
        ]
      },
      {
        "id": "qopensslbackend",
        "name": "Qt 6.11.1 OpenSSL TLS backend plugin (MinGW 64-bit kit)",
        "version": "6.11.1",
        "source": { "url": "https://download.qt.io/", "verification": "REPLACE_ME: Qt online installer kit, see runbook Part A3" },
        "archive": { "fileName": "REPLACE_ME", "sha256": "REPLACE_ME" },
        "modules": [
          { "relativePath": "plugins/tls/qopensslbackend.dll", "sha256": "REPLACE_ME", "fileVersion": "REPLACE_ME" }
        ]
      }
    ]
  }
}
```

The server module list for PHP intentionally omits PHP's own `libssl`/`libcrypto` copies in the draft: Windows resolves an implicit dependency to an already-loaded module of the same name, so which OpenSSL copy PHP actually uses inside `httpd` is a **staging observation** recorded by Task 19 (runbook Part A4), not an assumption. `cryptoModulePatterns` makes any loaded OpenSSL module that the manifest does not list a mismatch.

- [ ] **Step 2: Write `deploy/stack/loams-stack-manifest.schema.json`**

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "loams-stack-manifest.schema.json",
  "title": "LOAMS stack compatibility manifest",
  "description": "Mirrored by Test-LoamsStackManifest (deploy/server/LoamsHost/LoamsHost.Manifest.ps1). Windows PowerShell 5.1 has no Test-Json, so the PowerShell validator is authoritative; a Pester test keeps the key lists and status enum in sync. REPLACE_ME placeholders are allowed only when status is draft (enforced by the validator).",
  "type": "object",
  "additionalProperties": false,
  "required": ["schemaVersion", "manifestVersion", "status", "approval", "server", "client"],
  "definitions": {
    "sha256": { "type": "string", "pattern": "^([0-9a-f]{64}|REPLACE_ME)$" },
    "module": {
      "type": "object", "additionalProperties": false,
      "required": ["relativePath", "sha256", "fileVersion"],
      "properties": {
        "relativePath": { "type": "string", "pattern": "^[A-Za-z0-9._\\-/\\\\]+$" },
        "sha256": { "$ref": "#/definitions/sha256" },
        "fileVersion": { "type": "string", "minLength": 1 }
      }
    },
    "tool": {
      "type": "object", "additionalProperties": false,
      "required": ["name", "relativePath", "sha256", "fileVersion"],
      "properties": {
        "name": { "type": "string", "pattern": "^[a-z0-9-]+$" },
        "relativePath": { "type": "string", "pattern": "^[A-Za-z0-9._\\-/\\\\]+$" },
        "sha256": { "$ref": "#/definitions/sha256" },
        "fileVersion": { "type": "string", "minLength": 1 }
      }
    },
    "component": {
      "type": "object", "additionalProperties": false,
      "required": ["id", "name", "version", "source", "archive", "modules"],
      "properties": {
        "id": { "type": "string", "pattern": "^[a-z0-9-]+$" },
        "name": { "type": "string", "minLength": 1 },
        "version": { "type": "string", "minLength": 1 },
        "source": {
          "type": "object", "additionalProperties": false, "required": ["url", "verification"],
          "properties": { "url": { "type": "string", "pattern": "^https://" }, "verification": { "type": "string", "minLength": 1 } }
        },
        "archive": {
          "type": "object", "additionalProperties": false, "required": ["fileName", "sha256"],
          "properties": { "fileName": { "type": "string", "minLength": 1 }, "sha256": { "$ref": "#/definitions/sha256" } }
        },
        "modules": { "type": "array", "minItems": 1, "items": { "$ref": "#/definitions/module" } }
      }
    }
  },
  "properties": {
    "schemaVersion": { "const": 1 },
    "manifestVersion": { "type": "string", "pattern": "^\\d+\\.\\d+$" },
    "status": { "enum": ["draft", "staging-validated", "approved"] },
    "approval": {
      "type": "object", "additionalProperties": false,
      "required": ["approvedBy", "approvedOn", "stagingEvidence"],
      "properties": {
        "approvedBy": { "type": ["string", "null"] },
        "approvedOn": { "type": ["string", "null"] },
        "stagingEvidence": { "type": ["string", "null"] }
      }
    },
    "server": {
      "type": "object", "additionalProperties": false,
      "required": ["name", "layout", "phpSapi", "apacheVersion", "phpVersion", "requiredApacheModules", "requiredPhpExtensions", "cryptoModulePatterns", "tools", "components"],
      "properties": {
        "name": { "type": "string", "pattern": "^LOAMS Server Stack v\\d+\\.\\d+$" },
        "layout": { "const": "xampp" },
        "phpSapi": { "enum": ["apache2handler", "fastcgi"] },
        "apacheVersion": { "type": "string", "pattern": "^(2\\.4\\.\\d+|REPLACE_ME)$" },
        "phpVersion": { "type": "string", "pattern": "^(8\\.\\d+\\.\\d+|REPLACE_ME)$" },
        "requiredApacheModules": { "type": "array", "minItems": 1, "contains": { "const": "ssl_module" }, "items": { "type": "string", "pattern": "^[a-z0-9_]+_module$" } },
        "requiredPhpExtensions": { "type": "array", "minItems": 1, "contains": { "const": "openssl" }, "items": { "type": "string", "pattern": "^[a-z0-9_]+$" } },
        "cryptoModulePatterns": { "type": "array", "minItems": 1, "items": { "type": "string", "minLength": 1 } },
        "tools": { "type": "array", "minItems": 1, "items": { "$ref": "#/definitions/tool" } },
        "components": { "type": "array", "minItems": 1, "items": { "$ref": "#/definitions/component" } }
      }
    },
    "client": {
      "type": "object", "additionalProperties": false,
      "required": ["name", "qtVersion", "tlsBackend", "excludedPlugins", "components"],
      "properties": {
        "name": { "type": "string", "pattern": "^LOAMS Client TLS Runtime v\\d+\\.\\d+$" },
        "qtVersion": { "type": "string", "pattern": "^6\\.\\d+\\.\\d+$" },
        "tlsBackend": { "const": "openssl" },
        "excludedPlugins": { "type": "array", "minItems": 1, "items": { "type": "string", "minLength": 1 } },
        "components": { "type": "array", "minItems": 1, "items": { "$ref": "#/definitions/component" } }
      }
    }
  }
}
```

- [ ] **Step 3: Append the test helper to `deploy/tests/TestHelpers.ps1`**

```powershell
function ConvertTo-LoamsManifestObject {
    param([Parameter(Mandatory)][hashtable] $Manifest)
    return ($Manifest | ConvertTo-Json -Depth 12 | ConvertFrom-Json)
}
```

- [ ] **Step 4: Write the failing tests `deploy/tests/Manifest.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:RepoStack = Join-Path $PSScriptRoot '..\stack'
    $script:Xampp = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    function New-Valid { param([string] $Status = 'approved') New-LoamsTestManifest -XamppRoot $script:Xampp -Status $Status }
    function Get-Errors { param([hashtable] $m) (Test-LoamsStackManifest -Manifest (ConvertTo-LoamsManifestObject -Manifest $m)).Errors -join ' | ' }
}

Describe 'Committed manifest' {
    It 'passes schema validation' {
        $m = Read-LoamsStackManifest -Path (Join-Path $script:RepoStack 'loams-stack-manifest.json')
        $m.server.name | Should -Be 'LOAMS Server Stack v1.0'
    }
    It 'is committed as draft until the human selection procedure approves it' {
        $m = Read-LoamsStackManifest -Path (Join-Path $script:RepoStack 'loams-stack-manifest.json')
        $m.status | Should -BeIn @('draft', 'staging-validated', 'approved')
        if ($m.status -ne 'draft') { $m.approval.stagingEvidence | Should -Not -BeNullOrEmpty }
    }
    It 'schema file mirrors the validator key lists and status enum' {
        $schema = Get-Content -Raw (Join-Path $script:RepoStack 'loams-stack-manifest.schema.json') | ConvertFrom-Json
        $keys = Get-LoamsManifestSchemaKeys
        (@($schema.required) -join ",") | Should -Be ($keys.Top -join ",")
        (@($schema.properties.status.enum) -join ",") | Should -Be ($keys.Statuses -join ",")
        (@($schema.properties.server.required) -join ",") | Should -Be ($keys.Server -join ",")
        (@($schema.properties.client.required) -join ",") | Should -Be ($keys.Client -join ",")
        (@($schema.definitions.component.required) -join ",") | Should -Be ($keys.Component -join ",")
        (@($schema.definitions.module.required) -join ",") | Should -Be ($keys.Module -join ",")
    }
}

Describe 'Test-LoamsStackManifest' {
    It 'accepts a complete approved manifest' {
        Get-Errors (New-Valid) | Should -BeNullOrEmpty
    }
    It 'rejects unknown top-level keys' {
        $m = New-Valid; $m.extra = 1
        Get-Errors $m | Should -Match 'manifest.extra: unknown property'
    }
    It 'rejects an invalid status' {
        $m = New-Valid; $m.status = 'final'
        Get-Errors $m | Should -Match 'manifest.status'
    }
    It 'rejects a missing server.components' {
        $m = New-Valid; $m.server.Remove('components')
        Get-Errors $m | Should -Match 'server.components: required'
    }
    It 'rejects a non-https source url' {
        $m = New-Valid; $m.server.components[0].source.url = 'http://example.com/'
        Get-Errors $m | Should -Match 'source.url'
    }
    It 'rejects a malformed sha256' {
        $m = New-Valid; $m.server.components[0].archive.sha256 = 'ABC'
        Get-Errors $m | Should -Match 'archive.sha256'
    }
    It 'rejects an all-zero sha256' {
        $m = New-Valid; $m.server.components[0].archive.sha256 = ('0' * 64)
        Get-Errors $m | Should -Match 'all zeros'
    }
    It 'rejects REPLACE_ME when status is approved' {
        $m = New-Valid; $m.server.apacheVersion = 'REPLACE_ME'
        Get-Errors $m | Should -Match 'placeholder REPLACE_ME not allowed'
    }
    It 'accepts REPLACE_ME when status is draft' {
        $m = New-Valid -Status draft; $m.server.apacheVersion = 'REPLACE_ME'
        $m.approval = @{ approvedBy = $null; approvedOn = $null; stagingEvidence = $null }
        Get-Errors $m | Should -BeNullOrEmpty
    }
    It 'rejects a module path that escapes with ..' {
        $m = New-Valid; $m.server.components[0].modules[0].relativePath = '..\evil.dll'
        Get-Errors $m | Should -Match 'relativePath'
    }
    It 'rejects a tool path that escapes with ..' {
        $m = New-Valid; $m.server.tools[0].relativePath = '..\..\Windows\System32\openssl.exe'
        Get-Errors $m | Should -Match 'server\.tools\[0\]\.relativePath'
    }
    It 'rejects a rooted tool path' {
        $m = New-Valid; $m.server.tools[0].relativePath = 'C:\Windows\System32\openssl.exe'
        Get-Errors $m | Should -Match 'server\.tools\[0\]\.relativePath'
    }
    It 'requires ssl_module' {
        $m = New-Valid; $m.server.requiredApacheModules = @('php_module')
        Get-Errors $m | Should -Match 'must contain ssl_module'
    }
    It 'requires the pinned openssl tool' {
        $m = New-Valid; $m.server.tools = @(@{ name = 'other'; relativePath = 'apache\bin\other.exe'; sha256 = ('ab' * 32); fileVersion = '1' })
        Get-Errors $m | Should -Match 'must include the openssl tool'
    }
    It 'requires the qopensslbackend client component' {
        $m = New-Valid; $m.client.components = @($m.client.components[0])
        Get-Errors $m | Should -Match 'must include component qopensslbackend'
    }
    It 'rejects duplicate component ids' {
        $m = New-Valid; $m.server.components = @($m.server.components[0], $m.server.components[0])
        Get-Errors $m | Should -Match "duplicate component id 'apache'"
    }
    It 'requires approval fields when approved' {
        $m = New-Valid; $m.approval.approvedBy = ''
        Get-Errors $m | Should -Match 'approval.approvedBy'
    }
}

Describe 'Manifest property access contract (PS 5.1 array unrolling)' {
    It 'Get-LoamsPropList yields 2, 1, 0 and 0 elements for two-, one-, zero-element and absent arrays' {
        $o = '{"two":[1,2],"one":[3],"none":[]}' | ConvertFrom-Json
        @(Get-LoamsPropList $o 'two').Count | Should -Be 2
        @(Get-LoamsPropList $o 'one').Count | Should -Be 1
        @(Get-LoamsPropList $o 'none').Count | Should -Be 0
        @(Get-LoamsPropList $o 'absent').Count | Should -Be 0
    }
    It 'Get-LoamsProp keeps one-element and empty arrays as arrays when assigned' {
        $o = '{"one":[3],"none":[]}' | ConvertFrom-Json
        $v = Get-LoamsProp $o 'one'
        ($v -is [System.Array]) | Should -BeTrue
        $e = Get-LoamsProp $o 'none'
        ($e -is [System.Array]) | Should -BeTrue
    }
}

Describe 'Read-LoamsStackManifest' {
    It 'throws on invalid JSON' {
        $p = Join-Path $TestDrive 'bad.json'; Set-Content -Path $p -Value '{ not json'
        { Read-LoamsStackManifest -Path $p } | Should -Throw -ExpectedMessage '*is not valid JSON*'
    }
}

Describe 'Test-LoamsManifestStatusGate' {
    It 'allows an approved manifest without -Staging' {
        $m = ConvertTo-LoamsManifestObject -Manifest (New-Valid)
        (Test-LoamsManifestStatusGate -Manifest $m -IsStagingHost $false).Allowed | Should -BeTrue
    }
    It 'refuses a draft manifest without -Staging' {
        $m = ConvertTo-LoamsManifestObject -Manifest (New-Valid -Status draft)
        $r = Test-LoamsManifestStatusGate -Manifest $m -IsStagingHost $true
        $r.Allowed | Should -BeFalse
        $r.Reason | Should -Match 'not approved'
    }
    It 'refuses -Staging on a host that is not an authorized staging host' {
        $m = ConvertTo-LoamsManifestObject -Manifest (New-Valid -Status draft)
        $r = Test-LoamsManifestStatusGate -Manifest $m -Staging -IsStagingHost $false
        $r.Allowed | Should -BeFalse
        $r.Reason | Should -Match 'authorized staging host'
    }
    It 'allows a draft manifest with -Staging on a staging host' {
        $m = ConvertTo-LoamsManifestObject -Manifest (New-Valid -Status draft)
        (Test-LoamsManifestStatusGate -Manifest $m -Staging -IsStagingHost $true).Allowed | Should -BeTrue
    }
    It 'refuses a staging-validated manifest without -Staging' {
        $m = ConvertTo-LoamsManifestObject -Manifest (New-Valid -Status 'staging-validated')
        (Test-LoamsManifestStatusGate -Manifest $m -IsStagingHost $true).Allowed | Should -BeFalse
    }
}
```

- [ ] **Step 5: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Manifest.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Read-LoamsStackManifest`, `Test-LoamsStackManifest`, `Get-LoamsManifestSchemaKeys`, `Test-LoamsManifestStatusGate` not recognised.

- [ ] **Step 6: Implement `deploy/server/LoamsHost/LoamsHost.Manifest.ps1`**

```powershell
# Manifest reading, fixed-schema validation (PowerShell 5.1 has no Test-Json) and status gate.

$script:LoamsManifestTopKeys  = @('schemaVersion', 'manifestVersion', 'status', 'approval', 'server', 'client')
$script:LoamsManifestStatuses = @('draft', 'staging-validated', 'approved')
$script:LoamsServerKeys       = @('name', 'layout', 'phpSapi', 'apacheVersion', 'phpVersion', 'requiredApacheModules', 'requiredPhpExtensions', 'cryptoModulePatterns', 'tools', 'components')
$script:LoamsClientKeys       = @('name', 'qtVersion', 'tlsBackend', 'excludedPlugins', 'components')
$script:LoamsComponentKeys    = @('id', 'name', 'version', 'source', 'archive', 'modules')
$script:LoamsModuleKeys       = @('relativePath', 'sha256', 'fileVersion')
$script:LoamsPlaceholder      = 'REPLACE_ME'

function Get-LoamsProp {
    [CmdletBinding()]
    param([AllowNull()] $Object, [Parameter(Mandatory)][string] $Name)
    if ($null -eq $Object) { return $null }
    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) { return , $Object[$Name] }
        return $null
    }
    $p = $Object.PSObject.Properties[$Name]
    if ($null -eq $p) { return $null }
    return , $p.Value
}

function Get-LoamsPropList {
    [CmdletBinding()]
    param([AllowNull()] $Object, [Parameter(Mandatory)][string] $Name)
    # Assignment keeps the raw value intact; foreach then emits each element (never a nested array).
    $value = Get-LoamsProp -Object $Object -Name $Name
    if ($null -eq $value) { return }
    foreach ($item in $value) { $item }
}

function Test-LoamsManifestRelativePath {
    param($Value, [string] $Path, $Errors)
    Test-LoamsManifestString -Value $Value -Path $Path -Pattern '^[A-Za-z0-9._\-/\\]+$' -AllowPlaceholder $false -Errors $Errors
    if ($Value -is [string] -and ($Value -match '(^|[\\/])\.\.?([\\/]|$)' -or $Value -match '^[\\/]' -or $Value -match '^[A-Za-z]:')) {
        $Errors.Add("${Path}: must be a plain relative path below the XAMPP root (no '.', '..', root or drive)")
    }
}

function Get-LoamsManifestSchemaKeys {
    [CmdletBinding()] param()
    return [pscustomobject]@{
        Top = $script:LoamsManifestTopKeys; Statuses = $script:LoamsManifestStatuses
        Server = $script:LoamsServerKeys; Client = $script:LoamsClientKeys
        Component = $script:LoamsComponentKeys; Module = $script:LoamsModuleKeys
    }
}

function Test-LoamsObjectShape {
    param($Object, [string] $Path, [string[]] $Keys, $Errors)
    if ($Object -isnot [System.Management.Automation.PSCustomObject]) { $Errors.Add("${Path}: must be an object"); return $false }
    $names = @($Object.PSObject.Properties | ForEach-Object { $_.Name })
    foreach ($k in $Keys) { if ($names -notcontains $k) { $Errors.Add("${Path}.${k}: required") } }
    foreach ($n in $names) { if ($Keys -notcontains $n) { $Errors.Add("${Path}.${n}: unknown property") } }
    return $true
}

function Test-LoamsManifestString {
    param($Value, [string] $Path, [string] $Pattern, [bool] $AllowPlaceholder, $Errors)
    if ($Value -isnot [string] -or $Value -eq '') { $Errors.Add("${Path}: must be a non-empty string"); return }
    if ($Value -ceq $script:LoamsPlaceholder) {
        if (-not $AllowPlaceholder) { $Errors.Add("${Path}: placeholder REPLACE_ME not allowed unless status is draft") }
        return
    }
    if ($Pattern -and $Value -cnotmatch $Pattern) { $Errors.Add("${Path}: '$Value' does not match $Pattern") }
}

function Test-LoamsManifestSha256 {
    param($Value, [string] $Path, [bool] $AllowPlaceholder, $Errors)
    Test-LoamsManifestString -Value $Value -Path $Path -Pattern '^[0-9a-f]{64}$' -AllowPlaceholder $AllowPlaceholder -Errors $Errors
    if ($Value -is [string] -and $Value -ceq ('0' * 64)) { $Errors.Add("${Path}: sha256 must not be all zeros") }
}

function Test-LoamsManifestStringArray {
    param($Value, [string] $Path, [string] $Pattern, $Errors)
    if ($Value -isnot [System.Array] -or $Value.Count -eq 0) { $Errors.Add("${Path}: must be a non-empty array"); return }
    for ($i = 0; $i -lt $Value.Count; $i++) {
        Test-LoamsManifestString -Value $Value[$i] -Path "${Path}[$i]" -Pattern $Pattern -AllowPlaceholder $false -Errors $Errors
    }
}

function Test-LoamsManifestComponents {
    param($Components, [string] $Path, [bool] $AllowPlaceholder, $Errors)
    if ($Components -isnot [System.Array] -or $Components.Count -eq 0) { $Errors.Add("${Path}: must be a non-empty array"); return @() }
    $ids = @()
    for ($i = 0; $i -lt $Components.Count; $i++) {
        $c = $Components[$i]; $cp = "${Path}[$i]"
        if (-not (Test-LoamsObjectShape -Object $c -Path $cp -Keys $script:LoamsComponentKeys -Errors $Errors)) { continue }
        $id = Get-LoamsProp $c 'id'
        Test-LoamsManifestString -Value $id -Path "$cp.id" -Pattern '^[a-z0-9-]+$' -AllowPlaceholder $false -Errors $Errors
        if ($ids -contains $id) { $Errors.Add("${Path}: duplicate component id '$id'") } else { $ids += $id }
        Test-LoamsManifestString -Value (Get-LoamsProp $c 'name') -Path "$cp.name" -Pattern '' -AllowPlaceholder $false -Errors $Errors
        Test-LoamsManifestString -Value (Get-LoamsProp $c 'version') -Path "$cp.version" -Pattern '' -AllowPlaceholder $AllowPlaceholder -Errors $Errors
        $src = Get-LoamsProp $c 'source'
        if (Test-LoamsObjectShape -Object $src -Path "$cp.source" -Keys @('url', 'verification') -Errors $Errors) {
            Test-LoamsManifestString -Value (Get-LoamsProp $src 'url') -Path "$cp.source.url" -Pattern '^https://' -AllowPlaceholder $false -Errors $Errors
            Test-LoamsManifestString -Value (Get-LoamsProp $src 'verification') -Path "$cp.source.verification" -Pattern '' -AllowPlaceholder $AllowPlaceholder -Errors $Errors
        }
        $arc = Get-LoamsProp $c 'archive'
        if (Test-LoamsObjectShape -Object $arc -Path "$cp.archive" -Keys @('fileName', 'sha256') -Errors $Errors) {
            Test-LoamsManifestString -Value (Get-LoamsProp $arc 'fileName') -Path "$cp.archive.fileName" -Pattern '' -AllowPlaceholder $AllowPlaceholder -Errors $Errors
            Test-LoamsManifestSha256 -Value (Get-LoamsProp $arc 'sha256') -Path "$cp.archive.sha256" -AllowPlaceholder $AllowPlaceholder -Errors $Errors
        }
        $mods = Get-LoamsProp $c 'modules'
        if ($mods -isnot [System.Array] -or $mods.Count -eq 0) { $Errors.Add("$cp.modules: must be a non-empty array"); continue }
        for ($j = 0; $j -lt $mods.Count; $j++) {
            $m = $mods[$j]; $mp = "$cp.modules[$j]"
            if (-not (Test-LoamsObjectShape -Object $m -Path $mp -Keys $script:LoamsModuleKeys -Errors $Errors)) { continue }
            Test-LoamsManifestRelativePath -Value (Get-LoamsProp $m 'relativePath') -Path "$mp.relativePath" -Errors $Errors
            Test-LoamsManifestSha256 -Value (Get-LoamsProp $m 'sha256') -Path "$mp.sha256" -AllowPlaceholder $AllowPlaceholder -Errors $Errors
            Test-LoamsManifestString -Value (Get-LoamsProp $m 'fileVersion') -Path "$mp.fileVersion" -Pattern '' -AllowPlaceholder $AllowPlaceholder -Errors $Errors
        }
    }
    return $ids
}

function Test-LoamsStackManifest {
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowNull()] $Manifest)
    $errors = New-Object System.Collections.Generic.List[string]
    if (-not (Test-LoamsObjectShape -Object $Manifest -Path 'manifest' -Keys $script:LoamsManifestTopKeys -Errors $errors)) {
        return [pscustomobject]@{ IsValid = $false; Errors = @($errors) }
    }
    $schemaVersion = Get-LoamsProp $Manifest 'schemaVersion'
    if (-not ($schemaVersion -is [int] -or $schemaVersion -is [long]) -or $schemaVersion -ne 1) { $errors.Add('manifest.schemaVersion: must be integer 1') }
    Test-LoamsManifestString -Value (Get-LoamsProp $Manifest 'manifestVersion') -Path 'manifest.manifestVersion' -Pattern '^\d+\.\d+$' -AllowPlaceholder $false -Errors $errors
    $status = Get-LoamsProp $Manifest 'status'
    if ($script:LoamsManifestStatuses -cnotcontains $status) { $errors.Add("manifest.status: must be one of $($script:LoamsManifestStatuses -join ', ')") }
    $draft = ($status -ceq 'draft')

    $approval = Get-LoamsProp $Manifest 'approval'
    if (Test-LoamsObjectShape -Object $approval -Path 'approval' -Keys @('approvedBy', 'approvedOn', 'stagingEvidence') -Errors $errors) {
        if ($status -ceq 'staging-validated' -or $status -ceq 'approved') {
            Test-LoamsManifestString -Value (Get-LoamsProp $approval 'stagingEvidence') -Path 'approval.stagingEvidence' -Pattern '' -AllowPlaceholder $false -Errors $errors
        }
        if ($status -ceq 'approved') {
            Test-LoamsManifestString -Value (Get-LoamsProp $approval 'approvedBy') -Path 'approval.approvedBy' -Pattern '' -AllowPlaceholder $false -Errors $errors
            Test-LoamsManifestString -Value (Get-LoamsProp $approval 'approvedOn') -Path 'approval.approvedOn' -Pattern '^\d{4}-\d{2}-\d{2}$' -AllowPlaceholder $false -Errors $errors
        }
    }

    $server = Get-LoamsProp $Manifest 'server'
    if (Test-LoamsObjectShape -Object $server -Path 'server' -Keys $script:LoamsServerKeys -Errors $errors) {
        Test-LoamsManifestString -Value (Get-LoamsProp $server 'name') -Path 'server.name' -Pattern '^LOAMS Server Stack v\d+\.\d+$' -AllowPlaceholder $false -Errors $errors
        Test-LoamsManifestString -Value (Get-LoamsProp $server 'layout') -Path 'server.layout' -Pattern '^xampp$' -AllowPlaceholder $false -Errors $errors
        Test-LoamsManifestString -Value (Get-LoamsProp $server 'phpSapi') -Path 'server.phpSapi' -Pattern '^(apache2handler|fastcgi)$' -AllowPlaceholder $false -Errors $errors
        Test-LoamsManifestString -Value (Get-LoamsProp $server 'apacheVersion') -Path 'server.apacheVersion' -Pattern '^2\.4\.\d+$' -AllowPlaceholder $draft -Errors $errors
        Test-LoamsManifestString -Value (Get-LoamsProp $server 'phpVersion') -Path 'server.phpVersion' -Pattern '^8\.\d+\.\d+$' -AllowPlaceholder $draft -Errors $errors
        $am = Get-LoamsProp $server 'requiredApacheModules'
        Test-LoamsManifestStringArray -Value $am -Path 'server.requiredApacheModules' -Pattern '^[a-z0-9_]+_module$' -Errors $errors
        if ($am -is [System.Array] -and $am -cnotcontains 'ssl_module') { $errors.Add('server.requiredApacheModules: must contain ssl_module') }
        $pe = Get-LoamsProp $server 'requiredPhpExtensions'
        Test-LoamsManifestStringArray -Value $pe -Path 'server.requiredPhpExtensions' -Pattern '^[a-z0-9_]+$' -Errors $errors
        if ($pe -is [System.Array] -and $pe -cnotcontains 'openssl') { $errors.Add('server.requiredPhpExtensions: must contain openssl') }
        Test-LoamsManifestStringArray -Value (Get-LoamsProp $server 'cryptoModulePatterns') -Path 'server.cryptoModulePatterns' -Pattern '' -Errors $errors
        $tools = Get-LoamsProp $server 'tools'
        if ($tools -isnot [System.Array] -or $tools.Count -eq 0) { $errors.Add('server.tools: must be a non-empty array') }
        else {
            for ($t = 0; $t -lt $tools.Count; $t++) {
                $tp = "server.tools[$t]"
                if (-not (Test-LoamsObjectShape -Object $tools[$t] -Path $tp -Keys @('name', 'relativePath', 'sha256', 'fileVersion') -Errors $errors)) { continue }
                Test-LoamsManifestString -Value (Get-LoamsProp $tools[$t] 'name') -Path "$tp.name" -Pattern '^[a-z0-9-]+$' -AllowPlaceholder $false -Errors $errors
                Test-LoamsManifestRelativePath -Value (Get-LoamsProp $tools[$t] 'relativePath') -Path "$tp.relativePath" -Errors $errors
                Test-LoamsManifestSha256 -Value (Get-LoamsProp $tools[$t] 'sha256') -Path "$tp.sha256" -AllowPlaceholder $draft -Errors $errors
                Test-LoamsManifestString -Value (Get-LoamsProp $tools[$t] 'fileVersion') -Path "$tp.fileVersion" -Pattern '' -AllowPlaceholder $draft -Errors $errors
            }
            if (@($tools | ForEach-Object { Get-LoamsProp $_ 'name' }) -notcontains 'openssl') { $errors.Add('server.tools: must include the openssl tool') }
        }
        [void](Test-LoamsManifestComponents -Components (Get-LoamsProp $server 'components') -Path 'server.components' -AllowPlaceholder $draft -Errors $errors)
    }

    $client = Get-LoamsProp $Manifest 'client'
    if (Test-LoamsObjectShape -Object $client -Path 'client' -Keys $script:LoamsClientKeys -Errors $errors) {
        Test-LoamsManifestString -Value (Get-LoamsProp $client 'name') -Path 'client.name' -Pattern '^LOAMS Client TLS Runtime v\d+\.\d+$' -AllowPlaceholder $false -Errors $errors
        Test-LoamsManifestString -Value (Get-LoamsProp $client 'qtVersion') -Path 'client.qtVersion' -Pattern '^6\.\d+\.\d+$' -AllowPlaceholder $false -Errors $errors
        Test-LoamsManifestString -Value (Get-LoamsProp $client 'tlsBackend') -Path 'client.tlsBackend' -Pattern '^openssl$' -AllowPlaceholder $false -Errors $errors
        Test-LoamsManifestStringArray -Value (Get-LoamsProp $client 'excludedPlugins') -Path 'client.excludedPlugins' -Pattern '' -Errors $errors
        $ids = @(Test-LoamsManifestComponents -Components (Get-LoamsProp $client 'components') -Path 'client.components' -AllowPlaceholder $draft -Errors $errors)
        foreach ($need in @('openssl', 'qopensslbackend')) {
            if ($ids -notcontains $need) { $errors.Add("client.components: must include component $need") }
        }
    }
    return [pscustomobject]@{ IsValid = ($errors.Count -eq 0); Errors = @($errors) }
}

function Read-LoamsStackManifest {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    try {
        $obj = Get-Content -Raw -Path $Path -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "Manifest '$Path' is not valid JSON: $($_.Exception.Message)"
    }
    $result = Test-LoamsStackManifest -Manifest $obj
    if (-not $result.IsValid) {
        throw "Manifest '$Path' failed schema validation: $($result.Errors -join '; ')"
    }
    return $obj
}

function Test-LoamsManifestStatusGate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $Manifest,
        [switch] $Staging,
        [Parameter(Mandatory)][bool] $IsStagingHost
    )
    $status = Get-LoamsProp $Manifest 'status'
    if ($status -ceq 'approved') {
        return [pscustomobject]@{ Allowed = $true; Reason = 'manifest status approved' }
    }
    if (-not $Staging) {
        return [pscustomobject]@{ Allowed = $false; Reason = "manifest status '$status' is not approved; -Converge requires an approved manifest (-Staging is only for a staging host)" }
    }
    if (-not $IsStagingHost) {
        return [pscustomobject]@{ Allowed = $false; Reason = '-Staging refused: this host is not an authorized staging host (admin-only marker + approved machine identity, Task 16)' }
    }
    return [pscustomobject]@{ Allowed = $true; Reason = "staging run against '$status' manifest on a marked staging host" }
}
```

- [ ] **Step 7: Run to verify it passes**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Manifest.Tests.ps1 -Output Detailed"
```

Expected: PASS, 28 tests. Then set `deploy/tests/expected-test-count.txt` to `42` and run `deploy\tests\Invoke-LoamsPesterSuite.ps1` → `PASSED: 42 tests`.

- [ ] **Step 8: Commit**

Via the project `commit` skill — subject: `feat(deploy): add stack compatibility manifest, schema and PS 5.1 validator`

### Task 3: Manifest recording helpers (digests, publisher-checksum comparison, module entries)

These helpers are what the human component-selection procedure (Task 18 runbook Part A, executed in Task 19) uses to compute and record real values. They never download anything.

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.ManifestRecord.ps1`
- Create: `deploy/tests/ManifestRecord.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `49`

**Interfaces:**
- Consumes: `Get-LoamsProp` (Task 2).
- Produces:
  - `Get-LoamsFileDigest -Path <string>` → `[pscustomobject]@{ Path=[string]; Sha256=[string] (lower hex); FileVersion=[string] ('' if none); Length=[long] }`.
  - `Test-LoamsPublisherChecksum -ArchivePath <string> -Published <string>` → `[pscustomobject]@{ Match=[bool]; Computed=[string]; Expected=[string] }`. `-Published` accepts a bare 64-hex digest or a line in `SHA256(<file>)= <hex>` / `<hex> *<file>` / `<hex>  <file>` form.
  - `New-LoamsManifestModuleEntry -Path <string> -Root <string>` → `[ordered]@{ relativePath=[string] (forward slashes); sha256; fileVersion }`; throws if `Path` is not under `Root`.

- [ ] **Step 1: Write the failing tests `deploy/tests/ManifestRecord.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Dll = Join-Path $script:Root 'php\libssl-3-x64.dll'
    $script:Hash = Get-LoamsTestSha256 -Path $script:Dll
}

Describe 'Get-LoamsFileDigest' {
    It 'returns lower-case SHA-256 and length' {
        $d = Get-LoamsFileDigest -Path $script:Dll
        $d.Sha256 | Should -Be $script:Hash
        $d.Length | Should -Be (Get-Item $script:Dll).Length
        $d.FileVersion | Should -Be ''
    }
}

Describe 'Test-LoamsPublisherChecksum' {
    It 'matches a bare digest case-insensitively' {
        (Test-LoamsPublisherChecksum -ArchivePath $script:Dll -Published $script:Hash.ToUpperInvariant()).Match | Should -BeTrue
    }
    It 'matches the OpenSSL SHA256(file)= form' {
        (Test-LoamsPublisherChecksum -ArchivePath $script:Dll -Published "SHA256(libssl-3-x64.dll)= $script:Hash").Match | Should -BeTrue
    }
    It 'matches the sha256sum form' {
        (Test-LoamsPublisherChecksum -ArchivePath $script:Dll -Published "$script:Hash *libssl-3-x64.dll").Match | Should -BeTrue
    }
    It 'reports a mismatch' {
        $r = Test-LoamsPublisherChecksum -ArchivePath $script:Dll -Published ('ab' * 32)
        $r.Match | Should -BeFalse
        $r.Computed | Should -Be $script:Hash
    }
}

Describe 'New-LoamsManifestModuleEntry' {
    It 'records a forward-slash relative path and the hash' {
        $e = New-LoamsManifestModuleEntry -Path $script:Dll -Root $script:Root
        $e.relativePath | Should -Be 'php/libssl-3-x64.dll'
        $e.sha256 | Should -Be $script:Hash
    }
    It 'refuses a path outside the root' {
        $outside = Join-Path $TestDrive 'outside.dll'; Set-Content -Path $outside -Value 'x'
        { New-LoamsManifestModuleEntry -Path $outside -Root $script:Root } | Should -Throw -ExpectedMessage '*not under*'
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\ManifestRecord.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsFileDigest` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.ManifestRecord.ps1`**

```powershell
# Helpers for the human component-selection procedure: digests and manifest entries.

function Get-LoamsFileDigest {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    $version = ''
    if ($item.VersionInfo -and $item.VersionInfo.FileVersion) { $version = [string]$item.VersionInfo.FileVersion }
    return [pscustomobject]@{
        Path        = $item.FullName
        Sha256      = (Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash.ToLowerInvariant()
        FileVersion = $version.Trim()
        Length      = [long]$item.Length
    }
}

function Test-LoamsPublisherChecksum {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ArchivePath, [Parameter(Mandatory)][string] $Published)
    $m = [regex]::Match($Published, '(?i)\b([0-9a-f]{64})\b')
    if (-not $m.Success) { throw "No SHA-256 digest found in the published checksum text." }
    $expected = $m.Groups[1].Value.ToLowerInvariant()
    $computed = (Get-LoamsFileDigest -Path $ArchivePath).Sha256
    return [pscustomobject]@{ Match = ($computed -ceq $expected); Computed = $computed; Expected = $expected }
}

function New-LoamsManifestModuleEntry {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][string] $Root)
    $full = (Resolve-Path -LiteralPath $Path).ProviderPath
    $rootFull = (Resolve-Path -LiteralPath $Root).ProviderPath.TrimEnd('\') + '\'
    if (-not $full.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Module '$full' is not under root '$rootFull'."
    }
    $digest = Get-LoamsFileDigest -Path $full
    $fileVersion = $digest.FileVersion
    if ($fileVersion -eq '') { $fileVersion = 'none' }
    return [ordered]@{
        relativePath = $full.Substring($rootFull.Length).Replace('\', '/')
        sha256       = $digest.Sha256
        fileVersion  = $fileVersion
    }
}
```

A module without a version resource records `fileVersion: "none"` (the schema requires a non-empty string); the hash is the binding identity.

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 7 tests. Set `deploy/tests/expected-test-count.txt` to `49` (42 + 7) and run `deploy\tests\Invoke-LoamsPesterSuite.ps1` → `PASSED: 49 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): add manifest recording helpers for component selection`

### Task 4: Host detection (service, processes, XAMPP layout, httpd -v/-M, PHP SAPI)

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Detection.ps1`
- Create: `deploy/tests/Detection.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `60`

**Interfaces:**
- Consumes: `Invoke-LoamsExternal` (Task 1).
- Produces:
  - `Get-LoamsHttpdServices` → `[pscustomobject[]]` `@{ Name; DisplayName; StartName; PathName; State; StartMode; ProcessId=[int] }` for every `Win32_Service` whose `PathName` contains `\httpd.exe`.
  - `Get-LoamsHttpdProcesses` → `[pscustomobject[]]` `@{ ProcessId=[int]; ParentProcessId=[int]; ExecutablePath=[string] (may be $null when not elevated) }`.
  - `Get-LoamsXamppLayout -XamppRoot <string>` → `[pscustomobject]@{ Root; IsXampp=[bool]; Missing=[string[]]; HttpdExe; PhpExe; MysqldumpExe; ApiRoot; HttpdConf; XamppConf; PhpIni; HasControlPanel=[bool] }`.
  - `Get-LoamsHttpdInfo -HttpdExe <string>` → `[pscustomobject]@{ Version=[string]; VersionLine; Modules=[string[]]; HasModSsl; HasPhpModule; HasFcgid; HasProxyFcgi; ConfigOk=[bool]; ConfigError=[string] }`.
  - `Get-LoamsPhpSapi -HttpdInfo <pscustomobject> -ConfText <string>` → `'apache2handler'|'fastcgi'|'unknown'`.
  - `Get-LoamsPhpCliInfo -PhpExe <string>` → `[pscustomobject]@{ Version; Extensions=[string[]]; CliOpenSsl=[string] }` (informational only; never used as runtime evidence).

- [ ] **Step 1: Write the failing tests `deploy/tests/Detection.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
}

Describe 'Get-LoamsHttpdServices' {
    It 'returns only services whose binary is httpd.exe' {
        Mock -ModuleName LoamsHost Get-CimInstance {
            @(
                [pscustomobject]@{ Name = 'Apache2.4'; DisplayName = 'Apache2.4'; StartName = 'LocalSystem'; PathName = '"C:\xampp\apache\bin\httpd.exe" -k runservice'; State = 'Running'; StartMode = 'Auto'; ProcessId = 100 },
                [pscustomobject]@{ Name = 'mysql'; DisplayName = 'mysql'; StartName = 'LocalSystem'; PathName = 'C:\xampp\mysql\bin\mysqld.exe'; State = 'Running'; StartMode = 'Auto'; ProcessId = 200 }
            )
        } -ParameterFilter { $ClassName -eq 'Win32_Service' }
        $s = @(Get-LoamsHttpdServices)
        $s.Count | Should -Be 1
        $s[0].Name | Should -Be 'Apache2.4'
        $s[0].StartName | Should -Be 'LocalSystem'
    }
}

Describe 'Get-LoamsHttpdProcesses' {
    It 'maps Win32_Process fields' {
        Mock -ModuleName LoamsHost Get-CimInstance {
            @([pscustomobject]@{ ProcessId = 10; ParentProcessId = 5; ExecutablePath = 'C:\xampp\apache\bin\httpd.exe' })
        } -ParameterFilter { $ClassName -eq 'Win32_Process' }
        $p = @(Get-LoamsHttpdProcesses)
        $p[0].ProcessId | Should -Be 10
        $p[0].ParentProcessId | Should -Be 5
    }
}

Describe 'Get-LoamsXamppLayout' {
    It 'recognises a complete XAMPP layout' {
        $l = Get-LoamsXamppLayout -XamppRoot $script:Root
        $l.IsXampp | Should -BeTrue
        $l.Missing.Count | Should -Be 0
        $l.ApiRoot | Should -Be (Join-Path $script:Root 'htdocs\loams_api')
        $l.HasControlPanel | Should -BeTrue
    }
    It 'reports missing parts' {
        $other = Join-Path $TestDrive 'partial'
        New-LoamsFakeXampp -Root $other | Out-Null
        Remove-Item (Join-Path $other 'php\php.exe')
        $l = Get-LoamsXamppLayout -XamppRoot $other
        $l.IsXampp | Should -BeFalse
        $l.Missing | Should -Contain 'php\php.exe'
    }
}

Describe 'Get-LoamsHttpdInfo' {
    BeforeEach {
        Mock -ModuleName LoamsHost Invoke-LoamsExternal {
            [pscustomobject]@{ ExitCode = 0; StdOut = "Server version: Apache/2.4.56 (Win64)`r`nApache Lounge VS16 Server built:   Mar  7 2023"; StdErr = '' }
        } -ParameterFilter { $ArgumentList -contains '-v' }
    }
    It 'parses version and modules' {
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "Loaded Modules:`r`n core_module (static)`r`n authz_core_module (shared)`r`n ssl_module (shared)`r`n php_module (shared)"; StdErr = '' } } -ParameterFilter { $ArgumentList -contains '-M' }
        $i = Get-LoamsHttpdInfo -HttpdExe 'C:\fake\httpd.exe'
        $i.Version | Should -Be '2.4.56'
        $i.Modules | Should -Contain 'ssl_module'
        $i.HasModSsl | Should -BeTrue
        $i.HasPhpModule | Should -BeTrue
        $i.ConfigOk | Should -BeTrue
    }
    It 'reports a missing mod_ssl' {
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "Loaded Modules:`r`n core_module (static)`r`n php_module (shared)"; StdErr = '' } } -ParameterFilter { $ArgumentList -contains '-M' }
        (Get-LoamsHttpdInfo -HttpdExe 'C:\fake\httpd.exe').HasModSsl | Should -BeFalse
    }
    It 'reports a config error when -M fails' {
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 1; StdOut = ''; StdErr = 'Syntax error on line 1' } } -ParameterFilter { $ArgumentList -contains '-M' }
        $i = Get-LoamsHttpdInfo -HttpdExe 'C:\fake\httpd.exe'
        $i.ConfigOk | Should -BeFalse
        $i.ConfigError | Should -Match 'Syntax error'
    }
}

Describe 'Get-LoamsPhpSapi' {
    It 'is apache2handler when php_module is loaded' {
        $info = [pscustomobject]@{ HasPhpModule = $true; HasFcgid = $false; HasProxyFcgi = $false }
        Get-LoamsPhpSapi -HttpdInfo $info -ConfText '' | Should -Be 'apache2handler'
    }
    It 'is fastcgi when fcgid serves php-cgi' {
        $info = [pscustomobject]@{ HasPhpModule = $false; HasFcgid = $true; HasProxyFcgi = $false }
        Get-LoamsPhpSapi -HttpdInfo $info -ConfText 'FcgidWrapper "C:/xampp/php/php-cgi.exe" .php' | Should -Be 'fastcgi'
    }
    It 'is unknown otherwise' {
        $info = [pscustomobject]@{ HasPhpModule = $false; HasFcgid = $false; HasProxyFcgi = $false }
        Get-LoamsPhpSapi -HttpdInfo $info -ConfText '' | Should -Be 'unknown'
    }
}

Describe 'Get-LoamsPhpCliInfo' {
    It 'parses version, extensions and CLI OpenSSL' {
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "PHP 8.2.4 (cli) (built: Mar 14 2023)"; StdErr = '' } } -ParameterFilter { $ArgumentList -contains '-v' }
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "[PHP Modules]`r`nmysqli`r`nopenssl`r`n`r`n[Zend Modules]"; StdErr = '' } } -ParameterFilter { $ArgumentList -contains '-m' }
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "OpenSSL Library Version => OpenSSL 3.0.8 7 Feb 2023"; StdErr = '' } } -ParameterFilter { $ArgumentList -contains '-i' }
        $i = Get-LoamsPhpCliInfo -PhpExe 'C:\fake\php.exe'
        $i.Version | Should -Be '8.2.4'
        $i.Extensions | Should -Contain 'openssl'
        $i.CliOpenSsl | Should -Be 'OpenSSL 3.0.8 7 Feb 2023'
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Detection.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsHttpdServices` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.Detection.ps1`**

```powershell
# Read-only host detection. Nothing here mutates the host.

$script:LoamsXamppRequired = @(
    'apache\bin\httpd.exe', 'apache\conf\httpd.conf', 'apache\conf\extra\httpd-xampp.conf',
    'php\php.exe', 'php\php.ini', 'mysql\bin\mysqldump.exe', 'htdocs\loams_api'
)

function Get-LoamsHttpdServices {
    [CmdletBinding()] param()
    $all = @(Get-CimInstance -ClassName Win32_Service -ErrorAction Stop)
    foreach ($s in $all) {
        if ([string]$s.PathName -notmatch '(?i)\\httpd\.exe') { continue }
        [pscustomobject]@{
            Name = [string]$s.Name; DisplayName = [string]$s.DisplayName; StartName = [string]$s.StartName
            PathName = [string]$s.PathName; State = [string]$s.State; StartMode = [string]$s.StartMode
            ProcessId = [int]$s.ProcessId
        }
    }
}

function Get-LoamsHttpdProcesses {
    [CmdletBinding()] param()
    foreach ($p in @(Get-CimInstance -ClassName Win32_Process -Filter "Name='httpd.exe'" -ErrorAction Stop)) {
        [pscustomobject]@{
            ProcessId = [int]$p.ProcessId; ParentProcessId = [int]$p.ParentProcessId
            ExecutablePath = $p.ExecutablePath
        }
    }
}

function Get-LoamsXamppLayout {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $XamppRoot)
    $missing = @($script:LoamsXamppRequired | Where-Object { -not (Test-Path -LiteralPath (Join-Path $XamppRoot $_)) })
    return [pscustomobject]@{
        Root            = $XamppRoot
        IsXampp         = ($missing.Count -eq 0)
        Missing         = $missing
        HttpdExe        = Join-Path $XamppRoot 'apache\bin\httpd.exe'
        PhpExe          = Join-Path $XamppRoot 'php\php.exe'
        MysqldumpExe    = Join-Path $XamppRoot 'mysql\bin\mysqldump.exe'
        ApiRoot         = Join-Path $XamppRoot 'htdocs\loams_api'
        HttpdConf       = Join-Path $XamppRoot 'apache\conf\httpd.conf'
        XamppConf       = Join-Path $XamppRoot 'apache\conf\extra\httpd-xampp.conf'
        PhpIni          = Join-Path $XamppRoot 'php\php.ini'
        HasControlPanel = (Test-Path -LiteralPath (Join-Path $XamppRoot 'xampp-control.exe'))
    }
}

function Get-LoamsHttpdInfo {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $HttpdExe)
    $v = Invoke-LoamsExternal -FilePath $HttpdExe -ArgumentList @('-v') -AllowNonZeroExit
    $versionLine = (($v.StdOut -split "`r?`n") | Where-Object { $_ -match 'Server version:' } | Select-Object -First 1)
    $version = ''
    if ($versionLine -and $versionLine -match 'Apache/(\d+\.\d+\.\d+)') { $version = $Matches[1] }
    $m = Invoke-LoamsExternal -FilePath $HttpdExe -ArgumentList @('-M') -AllowNonZeroExit
    $modules = @()
    foreach ($line in ($m.StdOut -split "`r?`n")) {
        if ($line -match '^\s+([a-z0-9_]+_module)\s+\((static|shared)\)') { $modules += $Matches[1] }
    }
    return [pscustomobject]@{
        Version      = $version
        VersionLine  = [string]$versionLine
        Modules      = $modules
        HasModSsl    = ($modules -contains 'ssl_module')
        HasPhpModule = ($modules -contains 'php_module')
        HasFcgid     = ($modules -contains 'fcgid_module')
        HasProxyFcgi = ($modules -contains 'proxy_fcgi_module')
        ConfigOk     = ($m.ExitCode -eq 0)
        ConfigError  = ([string]$m.StdErr).Trim()
    }
}

function Get-LoamsPhpSapi {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $HttpdInfo, [AllowEmptyString()][string] $ConfText = '')
    if ($HttpdInfo.HasPhpModule) { return 'apache2handler' }
    if (($HttpdInfo.HasFcgid -or $HttpdInfo.HasProxyFcgi) -and $ConfText -match '(?i)php-cgi') { return 'fastcgi' }
    return 'unknown'
}

function Get-LoamsPhpCliInfo {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $PhpExe)
    $v = Invoke-LoamsExternal -FilePath $PhpExe -ArgumentList @('-v') -AllowNonZeroExit
    $version = ''
    if ($v.StdOut -match 'PHP (\d+\.\d+\.\d+)') { $version = $Matches[1] }
    $m = Invoke-LoamsExternal -FilePath $PhpExe -ArgumentList @('-m') -AllowNonZeroExit
    $ext = @()
    $inPhp = $false
    foreach ($line in ($m.StdOut -split "`r?`n")) {
        $t = $line.Trim()
        if ($t -eq '[PHP Modules]') { $inPhp = $true; continue }
        if ($t -like '`[*') { $inPhp = $false; continue }
        if ($inPhp -and $t) { $ext += $t.ToLowerInvariant() }
    }
    $i = Invoke-LoamsExternal -FilePath $PhpExe -ArgumentList @('-i') -AllowNonZeroExit
    $ossl = ''
    if ($i.StdOut -match 'OpenSSL Library Version => ([^\r\n]+)') { $ossl = $Matches[1].Trim() }
    return [pscustomobject]@{ Version = $version; Extensions = $ext; CliOpenSsl = $ossl }
}
```

`Get-LoamsPhpCliInfo` reports what the **CLI** links; the report labels it "CLI (informational)". The authoritative answer for the server is Task 5's loaded-module inventory of the running `httpd`.

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 11 tests. Set `expected-test-count.txt` to `60` and run the suite runner → `PASSED: 60 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): detect Apache service, processes, XAMPP layout and PHP SAPI`

---

### Task 5: Runtime verification from loaded modules

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.RuntimeModules.ps1`
- Create: `deploy/tests/RuntimeModules.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `71`

**Interfaces:**
- Consumes: `Get-LoamsFileDigest`, `New-LoamsManifestModuleEntry` (Task 3), `Get-LoamsProp`, `Get-LoamsPropList` (Task 2).
- Produces:
  - `Get-LoamsProcessModulePaths -ProcessId <int>` → `[pscustomobject]@{ ProcessId; Accessible=[bool]; Paths=[string[]] }` (wrapper over `(Get-Process -Id).Modules.FileName`; `Accessible=$false` on exception or zero modules — the non-elevated case observed on the dev box).
  - `Get-LoamsLoadedModuleInventory -ProcessIds <int[]>` → `[pscustomobject]@{ Complete=[bool]; Reason=[string]; Modules=[pscustomobject[]] (Path, FileName, Sha256, FileVersion); Inaccessible=[int[]] }`.
  - `Compare-LoamsRuntimeToManifest -Inventory <pscustomobject> -Manifest <pscustomobject> -XamppRoot <string>` → `[pscustomobject]@{ Status='Match'|'Mismatch'|'Unknown'; Findings=[pscustomobject[]] (Kind, Path, Detail); Checked=[int] }`. `Kind` ∈ `InventoryIncomplete`, `ManifestNotFinal`, `Missing`, `HashMismatch`, `UnlistedCrypto`.
  - `ConvertTo-LoamsManifestModuleEntries -Inventory <pscustomobject> -XamppRoot <string>` → `[ordered[]]` manifest module entries for every loaded module under the XAMPP root (used by the Task 19 recording procedure).

Rules: an incomplete inventory is **never** `Match` (status `Unknown`); a draft manifest (any `REPLACE_ME`) is `Unknown`; any loaded module matching `server.cryptoModulePatterns` that the manifest does not list is `UnlistedCrypto` (this is what catches today's Apache-OpenSSL-1.1.1t + PHP-OpenSSL-3.0.8 mix).

- [ ] **Step 1: Write the failing tests `deploy/tests/RuntimeModules.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Manifest = ConvertTo-LoamsManifestObject -Manifest (New-LoamsTestManifest -XamppRoot $script:Root)
    $script:ManifestPaths = @()
    foreach ($c in $script:Manifest.server.components) {
        foreach ($m in $c.modules) { $script:ManifestPaths += (Join-Path $script:Root ($m.relativePath -replace '/', '\')) }
    }
    $script:Kernel = Join-Path $env:SystemRoot 'System32\kernel32.dll'
    # Mock bodies run in the module's scope, so the fake module list is passed through a global.
    function Use-Modules { param([string[]] $Paths)
        $global:LoamsTestModules = @($Paths | ForEach-Object { [pscustomobject]@{ FileName = $_ } })
        Mock -ModuleName LoamsHost Get-Process { [pscustomobject]@{ Modules = $global:LoamsTestModules } }
    }
}
AfterAll { Remove-Variable -Name LoamsTestModules -Scope Global -ErrorAction SilentlyContinue }

Describe 'Get-LoamsLoadedModuleInventory' {
    It 'is incomplete when a process is not accessible' {
        Mock -ModuleName LoamsHost Get-Process { throw 'Access is denied' }
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $inv.Complete | Should -BeFalse
        $inv.Inaccessible | Should -Contain 10
    }
    It 'is incomplete when httpd is not running' {
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @()
        $inv.Complete | Should -BeFalse
        $inv.Reason | Should -Match 'not running'
    }
    It 'deduplicates modules across httpd processes' {
        Use-Modules -Paths @($script:ManifestPaths[0], $script:Kernel)
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10, 11)
        $inv.Complete | Should -BeTrue
        @($inv.Modules).Count | Should -Be 2
    }
}

Describe 'Compare-LoamsRuntimeToManifest' {
    It 'matches when every manifest module is loaded with its hash' {
        Use-Modules -Paths ($script:ManifestPaths + $script:Kernel)
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $r = Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $script:Manifest -XamppRoot $script:Root
        $r.Status | Should -Be 'Match'
        $r.Checked | Should -Be $script:ManifestPaths.Count
    }
    It 'checks every module of every component (fails on nested-array enumeration)' {
        $skip = @($script:ManifestPaths[0], $script:ManifestPaths[$script:ManifestPaths.Count - 1])
        Use-Modules -Paths @($script:ManifestPaths | Where-Object { $skip -notcontains $_ })
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $r = Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $script:Manifest -XamppRoot $script:Root
        @($script:Manifest.server.components).Count | Should -Be 2
        $r.Checked | Should -Be $script:ManifestPaths.Count
        (@($r.Findings | Where-Object Kind -eq 'Missing' | ForEach-Object Path) -join ',') | Should -Be ($skip -join ',')
    }
    It 'reports a manifest module that is not loaded' {
        Use-Modules -Paths ($script:ManifestPaths | Select-Object -Skip 1)
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $r = Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $script:Manifest -XamppRoot $script:Root
        $r.Status | Should -Be 'Mismatch'
        @($r.Findings | Where-Object Kind -eq 'Missing').Count | Should -Be 1
    }
    It 'reports a hash mismatch' {
        $copy = Join-Path $TestDrive 'xampp2'; New-LoamsFakeXampp -Root $copy | Out-Null
        [IO.File]::WriteAllText((Join-Path $copy 'php\php8ts.dll'), 'tampered')
        $paths = $script:ManifestPaths | ForEach-Object { $_.Replace($script:Root, $copy) }
        Use-Modules -Paths $paths
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $r = Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $script:Manifest -XamppRoot $copy
        $r.Status | Should -Be 'Mismatch'
        ($r.Findings | Where-Object Kind -eq 'HashMismatch').Path | Should -Match 'php8ts\.dll'
    }
    It 'reports an unlisted OpenSSL module (mixed stacks)' {
        $legacy = Join-Path $script:Root 'apache\bin\libssl-1_1-x64.dll'; Set-Content -Path $legacy -Value 'old'
        Use-Modules -Paths ($script:ManifestPaths + $legacy)
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $r = Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $script:Manifest -XamppRoot $script:Root
        $r.Status | Should -Be 'Mismatch'
        ($r.Findings | Where-Object Kind -eq 'UnlistedCrypto').Path | Should -Match 'libssl-1_1'
    }
    It 'is Unknown, never Match, when the inventory is incomplete' {
        $inv = [pscustomobject]@{ Complete = $false; Reason = 'not elevated'; Modules = @(); Inaccessible = @(10) }
        $r = Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $script:Manifest -XamppRoot $script:Root
        $r.Status | Should -Be 'Unknown'
        $r.Findings[0].Kind | Should -Be 'InventoryIncomplete'
    }
    It 'is Unknown against a draft manifest' {
        $draft = New-LoamsTestManifest -XamppRoot $script:Root -Status draft
        $draft.server.components[0].modules[0].sha256 = 'REPLACE_ME'
        $m = ConvertTo-LoamsManifestObject -Manifest $draft
        Use-Modules -Paths $script:ManifestPaths
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        (Compare-LoamsRuntimeToManifest -Inventory $inv -Manifest $m -XamppRoot $script:Root).Status | Should -Be 'Unknown'
    }
}

Describe 'ConvertTo-LoamsManifestModuleEntries' {
    It 'emits entries only for modules under the XAMPP root' {
        Use-Modules -Paths @($script:ManifestPaths[0], $script:Kernel)
        $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(10)
        $e = @(ConvertTo-LoamsManifestModuleEntries -Inventory $inv -XamppRoot $script:Root)
        $e.Count | Should -Be 1
        $e[0].relativePath | Should -Be 'apache/bin/libhttpd.dll'
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\RuntimeModules.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsLoadedModuleInventory` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.RuntimeModules.ps1`**

```powershell
# Runtime verification: what the running httpd processes actually loaded (spec §4, invariant 7).
# Never uses openssl.exe; never adds an HTTP endpoint.

function Get-LoamsProcessModulePaths {
    [CmdletBinding()]
    param([Parameter(Mandatory)][int] $ProcessId)
    try {
        $p = Get-Process -Id $ProcessId -ErrorAction Stop
        $paths = @($p.Modules | ForEach-Object { [string]$_.FileName } | Where-Object { $_ })
        return [pscustomobject]@{ ProcessId = $ProcessId; Accessible = ($paths.Count -gt 0); Paths = $paths }
    } catch {
        return [pscustomobject]@{ ProcessId = $ProcessId; Accessible = $false; Paths = @() }
    }
}

function Get-LoamsLoadedModuleInventory {
    [CmdletBinding()]
    param([AllowEmptyCollection()][int[]] $ProcessIds = @())
    if ($ProcessIds.Count -eq 0) {
        return [pscustomobject]@{ Complete = $false; Reason = 'httpd is not running'; Modules = @(); Inaccessible = @() }
    }
    $inaccessible = @()
    $seen = @{}
    foreach ($procId in $ProcessIds) {
        $r = Get-LoamsProcessModulePaths -ProcessId $procId
        if (-not $r.Accessible) { $inaccessible += $procId; continue }
        foreach ($path in $r.Paths) { $seen[$path.ToLowerInvariant()] = $path }
    }
    $modules = @()
    foreach ($path in $seen.Values) {
        $d = Get-LoamsFileDigest -Path $path
        $modules += [pscustomobject]@{ Path = $d.Path; FileName = (Split-Path -Leaf $d.Path); Sha256 = $d.Sha256; FileVersion = $d.FileVersion }
    }
    $complete = ($inaccessible.Count -eq 0)
    $reason = ''
    if (-not $complete) { $reason = 'module list not readable for httpd process(es) ' + ($inaccessible -join ',') + ' (run elevated)' }
    return [pscustomobject]@{ Complete = $complete; Reason = $reason; Modules = $modules; Inaccessible = $inaccessible }
}

function Compare-LoamsRuntimeToManifest {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Inventory, [Parameter(Mandatory)] $Manifest, [Parameter(Mandatory)][string] $XamppRoot)
    $findings = @()
    if (-not $Inventory.Complete) {
        $findings += [pscustomobject]@{ Kind = 'InventoryIncomplete'; Path = ''; Detail = $Inventory.Reason }
        return [pscustomobject]@{ Status = 'Unknown'; Findings = $findings; Checked = 0 }
    }
    $loaded = @{}
    foreach ($m in $Inventory.Modules) { $loaded[$m.Path.ToLowerInvariant()] = $m }
    $server = Get-LoamsProp $Manifest 'server'
    $notFinal = ((Get-LoamsProp $Manifest 'status') -ceq 'draft')
    $expectedPaths = @{}
    $checked = 0
    foreach ($c in @(Get-LoamsPropList $server 'components')) {
        foreach ($mod in @(Get-LoamsPropList $c 'modules')) {
            $full = Join-Path $XamppRoot (([string]$mod.relativePath) -replace '/', '\')
            $key = $full.ToLowerInvariant()
            $expectedPaths[$key] = $true
            $checked++
            if ($mod.sha256 -ceq 'REPLACE_ME') { $notFinal = $true }
            if (-not $loaded.ContainsKey($key)) {
                $findings += [pscustomobject]@{ Kind = 'Missing'; Path = $full; Detail = 'manifest module not loaded by httpd' }
                continue
            }
            if ($mod.sha256 -cne 'REPLACE_ME' -and $loaded[$key].Sha256 -cne $mod.sha256) {
                $findings += [pscustomobject]@{ Kind = 'HashMismatch'; Path = $full; Detail = "expected $($mod.sha256) actual $($loaded[$key].Sha256)" }
            }
        }
    }
    $patterns = @(Get-LoamsPropList $server 'cryptoModulePatterns')
    foreach ($m in $Inventory.Modules) {
        $isCrypto = $false
        foreach ($pat in $patterns) { if ($m.FileName -like $pat) { $isCrypto = $true } }
        if ($isCrypto -and -not $expectedPaths.ContainsKey($m.Path.ToLowerInvariant())) {
            $findings += [pscustomobject]@{ Kind = 'UnlistedCrypto'; Path = $m.Path; Detail = "loaded TLS library not in manifest (version $($m.FileVersion))" }
        }
    }
    if ($notFinal) {
        $findings += [pscustomobject]@{ Kind = 'ManifestNotFinal'; Path = ''; Detail = 'manifest is draft or contains REPLACE_ME' }
        $status = 'Unknown'
    } elseif ($findings.Count -gt 0) {
        $status = 'Mismatch'
    } else {
        $status = 'Match'
    }
    return [pscustomobject]@{ Status = $status; Findings = $findings; Checked = $checked }
}

function ConvertTo-LoamsManifestModuleEntries {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Inventory, [Parameter(Mandatory)][string] $XamppRoot)
    $rootFull = (Resolve-Path -LiteralPath $XamppRoot).ProviderPath.TrimEnd('\') + '\'
    foreach ($m in ($Inventory.Modules | Sort-Object Path)) {
        if ($m.Path.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
            New-LoamsManifestModuleEntry -Path $m.Path -Root $XamppRoot
        }
    }
}
```

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 11 tests. Set `expected-test-count.txt` to `71` and run the suite runner → `PASSED: 71 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): verify loaded httpd modules against the stack manifest`

---

### Task 6: Least-privilege ACL profile, application and compliance

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Acl.ps1`
- Create: `deploy/tests/Acl.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `93`

**Interfaces:**
- Consumes: `Invoke-LoamsExternal`, `Assert-LoamsMutationAllowed`, `Write-LoamsLog` (Task 1).
- Produces:
  - `Get-LoamsServiceSid -ServiceName <string>` → `[string]` `S-1-5-80-…` (SHA-1 of the upper-cased UTF-16LE service name; computable before the service exists).
  - `Get-LoamsAclProfile -XamppRoot <string> -ProgramDataRoot <string> [-ServiceName <string>='Apache2.4'] [-BridgeAccount <string>='']` → `[pscustomobject[]]` entries `@{ Id; Path; Kind='Directory'|'File'; Access='ReadExecute'|'Read'|'AppendOnly'|'Modify'|'None'; Mode='HardenInherited'|'Protected'|'Grant'|'VerifyOnly'; Grants=[string[]] (icacls syntax); AllowedSids=[string[]]; Optional=[bool]; Purpose }`, parents before children.
  - `Get-LoamsBridgeTaskAccount [-TaskName <string>='LOAMS Turnstile Bridge']` → `[string]` the bridge scheduled task's `Principal.UserId`, or `''` when the task does not exist.
  - `Get-LoamsAclSddl -Path <string>` → `[string]` (wrapper over `Get-Acl`, mocked in tests).
  - `Get-LoamsSidMask -Sddl <string> -Sids <string[]>` → `[int]` effective allow mask (allow minus deny, inherit-only ACEs ignored).
  - `Test-LoamsAclEntryCompliance -Entry <pscustomobject> -Sddl <string> -ServiceSid <string>` → `[string[]]` problems (empty = compliant).
  - `Test-LoamsAclCompliance -AclProfile <pscustomobject[]> -ServiceSid <string>` → `[pscustomobject]@{ Compliant=[bool]; Findings=[pscustomobject[]] (EntryId, Path, Problem) }`.
  - `Test-LoamsAdminOnlyAcl -Sddl <string>` → `[bool]` (owner and every write-capable ACE are Administrators/SYSTEM).
  - `Test-LoamsProtectedFolderAcl -Sddl <string>` → `[pscustomobject]@{ Ok; Problems }` — strict allowlist: DACL protected (no inheritance), owner Administrators or SYSTEM, every ACE an *allow* ACE for `S-1-5-32-544` or `S-1-5-18` with FullControl, both present; anything else (e.g. a leftover explicit `Users` / `Authenticated Users` ACE) is a named problem. Used for the reports folder (Task 11, runbook B0/F).
  - `Set-LoamsAclEntry -Entry <pscustomobject>` (mutating; guarded).
  - `Initialize-LoamsServerLayout -ProgramDataRoot <string>` → `[string[]]` paths it created (mutating; guarded).

**The profile (spec §4 table → exact grants; `<svc>` = `NT SERVICE\<ServiceName>`):**

| Id | Path | Mode | Grants | Spec row |
|---|---|---|---|---|
| `xampp-root` | `<XamppRoot>` | HardenInherited: `/inheritance:d`, `/remove:g *S-1-5-11`, `/grant:r *S-1-5-32-545:(OI)(CI)RX` | `<svc>:(OI)(CI)RX` | API code, Apache/PHP config, compat allowlist: read/execute (D6) |
| `apache-logs` | `apache\logs` | Grant | `<svc>:(OI)(CI)M` | Apache's own logs: modify |
| `xampp-tmp` | `tmp` | Grant | `<svc>:(OI)(CI)M` | PHP temp/upload/session (`upload_tmp_dir`, `session.save_path`, `TMP`) |
| `php-logs` | `php\logs` | Grant | `<svc>:(OI)(CI)M` | PHP `error_log` |
| `api-uploads` | `htdocs\loams_api\uploads` | Grant | `<svc>:(OI)(CI)M` | photo uploads (`register_student.php`, `upload_students_zip.php`) |
| `api-logs` | `htdocs\loams_api\logs` | Grant | `<svc>:(OI)(CI)M` | production `error_log` (`config.php`) |
| `verify-*` | `apache\bin\httpd.exe`, `apache\conf\httpd.conf`, `php\php.ini`, `htdocs\loams_api\config.php` | VerifyOnly | — | read-only proof points |
| `loams-root` | `<ProgramDataRoot>` | Protected | `*S-1-5-32-544:(OI)(CI)F`, `*S-1-5-18:(OI)(CI)F` | backups, reports, scripts: none |
| `bridge-log` | `<ProgramDataRoot>\loams-turnstile-bridge.log` (only when the bridge task exists; file optional) | Grant | `<bridge-task-user>:(M)` | keeps the legacy bridge's log writable by its own interactive account only (D11); Apache: none. Never `Users`: a service token carries `BUILTIN\Users`. |
| `server-config` | `<ProgramDataRoot>\server` | Protected | Admins F, SYSTEM F, `<svc>:(OI)(CI)R` | `compat.ini` + controller config: read |
| `server-logs` | `<ProgramDataRoot>\server\logs` | Protected | Admins F, SYSTEM F, `<svc>:(RX)` (this folder only) | traversal/attributes for the guard log |
| `compat-log` | `<ProgramDataRoot>\server\logs\compat-guard.log` | Grant | `<svc>:(AD,S)` (file only, never inheritable — `AD` on a folder means "create subfolders") | Compatibility log: append-only (`FILE_APPEND_DATA`) (D8) |
| `server-tls` | `<ProgramDataRoot>\server\tls` | Protected | Admins F, SYSTEM F, `<svc>:(OI)(CI)R` | server + CA certificates: read |
| `server-key` | `<ProgramDataRoot>\server\tls\private` | Protected | `*S-1-5-32-544:(OI)(CI)F`, `<svc>:(OI)(CI)R` — **no SYSTEM, no Users** | server key: Apache + Administrators only |

The compliance check evaluates the rights of the service SID **plus every group a service token carries** (`S-1-1-0` Everyone, `S-1-5-11` Authenticated Users, `S-1-5-32-545` Users, `S-1-5-6` SERVICE, `S-1-2-0` LOCAL, `S-1-5-15` This Organization). This static check is an approximation; the real token and real `fopen('a')` behaviour are verified on staging by the Task 18 probe (spec Layer 7 is owned by S1d's `Test-LoamsDeployment.ps1`).

- [ ] **Step 1: Write the failing tests `deploy/tests/Acl.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Svc = Get-LoamsServiceSid -ServiceName 'Apache2.4'
    $script:Profile = Get-LoamsAclProfile -XamppRoot 'C:\xampp' -ProgramDataRoot 'C:\ProgramData\LOAMS'
    function Get-Entry { param([string] $Id) $script:Profile | Where-Object Id -eq $Id }
}

Describe 'Get-LoamsServiceSid' {
    It 'matches the well-known TrustedInstaller service SID' {
        Get-LoamsServiceSid -ServiceName 'TrustedInstaller' |
            Should -Be 'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'
    }
}

Describe 'Get-LoamsAclProfile' {
    It 'makes the compat log append-only on the file, never inheritable' {
        $e = Get-Entry 'compat-log'
        $e.Kind | Should -Be 'File'
        $e.Access | Should -Be 'AppendOnly'
        $e.Grants | Should -Be @('NT SERVICE\Apache2.4:(AD,S)')
    }
    It 'limits the key folder to Administrators and the Apache account' {
        $e = Get-Entry 'server-key'
        $e.Mode | Should -Be 'Protected'
        ($e.Grants -join ' ') | Should -Be '*S-1-5-32-544:(OI)(CI)F NT SERVICE\Apache2.4:(OI)(CI)R'
        ($e.AllowedSids -join ' ') | Should -Be "S-1-5-32-544 $script:Svc"
    }
    It 'hardens the XAMPP root to read/execute' {
        $e = Get-Entry 'xampp-root'
        $e.Mode | Should -Be 'HardenInherited'
        $e.Access | Should -Be 'ReadExecute'
    }
    It 'adds the bridge log grant only for the bridge task account' {
        (Get-Entry 'bridge-log') | Should -BeNullOrEmpty
        $withBridge = Get-LoamsAclProfile -XamppRoot 'C:\xampp' -ProgramDataRoot 'C:\ProgramData\LOAMS' -BridgeAccount 'GATEPC\librarian'
        ($withBridge | Where-Object Id -eq 'bridge-log').Grants | Should -Be @('GATEPC\librarian:(M)')
    }
    It 'grants modify only on the inventoried write locations' {
        $mod = @($script:Profile | Where-Object Access -eq 'Modify' | ForEach-Object Id)
        ($mod -join ',') | Should -Be 'apache-logs,xampp-tmp,php-logs,api-uploads,api-logs'
    }
}

Describe 'Test-LoamsAclEntryCompliance' {
    It 'flags Authenticated Users modify on the XAMPP root' {
        $sddl = 'O:BAG:SYD:AI(A;OICIID;FA;;;BA)(A;OICIID;FA;;;SY)(A;OICIID;0x1200a9;;;BU)(A;ID;0x1301bf;;;AU)'
        Test-LoamsAclEntryCompliance -Entry (Get-Entry 'xampp-root') -Sddl $sddl -ServiceSid $script:Svc |
            Should -Contain 'service identity has write-class rights'
    }
    It 'accepts a hardened XAMPP root' {
        $sddl = "O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x1200a9;;;BU)(A;OICI;0x1200a9;;;$script:Svc)"
        @(Test-LoamsAclEntryCompliance -Entry (Get-Entry 'xampp-root') -Sddl $sddl -ServiceSid $script:Svc).Count | Should -Be 0
    }
    It 'accepts append-data + synchronize on the compat log' {
        $sddl = "O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x100004;;;$script:Svc)"
        @(Test-LoamsAclEntryCompliance -Entry (Get-Entry 'compat-log') -Sddl $sddl -ServiceSid $script:Svc).Count | Should -Be 0
    }
    It 'flags write-data on the compat log' {
        $sddl = "O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x100006;;;$script:Svc)"
        Test-LoamsAclEntryCompliance -Entry (Get-Entry 'compat-log') -Sddl $sddl -ServiceSid $script:Svc |
            Should -Contain 'service identity has more than append rights'
    }
    It 'flags any extra principal on the key folder' {
        $sddl = "O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;0x120089;;;$script:Svc)(A;OICI;0x120089;;;BU)"
        Test-LoamsAclEntryCompliance -Entry (Get-Entry 'server-key') -Sddl $sddl -ServiceSid $script:Svc |
            Should -Contain 'unexpected principal S-1-5-32-545'
    }
    It 'flags a protected entry that still inherits' {
        $sddl = "O:BAG:SYD:AI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x120089;;;$script:Svc)"
        Test-LoamsAclEntryCompliance -Entry (Get-Entry 'server-tls') -Sddl $sddl -ServiceSid $script:Svc |
            Should -Contain 'inheritance not disabled'
    }
}

Describe 'Test-LoamsAdminOnlyAcl' {
    It 'is true when only Administrators and SYSTEM can write' {
        Test-LoamsAdminOnlyAcl -Sddl 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x1200a9;;;BU)' | Should -BeTrue
    }
    It 'is false when Users can write' {
        Test-LoamsAdminOnlyAcl -Sddl 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;0x1301bf;;;BU)' | Should -BeFalse
    }
}

Describe 'Test-LoamsProtectedFolderAcl' {
    It 'accepts exactly Administrators and SYSTEM FullControl with protected inheritance' {
        (Test-LoamsProtectedFolderAcl -Sddl 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)').Ok | Should -BeTrue
    }
    It 'names a leftover explicit Users or Authenticated Users entry' {
        $r = Test-LoamsProtectedFolderAcl -Sddl 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x1200a9;;;BU)(A;OICI;0x1301bf;;;AU)'
        $r.Ok | Should -BeFalse
        $r.Problems | Should -Contain 'unexpected principal S-1-5-32-545'
        $r.Problems | Should -Contain 'unexpected principal S-1-5-11'
    }
    It 'rejects inherited rules and a missing SYSTEM entry' {
        $r = Test-LoamsProtectedFolderAcl -Sddl 'O:BAG:SYD:AI(A;OICIID;FA;;;BA)'
        $r.Problems | Should -Contain 'inheritance not disabled (access rules are not protected)'
        $r.Problems | Should -Contain 'missing FullControl for S-1-5-18'
    }
}

Describe 'Set-LoamsAclEntry' {
    BeforeEach {
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } }
        Mock -ModuleName LoamsHost Test-Path { $true }
    }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'hardens the XAMPP root with exact icacls calls' {
        Set-LoamsMode -Mode Converge
        Set-LoamsAclEntry -Entry (Get-Entry 'xampp-root')
        foreach ($expected in @('C:\xampp /inheritance:d', 'C:\xampp /remove:g *S-1-5-11', 'C:\xampp /grant:r *S-1-5-32-545:(OI)(CI)RX', 'C:\xampp /grant:r NT SERVICE\Apache2.4:(OI)(CI)RX')) {
            Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { ($ArgumentList -join ' ') -eq $expected }
        }
    }
    It 'protects the key folder: grants first, then strips inheritance and broad groups' {
        Set-LoamsMode -Mode Converge
        $e = Get-Entry 'server-key'
        Set-LoamsAclEntry -Entry $e
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { ($ArgumentList -join ' ') -eq 'C:\ProgramData\LOAMS\server\tls\private /grant:r *S-1-5-32-544:(OI)(CI)F NT SERVICE\Apache2.4:(OI)(CI)R' }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { ($ArgumentList -join ' ') -eq 'C:\ProgramData\LOAMS\server\tls\private /inheritance:r /remove:g *S-1-5-32-545 *S-1-5-11 *S-1-1-0 *S-1-3-0 *S-1-5-18' }
    }
    It 'refuses in Report mode without calling icacls' {
        Set-LoamsMode -Mode Report
        { Set-LoamsAclEntry -Entry (Get-Entry 'xampp-root') } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0
    }
}

Describe 'Initialize-LoamsServerLayout' {
    AfterEach { Set-LoamsMode -Mode Report }
    It 'creates the server tree and an empty compat log' {
        Set-LoamsMode -Mode Converge
        $root = Join-Path $TestDrive 'pd\LOAMS'
        $created = Initialize-LoamsServerLayout -ProgramDataRoot $root
        Test-Path (Join-Path $root 'server\tls\private') | Should -BeTrue
        (Get-Item (Join-Path $root 'server\logs\compat-guard.log')).Length | Should -Be 0
        $created | Should -Contain (Join-Path $root 'server')
    }
    It 'refuses in Report mode' {
        { Initialize-LoamsServerLayout -ProgramDataRoot (Join-Path $TestDrive 'pd2') } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        Test-Path (Join-Path $TestDrive 'pd2') | Should -BeFalse
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Acl.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsServiceSid` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.Acl.ps1`**

```powershell
# Least-privilege ACL profile (spec §4 "Service identity & least privilege").

$script:LoamsSidAdmins = 'S-1-5-32-544'
$script:LoamsSidSystem = 'S-1-5-18'
$script:LoamsServiceTokenGroupSids = @('S-1-1-0', 'S-1-5-11', 'S-1-5-32-545', 'S-1-5-6', 'S-1-2-0', 'S-1-5-15')
# WD|AD|WEA|DC|WA|DELETE|WRITE_DAC|WRITE_OWNER|GENERIC_ALL|GENERIC_WRITE
$script:LoamsWriteMask = 0x500D0156
$script:LoamsAppendData = 0x4
$script:LoamsWriteData = 0x2
$script:LoamsReadData = 0x1
$script:LoamsIcacls = Join-Path $env:SystemRoot 'System32\icacls.exe'

function Get-LoamsServiceSid {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName)
    $bytes = [Text.Encoding]::Unicode.GetBytes($ServiceName.ToUpperInvariant())
    $sha1 = [Security.Cryptography.SHA1]::Create()
    try { $hash = $sha1.ComputeHash($bytes) } finally { $sha1.Dispose() }
    $parts = for ($i = 0; $i -lt 20; $i += 4) { [BitConverter]::ToUInt32($hash, $i) }
    return 'S-1-5-80-' + ($parts -join '-')
}

function New-LoamsAclEntry {
    param([string] $Id, [string] $Path, [string] $Kind, [string] $Access, [string] $Mode,
          [string[]] $Grants = @(), [string[]] $AllowedSids = @(), [bool] $Optional = $false, [string] $Purpose)
    return [pscustomobject]@{ Id = $Id; Path = $Path; Kind = $Kind; Access = $Access; Mode = $Mode
        Grants = $Grants; AllowedSids = $AllowedSids; Optional = $Optional; Purpose = $Purpose }
}

function Get-LoamsAclProfile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string] $ProgramDataRoot,
          [string] $ServiceName = 'Apache2.4', [string] $BridgeAccount = '')
    $svc = "NT SERVICE\$ServiceName"
    $svcSid = Get-LoamsServiceSid -ServiceName $ServiceName
    $adm = '*S-1-5-32-544'; $sys = '*S-1-5-18'
    $x = { param($rel) Join-Path $XamppRoot $rel }
    $p = { param($rel) Join-Path $ProgramDataRoot $rel }
    $adminSys = @("${adm}:(OI)(CI)F", "${sys}:(OI)(CI)F")
    $bridge = @()
    if ($BridgeAccount) {
        $bridge = @(New-LoamsAclEntry -Id 'bridge-log' -Path (& $p 'loams-turnstile-bridge.log') -Kind File -Access None -Mode Grant -Grants @("${BridgeAccount}:(M)") -Optional $true -Purpose 'legacy bridge log stays writable by the bridge task account only (D11)')
    }
    return @(
        New-LoamsAclEntry -Id 'xampp-root' -Path $XamppRoot -Kind Directory -Access ReadExecute -Mode HardenInherited -Grants @("${svc}:(OI)(CI)RX") -Purpose 'API code, Apache/PHP config, compat allowlist: read/execute'
        New-LoamsAclEntry -Id 'apache-logs' -Path (& $x 'apache\logs') -Kind Directory -Access Modify -Mode Grant -Grants @("${svc}:(OI)(CI)M") -Purpose "Apache's own logs"
        New-LoamsAclEntry -Id 'xampp-tmp' -Path (& $x 'tmp') -Kind Directory -Access Modify -Mode Grant -Grants @("${svc}:(OI)(CI)M") -Purpose 'PHP temp/upload/session directory'
        New-LoamsAclEntry -Id 'php-logs' -Path (& $x 'php\logs') -Kind Directory -Access Modify -Mode Grant -Grants @("${svc}:(OI)(CI)M") -Purpose 'PHP error_log'
        New-LoamsAclEntry -Id 'api-uploads' -Path (& $x 'htdocs\loams_api\uploads') -Kind Directory -Access Modify -Mode Grant -Grants @("${svc}:(OI)(CI)M") -Purpose 'student photo uploads'
        New-LoamsAclEntry -Id 'api-logs' -Path (& $x 'htdocs\loams_api\logs') -Kind Directory -Access Modify -Mode Grant -Grants @("${svc}:(OI)(CI)M") -Purpose 'API production error_log'
        New-LoamsAclEntry -Id 'verify-httpd' -Path (& $x 'apache\bin\httpd.exe') -Kind File -Access ReadExecute -Mode VerifyOnly -Purpose 'Apache binary is not writable'
        New-LoamsAclEntry -Id 'verify-httpd-conf' -Path (& $x 'apache\conf\httpd.conf') -Kind File -Access ReadExecute -Mode VerifyOnly -Purpose 'Apache config is not writable'
        New-LoamsAclEntry -Id 'verify-php-ini' -Path (& $x 'php\php.ini') -Kind File -Access ReadExecute -Mode VerifyOnly -Purpose 'PHP config is not writable'
        New-LoamsAclEntry -Id 'verify-api-config' -Path (& $x 'htdocs\loams_api\config.php') -Kind File -Access ReadExecute -Mode VerifyOnly -Purpose 'API code is not writable'
        New-LoamsAclEntry -Id 'loams-root' -Path $ProgramDataRoot -Kind Directory -Access None -Mode Protected -Grants $adminSys -AllowedSids @($script:LoamsSidAdmins, $script:LoamsSidSystem) -Purpose 'backups, reports, scripts: no Apache access'
        $bridge
        New-LoamsAclEntry -Id 'server-config' -Path (& $p 'server') -Kind Directory -Access Read -Mode Protected -Grants ($adminSys + "${svc}:(OI)(CI)R") -AllowedSids @($script:LoamsSidAdmins, $script:LoamsSidSystem, $svcSid) -Purpose 'compat.ini and controller config: read'
        New-LoamsAclEntry -Id 'server-logs' -Path (& $p 'server\logs') -Kind Directory -Access ReadExecute -Mode Protected -Grants ($adminSys + "${svc}:(RX)") -AllowedSids @($script:LoamsSidAdmins, $script:LoamsSidSystem, $svcSid) -Purpose 'guard log folder: traverse/attributes only'
        New-LoamsAclEntry -Id 'compat-log' -Path (& $p 'server\logs\compat-guard.log') -Kind File -Access AppendOnly -Mode Grant -Grants @("${svc}:(AD,S)") -Purpose 'compatibility log: append-only (FILE_APPEND_DATA)'
        New-LoamsAclEntry -Id 'server-tls' -Path (& $p 'server\tls') -Kind Directory -Access Read -Mode Protected -Grants ($adminSys + "${svc}:(OI)(CI)R") -AllowedSids @($script:LoamsSidAdmins, $script:LoamsSidSystem, $svcSid) -Purpose 'server and CA certificates: read'
        New-LoamsAclEntry -Id 'server-key' -Path (& $p 'server\tls\private') -Kind Directory -Access Read -Mode Protected -Grants @("${adm}:(OI)(CI)F", "${svc}:(OI)(CI)R") -AllowedSids @($script:LoamsSidAdmins, $svcSid) -Purpose 'server key: Apache + Administrators only'
    )
}

function Get-LoamsBridgeTaskAccount {
    [CmdletBinding()]
    param([string] $TaskName = 'LOAMS Turnstile Bridge')
    $task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -eq $task) { return '' }
    return [string]$task.Principal.UserId
}

function Get-LoamsAclSddl {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    return (Get-Acl -LiteralPath $Path -ErrorAction Stop).Sddl
}

function Get-LoamsSidMask {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Sddl, [Parameter(Mandatory)][string[]] $Sids)
    $sd = New-Object System.Security.AccessControl.RawSecurityDescriptor($Sddl)
    if ($null -eq $sd.DiscretionaryAcl) { return 0x1F01FF }
    $allow = 0; $deny = 0
    foreach ($ace in $sd.DiscretionaryAcl) {
        if ($ace -isnot [System.Security.AccessControl.CommonAce]) { continue }
        if (($ace.AceFlags -band [System.Security.AccessControl.AceFlags]::InheritOnly) -ne 0) { continue }
        if ($Sids -notcontains $ace.SecurityIdentifier.Value) { continue }
        if ($ace.AceQualifier -eq [System.Security.AccessControl.AceQualifier]::AccessAllowed) { $allow = $allow -bor $ace.AccessMask }
        elseif ($ace.AceQualifier -eq [System.Security.AccessControl.AceQualifier]::AccessDenied) { $deny = $deny -bor $ace.AccessMask }
    }
    return ($allow -band (-bnot $deny))
}

function Test-LoamsAclEntryCompliance {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Entry, [Parameter(Mandatory)][string] $Sddl, [Parameter(Mandatory)][string] $ServiceSid)
    $problems = @()
    $mask = Get-LoamsSidMask -Sddl $Sddl -Sids (@($ServiceSid) + $script:LoamsServiceTokenGroupSids)
    switch ($Entry.Access) {
        { $_ -in @('ReadExecute', 'Read') } {
            if (($mask -band $script:LoamsWriteMask) -ne 0) { $problems += 'service identity has write-class rights' }
            if (($mask -band $script:LoamsReadData) -eq 0) { $problems += 'service identity cannot read' }
        }
        'AppendOnly' {
            if (($mask -band $script:LoamsWriteMask -band (-bnot $script:LoamsAppendData)) -ne 0) { $problems += 'service identity has more than append rights' }
            if (($mask -band $script:LoamsAppendData) -eq 0) { $problems += 'service identity cannot append' }
        }
        'Modify' {
            if (($mask -band $script:LoamsWriteData) -eq 0) { $problems += 'service identity cannot write' }
        }
        'None' {
            if ($mask -ne 0) { $problems += 'service identity has access' }
        }
    }
    if ($Entry.Mode -eq 'Protected') {
        $sd = New-Object System.Security.AccessControl.RawSecurityDescriptor($Sddl)
        if (($sd.ControlFlags -band [System.Security.AccessControl.ControlFlags]::DiscretionaryAclProtected) -eq 0) { $problems += 'inheritance not disabled' }
        foreach ($ace in $sd.DiscretionaryAcl) {
            if ($ace -isnot [System.Security.AccessControl.CommonAce]) { continue }
            $sid = $ace.SecurityIdentifier.Value
            if ($Entry.AllowedSids -notcontains $sid) { $problems += "unexpected principal $sid" }
        }
    }
    return , @($problems | Select-Object -Unique)
}

function Test-LoamsAclCompliance {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object[]] $AclProfile, [Parameter(Mandatory)][string] $ServiceSid)
    $findings = @()
    foreach ($e in $AclProfile) {
        if (-not (Test-Path -LiteralPath $e.Path)) {
            if (-not $e.Optional) { $findings += [pscustomobject]@{ EntryId = $e.Id; Path = $e.Path; Problem = 'missing' } }
            continue
        }
        foreach ($p in (Test-LoamsAclEntryCompliance -Entry $e -Sddl (Get-LoamsAclSddl -Path $e.Path) -ServiceSid $ServiceSid)) {
            $findings += [pscustomobject]@{ EntryId = $e.Id; Path = $e.Path; Problem = $p }
        }
    }
    return [pscustomobject]@{ Compliant = ($findings.Count -eq 0); Findings = $findings }
}

function Test-LoamsAdminOnlyAcl {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Sddl)
    $sd = New-Object System.Security.AccessControl.RawSecurityDescriptor($Sddl)
    $trusted = @($script:LoamsSidAdmins, $script:LoamsSidSystem)
    if ($null -eq $sd.Owner -or $trusted -notcontains $sd.Owner.Value) { return $false }
    if ($null -eq $sd.DiscretionaryAcl) { return $false }
    foreach ($ace in $sd.DiscretionaryAcl) {
        if ($ace -isnot [System.Security.AccessControl.CommonAce]) { continue }
        if ($ace.AceQualifier -ne [System.Security.AccessControl.AceQualifier]::AccessAllowed) { continue }
        if (($ace.AccessMask -band $script:LoamsWriteMask) -ne 0 -and $trusted -notcontains $ace.SecurityIdentifier.Value) { return $false }
    }
    return $true
}

function Test-LoamsProtectedFolderAcl {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Sddl)
    $problems = @()
    $sd = New-Object System.Security.AccessControl.RawSecurityDescriptor($Sddl)
    $trusted = @($script:LoamsSidAdmins, $script:LoamsSidSystem)
    if (($sd.ControlFlags -band [System.Security.AccessControl.ControlFlags]::DiscretionaryAclProtected) -eq 0) { $problems += 'inheritance not disabled (access rules are not protected)' }
    if ($null -eq $sd.Owner -or $trusted -notcontains $sd.Owner.Value) { $problems += 'owner is not Administrators or SYSTEM' }
    if ($null -eq $sd.DiscretionaryAcl) {
        $problems += 'no DACL (everyone has access)'
    } else {
        $seen = @()
        foreach ($ace in $sd.DiscretionaryAcl) {
            if ($ace -isnot [System.Security.AccessControl.CommonAce]) { $problems += 'unsupported ACE type'; continue }
            $sid = $ace.SecurityIdentifier.Value
            if ($trusted -notcontains $sid) { $problems += "unexpected principal $sid"; continue }
            if ($ace.AceQualifier -ne [System.Security.AccessControl.AceQualifier]::AccessAllowed -or $ace.AccessMask -ne 0x1F01FF) { $problems += "unexpected rights for $sid"; continue }
            $seen += $sid
        }
        foreach ($t in $trusted) { if ($seen -notcontains $t) { $problems += "missing FullControl for $t" } }
    }
    return [pscustomobject]@{ Ok = ($problems.Count -eq 0); Problems = @($problems | Select-Object -Unique) }
}

function Set-LoamsAclEntry {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Entry)
    if ($Entry.Mode -eq 'VerifyOnly') { return }
    Assert-LoamsMutationAllowed -Action "icacls $($Entry.Path)"
    if (-not (Test-Path -LiteralPath $Entry.Path)) {
        if ($Entry.Optional) { Write-LoamsLog -Message "ACL skipped (optional, absent): $($Entry.Path)"; return }
        throw "ACL target missing: $($Entry.Path)"
    }
    switch ($Entry.Mode) {
        'HardenInherited' {
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList @($Entry.Path, '/inheritance:d') | Out-Null
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList @($Entry.Path, '/remove:g', '*S-1-5-11') | Out-Null
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList @($Entry.Path, '/grant:r', '*S-1-5-32-545:(OI)(CI)RX') | Out-Null
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList (@($Entry.Path, '/grant:r') + $Entry.Grants) | Out-Null
        }
        'Protected' {
            # Grant first so the DACL is never empty, then drop inheritance and every principal not in the grant list.
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList (@($Entry.Path, '/grant:r') + $Entry.Grants) | Out-Null
            $remove = @('*S-1-5-32-545', '*S-1-5-11', '*S-1-1-0', '*S-1-3-0')
            if ($Entry.AllowedSids -notcontains $script:LoamsSidSystem) { $remove += '*S-1-5-18' }
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList (@($Entry.Path, '/inheritance:r', '/remove:g') + $remove) | Out-Null
        }
        'Grant' {
            Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList (@($Entry.Path, '/grant:r') + $Entry.Grants) | Out-Null
        }
    }
    Write-LoamsLog -Message "ACL applied: $($Entry.Id) -> $($Entry.Path)"
}

function Initialize-LoamsServerLayout {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot)
    Assert-LoamsMutationAllowed -Action "create $ProgramDataRoot layout"
    $created = @()
    foreach ($rel in @('', 'server', 'server\logs', 'server\tls', 'server\tls\private', 'backups', 'reports')) {
        $dir = if ($rel) { Join-Path $ProgramDataRoot $rel } else { $ProgramDataRoot }
        if (-not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
            $created += $dir
        }
    }
    $log = Join-Path $ProgramDataRoot 'server\logs\compat-guard.log'
    if (-not (Test-Path -LiteralPath $log)) {
        [IO.File]::WriteAllBytes($log, [byte[]]@())
        $created += $log
    }
    return , $created
}
```

The compat log is **pre-created** by privileged setup because the append-only grant deliberately gives the Apache account no right to create files in that folder. S1d's log rotation task must recreate it with the same grant (D5).

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 22 tests. Set `expected-test-count.txt` to `93` and run the suite runner → `PASSED: 93 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): add least-privilege ACL profile, apply and compliance check`

---

### Task 7: LOAMS-Transport event source

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.EventSource.ps1`
- Create: `deploy/tests/EventSource.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `102`

**Interfaces:**
- Consumes: `Get-LoamsSidMask` (Task 6), `Assert-LoamsMutationAllowed`, `Write-LoamsLog` (Task 1).
- Produces:
  - `Get-LoamsEventLogNames` → `[string[]]` (subkey names of `HKLM:\SYSTEM\CurrentControlSet\Services\EventLog`; readable without elevation; wrapper for tests).
  - `Test-LoamsRegistryKey -Path <string>` → `[bool]`; `Get-LoamsRegistryValue -Path <string> -Name <string>` → value or `$null`.
  - `Get-LoamsEventSourceState [-Source <string>='LOAMS-Transport'] [-ServiceSid <string>]` → `[pscustomobject]@{ Source; Registered=[bool]; LogName=[string]; ApplicationLogSddl=[string]; ServiceCanWrite=[bool] or $null }` (`$null` = cannot be decided statically; staging probe decides).
  - `Test-LoamsEventLogWritableBy -Sddl <string> -Sids <string[]>` → `[bool]` (`ELF_LOGFILE_WRITE` = `0x2`).
  - `Register-LoamsEventSource [-Source] [-LogName <string>='Application']` → `[pscustomobject]@{ Created=[bool] }` (mutating; guarded).
  - `Unregister-LoamsEventSource [-Source]` (mutating; guarded; used only to undo a registration this run created).

No elevation is needed to *detect* the source: the dev-box check `[EventLog]::SourceExists` throws `SecurityException` when not elevated (it scans the Security log), so detection reads the registry keys directly instead.

- [ ] **Step 1: Write the failing tests `deploy/tests/EventSource.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    $script:Svc = Get-LoamsServiceSid -ServiceName 'Apache2.4'
    $script:Base = 'HKLM:\SYSTEM\CurrentControlSet\Services\EventLog'
}

Describe 'Get-LoamsEventSourceState' {
    BeforeEach {
        Mock -ModuleName LoamsHost Get-LoamsEventLogNames { @('Application', 'System') }
        Mock -ModuleName LoamsHost Get-LoamsRegistryValue { $null }
    }
    It 'finds a source registered under Application' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $Path -like '*\Application\LOAMS-Transport' }
        $s = Get-LoamsEventSourceState
        $s.Registered | Should -BeTrue
        $s.LogName | Should -Be 'Application'
    }
    It 'reports an unregistered source' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $false }
        (Get-LoamsEventSourceState).Registered | Should -BeFalse
    }
    It 'leaves writability undecided when the log has no CustomSD' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $false }
        (Get-LoamsEventSourceState -ServiceSid $script:Svc).ServiceCanWrite | Should -BeNullOrEmpty
    }
}

Describe 'Test-LoamsEventLogWritableBy' {
    It 'is true when SERVICE has write on the log' {
        Test-LoamsEventLogWritableBy -Sddl 'O:BAG:SYD:(A;;0xf0007;;;SY)(A;;0x7;;;BA)(A;;0x3;;;SU)' -Sids @($script:Svc, 'S-1-5-6') | Should -BeTrue
    }
    It 'is false when only Administrators can write' {
        Test-LoamsEventLogWritableBy -Sddl 'O:BAG:SYD:(A;;0x7;;;BA)' -Sids @($script:Svc, 'S-1-5-6') | Should -BeFalse
    }
}

Describe 'Register-LoamsEventSource' {
    BeforeEach {
        Mock -ModuleName LoamsHost New-EventLog { }
        Mock -ModuleName LoamsHost Get-LoamsEventLogNames { @('Application', 'System') }
        Mock -ModuleName LoamsHost Get-LoamsRegistryValue { $null }
    }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'registers LOAMS-Transport under Application' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $false }
        Set-LoamsMode -Mode Converge
        (Register-LoamsEventSource).Created | Should -BeTrue
        Should -Invoke -ModuleName LoamsHost New-EventLog -Times 1 -Exactly -ParameterFilter { $LogName -eq 'Application' -and $Source -contains 'LOAMS-Transport' }
    }
    It 'does not re-register an existing source' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $Path -like '*\Application\LOAMS-Transport' }
        Set-LoamsMode -Mode Converge
        (Register-LoamsEventSource).Created | Should -BeFalse
        Should -Invoke -ModuleName LoamsHost New-EventLog -Times 0
    }
    It 'refuses a source bound to another log' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $Path -like '*\System\LOAMS-Transport' }
        Set-LoamsMode -Mode Converge
        { Register-LoamsEventSource } | Should -Throw -ExpectedMessage "*registered under log 'System'*"
    }
    It 'refuses in Report mode' {
        Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $false }
        { Register-LoamsEventSource } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        Should -Invoke -ModuleName LoamsHost New-EventLog -Times 0
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\EventSource.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsEventSourceState` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.EventSource.ps1`**

```powershell
# LOAMS-Transport event source (spec §4: registered during privileged setup; S1d's guard writes to it).

$script:LoamsEventSource = 'LOAMS-Transport'
$script:LoamsEventLogRoot = 'HKLM:\SYSTEM\CurrentControlSet\Services\EventLog'

function Get-LoamsEventLogNames {
    [CmdletBinding()] param()
    return @(Get-ChildItem -Path $script:LoamsEventLogRoot -ErrorAction SilentlyContinue | ForEach-Object { $_.PSChildName })
}

function Test-LoamsRegistryKey {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    return [bool](Test-Path -Path $Path -ErrorAction SilentlyContinue)
}

function Get-LoamsRegistryValue {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][string] $Name)
    $item = Get-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue
    if ($null -eq $item) { return $null }
    return $item.$Name
}

function Test-LoamsEventLogWritableBy {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Sddl, [Parameter(Mandatory)][string[]] $Sids)
    return (((Get-LoamsSidMask -Sddl $Sddl -Sids $Sids) -band 0x2) -ne 0)
}

function Get-LoamsEventSourceState {
    [CmdletBinding()]
    param([string] $Source = $script:LoamsEventSource, [string] $ServiceSid = '')
    $logName = ''
    foreach ($log in (Get-LoamsEventLogNames)) {
        if (Test-LoamsRegistryKey -Path "$($script:LoamsEventLogRoot)\$log\$Source") { $logName = $log; break }
    }
    $sddl = Get-LoamsRegistryValue -Path "$($script:LoamsEventLogRoot)\Application" -Name 'CustomSD'
    $canWrite = $null
    if ($ServiceSid -and $sddl) {
        $canWrite = Test-LoamsEventLogWritableBy -Sddl $sddl -Sids (@($ServiceSid) + $script:LoamsServiceTokenGroupSids)
    }
    return [pscustomobject]@{
        Source = $Source; Registered = ($logName -ne ''); LogName = $logName
        ApplicationLogSddl = [string]$sddl; ServiceCanWrite = $canWrite
    }
}

function Register-LoamsEventSource {
    [CmdletBinding()]
    param([string] $Source = $script:LoamsEventSource, [string] $LogName = 'Application')
    Assert-LoamsMutationAllowed -Action "register event source $Source"
    $state = Get-LoamsEventSourceState -Source $Source
    if ($state.Registered) {
        if ($state.LogName -ne $LogName) { throw "Event source '$Source' is registered under log '$($state.LogName)', expected '$LogName'." }
        Write-LoamsLog -Message "Event source $Source already registered under $LogName"
        return [pscustomobject]@{ Created = $false }
    }
    New-EventLog -LogName $LogName -Source $Source -ErrorAction Stop
    Write-LoamsLog -Message "Event source $Source registered under $LogName"
    return [pscustomobject]@{ Created = $true }
}

function Unregister-LoamsEventSource {
    [CmdletBinding()]
    param([string] $Source = $script:LoamsEventSource)
    Assert-LoamsMutationAllowed -Action "remove event source $Source"
    Remove-EventLog -Source $Source -ErrorAction Stop
    Write-LoamsLog -Message "Event source $Source removed"
}
```

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 9 tests. Set `expected-test-count.txt` to `102` and run the suite runner → `PASSED: 102 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): register and inspect the LOAMS-Transport event source`

---

### Task 8: Service identity (Apache registration, virtual account, generic start/stop)

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.ServiceIdentity.ps1`
- Create: `deploy/tests/ServiceIdentity.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `113`

**Interfaces:**
- Consumes: `Invoke-LoamsExternal`, `Assert-LoamsMutationAllowed`, `Write-LoamsLog` (Task 1).
- Produces:
  - `Get-LoamsVirtualAccountName -ServiceName <string>` → `"NT SERVICE\<ServiceName>"`.
  - `Get-LoamsServiceAccountKind -StartName <string>` → `'None'|'LocalSystem'|'LocalService'|'NetworkService'|'VirtualAccount'|'User'`.
  - `Get-LoamsServiceSnapshot -ServiceName <string>` → `[pscustomobject]@{ Exists=[bool]; Name; StartName; StartMode; PathName; State; AccountKind }` (service name validated `^[A-Za-z0-9._-]+$` before it is put in the WQL filter).
  - `Wait-LoamsServiceState -ServiceName <string> -State <'Running'|'Stopped'> [-TimeoutSec <int>=60]` → throws on timeout.
  - `Stop-LoamsWindowsService` / `Start-LoamsWindowsService -ServiceName <string>` (mutating).
  - `Stop-LoamsControlPanelProcesses -Processes <pscustomobject[]>` (mutating; stops Control-Panel-launched `httpd` parents first).
  - `Start-LoamsControlPanelHttpd -HttpdExe <string>` (mutating; undo path only: restarts Apache detached the way the Control Panel does).
  - `Register-LoamsApacheService -HttpdExe <string> -ServiceName <string>` (mutating: `httpd -k install -n <name>`, then `sc.exe config <name> start= auto`).
  - `Unregister-LoamsApacheService -HttpdExe <string> -ServiceName <string>` (mutating: `httpd -k uninstall -n <name>`).
  - `Set-LoamsServiceAccount -ServiceName <string> -Account <string>` (mutating: `sc.exe config <name> obj= <account> password= ""`; refuses `User` kinds — D4).

- [ ] **Step 1: Write the failing tests `deploy/tests/ServiceIdentity.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
}

Describe 'Account helpers' {
    It 'builds the virtual account name' {
        Get-LoamsVirtualAccountName -ServiceName 'Apache2.4' | Should -Be 'NT SERVICE\Apache2.4'
    }
    It 'classifies service accounts' {
        Get-LoamsServiceAccountKind -StartName 'LocalSystem' | Should -Be 'LocalSystem'
        Get-LoamsServiceAccountKind -StartName 'NT AUTHORITY\LocalService' | Should -Be 'LocalService'
        Get-LoamsServiceAccountKind -StartName 'NT AUTHORITY\NetworkService' | Should -Be 'NetworkService'
        Get-LoamsServiceAccountKind -StartName 'NT SERVICE\Apache2.4' | Should -Be 'VirtualAccount'
        Get-LoamsServiceAccountKind -StartName '.\librarian' | Should -Be 'User'
        Get-LoamsServiceAccountKind -StartName '' | Should -Be 'None'
    }
}

Describe 'Get-LoamsServiceSnapshot' {
    It 'maps an existing service' {
        Mock -ModuleName LoamsHost Get-CimInstance { [pscustomobject]@{ Name = 'Apache2.4'; StartName = 'LocalSystem'; StartMode = 'Auto'; PathName = '"C:\xampp\apache\bin\httpd.exe" -k runservice'; State = 'Running' } }
        $s = Get-LoamsServiceSnapshot -ServiceName 'Apache2.4'
        $s.Exists | Should -BeTrue
        $s.AccountKind | Should -Be 'LocalSystem'
    }
    It 'reports a missing service' {
        Mock -ModuleName LoamsHost Get-CimInstance { $null }
        (Get-LoamsServiceSnapshot -ServiceName 'Apache2.4').Exists | Should -BeFalse
    }
    It 'rejects a service name that could alter the WQL filter' {
        { Get-LoamsServiceSnapshot -ServiceName "x' OR Name='y" } | Should -Throw
    }
}

Describe 'Mutating service operations' {
    BeforeEach { Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'registers the service with httpd -k install -n and sets automatic start' {
        Set-LoamsMode -Mode Converge
        Register-LoamsApacheService -HttpdExe 'C:\xampp\apache\bin\httpd.exe' -ServiceName 'Apache2.4'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $FilePath -like '*httpd.exe' -and ($ArgumentList -join ' ') -eq '-k install -n Apache2.4' }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $FilePath -like '*sc.exe' -and ($ArgumentList -join ' ') -eq 'config Apache2.4 start= auto' }
    }
    It 'unregisters with httpd -k uninstall -n' {
        Set-LoamsMode -Mode Converge
        Unregister-LoamsApacheService -HttpdExe 'C:\xampp\apache\bin\httpd.exe' -ServiceName 'Apache2.4'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { ($ArgumentList -join ' ') -eq '-k uninstall -n Apache2.4' }
    }
    It 'switches to the virtual account with sc.exe config obj=' {
        Set-LoamsMode -Mode Converge
        Set-LoamsServiceAccount -ServiceName 'Apache2.4' -Account 'NT SERVICE\Apache2.4'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter {
            $FilePath -like '*sc.exe' -and $ArgumentList.Count -eq 6 -and $ArgumentList[2] -eq 'obj=' -and $ArgumentList[3] -eq 'NT SERVICE\Apache2.4' -and $ArgumentList[4] -eq 'password=' -and $ArgumentList[5] -eq ''
        }
    }
    It 'refuses a password-based account' {
        Set-LoamsMode -Mode Converge
        { Set-LoamsServiceAccount -ServiceName 'Apache2.4' -Account '.\librarian' } | Should -Throw -ExpectedMessage '*password*'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0
    }
    It 'refuses every mutation in Report mode' {
        { Register-LoamsApacheService -HttpdExe 'x' -ServiceName 'Apache2.4' } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        { Set-LoamsServiceAccount -ServiceName 'Apache2.4' -Account 'NT SERVICE\Apache2.4' } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        { Stop-LoamsWindowsService -ServiceName 'Apache2.4' } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0
    }
}

Describe 'Start-LoamsWindowsService' {
    AfterEach { Set-LoamsMode -Mode Report }
    It 'throws when the service never reaches Running' {
        Set-LoamsMode -Mode Converge
        Mock -ModuleName LoamsHost Start-Service { }
        Mock -ModuleName LoamsHost Start-Sleep { }
        Mock -ModuleName LoamsHost Get-Service { [pscustomobject]@{ Status = 'Stopped' } }
        { Start-LoamsWindowsService -ServiceName 'Apache2.4' -TimeoutSec 3 } | Should -Throw -ExpectedMessage "*did not reach Running*"
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\ServiceIdentity.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsVirtualAccountName` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.ServiceIdentity.ps1`**

```powershell
# Apache service registration and identity (spec §4: NT SERVICE\Apache2.4 virtual account).

$script:LoamsSc = Join-Path $env:SystemRoot 'System32\sc.exe'

function Get-LoamsVirtualAccountName {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName)
    return "NT SERVICE\$ServiceName"
}

function Get-LoamsServiceAccountKind {
    [CmdletBinding()]
    param([AllowEmptyString()][AllowNull()][string] $StartName)
    if ([string]::IsNullOrWhiteSpace($StartName)) { return 'None' }
    if ($StartName -match '^(?i)(LocalSystem|\.\\LocalSystem|NT AUTHORITY\\SYSTEM)$') { return 'LocalSystem' }
    if ($StartName -match '^(?i)NT AUTHORITY\\LocalService$') { return 'LocalService' }
    if ($StartName -match '^(?i)NT AUTHORITY\\NetworkService$') { return 'NetworkService' }
    if ($StartName -match '^(?i)NT SERVICE\\') { return 'VirtualAccount' }
    return 'User'
}

function Assert-LoamsServiceName {
    param([string] $ServiceName)
    if ($ServiceName -notmatch '^[A-Za-z0-9._-]+$') { throw "Invalid service name '$ServiceName'." }
}

function Get-LoamsServiceSnapshot {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName)
    Assert-LoamsServiceName -ServiceName $ServiceName
    $s = Get-CimInstance -ClassName Win32_Service -Filter "Name='$ServiceName'" -ErrorAction Stop
    if ($null -eq $s) {
        return [pscustomobject]@{ Exists = $false; Name = $ServiceName; StartName = ''; StartMode = ''; PathName = ''; State = ''; AccountKind = 'None' }
    }
    return [pscustomobject]@{
        Exists = $true; Name = [string]$s.Name; StartName = [string]$s.StartName; StartMode = [string]$s.StartMode
        PathName = [string]$s.PathName; State = [string]$s.State; AccountKind = (Get-LoamsServiceAccountKind -StartName $s.StartName)
    }
}

function Wait-LoamsServiceState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName, [Parameter(Mandatory)][ValidateSet('Running', 'Stopped')][string] $State, [int] $TimeoutSec = 60)
    for ($i = 0; $i -lt $TimeoutSec; $i++) {
        $svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
        if ($svc -and [string]$svc.Status -eq $State) { return }
        Start-Sleep -Seconds 1
    }
    throw "Service '$ServiceName' did not reach $State within $TimeoutSec s."
}

function Stop-LoamsWindowsService {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName, [int] $TimeoutSec = 60)
    Assert-LoamsMutationAllowed -Action "stop service $ServiceName"
    Stop-Service -Name $ServiceName -Force -ErrorAction Stop
    Wait-LoamsServiceState -ServiceName $ServiceName -State Stopped -TimeoutSec $TimeoutSec
    Write-LoamsLog -Message "Service $ServiceName stopped"
}

function Start-LoamsWindowsService {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName, [int] $TimeoutSec = 60)
    Assert-LoamsMutationAllowed -Action "start service $ServiceName"
    Start-Service -Name $ServiceName -ErrorAction Stop
    Wait-LoamsServiceState -ServiceName $ServiceName -State Running -TimeoutSec $TimeoutSec
    Write-LoamsLog -Message "Service $ServiceName running"
}

function Stop-LoamsControlPanelProcesses {
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Processes)
    Assert-LoamsMutationAllowed -Action 'stop Control-Panel httpd'
    $ids = @($Processes | ForEach-Object { $_.ProcessId })
    $parents = @($Processes | Where-Object { $ids -notcontains $_.ParentProcessId })
    foreach ($p in ($parents + @($Processes | Where-Object { $parents -notcontains $_ }))) {
        Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
    }
    Write-LoamsLog -Message ("Stopped Control-Panel httpd processes: " + ($ids -join ','))
}

function Start-LoamsControlPanelHttpd {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $HttpdExe)
    Assert-LoamsMutationAllowed -Action 'start Control-Panel httpd'
    Start-Process -FilePath $HttpdExe -WorkingDirectory (Split-Path -Parent $HttpdExe) -WindowStyle Hidden | Out-Null
    Write-LoamsLog -Message 'Restarted httpd detached (Control-Panel style)'
}

function Register-LoamsApacheService {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $HttpdExe, [Parameter(Mandatory)][string] $ServiceName)
    Assert-LoamsServiceName -ServiceName $ServiceName
    Assert-LoamsMutationAllowed -Action "register service $ServiceName"
    Invoke-LoamsExternal -FilePath $HttpdExe -ArgumentList @('-k', 'install', '-n', $ServiceName) | Out-Null
    Invoke-LoamsExternal -FilePath $script:LoamsSc -ArgumentList @('config', $ServiceName, 'start=', 'auto') | Out-Null
    Write-LoamsLog -Message "Registered service $ServiceName"
}

function Unregister-LoamsApacheService {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $HttpdExe, [Parameter(Mandatory)][string] $ServiceName)
    Assert-LoamsServiceName -ServiceName $ServiceName
    Assert-LoamsMutationAllowed -Action "unregister service $ServiceName"
    Invoke-LoamsExternal -FilePath $HttpdExe -ArgumentList @('-k', 'uninstall', '-n', $ServiceName) | Out-Null
    Write-LoamsLog -Message "Unregistered service $ServiceName"
}

function Set-LoamsServiceAccount {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName, [Parameter(Mandatory)][string] $Account)
    Assert-LoamsServiceName -ServiceName $ServiceName
    Assert-LoamsMutationAllowed -Action "set account of $ServiceName"
    if ((Get-LoamsServiceAccountKind -StartName $Account) -eq 'User') {
        throw "Account '$Account' needs a password; password-based service accounts are not supported (stop and report)."
    }
    Invoke-LoamsExternal -FilePath $script:LoamsSc -ArgumentList @('config', $ServiceName, 'obj=', $Account, 'password=', '') | Out-Null
    Write-LoamsLog -Message "Service $ServiceName now configured to run as $Account"
}
```

`sc.exe` needs `password= ""` for built-in and virtual accounts to be accepted uniformly; `ConvertTo-LoamsArgumentString` renders the empty argument as `""` (Task 1 test), which is not a secret.

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 11 tests. Set `expected-test-count.txt` to `113` and run the suite runner → `PASSED: 113 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): manage Apache service registration and virtual-account identity`

---

### Task 9: Situation classification (shared by Apache and MariaDB) and the host fingerprint

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Classification.ps1`
- Create: `deploy/tests/Classification.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `130`

**Interfaces:**
- Consumes: `Get-LoamsServiceAccountKind`, `Get-LoamsVirtualAccountName` (Task 8).
- Produces:
  - `Get-LoamsServiceSituation -Services <object[]> -Processes <object[]> -BinaryPath <string> -ExpectedServiceName <string>` → `[pscustomobject]@{ Situation; Stop=[bool]; ServiceName; CurrentAccount; Reasons=[string[]] }` — the one service-situation algorithm used for both Apache (Task 9) and MariaDB (Task 10).
  - `Get-LoamsHostClassification -Layout -Services -Processes -HttpdInfo -PhpSapi [-ExpectedServiceName 'Apache2.4']` → `[pscustomobject]@{ Situation; StopAndReport; Convergeable; ServiceName; CurrentAccount; TargetAccount; PhpSapi; Reasons }` (Apache).
  - `New-LoamsHostFingerprint -ComputerName <string> -ApacheService <object> -MariaDbService <object> -ConfigHashes <hashtable> -AclSddl <hashtable> -Modules <object[]> [-LoamsState <hashtable>]` → `[pscustomobject]@{ computerName; apacheService; mariaDbService; configHashes=[string[]]; aclSddl=[string[]]; modules=[string[]]; loamsState=[string[]] }` (sorted `key=value` strings; `loamsState` = event source, retention-task definition hash, `ProgramData\LOAMS` layout existence + SDDL, deployed tools hashes — Task 11; service running state is excluded because it is volatile).
  - `Get-LoamsFingerprintHash -Fingerprint <object>` → lower-hex SHA-256 (this is the report's `profileHash`).
  - `Compare-LoamsHostFingerprint -Expected <object> -Actual <object>` → `[string[]]` human-readable differences (empty = no drift).

**Situations (spec §4 table → tool behaviour; the same rules apply to the MariaDB service in Task 10):**

| Situation | Detected when | Convergeable |
|---|---|---|
| `FreshInstall` | layout present, no service for the layout binary, no process | yes — register service, then identity |
| `ControlPanel` | no service for the layout binary, processes running from the layout | yes — stop, register, switch account, ACLs, verify |
| `ServiceNonVirtualAccount` | service with the expected name under LocalSystem / LocalService / NetworkService | yes — re-point to the virtual account |
| `ServiceOtherName` | one layout service with another name | yes — virtual account `NT SERVICE\<that name>` (D3) |
| `Converged` | service runs as `NT SERVICE\<its name>` | yes (re-verifies; drift is re-applied) |
| `PhpSapi = fastcgi` (Apache only) | `fcgid`/`proxy_fcgi` + `php-cgi` | flagged: `auto_prepend_file` goes in `php.ini` (S1d); identity convergence allowed |
| `NonXamppLayout`, `ApacheConfigError`, `MissingModSsl`, `UnsupportedVersion`, `UnknownPhpSapi`, `Ambiguous`, `PasswordAccount` | as named | **no — stop and report** |

A service of the same image name whose binary lies **outside** the layout (e.g. a separate MySQL install, as on this dev box: `MySQL80`, stopped) is ignored with a note when stopped and is `Ambiguous` (stop) when running.

- [ ] **Step 1: Write the failing tests `deploy/tests/Classification.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    $script:Layout = [pscustomobject]@{ IsXampp = $true; Missing = @(); HttpdExe = 'C:\xampp\apache\bin\httpd.exe' }
    $script:Httpd = [pscustomobject]@{ Version = '2.4.56'; HasModSsl = $true; ConfigOk = $true; ConfigError = ''; Modules = @('ssl_module', 'php_module') }
    function New-Svc { param([string] $Name = 'Apache2.4', [string] $Account = 'LocalSystem', [string] $State = 'Running', [string] $Path = '"C:\xampp\apache\bin\httpd.exe" -k runservice')
        [pscustomobject]@{ Name = $Name; StartName = $Account; PathName = $Path; State = $State; StartMode = 'Auto'; Exists = $true }
    }
    $script:Proc = [pscustomobject]@{ ProcessId = 10; ParentProcessId = 5; ExecutablePath = 'C:\xampp\apache\bin\httpd.exe' }
    function Classify { param($Services = @(), $Processes = @(), $Layout = $script:Layout, $Httpd = $script:Httpd, $Sapi = 'apache2handler')
        Get-LoamsHostClassification -Layout $Layout -Services $Services -Processes $Processes -HttpdInfo $Httpd -PhpSapi $Sapi
    }
    function New-Fp { param([string] $Account = 'LocalSystem', [string] $ConfHash = 'aa', [string] $Sddl = 'D:P(A;;FA;;;BA)', [string] $ModHash = ('ab' * 32), [string] $State = 'absent')
        New-LoamsHostFingerprint -ComputerName 'gate' -ApacheService (New-Svc -Account $Account) -MariaDbService (New-Svc -Name 'mysql' -Path 'C:\xampp\mysql\bin\mysqld.exe') `
            -ConfigHashes @{ 'C:\xampp\apache\conf\httpd.conf' = $ConfHash } -AclSddl @{ 'C:\xampp\mysql\data' = $Sddl } `
            -Modules @([pscustomobject]@{ Path = 'C:\xampp\php\php8ts.dll'; Sha256 = $ModHash }) -LoamsState @{ eventSource = $State }
    }
}

Describe 'Get-LoamsHostClassification (Apache)' {
    It 'FreshInstall: layout only' { (Classify).Situation | Should -Be 'FreshInstall' }
    It 'ControlPanel: httpd running without a service' {
        $c = Classify -Processes @($script:Proc)
        $c.Situation | Should -Be 'ControlPanel'
        $c.Convergeable | Should -BeTrue
        $c.TargetAccount | Should -Be 'NT SERVICE\Apache2.4'
    }
    It 'ServiceNonVirtualAccount: Apache2.4 under LocalSystem' {
        $c = Classify -Services @(New-Svc) -Processes @($script:Proc)
        $c.Situation | Should -Be 'ServiceNonVirtualAccount'
        $c.CurrentAccount | Should -Be 'LocalSystem'
    }
    It 'ServiceOtherName: targets that service''s own virtual account' {
        $c = Classify -Services @(New-Svc -Name 'ApacheLoams') -Processes @($script:Proc)
        $c.Situation | Should -Be 'ServiceOtherName'
        $c.TargetAccount | Should -Be 'NT SERVICE\ApacheLoams'
    }
    It 'Converged: already on the virtual account' {
        (Classify -Services @(New-Svc -Account 'NT SERVICE\Apache2.4') -Processes @($script:Proc)).Situation | Should -Be 'Converged'
    }
    It 'stops on a non-XAMPP layout' {
        $c = Classify -Layout ([pscustomobject]@{ IsXampp = $false; Missing = @('php\php.exe'); HttpdExe = 'x' })
        $c.Situation | Should -Be 'NonXamppLayout'
        $c.StopAndReport | Should -BeTrue
    }
    It 'stops on a missing mod_ssl' {
        $h = [pscustomobject]@{ Version = '2.4.56'; HasModSsl = $false; ConfigOk = $true; ConfigError = ''; Modules = @() }
        (Classify -Httpd $h).Situation | Should -Be 'MissingModSsl'
    }
    It 'stops on an unsupported Apache version' {
        $h = [pscustomobject]@{ Version = '2.2.34'; HasModSsl = $true; ConfigOk = $true; ConfigError = ''; Modules = @() }
        (Classify -Httpd $h).Situation | Should -Be 'UnsupportedVersion'
    }
    It 'stops on a password-based service account' {
        $c = Classify -Services @(New-Svc -Account '.\librarian') -Processes @($script:Proc)
        $c.Situation | Should -Be 'PasswordAccount'
        $c.Convergeable | Should -BeFalse
    }
    It 'stops when more than one layout service exists' {
        (Classify -Services @((New-Svc), (New-Svc -Name 'Apache2.4b'))).Situation | Should -Be 'Ambiguous'
    }
    It 'flags FastCGI but still allows identity convergence' {
        $c = Classify -Services @(New-Svc) -Processes @($script:Proc) -Sapi 'fastcgi'
        $c.Convergeable | Should -BeTrue
        ($c.Reasons -join ' ') | Should -Match 'auto_prepend_file'
    }
}

Describe 'Get-LoamsServiceSituation (shared)' {
    It 'ignores a stopped service of the same image outside the layout, with a note' {
        $foreign = New-Svc -Name 'MySQL80' -State 'Stopped' -Path '"C:\Program Files\MySQL\MySQL Server 8.0\bin\mysqld.exe" MySQL80'
        $mine = New-Svc -Name 'mysql' -Path 'C:\xampp\mysql\bin\mysqld.exe --defaults-file=c:\xampp\mysql\bin\my.ini mysql'
        $s = Get-LoamsServiceSituation -Services @($foreign, $mine) -Processes @() -BinaryPath 'C:\xampp\mysql\bin\mysqld.exe' -ExpectedServiceName 'mysql'
        $s.Situation | Should -Be 'ServiceNonVirtualAccount'
        ($s.Reasons -join ' ') | Should -Match 'MySQL80'
    }
    It 'stops when a running service of the same image lives outside the layout' {
        $foreign = New-Svc -Name 'MySQL80' -State 'Running' -Path '"C:\Program Files\MySQL\MySQL Server 8.0\bin\mysqld.exe" MySQL80'
        (Get-LoamsServiceSituation -Services @($foreign) -Processes @() -BinaryPath 'C:\xampp\mysql\bin\mysqld.exe' -ExpectedServiceName 'mysql').Situation | Should -Be 'Ambiguous'
    }
}

Describe 'Host fingerprint' {
    It 'hashes deterministically and changes when a loaded module changes' {
        $a = Get-LoamsFingerprintHash -Fingerprint (New-Fp)
        $b = Get-LoamsFingerprintHash -Fingerprint (New-Fp)
        $c = Get-LoamsFingerprintHash -Fingerprint (New-Fp -ModHash ('cd' * 32))
        $a | Should -Be $b
        $a | Should -Not -Be $c
        $a | Should -Match '^[0-9a-f]{64}$'
    }
    It 'reports a changed service account' {
        $d = Compare-LoamsHostFingerprint -Expected (New-Fp) -Actual (New-Fp -Account 'NT SERVICE\Apache2.4')
        ($d -join ' ') | Should -Match 'apacheService'
    }
    It 'reports changed config hashes and ACLs, and survives a JSON round trip' {
        $expected = (New-Fp) | ConvertTo-Json -Depth 4 | ConvertFrom-Json
        $d = Compare-LoamsHostFingerprint -Expected $expected -Actual (New-Fp -ConfHash 'bb' -Sddl 'D:P(A;;FA;;;BU)')
        ($d -join ' ') | Should -Match 'configHashes: changed C:\\xampp\\apache\\conf\\httpd.conf'
        ($d -join ' ') | Should -Match 'aclSddl: changed C:\\xampp\\mysql\\data'
        @(Compare-LoamsHostFingerprint -Expected $expected -Actual (New-Fp)).Count | Should -Be 0
    }
    It 'reports changed LOAMS host state (event source, retention task, layout, tools)' {
        ((Compare-LoamsHostFingerprint -Expected (New-Fp) -Actual (New-Fp -State 'registered:Application')) -join ' ') | Should -Match 'loamsState: changed eventSource'
        (Get-LoamsFingerprintHash -Fingerprint (New-Fp)) | Should -Not -Be (Get-LoamsFingerprintHash -Fingerprint (New-Fp -State 'registered:Application'))
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Classification.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsHostClassification` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.Classification.ps1`**

```powershell
# Situation classification (spec §4 table) and the host fingerprint used for drift detection.

function Get-LoamsServiceSituation {
    [CmdletBinding()]
    param(
        [AllowEmptyCollection()][object[]] $Services = @(),
        [AllowEmptyCollection()][object[]] $Processes = @(),
        [Parameter(Mandatory)][string] $BinaryPath,
        [Parameter(Mandatory)][string] $ExpectedServiceName
    )
    $name = $ExpectedServiceName
    $account = ''
    $notes = @()
    $result = { param($situation, [bool] $stop, $why)
        [pscustomobject]@{ Situation = $situation; Stop = $stop; ServiceName = $name; CurrentAccount = $account
            Reasons = @(@($why) + $notes | Where-Object { $_ }) } }

    $mine = @($Services | Where-Object { [string]$_.PathName -like "*$BinaryPath*" })
    foreach ($f in @($Services | Where-Object { [string]$_.PathName -notlike "*$BinaryPath*" })) {
        if ($f.State -eq 'Running') { return (& $result 'Ambiguous' $true "running service '$($f.Name)' uses a binary outside the layout: $($f.PathName)") }
        $notes += "ignored stopped service '$($f.Name)' outside the layout ($($f.PathName))"
    }
    if ($mine.Count -gt 1) { return (& $result 'Ambiguous' $true ("more than one service uses ${BinaryPath}: " + (($mine | ForEach-Object Name) -join ', '))) }
    $foreignProc = @($Processes | Where-Object { $_.ExecutablePath -and $_.ExecutablePath -ne $BinaryPath })
    if ($foreignProc.Count -gt 0) { return (& $result 'Ambiguous' $true ('process running from outside the layout: ' + (($foreignProc | ForEach-Object ExecutablePath) -join ', '))) }

    if ($mine.Count -eq 1) {
        $svc = $mine[0]
        $name = $svc.Name
        $account = $svc.StartName
        $kind = Get-LoamsServiceAccountKind -StartName $svc.StartName
        if ($kind -eq 'User') { return (& $result 'PasswordAccount' $true "service '$name' runs as '$account', which needs a password; rollback could not restore it (D4)") }
        if ($svc.State -ne 'Running' -and $Processes.Count -gt 0) { return (& $result 'Ambiguous' $true "a process is running although service '$name' is not") }
        if ($kind -eq 'VirtualAccount' -and $svc.StartName -eq (Get-LoamsVirtualAccountName -ServiceName $svc.Name)) { return (& $result 'Converged' $false $null) }
        if ($svc.Name -ne $ExpectedServiceName) { return (& $result 'ServiceOtherName' $false "service is named '$name'; its virtual account NT SERVICE\$name will be used (D3)") }
        return (& $result 'ServiceNonVirtualAccount' $false $null)
    }
    if ($Processes.Count -gt 0) { return (& $result 'ControlPanel' $false 'running without a Windows service (XAMPP Control Panel)') }
    return (& $result 'FreshInstall' $false $null)
}

function New-LoamsClassification {
    param([string] $Situation, [bool] $Stop, [string] $ServiceName, [string] $CurrentAccount, [string] $PhpSapi, [string[]] $Reasons)
    return [pscustomobject]@{
        Situation = $Situation; StopAndReport = $Stop; Convergeable = (-not $Stop)
        ServiceName = $ServiceName; CurrentAccount = $CurrentAccount
        TargetAccount = (Get-LoamsVirtualAccountName -ServiceName $ServiceName)
        PhpSapi = $PhpSapi; Reasons = @($Reasons | Where-Object { $_ })
    }
}

function Get-LoamsHostClassification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $Layout,
        [AllowEmptyCollection()][object[]] $Services = @(),
        [AllowEmptyCollection()][object[]] $Processes = @(),
        [Parameter(Mandatory)] $HttpdInfo,
        [Parameter(Mandatory)][string] $PhpSapi,
        [string] $ExpectedServiceName = 'Apache2.4'
    )
    $stop = { param($situation, $why) New-LoamsClassification -Situation $situation -Stop $true -ServiceName $ExpectedServiceName -CurrentAccount '' -PhpSapi $PhpSapi -Reasons @($why) }
    if (-not $Layout.IsXampp) { return (& $stop 'NonXamppLayout' ('XAMPP layout incomplete: ' + ($Layout.Missing -join ', '))) }
    if (-not $HttpdInfo.ConfigOk) { return (& $stop 'ApacheConfigError' ('httpd -M failed: ' + $HttpdInfo.ConfigError)) }
    if (-not $HttpdInfo.HasModSsl) { return (& $stop 'MissingModSsl' 'mod_ssl (ssl_module) is not loaded') }
    if ($HttpdInfo.Version -notmatch '^2\.4\.\d+$') { return (& $stop 'UnsupportedVersion' "Apache version '$($HttpdInfo.Version)' is not 2.4.x") }
    if ($PhpSapi -eq 'unknown') { return (& $stop 'UnknownPhpSapi' 'PHP is neither php_module nor FastCGI') }

    $s = Get-LoamsServiceSituation -Services $Services -Processes $Processes -BinaryPath $Layout.HttpdExe -ExpectedServiceName $ExpectedServiceName
    $reasons = @($s.Reasons)
    if ($PhpSapi -eq 'fastcgi') { $reasons += 'PHP runs as FastCGI: auto_prepend_file must be set in php.ini and verified active (applied by S1d)' }
    return New-LoamsClassification -Situation $s.Situation -Stop $s.Stop -ServiceName $s.ServiceName -CurrentAccount $s.CurrentAccount -PhpSapi $PhpSapi -Reasons $reasons
}

function ConvertTo-LoamsFingerprintLines {
    param([hashtable] $Map)
    if ($null -eq $Map) { return , @() }
    return , @($Map.Keys | ForEach-Object { "$([string]$_)=$([string]$Map[$_])" } | Sort-Object)
}

function New-LoamsHostFingerprint {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $ComputerName, [AllowNull()] $ApacheService, [AllowNull()] $MariaDbService,
        [hashtable] $ConfigHashes = @{}, [hashtable] $AclSddl = @{}, [AllowEmptyCollection()][object[]] $Modules = @(),
        [hashtable] $LoamsState = @{}
    )
    $svc = { param($s) if ($null -eq $s -or -not $s.Exists) { 'absent' } else { '{0}|{1}|{2}|{3}' -f $s.Name, $s.StartName, $s.StartMode, $s.PathName } }
    return [pscustomobject]@{
        computerName   = $ComputerName.ToUpperInvariant()
        apacheService  = (& $svc $ApacheService)
        mariaDbService = (& $svc $MariaDbService)
        configHashes   = (ConvertTo-LoamsFingerprintLines -Map $ConfigHashes)
        aclSddl        = (ConvertTo-LoamsFingerprintLines -Map $AclSddl)
        modules        = @($Modules | ForEach-Object { "$($_.Path.ToLowerInvariant())=$($_.Sha256)" } | Sort-Object)
        loamsState     = (ConvertTo-LoamsFingerprintLines -Map $LoamsState)
    }
}

function Get-LoamsFingerprintHash {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Fingerprint)
    $lines = @("computer=$($Fingerprint.computerName)", "apache=$($Fingerprint.apacheService)", "mariadb=$($Fingerprint.mariaDbService)")
    foreach ($section in @('configHashes', 'aclSddl', 'modules', 'loamsState')) {
        if ($null -eq $Fingerprint.PSObject.Properties[$section]) { continue }
        foreach ($l in @($Fingerprint.$section)) { if ($l) { $lines += "$section|$l" } }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))) } finally { $sha.Dispose() }
    return (($hash | ForEach-Object { $_.ToString('x2') }) -join '')
}

function Compare-LoamsHostFingerprint {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Expected, [Parameter(Mandatory)] $Actual)
    $diff = @()
    foreach ($f in @('computerName', 'apacheService', 'mariaDbService')) {
        if ([string]$Expected.$f -ne [string]$Actual.$f) { $diff += "${f}: '$($Expected.$f)' -> '$($Actual.$f)'" }
    }
    foreach ($section in @('configHashes', 'aclSddl', 'modules', 'loamsState')) {
        $hasE = $null -ne $Expected.PSObject.Properties[$section]; $hasA = $null -ne $Actual.PSObject.Properties[$section]
        if (-not $hasE -and -not $hasA) { continue }
        if ($hasE -ne $hasA) { $diff += "${section}: present on one side only"; continue }
        $e = @{}; foreach ($l in @($Expected.$section)) { if ($l) { $i = $l.IndexOf('='); $e[$l.Substring(0, $i)] = $l.Substring($i + 1) } }
        $a = @{}; foreach ($l in @($Actual.$section)) { if ($l) { $i = $l.IndexOf('='); $a[$l.Substring(0, $i)] = $l.Substring($i + 1) } }
        foreach ($k in $e.Keys) {
            if (-not $a.ContainsKey($k)) { $diff += "${section}: removed $k" }
            elseif ($a[$k] -ne $e[$k]) { $diff += "${section}: changed $k" }
        }
        foreach ($k in $a.Keys) { if (-not $e.ContainsKey($k)) { $diff += "${section}: added $k" } }
    }
    return , @($diff)
}
```

Fingerprint keys are paths, which never contain `=`; a value may contain `=` (SDDL does not, hashes do not), and the split uses the first `=` only.

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 17 tests. Set `expected-test-count.txt` to `130` and run the suite runner → `PASSED: 130 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): classify host situations and fingerprint the host for drift checks`

---

### Task 10: MariaDB host support — detection, classification, ACL profile, service registration

The XAMPP MariaDB data directory today inherits `BUILTIN\Users:(RX)` and `Authenticated Users:(M)` (dev box, `icacls C:\xampp\mysql\data`), so any local user — and the Apache account — can read or alter raw database files. S1b hardens it (owner decision, replaces the earlier "defer to S4"): MariaDB runs as its own virtual account, the data and log directories are Administrators + SYSTEM + that account only, and the MariaDB checkpoints are mandatory for a production convergence (Task 17).

**Files:** (the fake XAMPP tree from Task 1 already contains `mysql\bin\{mysqld,mysql,mysqladmin}.exe`, a `my.ini` pointing into the fake tree, and `mysql\data\`)
- Create: `deploy/server/LoamsHost/LoamsHost.MariaDb.ps1`
- Create: `deploy/tests/MariaDb.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `147`

**Interfaces:**
- Consumes: `Invoke-LoamsExternal`, `Assert-LoamsMutationAllowed`, `Write-LoamsLog` (Task 1); `New-LoamsAclEntry`, `Get-LoamsServiceSid` (Task 6); `Get-LoamsServiceSituation`, `New-LoamsClassification` (Task 9); `Assert-LoamsServiceName` (Task 8).
- Produces:
  - `Get-LoamsMariaDbServices` → `[pscustomobject[]]` (same shape as `Get-LoamsHttpdServices`) for every `Win32_Service` whose `PathName` contains `\mysqld.exe`.
  - `Get-LoamsMariaDbProcesses` → `[pscustomobject[]]` `@{ ProcessId; ParentProcessId; ExecutablePath }` for `mysqld.exe`.
  - `Get-LoamsMyIniPath -XamppRoot <string> -Services <object[]>` → `[string]` (`--defaults-file=` of the layout service, else `<XamppRoot>\mysql\bin\my.ini`).
  - `Get-LoamsMariaDbLayout -XamppRoot <string> [-MyIniPath <string>]` → `[pscustomobject]@{ Root; IsXampp; MysqldExe; MysqlExe; MysqladminExe; MysqldumpExe; MyIni; DataDir; TmpDir; Port=[int] (default 3306); LogFiles=[string[]]; LogDirs=[string[]]; Problems=[string[]] }` (parses `[mysqld]`: `datadir`, `tmpdir`, `log_error`/`log-error`, `general_log_file`, `slow_query_log_file`; relative log paths resolve against `datadir`).
  - `Get-LoamsMariaDbClassification -Layout <pscustomobject> -Services <object[]> -Processes <object[]> [-ExpectedServiceName 'mysql']` → same shape as `Get-LoamsHostClassification` (`PhpSapi` = `''`). Stops: `NoXamppMariaDb`, `MariaDbLayoutOutsideTree`, plus the shared `Ambiguous` / `PasswordAccount`.
  - `Get-LoamsMariaDbAclProfile -XamppRoot <string> -Layout <pscustomobject> [-ServiceName 'mysql']` → ACL entries (Task 6 shape).
  - `Register-LoamsMariaDbService -MysqldExe -ServiceName -MyIni` (mutating: `mysqld --install <name> --defaults-file=<my.ini>`, then `sc.exe config <name> start= auto`).
  - `Unregister-LoamsMariaDbService -MysqldExe -ServiceName` (mutating: `mysqld --remove <name>`).
  - `Test-LoamsProcessAlive -ProcessId`, `Test-LoamsTcpPortListening -Port`, `Test-LoamsFileReleased -Path` → `[bool]` (read-only wrappers, mocked in tests).
  - `Stop-LoamsControlPanelMysqld -MysqladminExe -DefaultsFile -Processes [-Port 3306] [-DataDir] [-TimeoutSec 120]` (mutating: graceful `mysqladmin --defaults-extra-file=<f> shutdown` — never `Stop-Process` on a database — then **verifies** within the timeout that every captured PID has exited, the port is no longer listening and `ibdata1` can be opened exclusively; otherwise throws, so the checkpoint fails and rolls back).
  - `Start-LoamsControlPanelMysqld -MysqldExe -MyIni` (mutating; undo path only: `mysqld --defaults-file=<my.ini> --standalone`, detached).

**MariaDB identity decision (D16):** a Control-Panel-launched `mysqld` is **registered as a service** (`mysqld --install mysql --defaults-file=<my.ini>`) exactly like Apache, because a virtual account exists only for a service; leaving it Control-Panel-launched would keep it under the logged-on user and the data directory would have to stay readable by that user. Service name default `mysql` (XAMPP's own name, as on this dev box); another existing name keeps its own `NT SERVICE\<name>` (D3).

**MariaDB ACL profile (`<db>` = `NT SERVICE\<MariaDB service>`):**

| Id | Path | Mode | Grants | Why |
|---|---|---|---|---|
| `mariadb-tree` | `<XamppRoot>\mysql` | Grant | `<db>:(OI)(CI)RX` | binaries, `share`, `lib\plugin`, `my.ini` (read) |
| `mariadb-data` | `datadir` (default `<XamppRoot>\mysql\data`) | Protected | `*S-1-5-32-544:(OI)(CI)F`, `*S-1-5-18:(OI)(CI)F`, `<db>:(OI)(CI)M` | data, InnoDB/Aria logs, `mysql_error.log`, pid — **no Users, no Authenticated Users, no Apache** |
| `mariadb-logdir-N` | each log directory outside `datadir` (must be under `<XamppRoot>\mysql`) | Protected | same as data | general/slow/error logs |
| `mariadb-tmp` | `tmpdir` (dev box: `<XamppRoot>\tmp`, shared with PHP) | Grant | `<db>:(OI)(CI)M` | sort/temp files; not protected because PHP uses it too |

- [ ] **Step 1: Write the failing tests `deploy/tests/MariaDb.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Layout = Get-LoamsMariaDbLayout -XamppRoot $script:Root
    $script:DbSid = Get-LoamsServiceSid -ServiceName 'mysql'
    function New-DbSvc { param([string] $Name = 'mysql', [string] $Account = 'LocalSystem', [string] $State = 'Running', [string] $Path)
        if (-not $Path) { $Path = "$($script:Layout.MysqldExe) --defaults-file=$($script:Layout.MyIni) $Name" }
        [pscustomobject]@{ Name = $Name; StartName = $Account; PathName = $Path; State = $State; StartMode = 'Auto'; ProcessId = 20 }
    }
    $script:DbProc = [pscustomobject]@{ ProcessId = 20; ParentProcessId = 4; ExecutablePath = $script:Layout.MysqldExe }
    function Classify-Db { param($Services = @(), $Processes = @(), $Layout = $script:Layout)
        Get-LoamsMariaDbClassification -Layout $Layout -Services $Services -Processes $Processes
    }
}

Describe 'MariaDB detection' {
    It 'returns only services whose binary is mysqld.exe' {
        Mock -ModuleName LoamsHost Get-CimInstance {
            @([pscustomobject]@{ Name = 'mysql'; DisplayName = 'mysql'; StartName = 'LocalSystem'; PathName = 'C:\xampp\mysql\bin\mysqld.exe --defaults-file=c:\xampp\mysql\bin\my.ini mysql'; State = 'Running'; StartMode = 'Auto'; ProcessId = 20 },
              [pscustomobject]@{ Name = 'Apache2.4'; DisplayName = 'Apache2.4'; StartName = 'LocalSystem'; PathName = '"C:\xampp\apache\bin\httpd.exe" -k runservice'; State = 'Running'; StartMode = 'Auto'; ProcessId = 10 })
        } -ParameterFilter { $ClassName -eq 'Win32_Service' }
        $s = @(Get-LoamsMariaDbServices)
        $s.Count | Should -Be 1
        $s[0].Name | Should -Be 'mysql'
    }
    It 'parses my.ini and resolves the relative error log against datadir' {
        $script:Layout.IsXampp | Should -BeTrue
        $script:Layout.DataDir | Should -Be (Join-Path $script:Root 'mysql\data')
        $script:Layout.TmpDir | Should -Be (Join-Path $script:Root 'tmp')
        $script:Layout.LogFiles | Should -Contain (Join-Path $script:Root 'mysql\data\mysql_error.log')
        $script:Layout.Problems.Count | Should -Be 0
    }
    It 'reports a log file outside the MariaDB tree as a problem' {
        $other = Join-Path $TestDrive 'xampp-log'; New-LoamsFakeXampp -Root $other | Out-Null
        Add-Content -Path (Join-Path $other 'mysql\bin\my.ini') -Value 'general_log_file="C:/elsewhere/general.log"'
        (Get-LoamsMariaDbLayout -XamppRoot $other).Problems -join ' ' | Should -Match 'outside'
    }
}

Describe 'Get-LoamsMariaDbClassification' {
    It 'ServiceNonVirtualAccount: XAMPP mysql service under LocalSystem' {
        $c = Classify-Db -Services @(New-DbSvc) -Processes @($script:DbProc)
        $c.Situation | Should -Be 'ServiceNonVirtualAccount'
        $c.TargetAccount | Should -Be 'NT SERVICE\mysql'
    }
    It 'ControlPanel: mysqld running without a service' {
        (Classify-Db -Processes @($script:DbProc)).Situation | Should -Be 'ControlPanel'
    }
    It 'Converged: already on NT SERVICE\mysql' {
        (Classify-Db -Services @(New-DbSvc -Account 'NT SERVICE\mysql') -Processes @($script:DbProc)).Situation | Should -Be 'Converged'
    }
    It 'stops when the XAMPP MariaDB binaries are missing' {
        $c = Classify-Db -Layout ([pscustomobject]@{ IsXampp = $false; MysqldExe = 'x'; Problems = @('mysql\bin\mysqld.exe missing') })
        $c.Situation | Should -Be 'NoXamppMariaDb'
        $c.StopAndReport | Should -BeTrue
    }
    It 'ignores a stopped MySQL install outside the layout' {
        $foreign = New-DbSvc -Name 'MySQL80' -State 'Stopped' -Path '"C:\Program Files\MySQL\MySQL Server 8.0\bin\mysqld.exe" MySQL80'
        (Classify-Db -Services @($foreign, (New-DbSvc)) -Processes @($script:DbProc)).Situation | Should -Be 'ServiceNonVirtualAccount'
    }
}

Describe 'Get-LoamsMariaDbAclProfile' {
    BeforeAll { $script:DbProfile = Get-LoamsMariaDbAclProfile -XamppRoot $script:Root -Layout $script:Layout }
    It 'protects the data directory for Administrators, SYSTEM and the MariaDB account only' {
        $e = $script:DbProfile | Where-Object Id -eq 'mariadb-data'
        $e.Mode | Should -Be 'Protected'
        ($e.Grants -join ' ') | Should -Be '*S-1-5-32-544:(OI)(CI)F *S-1-5-18:(OI)(CI)F NT SERVICE\mysql:(OI)(CI)M'
        ($e.AllowedSids -join ' ') | Should -Be "S-1-5-32-544 S-1-5-18 $script:DbSid"
    }
    It 'grants read on the MariaDB tree and modify on tmpdir' {
        ($script:DbProfile | Where-Object Id -eq 'mariadb-tree').Grants | Should -Be @('NT SERVICE\mysql:(OI)(CI)RX')
        ($script:DbProfile | Where-Object Id -eq 'mariadb-tmp').Grants | Should -Be @('NT SERVICE\mysql:(OI)(CI)M')
    }
    It 'flags ordinary Users read on the data directory' {
        $e = $script:DbProfile | Where-Object Id -eq 'mariadb-data'
        $sddl = "O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x1301bf;;;$script:DbSid)(A;OICI;0x1200a9;;;BU)"
        Test-LoamsAclEntryCompliance -Entry $e -Sddl $sddl -ServiceSid $script:DbSid | Should -Contain 'unexpected principal S-1-5-32-545'
    }
}

Describe 'MariaDB service operations' {
    BeforeEach { Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'registers mysqld as a service with its my.ini' {
        Set-LoamsMode -Mode Converge
        Register-LoamsMariaDbService -MysqldExe 'C:\xampp\mysql\bin\mysqld.exe' -ServiceName 'mysql' -MyIni 'C:\xampp\mysql\bin\my.ini'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $FilePath -like '*mysqld.exe' -and ($ArgumentList -join ' ') -eq '--install mysql --defaults-file=C:\xampp\mysql\bin\my.ini' }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $FilePath -like '*sc.exe' -and ($ArgumentList -join ' ') -eq 'config mysql start= auto' }
    }
    It 'unregisters with mysqld --remove' {
        Set-LoamsMode -Mode Converge
        Unregister-LoamsMariaDbService -MysqldExe 'C:\xampp\mysql\bin\mysqld.exe' -ServiceName 'mysql'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { ($ArgumentList -join ' ') -eq '--remove mysql' }
    }
    It 'stops a Control-Panel mysqld gracefully and verifies PIDs, port and data files' {
        Set-LoamsMode -Mode Converge
        Mock -ModuleName LoamsHost Test-LoamsProcessAlive { $false }
        Mock -ModuleName LoamsHost Test-LoamsTcpPortListening { $false }
        Mock -ModuleName LoamsHost Test-LoamsFileReleased { $true }
        Stop-LoamsControlPanelMysqld -MysqladminExe 'C:\xampp\mysql\bin\mysqladmin.exe' -DefaultsFile 'C:\b\x.cnf' -Processes @($script:DbProc) -Port 3306 -DataDir $script:Layout.DataDir
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $ArgumentList[0] -eq '--defaults-extra-file=C:\b\x.cnf' -and $ArgumentList[1] -eq 'shutdown' }
        Should -Invoke -ModuleName LoamsHost Test-LoamsProcessAlive -ParameterFilter { $ProcessId -eq 20 }
    }
    It 'fails when a captured mysqld PID is still running after the timeout' {
        Set-LoamsMode -Mode Converge
        Mock -ModuleName LoamsHost Test-LoamsProcessAlive { $true }
        Mock -ModuleName LoamsHost Test-LoamsTcpPortListening { $false }
        Mock -ModuleName LoamsHost Test-LoamsFileReleased { $true }
        Mock -ModuleName LoamsHost Start-Sleep { }
        { Stop-LoamsControlPanelMysqld -MysqladminExe 'x' -DefaultsFile 'y' -Processes @($script:DbProc) -TimeoutSec 2 } | Should -Throw -ExpectedMessage '*did not stop within 2 s: running PIDs `[20`]*'
    }
    It 'fails when the MariaDB port is still listening after the timeout' {
        Set-LoamsMode -Mode Converge
        Mock -ModuleName LoamsHost Test-LoamsProcessAlive { $false }
        Mock -ModuleName LoamsHost Test-LoamsTcpPortListening { $true }
        Mock -ModuleName LoamsHost Test-LoamsFileReleased { $true }
        Mock -ModuleName LoamsHost Start-Sleep { }
        { Stop-LoamsControlPanelMysqld -MysqladminExe 'x' -DefaultsFile 'y' -Processes @($script:DbProc) -TimeoutSec 2 } | Should -Throw -ExpectedMessage '*port 3306 listening: True*'
    }
    It 'refuses every mutation in Report mode' {
        { Register-LoamsMariaDbService -MysqldExe 'x' -ServiceName 'mysql' -MyIni 'y' } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        { Stop-LoamsControlPanelMysqld -MysqladminExe 'x' -DefaultsFile 'y' -Processes @() } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\MariaDb.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsMariaDbLayout` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.MariaDb.ps1`**

```powershell
# MariaDB (XAMPP mysqld) identity and data-directory hardening (owner decision D15/D16).

function Get-LoamsMariaDbServices {
    [CmdletBinding()] param()
    foreach ($s in @(Get-CimInstance -ClassName Win32_Service -ErrorAction Stop)) {
        if ([string]$s.PathName -notmatch '(?i)\\mysqld\.exe') { continue }
        [pscustomobject]@{
            Name = [string]$s.Name; DisplayName = [string]$s.DisplayName; StartName = [string]$s.StartName
            PathName = [string]$s.PathName; State = [string]$s.State; StartMode = [string]$s.StartMode; ProcessId = [int]$s.ProcessId
        }
    }
}

function Get-LoamsMariaDbProcesses {
    [CmdletBinding()] param()
    foreach ($p in @(Get-CimInstance -ClassName Win32_Process -Filter "Name='mysqld.exe'" -ErrorAction Stop)) {
        [pscustomobject]@{ ProcessId = [int]$p.ProcessId; ParentProcessId = [int]$p.ParentProcessId; ExecutablePath = $p.ExecutablePath }
    }
}

function Get-LoamsMyIniPath {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $XamppRoot, [AllowEmptyCollection()][object[]] $Services = @())
    $mysqld = Join-Path $XamppRoot 'mysql\bin\mysqld.exe'
    foreach ($s in $Services) {
        if ([string]$s.PathName -like "*$mysqld*" -and [string]$s.PathName -match '(?i)--defaults-file=("([^"]+)"|(\S+))') {
            $v = $Matches[2]; if (-not $v) { $v = $Matches[3] }
            return ($v -replace '/', '\')
        }
    }
    return (Join-Path $XamppRoot 'mysql\bin\my.ini')
}

function ConvertTo-LoamsWindowsPath {
    param([string] $Value, [string] $RelativeTo)
    $v = $Value.Trim().Trim('"').Trim("'") -replace '/', '\'
    if (-not [IO.Path]::IsPathRooted($v)) { $v = Join-Path $RelativeTo $v }
    return [IO.Path]::GetFullPath($v).TrimEnd('\')
}

function Get-LoamsMariaDbLayout {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $XamppRoot, [string] $MyIniPath = '')
    $tree = (Join-Path $XamppRoot 'mysql').TrimEnd('\')
    if (-not $MyIniPath) { $MyIniPath = Join-Path $tree 'bin\my.ini' }
    $layout = [ordered]@{
        Root = $XamppRoot; IsXampp = $false
        MysqldExe = Join-Path $tree 'bin\mysqld.exe'; MysqlExe = Join-Path $tree 'bin\mysql.exe'
        MysqladminExe = Join-Path $tree 'bin\mysqladmin.exe'; MysqldumpExe = Join-Path $tree 'bin\mysqldump.exe'
        MyIni = $MyIniPath; DataDir = (Join-Path $tree 'data'); TmpDir = ''; Port = 3306; LogFiles = @(); LogDirs = @(); Problems = @()
    }
    foreach ($req in @($layout.MysqldExe, $layout.MysqlExe, $layout.MysqladminExe, $layout.MysqldumpExe, $layout.MyIni)) {
        if (-not (Test-Path -LiteralPath $req)) { $layout.Problems += "missing $req" }
    }
    if ($layout.Problems.Count -gt 0) { return [pscustomobject]$layout }

    $section = ''; $values = @{}
    foreach ($raw in (Get-Content -LiteralPath $layout.MyIni)) {
        $line = $raw.Trim()
        if ($line -eq '' -or $line.StartsWith('#') -or $line.StartsWith(';')) { continue }
        if ($line -match '^\[(.+)\]$') { $section = $Matches[1].ToLowerInvariant(); continue }
        if ($section -ne 'mysqld' -or $line -notmatch '^([^=]+)=(.*)$') { continue }
        $values[($Matches[1].Trim().ToLowerInvariant() -replace '-', '_')] = $Matches[2].Trim()
    }
    if ($values.ContainsKey('datadir')) { $layout.DataDir = ConvertTo-LoamsWindowsPath -Value $values['datadir'] -RelativeTo $tree }
    if ($values.ContainsKey('tmpdir')) { $layout.TmpDir = ConvertTo-LoamsWindowsPath -Value $values['tmpdir'] -RelativeTo $tree }
    if ($values.ContainsKey('port') -and $values['port'].Trim('"') -match '^\d+$') { $layout.Port = [int]$values['port'].Trim('"') }
    foreach ($k in @('log_error', 'general_log_file', 'slow_query_log_file')) {
        if ($values.ContainsKey($k) -and $values[$k].Trim('"') -ne '') { $layout.LogFiles += (ConvertTo-LoamsWindowsPath -Value $values[$k] -RelativeTo $layout.DataDir) }
    }
    $treePrefix = $tree + '\'
    if (-not $layout.DataDir.StartsWith($treePrefix, [StringComparison]::OrdinalIgnoreCase)) { $layout.Problems += "datadir '$($layout.DataDir)' is outside $tree" }
    foreach ($f in $layout.LogFiles) {
        $dir = Split-Path -Parent $f
        if (-not $dir.StartsWith($treePrefix, [StringComparison]::OrdinalIgnoreCase) -and $dir -ne $tree) { $layout.Problems += "log file '$f' is outside $tree" }
        elseif ($dir -ne $layout.DataDir -and $layout.LogDirs -notcontains $dir) { $layout.LogDirs += $dir }
    }
    if (-not (Test-Path -LiteralPath $layout.DataDir)) { $layout.Problems += "datadir '$($layout.DataDir)' does not exist" }
    $layout.IsXampp = ($layout.Problems.Count -eq 0)
    return [pscustomobject]$layout
}

function Get-LoamsMariaDbClassification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $Layout,
        [AllowEmptyCollection()][object[]] $Services = @(),
        [AllowEmptyCollection()][object[]] $Processes = @(),
        [string] $ExpectedServiceName = 'mysql'
    )
    if (-not $Layout.IsXampp) {
        $situation = 'NoXamppMariaDb'
        if (@($Layout.Problems | Where-Object { $_ -match 'outside' }).Count -gt 0) { $situation = 'MariaDbLayoutOutsideTree' }
        return New-LoamsClassification -Situation $situation -Stop $true -ServiceName $ExpectedServiceName -CurrentAccount '' -PhpSapi '' -Reasons @($Layout.Problems)
    }
    $s = Get-LoamsServiceSituation -Services $Services -Processes $Processes -BinaryPath $Layout.MysqldExe -ExpectedServiceName $ExpectedServiceName
    return New-LoamsClassification -Situation $s.Situation -Stop $s.Stop -ServiceName $s.ServiceName -CurrentAccount $s.CurrentAccount -PhpSapi '' -Reasons $s.Reasons
}

function Get-LoamsMariaDbAclProfile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)] $Layout, [string] $ServiceName = 'mysql')
    $db = "NT SERVICE\$ServiceName"
    $dbSid = Get-LoamsServiceSid -ServiceName $ServiceName
    $protected = @('*S-1-5-32-544:(OI)(CI)F', '*S-1-5-18:(OI)(CI)F', "${db}:(OI)(CI)M")
    $allowed = @('S-1-5-32-544', 'S-1-5-18', $dbSid)
    $entries = @(
        New-LoamsAclEntry -Id 'mariadb-tree' -Path (Join-Path $XamppRoot 'mysql') -Kind Directory -Access ReadExecute -Mode Grant -Grants @("${db}:(OI)(CI)RX") -Purpose 'MariaDB binaries, share, plugins, my.ini: read'
        New-LoamsAclEntry -Id 'mariadb-data' -Path $Layout.DataDir -Kind Directory -Access Modify -Mode Protected -Grants $protected -AllowedSids $allowed -Purpose 'MariaDB data, InnoDB/Aria logs, error log: MariaDB account + Administrators + SYSTEM only'
    )
    $i = 0
    foreach ($dir in @($Layout.LogDirs)) {
        $i++
        $entries += New-LoamsAclEntry -Id ("mariadb-logdir-{0}" -f $i) -Path $dir -Kind Directory -Access Modify -Mode Protected -Grants $protected -AllowedSids $allowed -Purpose 'MariaDB log directory outside datadir'
    }
    if ($Layout.TmpDir -and -not $Layout.TmpDir.StartsWith($Layout.DataDir, [StringComparison]::OrdinalIgnoreCase)) {
        $entries += New-LoamsAclEntry -Id 'mariadb-tmp' -Path $Layout.TmpDir -Kind Directory -Access Modify -Mode Grant -Grants @("${db}:(OI)(CI)M") -Purpose 'MariaDB tmpdir (shared; not protected)'
    }
    return , $entries
}

function Register-LoamsMariaDbService {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $MysqldExe, [Parameter(Mandatory)][string] $ServiceName, [Parameter(Mandatory)][string] $MyIni)
    Assert-LoamsServiceName -ServiceName $ServiceName
    Assert-LoamsMutationAllowed -Action "register MariaDB service $ServiceName"
    Invoke-LoamsExternal -FilePath $MysqldExe -ArgumentList @('--install', $ServiceName, "--defaults-file=$MyIni") | Out-Null
    Invoke-LoamsExternal -FilePath $script:LoamsSc -ArgumentList @('config', $ServiceName, 'start=', 'auto') | Out-Null
    Write-LoamsLog -Message "Registered MariaDB service $ServiceName"
}

function Unregister-LoamsMariaDbService {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $MysqldExe, [Parameter(Mandatory)][string] $ServiceName)
    Assert-LoamsServiceName -ServiceName $ServiceName
    Assert-LoamsMutationAllowed -Action "unregister MariaDB service $ServiceName"
    Invoke-LoamsExternal -FilePath $MysqldExe -ArgumentList @('--remove', $ServiceName) | Out-Null
    Write-LoamsLog -Message "Unregistered MariaDB service $ServiceName"
}

function Test-LoamsProcessAlive {
    [CmdletBinding()]
    param([Parameter(Mandatory)][int] $ProcessId)
    return [bool](Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)
}

function Test-LoamsTcpPortListening {
    [CmdletBinding()]
    param([Parameter(Mandatory)][int] $Port)
    return [bool](Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)
}

function Test-LoamsFileReleased {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $true }
    try { $fs = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None); $fs.Dispose(); return $true }
    catch { return $false }
}

function Stop-LoamsControlPanelMysqld {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MysqladminExe, [Parameter(Mandatory)][string] $DefaultsFile,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Processes, [int] $Port = 3306, [string] $DataDir = '', [int] $TimeoutSec = 120
    )
    Assert-LoamsMutationAllowed -Action 'stop Control-Panel mysqld'
    Invoke-LoamsExternal -FilePath $MysqladminExe -ArgumentList @("--defaults-extra-file=$DefaultsFile", 'shutdown') | Out-Null
    $ids = @($Processes | ForEach-Object { $_.ProcessId })
    $alive = @(); $listening = $false; $locked = $false
    for ($i = 0; $i -lt $TimeoutSec; $i++) {
        $alive = @($ids | Where-Object { Test-LoamsProcessAlive -ProcessId $_ })
        $listening = Test-LoamsTcpPortListening -Port $Port
        $locked = ($DataDir -ne '' -and -not (Test-LoamsFileReleased -Path (Join-Path $DataDir 'ibdata1')))
        if ($alive.Count -eq 0 -and -not $listening -and -not $locked) {
            Write-LoamsLog -Message "Control-Panel mysqld stopped (PIDs $($ids -join ','), port $Port free, data files released)"
            return
        }
        Start-Sleep -Seconds 1
    }
    throw ("mysqld did not stop within {0} s: running PIDs [{1}], port {2} listening: {3}, data files locked: {4}" -f $TimeoutSec, ($alive -join ','), $Port, $listening, $locked)
}

function Start-LoamsControlPanelMysqld {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $MysqldExe, [Parameter(Mandatory)][string] $MyIni)
    Assert-LoamsMutationAllowed -Action 'start Control-Panel mysqld'
    Start-Process -FilePath $MysqldExe -ArgumentList @("--defaults-file=`"$MyIni`"", '--standalone') -WorkingDirectory (Split-Path -Parent $MysqldExe) -WindowStyle Hidden | Out-Null
    Write-LoamsLog -Message 'Restarted mysqld detached (Control-Panel style)'
}
```

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 17 tests. Set `expected-test-count.txt` to `147` and run the suite runner → `PASSED: 147 tests` (earlier suites still pass with the extended fake tree).

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): detect and harden the XAMPP MariaDB service identity and data ACLs`

---

### Task 11: `-Report` (read-only, Apache + MariaDB, fingerprint) and the thin entry script

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.HostState.ps1`
- Create: `deploy/server/LoamsHost/LoamsHost.Report.ps1`
- Create: `deploy/server/Test-LoamsServerHost.ps1` (Report parameter set only; Task 17 adds `-Converge`)
- Create: `deploy/tests/Report.Tests.ps1`
- Modify: `deploy/tests/TestHelpers.ps1` (append `Get-LoamsTreeFingerprint`, `Set-LoamsReportMocks`)
- Modify: `deploy/tests/expected-test-count.txt` → `165`

**Interfaces:**
- Consumes: Tasks 2–10.
- Produces:
  - `Get-LoamsFingerprintAclPaths -AclProfile <object[]> -ProgramDataRoot <string>` → `[string[]]` XAMPP-side ACL paths (LOAMS-owned `ProgramData` paths are excluded: the tool creates them itself, so they would always "drift").
  - `LoamsHost.HostState.ps1` (read-only): `Get-LoamsScheduledTaskXml -TaskName` → task XML or `$null`; `Get-LoamsTaskDefinitionHash -Xml` → SHA-256 of the definition without `RegistrationInfo`, or `'absent'`; `Get-LoamsLayoutState -ProgramDataRoot` → `{ path; exists }` for `server`, `server\logs`, `server\logs\compat-guard.log`, `server\tls`, `server\tls\private`, `tools`; `Get-LoamsToolsState -ProgramDataRoot` → the whole tree below `tools\`: `{ path (relative); type='dir'|'file'; sha256 (files) }`; `Get-LoamsVerifySchemaFolders -DataDir` → names of `loams_s1b_verify*` schema folders in the MariaDB datadir (read-only folder listing, no credentials); `Get-LoamsLoamsState -ProgramDataRoot [-MariaDbDataDir]` → hashtable `eventSource`, `retentionTask`, `layout:<path>` (SDDL or `absent`), `tool:<path>` (hash or `dir`), `verifySchemas` (names or `none`). These make every LOAMS-owned mutation (Task 14 inventory) part of the fingerprint.
  - `Get-LoamsLiveFingerprint -XamppRoot <string> -ProgramDataRoot <string> -ApacheServiceName <string> -MariaDbServiceName <string> -MariaDbLayout <pscustomobject> -AclPaths <string[]>` → fingerprint (Task 9 shape, incl. `loamsState`) read **live** from the host: both service configurations, hashes of `httpd.conf`, `extra\httpd-xampp.conf`, `extra\httpd-ssl.conf`, `php.ini`, `my.ini` (never `config.php`: hashing a small file that contains the DB password would allow offline guessing), SDDL of every ACL path, and the loaded `httpd` modules. Used by the report **and** by the pre-change drift re-check (Task 16/17), so both are computed by one code path.
  - `New-LoamsHostReport -XamppRoot -ManifestPath -ProgramDataRoot [-ServiceName 'Apache2.4'] [-MariaDbServiceName 'mysql']` → `[pscustomobject]` with `reportVersion` (2), `generatedUtc`, `computerName`, `elevated`, `manifest`, Apache fields (`layout`, `services`, `processes`, `httpd`, `phpSapi`, `phpCli`, `classification`, `runtime`, `inventoryComplete`, `loadedModules`, `acl`), `mariaDb` {`layout`, `services`, `processes`, `classification`, `acl`}, `eventSource`, `fingerprint`, `profileHash` (= `Get-LoamsFingerprintHash fingerprint`), `convergenceNeeded`, `outcome` (`Success`|`StopAndReport`|`Incomplete`), `outcomeReasons`.
  - `Format-LoamsHostReport -Report` → `[string[]]`; `Get-LoamsReportFolder -ProgramDataRoot` → `<ProgramDataRoot>\reports`; `Test-LoamsReportLocation -Path -ProgramDataRoot` → `{ Ok; Problems }` (file directly inside the reports folder, folder exists, not a junction, ACL passes `Test-LoamsProtectedFolderAcl`); `Initialize-LoamsReportFolder -ProgramDataRoot` (mutating, runbook B0/F only: creates the folder if needed, applies a **fresh protected** security descriptor with only Administrators + SYSTEM FullControl via `Set-LoamsDirectorySecurity`, then re-reads and **stops** on any allowlist mismatch); `Save-LoamsHostReport -Report -Path [-ProgramDataRoot]` (UTF-8 without BOM; writing into the LOAMS reports folder additionally requires `Test-LoamsReportLocation`; **never creates directories** — a missing folder throws `"... -Report never creates directories ..."`, so `C:\ProgramData\LOAMS\reports` is provisioned in runbook B0); `Invoke-LoamsHostReport -XamppRoot -ManifestPath -ProgramDataRoot -ServiceName -MariaDbServiceName -ReportPath` → report (forces Report mode).
  - Entry: `Test-LoamsServerHost.ps1 [-Report] [-XamppRoot] [-ManifestPath] [-ServiceName] [-MariaDbServiceName] [-ProgramDataRoot] [-ReportPath]`.

**Outcome rules:** manifest invalid, Apache stop, or **MariaDB stop** ⇒ `StopAndReport` (exit 2); not elevated, `httpd` not running, or module list unreadable ⇒ `Incomplete` (exit 3); runtime `Mismatch` or a draft manifest (`Unknown`) ⇒ `StopAndReport` ("stack does not match an approved manifest"); otherwise `Success` (exit 0). On this dev box today the expected result is `StopAndReport` elevated (draft manifest; OpenSSL 1.1.1t loaded) or `Incomplete` non-elevated.

- [ ] **Step 1: Append test helpers to `deploy/tests/TestHelpers.ps1`**

```powershell
function Get-LoamsTreeFingerprint {
    param([Parameter(Mandatory)][string] $Root)
    if (-not (Test-Path $Root)) { return '<absent>' }
    $items = Get-ChildItem -LiteralPath $Root -Recurse -Force | Sort-Object FullName | ForEach-Object {
        if ($_.PSIsContainer) { "D|$($_.FullName)|$((Get-Acl -LiteralPath $_.FullName).Sddl)" }
        else { "F|$($_.FullName)|$((Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash)|$((Get-Acl -LiteralPath $_.FullName).Sddl)" }
    }
    return ($items -join "`n")
}

# Mock bodies run in the module scope; fixture data is passed through globals named $global:LoamsT*.
function Set-LoamsReportMocks {
    param([Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string[]] $ModulePaths,
          [string] $Account = 'NT SERVICE\Apache2.4', [string] $DbAccount = 'NT SERVICE\mysql')
    $global:LoamsTHttpd = Join-Path $XamppRoot 'apache\bin\httpd.exe'
    $global:LoamsTMysqld = Join-Path $XamppRoot 'mysql\bin\mysqld.exe'
    $global:LoamsTMyIni = Join-Path $XamppRoot 'mysql\bin\my.ini'
    $global:LoamsTAccount = $Account
    $global:LoamsTDbAccount = $DbAccount
    $global:LoamsTModules = @($ModulePaths | ForEach-Object { [pscustomobject]@{ FileName = $_ } })
    Mock -ModuleName LoamsHost Test-LoamsElevated { $true }
    Mock -ModuleName LoamsHost Get-CimInstance {
        $all = @(
            [pscustomobject]@{ Name = 'Apache2.4'; DisplayName = 'Apache2.4'; StartName = $global:LoamsTAccount; PathName = "`"$global:LoamsTHttpd`" -k runservice"; State = 'Running'; StartMode = 'Auto'; ProcessId = 10 },
            [pscustomobject]@{ Name = 'mysql'; DisplayName = 'mysql'; StartName = $global:LoamsTDbAccount; PathName = "$global:LoamsTMysqld --defaults-file=$global:LoamsTMyIni mysql"; State = 'Running'; StartMode = 'Auto'; ProcessId = 20 })
        if ($Filter -match "Name='([^']+)'") { $all | Where-Object Name -eq $Matches[1] } else { $all }
    } -ParameterFilter { $ClassName -eq 'Win32_Service' }
    Mock -ModuleName LoamsHost Get-CimInstance {
        if ($Filter -eq "Name='mysqld.exe'") { return @([pscustomobject]@{ ProcessId = 20; ParentProcessId = 4; ExecutablePath = $global:LoamsTMysqld }) }
        @([pscustomobject]@{ ProcessId = 10; ParentProcessId = 4; ExecutablePath = $global:LoamsTHttpd },
          [pscustomobject]@{ ProcessId = 11; ParentProcessId = 10; ExecutablePath = $global:LoamsTHttpd })
    } -ParameterFilter { $ClassName -eq 'Win32_Process' }
    Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = 'Server version: Apache/2.4.99 (Win64)'; StdErr = '' } } -ParameterFilter { $FilePath -like '*httpd.exe' -and $ArgumentList -contains '-v' }
    Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "Loaded Modules:`r`n core_module (static)`r`n ssl_module (shared)`r`n php_module (shared)"; StdErr = '' } } -ParameterFilter { $FilePath -like '*httpd.exe' -and $ArgumentList -contains '-M' }
    Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "PHP 8.3.99 (cli)`r`n[PHP Modules]`r`nopenssl`r`n`r`n[Zend Modules]`r`nOpenSSL Library Version => OpenSSL 3.0.99"; StdErr = '' } } -ParameterFilter { $FilePath -like '*php.exe' }
    Mock -ModuleName LoamsHost Get-Process { [pscustomobject]@{ Modules = $global:LoamsTModules } }
    Mock -ModuleName LoamsHost Get-LoamsBridgeTaskAccount { '' }
    Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)' }
    Mock -ModuleName LoamsHost Get-LoamsEventLogNames { @('Application', 'System') }
    Mock -ModuleName LoamsHost Test-LoamsRegistryKey { $false }
    Mock -ModuleName LoamsHost Get-LoamsRegistryValue { $null }
    Mock -ModuleName LoamsHost Get-LoamsScheduledTaskXml { $null }
}
```

- [ ] **Step 2: Write the failing tests `deploy/tests/Report.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Pd = Join-Path $TestDrive 'pd\LOAMS'
    $script:ManifestPath = Join-Path $TestDrive 'manifest.json'
    Save-LoamsTestManifest -Manifest (New-LoamsTestManifest -XamppRoot $script:Root) -Path $script:ManifestPath
    $m = Get-Content -Raw $script:ManifestPath | ConvertFrom-Json
    $script:Mods = @()
    foreach ($c in $m.server.components) { foreach ($mod in $c.modules) { $script:Mods += (Join-Path $script:Root ($mod.relativePath -replace '/', '\')) } }
    $script:Entry = Join-Path $PSScriptRoot '..\server\Test-LoamsServerHost.ps1'
    function Run-Report { param([string] $Manifest = $script:ManifestPath)
        New-LoamsHostReport -XamppRoot $script:Root -ManifestPath $Manifest -ProgramDataRoot $script:Pd
    }
}
AfterAll { Remove-Variable -Name LoamsTHttpd, LoamsTMysqld, LoamsTMyIni, LoamsTAccount, LoamsTDbAccount, LoamsTModules -Scope Global -ErrorAction SilentlyContinue }

Describe 'New-LoamsHostReport outcomes' {
    It 'is Success for a converged host whose loaded modules match the approved manifest' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $r = Run-Report
        $r.classification.Situation | Should -Be 'Converged'
        $r.mariaDb.classification.Situation | Should -Be 'Converged'
        $r.runtime.Status | Should -Be 'Match'
        $r.outcome | Should -Be 'Success'
    }
    It 'is Incomplete when not elevated' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        Mock -ModuleName LoamsHost Test-LoamsElevated { $false }
        Mock -ModuleName LoamsHost Get-Process { throw 'Access is denied' }
        $r = Run-Report
        $r.outcome | Should -Be 'Incomplete'
        $r.runtime.Status | Should -Be 'Unknown'
    }
    It 'is StopAndReport when mod_ssl is missing' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = "Loaded Modules:`r`n php_module (shared)"; StdErr = '' } } -ParameterFilter { $FilePath -like '*httpd.exe' -and $ArgumentList -contains '-M' }
        $r = Run-Report
        $r.classification.Situation | Should -Be 'MissingModSsl'
        $r.outcome | Should -Be 'StopAndReport'
    }
    It 'is StopAndReport when the manifest is invalid' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $bad = Join-Path $TestDrive 'bad.json'; Set-Content -Path $bad -Value '{ nope'
        $r = Run-Report -Manifest $bad
        $r.manifest.valid | Should -BeFalse
        $r.outcome | Should -Be 'StopAndReport'
    }
    It 'is StopAndReport when an unlisted OpenSSL is loaded (mixed stack)' {
        $legacy = Join-Path $script:Root 'apache\bin\libssl-1_1-x64.dll'; Set-Content -Path $legacy -Value 'old'
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths ($script:Mods + $legacy)
        $r = Run-Report
        $r.runtime.Status | Should -Be 'Mismatch'
        $r.outcome | Should -Be 'StopAndReport'
        Remove-Item $legacy
    }
    It 'is StopAndReport when the MariaDB layout cannot be handled' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        Mock -ModuleName LoamsHost Get-LoamsMariaDbLayout { [pscustomobject]@{ IsXampp = $false; MysqldExe = 'x'; MyIni = 'y'; DataDir = 'z'; TmpDir = ''; LogDirs = @(); LogFiles = @(); Problems = @('missing mysqld.exe') } }
        $r = Run-Report
        $r.mariaDb.classification.Situation | Should -Be 'NoXamppMariaDb'
        $r.outcome | Should -Be 'StopAndReport'
    }
    It 'reports a MariaDB service that still needs convergence' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods -DbAccount 'LocalSystem'
        $r = Run-Report
        $r.mariaDb.classification.Situation | Should -Be 'ServiceNonVirtualAccount'
        $r.convergenceNeeded | Should -BeTrue
    }
}

Describe 'Invoke-LoamsHostReport output' {
    It 'prints a readable report and saves a JSON report whose profile hash is the fingerprint hash' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $out = Join-Path $TestDrive 'report.json'
        $all = Invoke-LoamsHostReport -XamppRoot $script:Root -ManifestPath $script:ManifestPath -ProgramDataRoot $script:Pd -ServiceName 'Apache2.4' -MariaDbServiceName 'mysql' -ReportPath $out 6>&1
        $text = ($all | Where-Object { $_ -is [System.Management.Automation.InformationRecord] } | ForEach-Object { $_.MessageData.ToString() }) -join "`n"
        $text | Should -Match 'Situation: Converged'
        $text | Should -Match 'MariaDB: Converged'
        $text | Should -Match 'never openssl.exe'
        $json = Get-Content -Raw $out | ConvertFrom-Json
        $json.computerName | Should -Be $env:COMPUTERNAME
        $json.profileHash | Should -Be (Get-LoamsFingerprintHash -Fingerprint $json.fingerprint)
        $json.outcome | Should -Be 'Success'
    }
}

Describe '-Report is read-only' {
    It 'changes nothing on the host and calls only read-only externals' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $mutating = @('New-EventLog', 'Remove-EventLog', 'Stop-Service', 'Start-Service', 'Stop-Process', 'Start-Process', 'Set-Acl', 'New-Item', 'Remove-Item', 'Copy-Item', 'Register-ScheduledTask')
        foreach ($c in $mutating) { Mock -ModuleName LoamsHost $c { throw 'mutating call during -Report' } }
        $before = (Get-LoamsTreeFingerprint -Root $script:Root) + (Get-LoamsTreeFingerprint -Root $script:Pd)
        $out = Join-Path $TestDrive 'ro-report.json'
        Invoke-LoamsHostReport -XamppRoot $script:Root -ManifestPath $script:ManifestPath -ProgramDataRoot $script:Pd -ServiceName 'Apache2.4' -MariaDbServiceName 'mysql' -ReportPath $out 6>$null | Out-Null
        $after = (Get-LoamsTreeFingerprint -Root $script:Root) + (Get-LoamsTreeFingerprint -Root $script:Pd)
        $after | Should -Be $before
        foreach ($c in $mutating) { Should -Invoke -ModuleName LoamsHost $c -Times 0 }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0 -ParameterFilter {
            -not (($FilePath -like '*httpd.exe' -and ($ArgumentList -join ' ') -in @('-v', '-M')) -or ($FilePath -like '*php.exe' -and ($ArgumentList -join ' ') -in @('-v', '-m', '-i')))
        }
        Get-LoamsMode | Should -Be 'Report'
    }
}

Describe 'Report output location' {
    It 'provisions the reports folder with a fresh protected allowlist ACL' {
        Set-LoamsMode -Mode Converge
        try {
            Mock -ModuleName LoamsHost Set-LoamsDirectorySecurity { $global:LoamsTSec = $Security }
            Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)' }
            $pd = Join-Path $TestDrive 'pd-reports'
            $folder = Initialize-LoamsReportFolder -ProgramDataRoot $pd
            Test-Path $folder | Should -BeTrue
            $global:LoamsTSec.AreAccessRulesProtected | Should -BeTrue
            (@($global:LoamsTSec.GetAccessRules($true, $true, [System.Security.Principal.SecurityIdentifier]) | ForEach-Object { $_.IdentityReference.Value } | Sort-Object) -join ',') | Should -Be 'S-1-5-18,S-1-5-32-544'
        } finally { Set-LoamsMode -Mode Report; Remove-Variable -Name LoamsTSec -Scope Global -ErrorAction SilentlyContinue }
    }
    It 'stops when the folder does not match the allowlist after provisioning' {
        Set-LoamsMode -Mode Converge
        try {
            Mock -ModuleName LoamsHost Set-LoamsDirectorySecurity { }
            Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x1200a9;;;BU)' }
            { Initialize-LoamsReportFolder -ProgramDataRoot (Join-Path $TestDrive 'pd-reports2') } | Should -Throw -ExpectedMessage 'STOP:*unexpected principal S-1-5-32-545*'
        } finally { Set-LoamsMode -Mode Report }
    }
    It 'refuses to write into a reports folder whose ACL is not the allowlist' {
        $pd = Join-Path $TestDrive 'pd-reports3'
        New-Item -ItemType Directory -Force -Path (Join-Path $pd 'reports') | Out-Null
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:AI(A;OICIID;FA;;;BA)(A;OICIID;FA;;;SY)(A;OICIID;0x1301bf;;;AU)' }
        { Save-LoamsHostReport -Report ([pscustomobject]@{ outcome = 'x' }) -Path (Join-Path $pd 'reports\r.json') -ProgramDataRoot $pd } | Should -Throw -ExpectedMessage '*Refusing to write into the LOAMS reports folder*'
        Test-Path (Join-Path $pd 'reports\r.json') | Should -BeFalse
    }
    It 'Test-LoamsReportLocation refuses a path outside the reports folder' {
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)' }
        $pd = Join-Path $TestDrive 'pd-reports4'
        New-Item -ItemType Directory -Force -Path (Join-Path $pd 'reports') | Out-Null
        $r = Test-LoamsReportLocation -Path (Join-Path $TestDrive 'elsewhere\r.json') -ProgramDataRoot $pd
        $r.Ok | Should -BeFalse
        ($r.Problems -join ' ') | Should -Match 'must be a file directly inside'
        (Test-LoamsReportLocation -Path (Join-Path $pd 'reports\r.json') -ProgramDataRoot $pd).Ok | Should -BeTrue
    }
    It 'fails closed with a clear message instead of creating a missing report directory' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $missing = Join-Path $TestDrive 'no-such-reports\report.json'
        { Invoke-LoamsHostReport -XamppRoot $script:Root -ManifestPath $script:ManifestPath -ProgramDataRoot $script:Pd -ServiceName 'Apache2.4' -MariaDbServiceName 'mysql' -ReportPath $missing 6>$null } |
            Should -Throw -ExpectedMessage '*-Report never creates directories*'
        Test-Path (Join-Path $TestDrive 'no-such-reports') | Should -BeFalse
    }
}

Describe 'Host state helpers' {
    It 'hashes a task definition independently of its registration info, and reports absence' {
        $a = '<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"><RegistrationInfo><Date>2026-01-01</Date></RegistrationInfo><Actions><Exec><Command>x.exe</Command></Exec></Actions></Task>'
        $b = '<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"><RegistrationInfo><Date>2026-09-09</Date></RegistrationInfo><Actions><Exec><Command>x.exe</Command></Exec></Actions></Task>'
        Get-LoamsTaskDefinitionHash -Xml $a | Should -Be (Get-LoamsTaskDefinitionHash -Xml $b)
        Get-LoamsTaskDefinitionHash -Xml $null | Should -Be 'absent'
    }
    It 'captures event source, retention task, layout and tools in the LOAMS state' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $pd = Join-Path $TestDrive 'pd-state'
        New-Item -ItemType Directory -Force -Path (Join-Path $pd 'tools') | Out-Null
        Set-Content -Path (Join-Path $pd 'tools\Invoke-LoamsBackupRetention.ps1') -Value 'x'
        $st = Get-LoamsLoamsState -ProgramDataRoot $pd
        $st['eventSource'] | Should -Be 'absent'
        $st['retentionTask'] | Should -Be 'absent'
        $st["layout:$(Join-Path $pd 'server')"] | Should -Be 'absent'
        $st["layout:$(Join-Path $pd 'tools')"] | Should -Match '^O:'
        $st.ContainsKey('tool:Invoke-LoamsBackupRetention.ps1') | Should -BeTrue
        $st['verifySchemas'] | Should -Be 'none'
    }
    It 'reports leftover MariaDB verification schemas in the report and the fingerprint' {
        Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:Mods
        $left = Join-Path $script:Root 'mysql\data\loams_s1b_verify_20261006t000000z'
        New-Item -ItemType Directory -Path $left | Out-Null
        try {
            $r = Run-Report
            (@($r.mariaDb.leftoverVerifySchemas) -join ',') | Should -Be 'loams_s1b_verify_20261006t000000z'
            @($r.fingerprint.loamsState) | Should -Contain 'verifySchemas=loams_s1b_verify_20261006t000000z'
        } finally { Remove-Item -LiteralPath $left -Recurse -Force }
    }
}

Describe 'Test-LoamsServerHost.ps1 entry' {
    It 'defaults to the Report parameter set' {
        (Get-Command $script:Entry).DefaultParameterSet | Should -Be 'Report'
    }
}
```

- [ ] **Step 3: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Report.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `New-LoamsHostReport` not recognised; entry script not found.

- [ ] **Step 4: Implement `deploy/server/LoamsHost/LoamsHost.HostState.ps1` and `deploy/server/LoamsHost/LoamsHost.Report.ps1`**

```powershell
# Read-only view of LOAMS-owned host state (event source, retention task, deployed tools, ProgramData layout).
# Part of the host fingerprint so drift and restoration are verifiable (mutation inventory, Task 14).

$script:LoamsRetentionTaskName = 'LOAMS Backup Retention'
$script:LoamsLayoutRelPaths = @('server', 'server\logs', 'server\logs\compat-guard.log', 'server\tls', 'server\tls\private', 'tools')

function Get-LoamsScheduledTaskXml {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $TaskName)
    $task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -eq $task) { return $null }
    return [string](Export-ScheduledTask -TaskName $TaskName -ErrorAction Stop)
}

function Get-LoamsTaskDefinitionHash {
    [CmdletBinding()]
    param([AllowNull()][AllowEmptyString()][string] $Xml)
    if ([string]::IsNullOrEmpty($Xml)) { return 'absent' }
    $doc = New-Object System.Xml.XmlDocument
    $doc.LoadXml($Xml)
    $ri = $doc.DocumentElement.SelectSingleNode("*[local-name()='RegistrationInfo']")
    if ($null -ne $ri) { [void]$doc.DocumentElement.RemoveChild($ri) }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $h = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($doc.DocumentElement.OuterXml)) } finally { $sha.Dispose() }
    return (($h | ForEach-Object { $_.ToString('x2') }) -join '')
}

function Get-LoamsLayoutState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot)
    foreach ($rel in $script:LoamsLayoutRelPaths) {
        $p = Join-Path $ProgramDataRoot $rel
        [pscustomobject]@{ path = $p; exists = (Test-Path -LiteralPath $p) }
    }
}

function Get-LoamsToolsState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot)
    # The whole tree below tools\: directories (type 'dir') and files (type 'file' + SHA-256).
    $tools = Join-Path $ProgramDataRoot 'tools'
    if (-not (Test-Path -LiteralPath $tools)) { return }
    $base = (Resolve-Path -LiteralPath $tools).ProviderPath.TrimEnd('\') + '\'
    foreach ($i in (Get-ChildItem -LiteralPath $tools -Recurse -Force | Sort-Object FullName)) {
        $rel = $i.FullName.Substring($base.Length)
        if ($i.PSIsContainer) { [pscustomobject]@{ path = $rel; type = 'dir'; sha256 = '' } }
        else { [pscustomobject]@{ path = $rel; type = 'file'; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $i.FullName).Hash.ToLowerInvariant() } }
    }
}

function Get-LoamsVerifySchemaFolders {
    [CmdletBinding()]
    param([AllowEmptyString()][string] $DataDir)
    # MariaDB keeps each schema as a folder in datadir; a read-only folder listing needs no DB credentials.
    if (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir)) { return }
    Get-ChildItem -LiteralPath $DataDir -Directory -Force -Filter 'loams_s1b_verify*' | Sort-Object Name | ForEach-Object { $_.Name }
}

function Get-LoamsLoamsState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot, [AllowEmptyString()][string] $MariaDbDataDir = '')
    $state = @{}
    $es = Get-LoamsEventSourceState
    $state['eventSource'] = $(if ($es.Registered) { "registered:$($es.LogName)" } else { 'absent' })
    $state['retentionTask'] = Get-LoamsTaskDefinitionHash -Xml (Get-LoamsScheduledTaskXml -TaskName $script:LoamsRetentionTaskName)
    foreach ($l in @(Get-LoamsLayoutState -ProgramDataRoot $ProgramDataRoot)) {
        $state["layout:$($l.path)"] = $(if ($l.exists) { Get-LoamsAclSddl -Path $l.path } else { 'absent' })
    }
    foreach ($t in @(Get-LoamsToolsState -ProgramDataRoot $ProgramDataRoot)) { $state["tool:$($t.path)"] = $(if ($t.type -eq 'dir') { 'dir' } else { $t.sha256 }) }
    $verify = @(Get-LoamsVerifySchemaFolders -DataDir $MariaDbDataDir)
    $state['verifySchemas'] = $(if ($verify.Count -gt 0) { $verify -join ',' } else { 'none' })
    return $state
}
```

```powershell
# -Report: read-only host report (human-readable + JSON) and the live host fingerprint.

function Get-LoamsFingerprintAclPaths {
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyCollection()][object[]] $AclProfile, [Parameter(Mandatory)][string] $ProgramDataRoot)
    $pd = $ProgramDataRoot.TrimEnd('\')
    return , @($AclProfile | Where-Object { -not ($_.Path -eq $pd -or $_.Path.StartsWith($pd + '\', [StringComparison]::OrdinalIgnoreCase)) } |
        ForEach-Object { $_.Path } | Select-Object -Unique)
}

function Get-LoamsLiveFingerprint {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string] $ProgramDataRoot, [Parameter(Mandatory)][string] $ApacheServiceName,
        [Parameter(Mandatory)][string] $MariaDbServiceName, [Parameter(Mandatory)] $MariaDbLayout,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]] $AclPaths
    )
    $apache = Get-LoamsServiceSnapshot -ServiceName $ApacheServiceName
    $db = Get-LoamsServiceSnapshot -ServiceName $MariaDbServiceName
    $inv = Get-LoamsLoadedModuleInventory -ProcessIds @(Get-LoamsHttpdProcesses | ForEach-Object { $_.ProcessId })
    $conf = @{}
    $files = @('apache\conf\httpd.conf', 'apache\conf\extra\httpd-xampp.conf', 'apache\conf\extra\httpd-ssl.conf', 'php\php.ini') | ForEach-Object { Join-Path $XamppRoot $_ }
    foreach ($f in (@($files) + @($MariaDbLayout.MyIni))) {
        if ($f -and (Test-Path -LiteralPath $f)) { $conf[$f] = (Get-FileHash -Algorithm SHA256 -LiteralPath $f).Hash.ToLowerInvariant() } else { $conf[[string]$f] = '<absent>' }
    }
    $acl = @{}
    foreach ($p in $AclPaths) {
        if (Test-Path -LiteralPath $p) { $acl[$p] = Get-LoamsAclSddl -Path $p } else { $acl[$p] = '<absent>' }
    }
    return New-LoamsHostFingerprint -ComputerName $env:COMPUTERNAME -ApacheService $apache -MariaDbService $db -ConfigHashes $conf -AclSddl $acl -Modules @($inv.Modules) `
        -LoamsState (Get-LoamsLoamsState -ProgramDataRoot $ProgramDataRoot -MariaDbDataDir $MariaDbLayout.DataDir)
}

function New-LoamsHostReport {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $XamppRoot,
        [Parameter(Mandatory)][string] $ManifestPath,
        [Parameter(Mandatory)][string] $ProgramDataRoot,
        [string] $ServiceName = 'Apache2.4',
        [string] $MariaDbServiceName = 'mysql'
    )
    $elevated = Test-LoamsElevated
    $manifest = $null
    $manifestInfo = [pscustomobject]@{ path = $ManifestPath; valid = $false; status = ''; manifestVersion = ''; errors = @() }
    try {
        $manifest = Read-LoamsStackManifest -Path $ManifestPath
        $manifestInfo.valid = $true
        $manifestInfo.status = [string]$manifest.status
        $manifestInfo.manifestVersion = [string]$manifest.manifestVersion
    } catch {
        $manifestInfo.errors = @($_.Exception.Message)
    }

    # Apache
    $layout = Get-LoamsXamppLayout -XamppRoot $XamppRoot
    $services = @(Get-LoamsHttpdServices)
    $processes = @(Get-LoamsHttpdProcesses)
    $phpCli = $null
    if ($layout.IsXampp) {
        $httpd = Get-LoamsHttpdInfo -HttpdExe $layout.HttpdExe
        $conf = (Get-Content -Raw -LiteralPath $layout.HttpdConf) + "`n" + (Get-Content -Raw -LiteralPath $layout.XamppConf)
        $sapi = Get-LoamsPhpSapi -HttpdInfo $httpd -ConfText $conf
        $phpCli = Get-LoamsPhpCliInfo -PhpExe $layout.PhpExe
    } else {
        $httpd = [pscustomobject]@{ Version = ''; VersionLine = ''; Modules = @(); HasModSsl = $false; HasPhpModule = $false; HasFcgid = $false; HasProxyFcgi = $false; ConfigOk = $false; ConfigError = 'layout is not XAMPP' }
        $sapi = 'unknown'
    }
    $class = Get-LoamsHostClassification -Layout $layout -Services $services -Processes $processes -HttpdInfo $httpd -PhpSapi $sapi -ExpectedServiceName $ServiceName
    $inventory = Get-LoamsLoadedModuleInventory -ProcessIds @($processes | ForEach-Object { $_.ProcessId })
    if ($manifest) {
        $runtime = Compare-LoamsRuntimeToManifest -Inventory $inventory -Manifest $manifest -XamppRoot $XamppRoot
    } else {
        $runtime = [pscustomobject]@{ Status = 'Unknown'; Checked = 0
            Findings = @([pscustomobject]@{ Kind = 'ManifestInvalid'; Path = $ManifestPath; Detail = ($manifestInfo.errors -join '; ') }) }
    }
    $serviceSid = Get-LoamsServiceSid -ServiceName $class.ServiceName
    $aclProfile = @()
    $acl = $null
    if ($layout.IsXampp) {
        $aclProfile = Get-LoamsAclProfile -XamppRoot $XamppRoot -ProgramDataRoot $ProgramDataRoot -ServiceName $class.ServiceName -BridgeAccount (Get-LoamsBridgeTaskAccount)
        $acl = Test-LoamsAclCompliance -AclProfile $aclProfile -ServiceSid $serviceSid
    }

    # MariaDB
    $dbServices = @(Get-LoamsMariaDbServices)
    $dbProcesses = @(Get-LoamsMariaDbProcesses)
    $dbLayout = Get-LoamsMariaDbLayout -XamppRoot $XamppRoot -MyIniPath (Get-LoamsMyIniPath -XamppRoot $XamppRoot -Services $dbServices)
    $dbClass = Get-LoamsMariaDbClassification -Layout $dbLayout -Services $dbServices -Processes $dbProcesses -ExpectedServiceName $MariaDbServiceName
    $dbProfile = @()
    $dbAcl = $null
    if ($dbLayout.IsXampp) {
        $dbProfile = Get-LoamsMariaDbAclProfile -XamppRoot $XamppRoot -Layout $dbLayout -ServiceName $dbClass.ServiceName
        $dbAcl = Test-LoamsAclCompliance -AclProfile $dbProfile -ServiceSid (Get-LoamsServiceSid -ServiceName $dbClass.ServiceName)
    }

    $eventSource = Get-LoamsEventSourceState -ServiceSid $serviceSid
    $fingerprint = Get-LoamsLiveFingerprint -XamppRoot $XamppRoot -ProgramDataRoot $ProgramDataRoot -ApacheServiceName $class.ServiceName -MariaDbServiceName $dbClass.ServiceName `
        -MariaDbLayout $dbLayout -AclPaths (Get-LoamsFingerprintAclPaths -AclProfile (@($aclProfile) + @($dbProfile)) -ProgramDataRoot $ProgramDataRoot)

    $reasons = @()
    if (-not $manifestInfo.valid) { $outcome = 'StopAndReport'; $reasons += 'manifest is invalid' }
    elseif ($class.StopAndReport) { $outcome = 'StopAndReport'; $reasons += $class.Reasons }
    elseif ($dbClass.StopAndReport) { $outcome = 'StopAndReport'; $reasons += @($dbClass.Reasons | ForEach-Object { "MariaDB: $_" }) }
    elseif (-not $elevated) { $outcome = 'Incomplete'; $reasons += 'not elevated: loaded modules of httpd cannot be read; re-run as Administrator' }
    elseif ($processes.Count -eq 0) { $outcome = 'Incomplete'; $reasons += 'httpd is not running: runtime modules cannot be verified' }
    elseif (-not $inventory.Complete) { $outcome = 'Incomplete'; $reasons += $inventory.Reason }
    elseif ($runtime.Status -ne 'Match') { $outcome = 'StopAndReport'; $reasons += "stack does not match an approved manifest (runtime $($runtime.Status))" }
    else { $outcome = 'Success' }

    $needsConvergence = ($class.Situation -ne 'Converged') -or ($dbClass.Situation -ne 'Converged') -or
        ($null -ne $acl -and -not $acl.Compliant) -or ($null -ne $dbAcl -and -not $dbAcl.Compliant) -or (-not $eventSource.Registered)
    return [pscustomobject]@{
        reportVersion = 2
        generatedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
        computerName = $env:COMPUTERNAME
        elevated = $elevated
        manifest = $manifestInfo
        layout = $layout
        services = $services
        processes = $processes
        httpd = $httpd
        phpSapi = $sapi
        phpCli = $phpCli
        classification = $class
        runtime = $runtime
        inventoryComplete = $inventory.Complete
        loadedModules = @($inventory.Modules)
        acl = $acl
        mariaDb = [pscustomobject]@{ layout = $dbLayout; services = $dbServices; processes = $dbProcesses; classification = $dbClass; acl = $dbAcl
            leftoverVerifySchemas = @(Get-LoamsVerifySchemaFolders -DataDir $dbLayout.DataDir) }
        eventSource = $eventSource
        fingerprint = $fingerprint
        profileHash = (Get-LoamsFingerprintHash -Fingerprint $fingerprint)
        convergenceNeeded = $needsConvergence
        outcome = $outcome
        outcomeReasons = @($reasons)
    }
}

function Format-LoamsHostReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Report)
    $yn = { param($b) if ($b) { 'yes' } else { 'no' } }
    $aclLine = { param($label, $a)
        if ($null -eq $a) { return @("${label}: not evaluated") }
        $out = @("${label}: $(if ($a.Compliant) { 'compliant' } else { "$(@($a.Findings).Count) finding(s)" })")
        foreach ($f in $a.Findings) { $out += "  - [$($f.EntryId)] $($f.Path): $($f.Problem)" }
        return $out }
    $lines = @()
    $lines += "LOAMS server host report - $($Report.computerName) - $($Report.generatedUtc)"
    $lines += "Outcome: $($Report.outcome)"
    foreach ($r in $Report.outcomeReasons) { $lines += "  - $r" }
    $lines += "Elevated: $(& $yn $Report.elevated)"
    $lines += "Manifest: $($Report.manifest.path) (valid: $(& $yn $Report.manifest.valid), status: $($Report.manifest.status), version: $($Report.manifest.manifestVersion))"
    $c = $Report.classification
    $lines += "Situation: $($c.Situation) (convergeable: $(& $yn $c.Convergeable))"
    foreach ($r in $c.Reasons) { $lines += "  - $r" }
    $lines += "Apache service: $($c.ServiceName); account now: '$($c.CurrentAccount)'; target: $($c.TargetAccount)"
    $lines += "Apache: $($Report.httpd.Version); mod_ssl: $(& $yn $Report.httpd.HasModSsl); PHP SAPI: $($Report.phpSapi)"
    if ($Report.phpCli) { $lines += "PHP CLI (informational, not runtime evidence): $($Report.phpCli.Version); CLI OpenSSL: $($Report.phpCli.CliOpenSsl)" }
    $lines += "Runtime modules of running httpd vs manifest (never openssl.exe): $($Report.runtime.Status) (checked $($Report.runtime.Checked))"
    foreach ($f in $Report.runtime.Findings) { $lines += "  - [$($f.Kind)] $($f.Path) $($f.Detail)" }
    $lines += (& $aclLine 'Apache ACL compliance' $Report.acl)
    $d = $Report.mariaDb.classification
    $lines += "MariaDB: $($d.Situation) (convergeable: $(& $yn $d.Convergeable)); service: $($d.ServiceName); account now: '$($d.CurrentAccount)'; target: $($d.TargetAccount); datadir: $($Report.mariaDb.layout.DataDir)"
    foreach ($r in $d.Reasons) { $lines += "  - $r" }
    $lines += (& $aclLine 'MariaDB ACL compliance' $Report.mariaDb.acl)
    $es = $Report.eventSource
    $write = if ($null -eq $es.ServiceCanWrite) { 'undetermined statically (verify on staging, runbook Part C)' } elseif ($es.ServiceCanWrite) { 'yes' } else { 'NO' }
    $lines += "Event source $($es.Source): registered: $(& $yn $es.Registered) $($es.LogName); Apache account can write: $write"
    $lines += "Profile hash (host fingerprint): $($Report.profileHash)"
    $lines += "Convergence needed: $(& $yn $Report.convergenceNeeded)"
    return , $lines
}

function Get-LoamsReportFolder {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot)
    return [IO.Path]::GetFullPath((Join-Path $ProgramDataRoot 'reports')).TrimEnd('\')
}

function Test-LoamsReportLocation {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][string] $ProgramDataRoot)
    $problems = @()
    $folder = Get-LoamsReportFolder -ProgramDataRoot $ProgramDataRoot
    $full = [IO.Path]::GetFullPath($Path)
    if ((Split-Path -Parent $full).TrimEnd('\') -ne $folder) { $problems += "-ReportPath must be a file directly inside the protected reports folder $folder" }
    if (Test-Path -LiteralPath $full -PathType Container) { $problems += "-ReportPath '$full' is a directory" }
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) {
        $problems += "reports folder $folder does not exist: provision it first (runbook Part B0, elevated)"
    } else {
        if (((Get-Item -LiteralPath $folder -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { $problems += "reports folder $folder is a junction or symbolic link" }
        $acl = Test-LoamsProtectedFolderAcl -Sddl (Get-LoamsAclSddl -Path $folder)
        foreach ($p in $acl.Problems) { $problems += "reports folder ACL: $p" }
    }
    return [pscustomobject]@{ Ok = ($problems.Count -eq 0); Problems = $problems }
}

function Set-LoamsDirectorySecurity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)] $Security)
    Assert-LoamsMutationAllowed -Action "set ACL on $Path"
    Set-Acl -LiteralPath $Path -AclObject $Security -ErrorAction Stop
}

function Initialize-LoamsReportFolder {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot)
    Assert-LoamsMutationAllowed -Action 'provision the LOAMS reports folder'
    $folder = Get-LoamsReportFolder -ProgramDataRoot $ProgramDataRoot
    if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Force -Path $folder | Out-Null }
    # A fresh security descriptor: protected, no inherited and no leftover explicit rules, only the allowlist.
    $sec = New-Object System.Security.AccessControl.DirectorySecurity
    $sec.SetAccessRuleProtection($true, $false)
    $inherit = [System.Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit'
    foreach ($sid in @('S-1-5-32-544', 'S-1-5-18')) {
        $id = New-Object System.Security.Principal.SecurityIdentifier($sid)
        $sec.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($id, [System.Security.AccessControl.FileSystemRights]::FullControl, $inherit, [System.Security.AccessControl.PropagationFlags]::None, [System.Security.AccessControl.AccessControlType]::Allow)))
    }
    $sec.SetOwner((New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-544')))
    Set-LoamsDirectorySecurity -Path $folder -Security $sec
    $check = Test-LoamsProtectedFolderAcl -Sddl (Get-LoamsAclSddl -Path $folder)
    if (-not $check.Ok) { throw "STOP: $folder does not match the protected allowlist after provisioning: $($check.Problems -join '; ')" }
    Write-LoamsLog -Message "Reports folder provisioned and verified: $folder"
    return $folder
}

function Save-LoamsHostReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Report, [Parameter(Mandatory)][string] $Path, [string] $ProgramDataRoot = '')
    $dir = Split-Path -Parent ([IO.Path]::GetFullPath($Path))
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
        throw "Report directory '$dir' does not exist: -Report never creates directories. Provision it first (runbook Part B0, elevated) or pass -ReportPath in an existing folder."
    }
    if ($ProgramDataRoot -and $dir.TrimEnd('\') -eq (Get-LoamsReportFolder -ProgramDataRoot $ProgramDataRoot)) {
        $loc = Test-LoamsReportLocation -Path $Path -ProgramDataRoot $ProgramDataRoot
        if (-not $loc.Ok) { throw ("Refusing to write into the LOAMS reports folder: " + ($loc.Problems -join '; ')) }
    }
    $json = $Report | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText($Path, $json, (New-Object System.Text.UTF8Encoding($false)))
}

function Invoke-LoamsHostReport {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string] $ManifestPath,
        [Parameter(Mandatory)][string] $ProgramDataRoot, [string] $ServiceName = 'Apache2.4',
        [string] $MariaDbServiceName = 'mysql', [Parameter(Mandatory)][string] $ReportPath
    )
    Set-LoamsMode -Mode Report
    $report = New-LoamsHostReport -XamppRoot $XamppRoot -ManifestPath $ManifestPath -ProgramDataRoot $ProgramDataRoot -ServiceName $ServiceName -MariaDbServiceName $MariaDbServiceName
    foreach ($line in (Format-LoamsHostReport -Report $report)) { Write-Host $line }
    Save-LoamsHostReport -Report $report -Path $ReportPath -ProgramDataRoot $ProgramDataRoot
    Write-Host "Report JSON: $ReportPath"
    return $report
}
```

- [ ] **Step 5: Implement `deploy/server/Test-LoamsServerHost.ps1` (Report only for now)**

```powershell
#Requires -Version 5.1
<#
.SYNOPSIS
    LOAMS server host check (spec S1 section 4). -Report (default) is read-only.
.DESCRIPTION
    Detects Apache/PHP and MariaDB, verifies the modules actually loaded by the
    running httpd against deploy/stack/loams-stack-manifest.json, classifies the
    deployment situation, checks service identities, ACLs and the LOAMS-Transport
    event source, and fingerprints the host. Run elevated: without elevation the
    loaded-module list cannot be read and the result is Incomplete (exit 3).
    Exit codes: 0 Success, 1 Refused, 2 StopAndReport, 3 Incomplete,
    4 RecoveryRequired, 5 RolledBack, 6 SuccessReportNotSaved.
#>
[CmdletBinding(DefaultParameterSetName = 'Report')]
param(
    [Parameter(ParameterSetName = 'Report')][switch] $Report,
    [string] $XamppRoot = 'C:\xampp',
    [string] $ManifestPath = (Join-Path $PSScriptRoot '..\stack\loams-stack-manifest.json'),
    [string] $ServiceName = 'Apache2.4',
    [string] $MariaDbServiceName = 'mysql',
    [string] $ProgramDataRoot = (Join-Path $env:ProgramData 'LOAMS'),
    [string] $ReportPath = ''
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'LoamsHost\LoamsHost.psm1') -Force
if (-not $ReportPath) {
    $ReportPath = Join-Path (Join-Path $ProgramDataRoot 'reports') ('loams-host-report-{0}-{1}.json' -f $env:COMPUTERNAME, [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
}
$result = Invoke-LoamsHostReport -XamppRoot $XamppRoot -ManifestPath $ManifestPath -ProgramDataRoot $ProgramDataRoot -ServiceName $ServiceName -MariaDbServiceName $MariaDbServiceName -ReportPath $ReportPath
exit (Get-LoamsExitCode -Outcome $result.outcome)
```

- [ ] **Step 6: Run to verify it passes**

Same command as Step 3. Expected: PASS, 18 tests. Set `expected-test-count.txt` to `165` and run the suite runner → `PASSED: 165 tests`.

- [ ] **Step 7: Read-only smoke on this dev box (manual; non-elevated is fine)**

```powershell
$before = (icacls C:\xampp) + (icacls C:\xampp\mysql\data)
$svcBefore = Get-CimInstance Win32_Service -Filter "Name='Apache2.4' OR Name='mysql'" | Select-Object Name, StartName, StartMode, State
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\server\Test-LoamsServerHost.ps1 -Report -ReportPath "$env:TEMP\loams-host-report-dev.json"; "exit=$LASTEXITCODE"
$after = (icacls C:\xampp) + (icacls C:\xampp\mysql\data)
$svcAfter = Get-CimInstance Win32_Service -Filter "Name='Apache2.4' OR Name='mysql'" | Select-Object Name, StartName, StartMode, State
Compare-Object $before $after; Compare-Object @($svcBefore) @($svcAfter) -Property Name, StartName, StartMode, State
```

Expected: non-elevated → `Outcome: Incomplete`, `exit=3`; elevated → `Outcome: StopAndReport`, `exit=2` (draft manifest; `UnlistedCrypto` for Apache's OpenSSL 1.1.1t DLLs if both lineages are loaded). The report shows `MariaDB: ServiceNonVirtualAccount ... service: mysql; account now: 'LocalSystem'` and lists `MySQL80` as an ignored stopped service. Both `Compare-Object` calls print nothing. Do **not** commit the generated report (it names this machine).

- [ ] **Step 8: Commit**

Via the project `commit` skill — subject: `feat(deploy): add read-only Test-LoamsServerHost -Report covering Apache and MariaDB`

---

### Task 12: Backup encryption primitives — pinned OpenSSL, recovery certificate, streaming CMS, archive container

**Approach (owner change D14):** every backup set is encrypted to an administrator's **recovery certificate** (public key only on the server) with the **manifest-pinned** `openssl.exe` (`openssl cms -encrypt -binary -stream -aes256 -outform DER … <recipient.crt>`). Plaintext is streamed from its source (the `mysqldump` process, or files read one by one into an in-process archive stream) straight into `openssl`'s stdin, so **no plaintext dump or file copy is ever written on the server**. The server never holds the decryption key; the recovery private key is generated and kept offline under the CA-key custody rules (spec §5), and decryption happens only on a recovery workstation or during the staging restore drill.

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Crypto.ps1`
- Create: `deploy/server/LoamsHost/LoamsHost.BackupArchive.ps1`
- Create: `deploy/tests/Crypto.Tests.ps1`
- Modify: `deploy/tests/TestHelpers.ps1` (append `Get-LoamsTestOpenSsl`, `New-LoamsTestRecoveryCert`)
- Modify: `deploy/tests/expected-test-count.txt` → `182`

**Interfaces:**
- Consumes: `Invoke-LoamsExternal`, `ConvertTo-LoamsArgumentString`, `Protect-LoamsText`, `Assert-LoamsMutationAllowed`, `Write-LoamsLog` (Task 1); `Get-LoamsProp` (Task 2).
- Produces:
  - `Resolve-LoamsContainedPath -Root <string> -RelativePath <string>` → full path strictly below `Root`; throws for rooted paths, `.`/`..` segments, a result outside `Root`, a missing path, or any path component (below `Root`) that is a junction or symbolic link.
  - `Get-LoamsPinnedOpenSsl -Manifest <object> -XamppRoot <string>` → `[string]` path of `server.tools[name=openssl]` (selected via `Get-LoamsPropList`) after containment (`Resolve-LoamsContainedPath`) and its SHA-256 matched; throws when the hash is `REPLACE_ME`, the path escapes, or the hash differs.
  - `Test-LoamsRecipientCertificate -Path <string> -ExpectedSha256 <string>` → `[pscustomobject]@{ Path; Sha256; Subject; NotAfter }`; throws on fingerprint mismatch, expiry, or if the file contains a private key.
  - `Start-LoamsRedirectedProcess -FilePath <string> [-ArgumentList <string[]>] [-RedirectStdin] [-RedirectStdout]` → `[System.Diagnostics.Process]` (stderr always redirected; command line logged redacted).
  - `Invoke-LoamsEncryptStream -OpenSslExe <string> -RecipientCert <string> -OutFile <string> -Writer <scriptblock>` → `[pscustomobject]@{ OutFile; PlainSha256; CipherSha256 }`. `Writer` is called as `& $Writer $stream` with a write-only stream that hashes everything written to it on the way into `openssl`; on any failure the partial `OutFile` is deleted. Mutating (guarded).
  - `Test-LoamsCmsFile -OpenSslExe <string> -Path <string>` → `[bool]` (parses as CMS EnvelopedData without needing the key).
  - `Invoke-LoamsDecryptFile -OpenSslExe -InFile -RecipientCert -RecoveryKey -OutFile` (recovery workstation / staging drill only; interactive passphrase prompt if the key is encrypted; refuses to overwrite).
  - `Write-LoamsBackupArchive -Stream <IO.Stream> -Items <object[]>` → `[int]` entry count. Items `@{ Source=<full path>; Name=<archive name> }`. Format `LOAMSARC1`: magic line, then per entry `Int32 nameLength, UTF-8 name, Int64 length, bytes, 32-byte SHA-256`, terminated by `Int32 -1`.
  - `Read-LoamsBackupArchive -Stream <IO.Stream> -Destination <string>` → `[pscustomobject[]]` `@{ Name; Length; Sha256 }`; throws on a bad magic, a hash mismatch, or a name that is rooted or contains `..`.
  - `Expand-LoamsEncryptedArchive -OpenSslExe -InFile -RecipientCert -RecoveryKey -Destination` → entries (decrypts to a temporary file inside `Destination`, extracts, verifies, deletes the temporary file in `finally`).
  - Test helpers: `Get-LoamsTestOpenSsl` → path (`$env:LOAMS_OPENSSL`, else `C:\xampp\apache\bin\openssl.exe`); `New-LoamsTestRecoveryCert -OpenSslExe -Directory` → `@{ Cert; Key; Sha256 }` (throwaway RSA-2048 test pair, test-only).

Tests use a **real** OpenSSL (any 1.1.1+/3.x `openssl.exe`, e.g. XAMPP's on a dev box, or `LOAMS_OPENSSL`): a missing OpenSSL is a test **failure**, not a skip (spec §6). Production code only ever uses the manifest-pinned tool.

- [ ] **Step 1: Append test helpers to `deploy/tests/TestHelpers.ps1`**

```powershell
function Get-LoamsTestOpenSsl {
    if ($env:LOAMS_OPENSSL) { return $env:LOAMS_OPENSSL }
    return 'C:\xampp\apache\bin\openssl.exe'
}

function New-LoamsTestRecoveryCert {
    param([Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)][string] $Directory)
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $cnf = Join-Path $Directory 'req.cnf'
    Set-Content -Path $cnf -Encoding ASCII -Value "[req]`r`ndistinguished_name = dn`r`nprompt = no`r`n[dn]`r`nCN = LOAMS Test Recovery`r`n"
    $key = Join-Path $Directory 'test-recovery.key'
    $cert = Join-Path $Directory 'test-recovery.crt'
    & $OpenSslExe req -x509 -newkey rsa:2048 -nodes -keyout $key -out $cert -days 2 -config $cnf 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $cert)) { throw "could not create the test recovery certificate with $OpenSslExe" }
    $c = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 -ArgumentList $cert
    $sha = [Security.Cryptography.SHA256]::Create()
    $fp = (($sha.ComputeHash($c.RawData) | ForEach-Object { $_.ToString('x2') }) -join '')
    return [pscustomobject]@{ Cert = $cert; Key = $key; Sha256 = $fp }
}
```

- [ ] **Step 2: Write the failing tests `deploy/tests/Crypto.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Ossl = Get-LoamsTestOpenSsl
    if (-not (Test-Path -LiteralPath $script:Ossl)) { throw "OpenSSL not found at '$script:Ossl': set LOAMS_OPENSSL (prerequisite; this is a failure, not a skip)" }
    $script:Pki = New-LoamsTestRecoveryCert -OpenSslExe $script:Ossl -Directory (Join-Path $TestDrive 'pki')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    function Decrypt { param([string] $In, [string] $Out)
        & $script:Ossl cms -decrypt -binary -inform DER -in $In -recip $script:Pki.Cert -inkey $script:Pki.Key -out $Out 2>&1 | Out-Null
        $LASTEXITCODE | Should -Be 0
    }
}

Describe 'Get-LoamsPinnedOpenSsl' {
    It 'refuses a manifest whose OpenSSL tool hash is not recorded' {
        $m = New-LoamsTestManifest -XamppRoot $script:Root -Status draft
        $m.server.tools[0].sha256 = 'REPLACE_ME'
        { Get-LoamsPinnedOpenSsl -Manifest (ConvertTo-LoamsManifestObject -Manifest $m) -XamppRoot $script:Root } | Should -Throw -ExpectedMessage '*not recorded*'
    }
    It 'refuses a tool whose hash differs' {
        $m = New-LoamsTestManifest -XamppRoot $script:Root
        $m.server.tools[0].sha256 = ('cd' * 32)
        { Get-LoamsPinnedOpenSsl -Manifest (ConvertTo-LoamsManifestObject -Manifest $m) -XamppRoot $script:Root } | Should -Throw -ExpectedMessage '*hash mismatch*'
    }
    It 'returns the tool path when the hash matches' {
        $m = ConvertTo-LoamsManifestObject -Manifest (New-LoamsTestManifest -XamppRoot $script:Root)
        Get-LoamsPinnedOpenSsl -Manifest $m -XamppRoot $script:Root | Should -Be (Join-Path $script:Root 'apache\bin\openssl.exe')
    }
}

Describe 'Get-LoamsPinnedOpenSsl containment and selection' {
    It 'selects the openssl entry when several tools are listed' {
        $m = New-LoamsTestManifest -XamppRoot $script:Root
        $other = @{ name = 'other'; relativePath = 'apache\bin\httpd.exe'; sha256 = (Get-LoamsTestSha256 -Path (Join-Path $script:Root 'apache\bin\httpd.exe')); fileVersion = '1' }
        $m.server.tools = @($other, $m.server.tools[0])
        Get-LoamsPinnedOpenSsl -Manifest (ConvertTo-LoamsManifestObject -Manifest $m) -XamppRoot $script:Root | Should -Be (Join-Path $script:Root 'apache\bin\openssl.exe')
    }
    It 'refuses a tool path that leaves the XAMPP root' {
        $m = New-LoamsTestManifest -XamppRoot $script:Root
        $m.server.tools[0].relativePath = '..\outside\openssl.exe'
        { Get-LoamsPinnedOpenSsl -Manifest (ConvertTo-LoamsManifestObject -Manifest $m) -XamppRoot $script:Root } | Should -Throw -ExpectedMessage '*not a plain relative path*'
    }
    It 'refuses a tool reached through a junction out of the XAMPP root' {
        $outside = Join-Path $TestDrive 'outside-bin'; New-Item -ItemType Directory -Force -Path $outside | Out-Null
        Copy-Item (Join-Path $script:Root 'apache\bin\openssl.exe') (Join-Path $outside 'openssl.exe')
        $junction = Join-Path $script:Root 'apache\jbin'
        New-Item -ItemType Junction -Path $junction -Target $outside | Out-Null
        try {
            $m = New-LoamsTestManifest -XamppRoot $script:Root
            $m.server.tools[0].relativePath = 'apache\jbin\openssl.exe'
            $m.server.tools[0].sha256 = (Get-LoamsTestSha256 -Path (Join-Path $outside 'openssl.exe'))
            { Get-LoamsPinnedOpenSsl -Manifest (ConvertTo-LoamsManifestObject -Manifest $m) -XamppRoot $script:Root } | Should -Throw -ExpectedMessage '*junction or symbolic link*'
        } finally { [IO.Directory]::Delete($junction) }
    }
}

Describe 'Test-LoamsRecipientCertificate' {
    It 'accepts the pinned recovery certificate' {
        (Test-LoamsRecipientCertificate -Path $script:Pki.Cert -ExpectedSha256 $script:Pki.Sha256).Sha256 | Should -Be $script:Pki.Sha256
    }
    It 'refuses a different fingerprint' {
        { Test-LoamsRecipientCertificate -Path $script:Pki.Cert -ExpectedSha256 ('ab' * 32) } | Should -Throw -ExpectedMessage '*fingerprint*'
    }
    It 'refuses a file that carries a private key' {
        $both = Join-Path $TestDrive 'both.pem'
        Set-Content -Path $both -Value ((Get-Content -Raw $script:Pki.Cert) + (Get-Content -Raw $script:Pki.Key))
        { Test-LoamsRecipientCertificate -Path $both -ExpectedSha256 $script:Pki.Sha256 } | Should -Throw -ExpectedMessage '*private key*'
    }
}

Describe 'Invoke-LoamsEncryptStream' {
    AfterEach { Set-LoamsMode -Mode Report }
    It 'streams plaintext into a CMS file that only the recovery key decrypts' {
        Set-LoamsMode -Mode Converge
        $out = Join-Path $TestDrive 'stream.p7m'
        $payload = [Text.Encoding]::UTF8.GetBytes(('synthetic backup payload ' * 5000))
        $r = Invoke-LoamsEncryptStream -OpenSslExe $script:Ossl -RecipientCert $script:Pki.Cert -OutFile $out -Writer { param($s) $s.Write($payload, 0, $payload.Length) }
        $sha = [Security.Cryptography.SHA256]::Create()
        $r.PlainSha256 | Should -Be ((($sha.ComputeHash($payload)) | ForEach-Object { $_.ToString('x2') }) -join '')
        Test-LoamsCmsFile -OpenSslExe $script:Ossl -Path $out | Should -BeTrue
        $plain = Join-Path $TestDrive 'stream.out'
        Decrypt -In $out -Out $plain
        [IO.File]::ReadAllBytes($plain).Length | Should -Be $payload.Length
    }
    It 'removes the partial output when the writer fails' {
        Set-LoamsMode -Mode Converge
        $out = Join-Path $TestDrive 'failed.p7m'
        { Invoke-LoamsEncryptStream -OpenSslExe $script:Ossl -RecipientCert $script:Pki.Cert -OutFile $out -Writer { param($s) throw 'source failed' } } | Should -Throw -ExpectedMessage '*source failed*'
        Test-Path $out | Should -BeFalse
    }
    It 'refuses in Report mode' {
        { Invoke-LoamsEncryptStream -OpenSslExe $script:Ossl -RecipientCert $script:Pki.Cert -OutFile (Join-Path $TestDrive 'x.p7m') -Writer { } } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
    }
}

Describe 'Test-LoamsCmsFile' {
    It 'is false for a plaintext file' {
        Test-LoamsCmsFile -OpenSslExe $script:Ossl -Path (Join-Path $script:Root 'php\php.ini') | Should -BeFalse
    }
}

Describe 'Backup archive container' {
    It 'round-trips files with per-entry hashes' {
        $ms = New-Object System.IO.MemoryStream
        $items = @(@{ Source = (Join-Path $script:Root 'php\php.ini'); Name = 'php-ini/php.ini' }, @{ Source = (Join-Path $script:Root 'htdocs\loams_api\config.php'); Name = 'loams_api/config.php' })
        Write-LoamsBackupArchive -Stream $ms -Items $items | Should -Be 2
        $ms.Position = 0
        $dest = Join-Path $TestDrive 'unpacked'
        $entries = Read-LoamsBackupArchive -Stream $ms -Destination $dest
        @($entries).Count | Should -Be 2
        Get-Content -Raw (Join-Path $dest 'loams_api\config.php') | Should -Be (Get-Content -Raw (Join-Path $script:Root 'htdocs\loams_api\config.php'))
    }
    It 'rejects a corrupted entry' {
        $ms = New-Object System.IO.MemoryStream
        Write-LoamsBackupArchive -Stream $ms -Items @(@{ Source = (Join-Path $script:Root 'php\php.ini'); Name = 'php.ini' }) | Out-Null
        $bytes = $ms.ToArray(); $bytes[30] = $bytes[30] -bxor 0xFF
        { Read-LoamsBackupArchive -Stream (New-Object System.IO.MemoryStream(, $bytes)) -Destination (Join-Path $TestDrive 'bad') } | Should -Throw -ExpectedMessage '*hash mismatch*'
    }
    It 'rejects entry names that escape the destination' {
        $ms = New-Object System.IO.MemoryStream
        Write-LoamsBackupArchive -Stream $ms -Items @(@{ Source = (Join-Path $script:Root 'php\php.ini'); Name = '../evil.ini' }) | Out-Null
        $ms.Position = 0
        { Read-LoamsBackupArchive -Stream $ms -Destination (Join-Path $TestDrive 'evil') } | Should -Throw -ExpectedMessage '*unsafe entry name*'
    }
    It 'expands an encrypted archive with the recovery key and leaves no temporary plaintext' {
        Set-LoamsMode -Mode Converge
        $out = Join-Path $TestDrive 'files.p7m'
        $items = @(@{ Source = (Join-Path $script:Root 'htdocs\loams_api\config.php'); Name = 'loams_api/config.php' })
        Invoke-LoamsEncryptStream -OpenSslExe $script:Ossl -RecipientCert $script:Pki.Cert -OutFile $out -Writer { param($s) Write-LoamsBackupArchive -Stream $s -Items $items | Out-Null } | Out-Null
        Set-LoamsMode -Mode Report
        $dest = Join-Path $TestDrive 'restored'
        $entries = Expand-LoamsEncryptedArchive -OpenSslExe $script:Ossl -InFile $out -RecipientCert $script:Pki.Cert -RecoveryKey $script:Pki.Key -Destination $dest
        @($entries).Count | Should -Be 1
        @(Get-ChildItem -Path $dest -Filter '*.tmp' -Force).Count | Should -Be 0
    }
}
```

- [ ] **Step 3: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Crypto.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsPinnedOpenSsl` not recognised.

- [ ] **Step 4: Implement `deploy/server/LoamsHost/LoamsHost.Crypto.ps1`**

```powershell
# Backup encryption: manifest-pinned OpenSSL, pinned recovery certificate, streaming CMS (owner change D14).

function Resolve-LoamsContainedPath {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $RelativePath)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $rel = $RelativePath -replace '/', '\'
    if ([IO.Path]::IsPathRooted($rel) -or $rel -match '(^|\\)\.\.?(\\|$)') { throw "Path '$RelativePath' is not a plain relative path below $rootFull." }
    $full = [IO.Path]::GetFullPath((Join-Path $rootFull $rel))
    if (-not $full.StartsWith($rootFull + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Path '$RelativePath' escapes $rootFull." }
    $cursor = $rootFull
    foreach ($segment in $full.Substring($rootFull.Length + 1).Split('\')) {
        $cursor = Join-Path $cursor $segment
        if (-not (Test-Path -LiteralPath $cursor)) { throw "Path not found: $full" }
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Path '$RelativePath' passes through a junction or symbolic link ($cursor)." }
    }
    return $full
}

function Get-LoamsPinnedOpenSsl {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Manifest, [Parameter(Mandatory)][string] $XamppRoot)
    $server = Get-LoamsProp $Manifest 'server'
    $tool = @(Get-LoamsPropList $server 'tools') | Where-Object { (Get-LoamsProp $_ 'name') -eq 'openssl' } | Select-Object -First 1
    if ($null -eq $tool) { throw 'The manifest has no server.tools entry named openssl.' }
    if ($tool.sha256 -ceq 'REPLACE_ME') { throw 'The manifest-pinned OpenSSL tool hash is not recorded yet (runbook Part A1); encryption is refused.' }
    $path = Resolve-LoamsContainedPath -Root $XamppRoot -RelativePath ([string]$tool.relativePath)
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -cne $tool.sha256) { throw "Pinned OpenSSL tool hash mismatch: $path" }
    return $path
}

function Test-LoamsRecipientCertificate {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][string] $ExpectedSha256)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Backup recovery certificate not found: $Path" }
    if ((Get-Content -Raw -LiteralPath $Path) -match 'PRIVATE KEY') { throw "The file '$Path' contains a private key: the recovery private key must never be on the server." }
    $full = (Resolve-Path -LiteralPath $Path).ProviderPath
    $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 -ArgumentList $full
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $fp = (($sha.ComputeHash($cert.RawData) | ForEach-Object { $_.ToString('x2') }) -join '') } finally { $sha.Dispose() }
    $expected = ($ExpectedSha256 -replace '[:\s]', '').ToLowerInvariant()
    if ($fp -cne $expected) { throw "Recovery certificate fingerprint $fp does not match the expected fingerprint (verify it out of band)." }
    if ($cert.NotAfter.ToUniversalTime() -lt [DateTime]::UtcNow) { throw "Recovery certificate expired on $($cert.NotAfter.ToString('yyyy-MM-dd'))." }
    return [pscustomobject]@{ Path = $full; Sha256 = $fp; Subject = $cert.Subject; NotAfter = $cert.NotAfter.ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ') }
}

function Start-LoamsRedirectedProcess {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $FilePath, [AllowEmptyCollection()][AllowEmptyString()][string[]] $ArgumentList = @(),
          [switch] $RedirectStdin, [switch] $RedirectStdout)
    $argString = ConvertTo-LoamsArgumentString -ArgumentList $ArgumentList
    Write-LoamsLog -Message ("exec (stream): {0} {1}" -f $FilePath, $argString)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = $argString
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardInput = [bool]$RedirectStdin
    $psi.RedirectStandardOutput = [bool]$RedirectStdout
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()
    return $proc
}

function Invoke-LoamsEncryptStream {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)][string] $RecipientCert,
        [Parameter(Mandatory)][string] $OutFile, [Parameter(Mandatory)][scriptblock] $Writer
    )
    Assert-LoamsMutationAllowed -Action "encrypt to $OutFile"
    $enc = Start-LoamsRedirectedProcess -FilePath $OpenSslExe -RedirectStdin -ArgumentList @(
        'cms', '-encrypt', '-binary', '-stream', '-aes256', '-outform', 'DER', '-out', $OutFile, $RecipientCert)
    $encErr = $enc.StandardError.ReadToEndAsync()
    $hasher = [Security.Cryptography.SHA256]::Create()
    $hashing = New-Object System.Security.Cryptography.CryptoStream($enc.StandardInput.BaseStream, $hasher, [System.Security.Cryptography.CryptoStreamMode]::Write)
    $failure = $null
    try {
        & $Writer $hashing
        $hashing.FlushFinalBlock()
    } catch {
        $failure = $_
    } finally {
        try { $hashing.Dispose() } catch { }
        $enc.WaitForExit()
    }
    if ($null -ne $failure) {
        Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
        throw (Protect-LoamsText -Text $failure.Exception.Message)
    }
    if ($enc.ExitCode -ne 0) {
        Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
        throw (Protect-LoamsText -Text ("openssl cms -encrypt failed with exit code {0}: {1}" -f $enc.ExitCode, ([string]$encErr.Result).Trim()))
    }
    $plain = (($hasher.Hash | ForEach-Object { $_.ToString('x2') }) -join '')
    $cipher = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutFile).Hash.ToLowerInvariant()
    Write-LoamsLog -Message "Encrypted $OutFile (cipher $cipher)"
    return [pscustomobject]@{ OutFile = $OutFile; PlainSha256 = $plain; CipherSha256 = $cipher }
}

function Test-LoamsCmsFile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)][string] $Path)
    $r = Invoke-LoamsExternal -FilePath $OpenSslExe -ArgumentList @('cms', '-cmsout', '-print', '-noout', '-inform', 'DER', '-in', $Path) -AllowNonZeroExit
    return ($r.ExitCode -eq 0 -and $r.StdOut -match 'envelopedData')
}

function Invoke-LoamsDecryptFile {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)][string] $InFile, [Parameter(Mandatory)][string] $RecipientCert,
        [Parameter(Mandatory)][string] $RecoveryKey, [Parameter(Mandatory)][string] $OutFile
    )
    if (Test-Path -LiteralPath $OutFile) { throw "Refusing to overwrite $OutFile" }
    $argString = ConvertTo-LoamsArgumentString -ArgumentList @('cms', '-decrypt', '-binary', '-inform', 'DER', '-in', $InFile, '-recip', $RecipientCert, '-inkey', $RecoveryKey, '-out', $OutFile)
    # Not redirected: an encrypted recovery key prompts for its passphrase on the console.
    $p = Start-Process -FilePath $OpenSslExe -ArgumentList $argString -Wait -NoNewWindow -PassThru
    if ($p.ExitCode -ne 0) {
        Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
        throw "openssl cms -decrypt failed with exit code $($p.ExitCode) for $InFile"
    }
}
```

- [ ] **Step 5: Implement `deploy/server/LoamsHost/LoamsHost.BackupArchive.ps1`**

```powershell
# LOAMSARC1: a minimal streaming container so many files can be encrypted as one CMS stream
# without writing a plaintext archive (or plaintext copies) to disk.

$script:LoamsArchiveMagic = [Text.Encoding]::ASCII.GetBytes("LOAMSARC1`n")

function Write-LoamsBackupArchive {
    [CmdletBinding()]
    param([Parameter(Mandatory)][IO.Stream] $Stream, [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Items)
    $Stream.Write($script:LoamsArchiveMagic, 0, $script:LoamsArchiveMagic.Length)
    $bw = New-Object System.IO.BinaryWriter($Stream, [Text.Encoding]::UTF8, $true)
    $buf = New-Object byte[] 65536
    $count = 0
    foreach ($it in $Items) {
        $nameBytes = [Text.Encoding]::UTF8.GetBytes([string]$it.Name)
        $fs = [IO.File]::Open([string]$it.Source, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $length = [long]$fs.Length
            $bw.Write([int]$nameBytes.Length); $bw.Write($nameBytes); $bw.Write($length)
            $remaining = $length
            while ($remaining -gt 0) {
                $n = $fs.Read($buf, 0, [int][Math]::Min([long]$buf.Length, $remaining))
                if ($n -le 0) { throw "file shrank while archiving: $($it.Name)" }
                [void]$sha.TransformBlock($buf, 0, $n, $null, 0)
                $bw.Write($buf, 0, $n)
                $remaining -= $n
            }
            [void]$sha.TransformFinalBlock((New-Object byte[] 0), 0, 0)
            $bw.Write($sha.Hash)
        } finally { $fs.Dispose(); $sha.Dispose() }
        $count++
    }
    $bw.Write([int]-1)
    $bw.Flush()
    return $count
}

function Read-LoamsBackupArchive {
    [CmdletBinding()]
    param([Parameter(Mandatory)][IO.Stream] $Stream, [Parameter(Mandatory)][string] $Destination)
    $br = New-Object System.IO.BinaryReader($Stream, [Text.Encoding]::UTF8, $true)
    $magic = $br.ReadBytes($script:LoamsArchiveMagic.Length)
    if ([Convert]::ToBase64String($magic) -ne [Convert]::ToBase64String($script:LoamsArchiveMagic)) { throw 'Not a LOAMSARC1 archive.' }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $destFull = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
    $entries = @()
    $buf = New-Object byte[] 65536
    while ($true) {
        $nameLen = $br.ReadInt32()
        if ($nameLen -eq -1) { break }
        $name = [Text.Encoding]::UTF8.GetString($br.ReadBytes($nameLen))
        if ($name -match '(^|[\\/])\.\.([\\/]|$)' -or [IO.Path]::IsPathRooted($name)) { throw "unsafe entry name '$name'" }
        $target = [IO.Path]::GetFullPath((Join-Path $destFull ($name -replace '/', '\')))
        if (-not $target.StartsWith($destFull, [StringComparison]::OrdinalIgnoreCase)) { throw "unsafe entry name '$name'" }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        $length = $br.ReadInt64()
        $sha = [Security.Cryptography.SHA256]::Create()
        $out = [IO.File]::Open($target, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
        try {
            $remaining = $length
            while ($remaining -gt 0) {
                $n = $br.Read($buf, 0, [int][Math]::Min([long]$buf.Length, $remaining))
                if ($n -le 0) { throw "archive truncated in '$name'" }
                [void]$sha.TransformBlock($buf, 0, $n, $null, 0)
                $out.Write($buf, 0, $n)
                $remaining -= $n
            }
        } finally { $out.Dispose() }
        [void]$sha.TransformFinalBlock((New-Object byte[] 0), 0, 0)
        $expected = $br.ReadBytes(32)
        $actualHex = (($sha.Hash | ForEach-Object { $_.ToString('x2') }) -join '')
        $expectedHex = (($expected | ForEach-Object { $_.ToString('x2') }) -join '')
        $sha.Dispose()
        if ($actualHex -ne $expectedHex) { throw "hash mismatch for archive entry '$name'" }
        $entries += [pscustomobject]@{ Name = $name; Length = $length; Sha256 = $actualHex }
    }
    return , $entries
}

function Expand-LoamsEncryptedArchive {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)][string] $InFile, [Parameter(Mandatory)][string] $RecipientCert,
        [Parameter(Mandatory)][string] $RecoveryKey, [Parameter(Mandatory)][string] $Destination
    )
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $tmp = Join-Path $Destination ('.loams-archive-{0}.tmp' -f [guid]::NewGuid().ToString('N'))
    try {
        Invoke-LoamsDecryptFile -OpenSslExe $OpenSslExe -InFile $InFile -RecipientCert $RecipientCert -RecoveryKey $RecoveryKey -OutFile $tmp
        $fs = [IO.File]::OpenRead($tmp)
        try { return (Read-LoamsBackupArchive -Stream $fs -Destination $Destination) } finally { $fs.Dispose() }
    } finally {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
}
```

The temporary plaintext in `Expand-LoamsEncryptedArchive` exists only on the recovery workstation (or staging, during the drill) inside the destination the operator chose for restored data, and is always deleted in `finally`.

- [ ] **Step 6: Run to verify it passes**

Same command as Step 3. Expected: PASS, 17 tests. Set `expected-test-count.txt` to `182` and run the suite runner → `PASSED: 182 tests`.

- [ ] **Step 7: Commit**

Via the project `commit` skill — subject: `feat(deploy): add streaming CMS backup encryption to a pinned recovery certificate`

---

### Task 13: DB credentials, encrypted streaming dump, and MariaDB verification on a scratch schema

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Secrets.ps1`
- Create: `deploy/server/LoamsHost/LoamsHost.Database.ps1`
- Create: `deploy/tests/Secrets.Tests.ps1`
- Create: `deploy/tests/Database.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `205`

**Interfaces:**
- Consumes: Task 1 helpers; `Start-LoamsRedirectedProcess`, `Invoke-LoamsEncryptStream` (Task 12).
- Produces (`LoamsHost.Secrets.ps1`):
  - `Get-LoamsPhpDefine -Text <string> -Name <string>` → `[string]` or `$null` (string-literal `define()` only).
  - `ConvertFrom-LoamsSecureString -Secure <SecureString>` → `[string]` (BSTR zero-freed).
  - `Read-LoamsDbCredential -UserName <string>` → `[pscredential]` (interactive; mocked in tests).
  - `Get-LoamsDbConnectionInfo -ApiRoot <string> [-DbCredential <pscredential>]` → `[pscustomobject]@{ Host; User; Database; Password=[SecureString]; Source='credential'|'config.php'|'prompt' }`; registers the plaintext for redaction.
  - `ConvertTo-LoamsMyCnfValue -Value <string>` → quoted option-file value.
  - `New-LoamsMysqlDefaultsFile -Directory -ConnectionInfo` → `[string]`: the path is **registered in the run's credential-file list before the file is created**, the admin-only ACL is applied before the password is written.
  - `Remove-LoamsFileSecurely -Path` (overwrite with zeros, then delete; throws on failure — the deletion boundary, mocked in tests); `Remove-LoamsMysqlDefaultsFile -Path` (removes via `Remove-LoamsFileSecurely`, unregisters only after success, so a failed deletion stays tracked).
  - `Get-LoamsCredentialFiles` → `[string[]]` tracked paths; `Clear-LoamsCredentialFiles` → overwrites + deletes every tracked file, throws `"credential files could not be overwritten and deleted (they contain the DB password): <paths>"` if any remains. Used by the `MariaDbVerification` undo and the end-of-run sweep (Task 17); any failure ⇒ `RECOVERY REQUIRED`.
  - `Invoke-LoamsEncryptedDatabaseDump -MysqldumpExe -ConnectionInfo -OpenSslExe -RecipientCert -OutFile -WorkDirectory [-Database <string>]` → `[pscustomobject]@{ Path; PlainSha256; CipherSha256 }`. `mysqldump` stdout is streamed straight into `Invoke-LoamsEncryptStream`; **no plaintext dump file exists at any point**; throws (redacted) on failure or a missing `-- Dump completed` trailer, leaving no partial file.
- Produces (`LoamsHost.Database.ps1`):
  - `Invoke-LoamsMysqlQuery -MysqlExe -DefaultsFile -Sql` → `[string]` trimmed stdout (`--defaults-extra-file` first, `--batch --skip-column-names -e <sql>`).
  - `Copy-LoamsDbSchemaPipe -MysqldumpExe -MysqlExe -DefaultsFile -Source <schema> -Target <schema>` (mutating: `mysqldump <Source>` piped into `mysql <Target>` in memory).
  - `Get-LoamsDatabaseBackupReadiness -MariaDbLayout -Processes -ApplicationDatabase -ServiceName` → `[pscustomobject]@{ Ready; State='running'|'stopped-with-data'|'no-application-database'; Reason }` (decision D19, below).
  - `Invoke-LoamsDbVerification -MysqlExe -MysqldumpExe -ConnectionInfo -WorkDirectory -Schema <name> -RestoreSchema <name>` → `[pscustomobject]@{ Passed=[bool]; Steps=[pscustomobject[]] (Name, Ok, Detail) }`. Names are **run-specific** (`loams_s1b_verify_<backup-set id>` and `…_r`, from the backup manifest `verifySchemas`) and must match `^loams_s1b_verify_[0-9a-z_]+$`. Steps: `read` (`SELECT VERSION()`), `write` (create the scratch schema, a table, 3 rows), `backup-restore` (dump it and restore it into the restore schema, row count must be 3), `cleanup` (drops both). A failed `cleanup` makes `Passed` false; the checkpoint's undo (`Remove-LoamsVerifySchemas`) then drops them again and fails loudly if it cannot. It **never** reads or writes the application database or attendance tables.
  - `Remove-LoamsVerifySchemas -MysqlExe -ConnectionInfo -WorkDirectory -Schemas <string[]>` (mutating; idempotent): refuses any name outside `^loams_s1b_verify_[0-9a-z_]+$`, runs `DROP DATABASE IF EXISTS` for exactly the given names, then confirms via `information_schema.SCHEMATA` that none remains — otherwise throws (⇒ `RECOVERY REQUIRED` when called from an undo).

**Database backup readiness (D19):** the backup is never skipped and S1b never starts a database before backing it up.

| MariaDB at preflight | Data directory | Behaviour |
|---|---|---|
| running (service or Control-Panel process) | any | encrypted dump of the application database |
| not running | contains any non-system schema folder (anything except `mysql`, `performance_schema`, `sys`, `test`, `phpmyadmin`) | **refused**: start MariaDB (`Start-Service <name>` or the Control Panel), confirm attendance works, re-run `-Report` and `-Converge` |
| not running | system schemas only (fresh install) | **refused**: import the LOAMS database and start MariaDB first — post-change validation must prove PHP + database reads, so a host without its application database could only end in rollback |

Starting a stopped database automatically before the backup could trigger crash recovery or change state outside the maintenance plan; refusing with instructions keeps the operator in control, and there is no path where a backup is silently skipped.

**Secret rules:** the DB password exists only as a `SecureString`, a line in an admin-only `--defaults-extra-file` that lives for one call, and a registered redaction value; never in `ArgumentList`, logs, transcripts, `-Verbose`, exceptions or the backup manifest. `--single-transaction` gives a consistent snapshot for InnoDB tables (runbook B1 verifies the engines).

- [ ] **Step 1: Write the failing tests `deploy/tests/Secrets.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Api = Join-Path $script:Root 'htdocs\loams_api'
    $script:Secret = 'S1b-test-db-pass'
    $script:Ossl = Get-LoamsTestOpenSsl
    if (-not (Test-Path -LiteralPath $script:Ossl)) { throw "OpenSSL not found at '$script:Ossl': set LOAMS_OPENSSL (prerequisite; failure, not skip)" }
    $script:Pki = New-LoamsTestRecoveryCert -OpenSslExe $script:Ossl -Directory (Join-Path $TestDrive 'pki')
    $global:LoamsTDumpFixture = Join-Path $TestDrive 'fixture.sql'
    Set-Content -Path $global:LoamsTDumpFixture -Value "-- MariaDB dump (synthetic)`r`nCREATE TABLE t (id int);`r`n-- Dump completed on 2026-10-05 10:00:00"
    $global:LoamsTNoTrailer = Join-Path $TestDrive 'no-trailer.sql'
    Set-Content -Path $global:LoamsTNoTrailer -Value "-- MariaDB dump (synthetic)`r`nCREATE TABLE t (id int);"
    # Fake mysqldump: a real child process that prints a fixture (and optionally fails), so the streaming path is exercised.
    function Use-FakeDump { param([string] $Fixture = $global:LoamsTDumpFixture, [int] $Exit = 0)
        $global:LoamsTFixtureNow = $Fixture; $global:LoamsTDumpExitNow = $Exit
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } -ParameterFilter { $FilePath -like '*icacls.exe' }
        Mock -ModuleName LoamsHost Start-LoamsRedirectedProcess {
            $cmd = "/s /c `"type `"$global:LoamsTFixtureNow`" & (if $global:LoamsTDumpExitNow neq 0 echo Access denied using password S1b-test-db-pass 1>&2) & exit $global:LoamsTDumpExitNow`""
            $psi = New-Object System.Diagnostics.ProcessStartInfo("$env:SystemRoot\System32\cmd.exe", $cmd)
            $psi.UseShellExecute = $false; $psi.RedirectStandardOutput = $true; $psi.RedirectStandardError = $true; $psi.CreateNoWindow = $true
            [System.Diagnostics.Process]::Start($psi)
        } -ParameterFilter { $FilePath -like '*mysqldump.exe' }
    }
    function Dump { param($Conn, [string] $Work)
        Invoke-LoamsEncryptedDatabaseDump -MysqldumpExe 'C:\x\mysqldump.exe' -ConnectionInfo $Conn -OpenSslExe $script:Ossl -RecipientCert $script:Pki.Cert -OutFile (Join-Path $Work 'database.p7m') -WorkDirectory $Work
    }
}
AfterAll { Remove-Variable -Name LoamsTDumpFixture, LoamsTNoTrailer, LoamsTFixtureNow, LoamsTDumpExitNow -Scope Global -ErrorAction SilentlyContinue }

Describe 'Get-LoamsDbConnectionInfo' {
    BeforeEach { Clear-LoamsSecrets }
    It 'reads host, user, database and password from config.php' {
        $c = Get-LoamsDbConnectionInfo -ApiRoot $script:Api
        $c.Host | Should -Be 'localhost'
        $c.User | Should -Be 'loams_test'
        $c.Database | Should -Be 'wits_test'
        $c.Source | Should -Be 'config.php'
        ConvertFrom-LoamsSecureString -Secure $c.Password | Should -Be $script:Secret
        Protect-LoamsText -Text $script:Secret | Should -Be '<REDACTED>'
    }
    It 'prefers an explicit credential' {
        $cred = New-Object pscredential('backup_user', (ConvertTo-SecureString 'Other-test-pass' -AsPlainText -Force))
        $c = Get-LoamsDbConnectionInfo -ApiRoot $script:Api -DbCredential $cred
        $c.User | Should -Be 'backup_user'
        $c.Source | Should -Be 'credential'
    }
    It 'prompts when config.php has no literal DB_PASS' {
        $api2 = Join-Path $TestDrive 'api2'; New-Item -ItemType Directory -Path $api2 | Out-Null
        Set-Content -Path (Join-Path $api2 'config.php') -Value "<?php define('DB_HOST','localhost'); define('DB_USER','loams_test'); define('DB_NAME','wits_test'); define('DB_PASS', getenv('X'));"
        Mock -ModuleName LoamsHost Read-LoamsDbCredential { New-Object pscredential('loams_test', (ConvertTo-SecureString 'Prompted-test-pass' -AsPlainText -Force)) }
        (Get-LoamsDbConnectionInfo -ApiRoot $api2).Source | Should -Be 'prompt'
        Should -Invoke -ModuleName LoamsHost Read-LoamsDbCredential -Times 1 -Exactly
    }
    It 'fails when DB_NAME is missing' {
        $api3 = Join-Path $TestDrive 'api3'; New-Item -ItemType Directory -Path $api3 | Out-Null
        Set-Content -Path (Join-Path $api3 'config.php') -Value "<?php define('DB_HOST','localhost'); define('DB_USER','u');"
        { Get-LoamsDbConnectionInfo -ApiRoot $api3 } | Should -Throw -ExpectedMessage '*DB_NAME*'
    }
}

Describe 'Run-owned credential files' {
    BeforeEach { Clear-LoamsSecrets; Set-LoamsMode -Mode Converge }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'registers the defaults file before anything is written, so a failure while creating it stays tracked' {
        $conn = Get-LoamsDbConnectionInfo -ApiRoot $script:Api
        $work = Join-Path $TestDrive ('track-' + [guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Path $work | Out-Null
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { throw 'icacls failed' } -ParameterFilter { $FilePath -like '*icacls.exe' }
        { New-LoamsMysqlDefaultsFile -Directory $work -ConnectionInfo $conn } | Should -Throw -ExpectedMessage '*icacls failed*'
        $tracked = @(Get-LoamsCredentialFiles | Where-Object { $_ -like "$work\*" })
        $tracked.Count | Should -Be 1
        Clear-LoamsCredentialFiles
        Test-Path $tracked[0] | Should -BeFalse
        @(Get-LoamsCredentialFiles | Where-Object { $_ -like "$work\*" }).Count | Should -Be 0
    }
    It 'Clear-LoamsCredentialFiles throws and keeps tracking a file it cannot delete' {
        $conn = Get-LoamsDbConnectionInfo -ApiRoot $script:Api
        $work = Join-Path $TestDrive ('lock-' + [guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Path $work | Out-Null
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } -ParameterFilter { $FilePath -like '*icacls.exe' }
        $path = New-LoamsMysqlDefaultsFile -Directory $work -ConnectionInfo $conn
        Mock -ModuleName LoamsHost Remove-LoamsFileSecurely { throw 'The process cannot access the file because it is being used by another process' }
        { Clear-LoamsCredentialFiles } | Should -Throw -ExpectedMessage '*credential files could not be overwritten and deleted*'
        Get-LoamsCredentialFiles | Should -Contain $path
        [IO.File]::Delete($path)
        Clear-LoamsCredentialFiles
    }
}

Describe 'ConvertTo-LoamsMyCnfValue' {
    It 'quotes and escapes backslashes' { ConvertTo-LoamsMyCnfValue -Value 'a\b' | Should -Be '"a\\b"' }
    It 'switches to single quotes when the value has a double quote' { ConvertTo-LoamsMyCnfValue -Value 'a"b' | Should -Be "'a`"b'" }
}

Describe 'Invoke-LoamsEncryptedDatabaseDump' {
    BeforeEach {
        Clear-LoamsSecrets
        Set-LoamsMode -Mode Converge
        $script:Work = Join-Path $TestDrive ([guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Path $script:Work | Out-Null
        $script:Conn = Get-LoamsDbConnectionInfo -ApiRoot $script:Api
    }
    AfterEach { Set-LoamsMode -Mode Report; Set-LoamsLogPath -Path '' }
    It 'never passes the password as an argument and puts the defaults file first' {
        Use-FakeDump
        Dump -Conn $script:Conn -Work $script:Work | Out-Null
        Should -Invoke -ModuleName LoamsHost Start-LoamsRedirectedProcess -Times 1 -Exactly -ParameterFilter { $FilePath -like '*mysqldump.exe' -and $ArgumentList[0] -like '--defaults-extra-file=*' -and $ArgumentList -contains '--single-transaction' -and ($ArgumentList -join ' ') -notmatch 'S1b-test-db-pass' }
    }
    It 'writes only ciphertext that decrypts to the dump, and no plaintext file' {
        Use-FakeDump
        $r = Dump -Conn $script:Conn -Work $script:Work
        @(Get-ChildItem -Path $script:Work -File | Where-Object { $_.Name -ne 'database.p7m' }).Count | Should -Be 0
        $plain = Join-Path $TestDrive ('dump-' + [guid]::NewGuid().ToString('N') + '.sql')
        & $script:Ossl cms -decrypt -binary -inform DER -in $r.Path -recip $script:Pki.Cert -inkey $script:Pki.Key -out $plain 2>&1 | Out-Null
        Get-Content -Raw $plain | Should -Match '-- Dump completed'
        (Get-FileHash -Algorithm SHA256 $plain).Hash.ToLowerInvariant() | Should -Be $r.PlainSha256
    }
    It 'removes the defaults file and the partial ciphertext, and redacts the error, after a failure' {
        Use-FakeDump -Exit 2
        $err = $null
        try { Dump -Conn $script:Conn -Work $script:Work } catch { $err = $_.Exception.Message }
        $err | Should -Match 'mysqldump failed'
        $err | Should -Not -Match 'S1b-test-db-pass'
        @(Get-ChildItem -Path $script:Work -File).Count | Should -Be 0
    }
    It 'rejects a dump without the completion trailer' {
        Use-FakeDump -Fixture $global:LoamsTNoTrailer
        { Dump -Conn $script:Conn -Work $script:Work } | Should -Throw -ExpectedMessage '*not verified*'
        @(Get-ChildItem -Path $script:Work -File).Count | Should -Be 0
    }
    It 'keeps the password out of the log file, transcript and verbose output' {
        Use-FakeDump -Exit 2
        $log = Join-Path $TestDrive 'session.log'; $tx = Join-Path $TestDrive 'transcript.txt'
        Set-LoamsLogPath -Path $log
        Start-Transcript -Path $tx | Out-Null
        try {
            $conn = Get-LoamsDbConnectionInfo -ApiRoot $script:Api -Verbose
            try { Dump -Conn $conn -Work $script:Work -Verbose 4>&1 | Out-String | Should -Not -Match 'S1b-test-db-pass' } catch { $_.Exception.Message | Should -Not -Match 'S1b-test-db-pass' }
        } finally { Stop-Transcript | Out-Null }
        Get-Content -Raw $log | Should -Not -Match 'S1b-test-db-pass'
        Get-Content -Raw $tx | Should -Not -Match 'S1b-test-db-pass'
    }
}
```

- [ ] **Step 2: Write the failing tests `deploy/tests/Database.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    $script:Conn = [pscustomobject]@{ Host = 'localhost'; User = 'loams_test'; Database = 'wits_test'; Password = (ConvertTo-SecureString 'S1b-test-db-pass' -AsPlainText -Force); Source = 'credential' }
    function Use-DbMocks { param([switch] $FailWrite)
        $global:LoamsTSql = New-Object System.Collections.Generic.List[string]
        $global:LoamsTFailWrite = [bool]$FailWrite
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } -ParameterFilter { $FilePath -like '*icacls.exe' }
        Mock -ModuleName LoamsHost Invoke-LoamsMysqlQuery {
            $global:LoamsTSql.Add($Sql)
            if ($global:LoamsTFailWrite -and $Sql -match 'CREATE TABLE') { throw 'ERROR 1142: CREATE command denied' }
            if ($Sql -match 'COUNT\(\*\)') { return '3' }
            return '10.4.28-MariaDB'
        }
        Mock -ModuleName LoamsHost Copy-LoamsDbSchemaPipe { }
    }
    function Verify { Invoke-LoamsDbVerification -MysqlExe 'C:\x\mysql.exe' -MysqldumpExe 'C:\x\mysqldump.exe' -ConnectionInfo $script:Conn -WorkDirectory $TestDrive -Schema 'loams_s1b_verify_t1' -RestoreSchema 'loams_s1b_verify_t1_r' }
}
AfterAll { Remove-Variable -Name LoamsTSql, LoamsTFailWrite -Scope Global -ErrorAction SilentlyContinue }

Describe 'Get-LoamsDatabaseBackupReadiness' {
    BeforeAll {
        function New-DataDir { param([string[]] $Schemas)
            $d = Join-Path $TestDrive ('data-' + [guid]::NewGuid().ToString('N'))
            New-Item -ItemType Directory -Path $d | Out-Null
            Set-Content -Path (Join-Path $d 'ibdata1') -Value 'x'
            foreach ($s in $Schemas) { New-Item -ItemType Directory -Path (Join-Path $d $s) | Out-Null }
            return [pscustomobject]@{ DataDir = $d }
        }
    }
    It 'is ready when MariaDB is running' {
        (Get-LoamsDatabaseBackupReadiness -MariaDbLayout (New-DataDir @('mysql', 'wits_test')) -Processes @([pscustomobject]@{ ProcessId = 20 }) -ApplicationDatabase 'wits_test' -ServiceName 'mysql').Ready | Should -BeTrue
    }
    It 'refuses a stopped MariaDB that holds the application database' {
        $r = Get-LoamsDatabaseBackupReadiness -MariaDbLayout (New-DataDir @('mysql', 'performance_schema', 'wits_test')) -Processes @() -ApplicationDatabase 'wits_test' -ServiceName 'mysql'
        $r.Ready | Should -BeFalse
        $r.State | Should -Be 'stopped-with-data'
        $r.Reason | Should -Match 'wits_test'
    }
    It 'refuses a stopped MariaDB that holds any other non-system schema' {
        (Get-LoamsDatabaseBackupReadiness -MariaDbLayout (New-DataDir @('mysql', 'otherapp')) -Processes @() -ApplicationDatabase 'wits_test' -ServiceName 'mysql').State | Should -Be 'stopped-with-data'
    }
    It 'refuses a fresh host with no application database' {
        $r = Get-LoamsDatabaseBackupReadiness -MariaDbLayout (New-DataDir @('mysql', 'performance_schema', 'phpmyadmin', 'test')) -Processes @() -ApplicationDatabase 'wits_test' -ServiceName 'mysql'
        $r.State | Should -Be 'no-application-database'
        $r.Reason | Should -Match 'Import the LOAMS database'
    }
}

Describe 'Invoke-LoamsDbVerification' {
    BeforeEach { Set-LoamsMode -Mode Converge }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'passes read, write and backup-restore on the scratch schema and always cleans up' {
        Use-DbMocks
        $r = Verify
        $r.Passed | Should -BeTrue
        ($r.Steps.Name -join ',') | Should -Be 'read,write,backup-restore,cleanup'
        $global:LoamsTSql[$global:LoamsTSql.Count - 1] | Should -Match 'DROP DATABASE IF EXISTS loams_s1b_verify_t1; DROP DATABASE IF EXISTS loams_s1b_verify_t1_r'
        Should -Invoke -ModuleName LoamsHost Copy-LoamsDbSchemaPipe -Times 1 -Exactly -ParameterFilter { $Source -eq 'loams_s1b_verify_t1' -and $Target -eq 'loams_s1b_verify_t1_r' }
    }
    It 'fails when the write step fails, skips backup-restore, and still cleans up' {
        Use-DbMocks -FailWrite
        $r = Verify
        $r.Passed | Should -BeFalse
        ($r.Steps.Name -join ',') | Should -Be 'read,write,cleanup'
        Should -Invoke -ModuleName LoamsHost Copy-LoamsDbSchemaPipe -Times 0
    }
    It 'never touches the application database' {
        Use-DbMocks
        Verify | Out-Null
        ($global:LoamsTSql -join ' ') | Should -Not -Match 'wits_test'
        ($global:LoamsTSql -join ' ') | Should -Not -Match 'library_visits|turnstile_events|students'
    }
    It 'refuses in Report mode' {
        Set-LoamsMode -Mode Report
        Use-DbMocks
        { Verify } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
    }
}

Describe 'Remove-LoamsVerifySchemas' {
    BeforeEach { Set-LoamsMode -Mode Converge }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'refuses to drop anything that is not a verification schema name' {
        Use-DbMocks
        { Remove-LoamsVerifySchemas -MysqlExe 'C:\x\mysql.exe' -ConnectionInfo $script:Conn -WorkDirectory $TestDrive -Schemas @('loams_s1b_verify_t1', 'wits_test') } | Should -Throw -ExpectedMessage "*Refusing to drop 'wits_test'*"
        @($global:LoamsTSql).Count | Should -Be 0
    }
    It 'fails loudly when a schema is still present after the drop' {
        Use-DbMocks
        Mock -ModuleName LoamsHost Invoke-LoamsMysqlQuery { $global:LoamsTSql.Add($Sql); if ($Sql -match 'SCHEMA_NAME') { return 'loams_s1b_verify_t1' }; '' }
        { Remove-LoamsVerifySchemas -MysqlExe 'C:\x\mysql.exe' -ConnectionInfo $script:Conn -WorkDirectory $TestDrive -Schemas @('loams_s1b_verify_t1') } | Should -Throw -ExpectedMessage '*still present after DROP*'
        ($global:LoamsTSql -join ' ') | Should -Match 'DROP DATABASE IF EXISTS loams_s1b_verify_t1;'
    }
}
```

- [ ] **Step 3: Run to verify they fail**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Secrets.Tests.ps1,deploy\tests\Database.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Get-LoamsDbConnectionInfo` / `Invoke-LoamsDbVerification` not recognised.

- [ ] **Step 4: Implement `deploy/server/LoamsHost/LoamsHost.Secrets.ps1`**

```powershell
# DB credentials and the encrypted, streamed database dump. The password never reaches a command line, log or transcript.

function Get-LoamsPhpDefine {
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyString()][string] $Text, [Parameter(Mandatory)][string] $Name)
    $pattern = 'define\(\s*[''"]' + [regex]::Escape($Name) + '[''"]\s*,\s*(?:''((?:[^''\\]|\\.)*)''|"((?:[^"\\$]|\\.)*)")\s*\)'
    $m = [regex]::Match($Text, $pattern)
    if (-not $m.Success) { return $null }
    if ($m.Groups[1].Success) { return ($m.Groups[1].Value -replace '\\([''\\])', '$1') }
    return ($m.Groups[2].Value -replace '\\(["\\$])', '$1')
}

function ConvertFrom-LoamsSecureString {
    [CmdletBinding()]
    param([Parameter(Mandatory)][Security.SecureString] $Secure)
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Secure)
    try { return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
}

function Read-LoamsDbCredential {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $UserName)
    return Get-Credential -UserName $UserName -Message 'LOAMS database password for the pre-change backup (never stored or logged)'
}

function Get-LoamsDbConnectionInfo {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ApiRoot, [pscredential] $DbCredential)
    $text = Get-Content -Raw -LiteralPath (Join-Path $ApiRoot 'config.php') -ErrorAction Stop
    $dbHost = Get-LoamsPhpDefine -Text $text -Name 'DB_HOST'
    $user = Get-LoamsPhpDefine -Text $text -Name 'DB_USER'
    $db = Get-LoamsPhpDefine -Text $text -Name 'DB_NAME'
    if (-not $dbHost -or -not $user -or -not $db) { throw 'config.php does not define DB_HOST, DB_USER and DB_NAME as string literals.' }
    if ($DbCredential) {
        $secure = $DbCredential.Password.Copy(); $user = $DbCredential.UserName; $source = 'credential'
    } else {
        $plain = Get-LoamsPhpDefine -Text $text -Name 'DB_PASS'
        if ($null -ne $plain) {
            $secure = New-Object Security.SecureString
            foreach ($ch in $plain.ToCharArray()) { $secure.AppendChar($ch) }
            $source = 'config.php'
        } else {
            $secure = (Read-LoamsDbCredential -UserName $user).Password.Copy()
            $source = 'prompt'
        }
        $plain = $null
    }
    $text = $null
    $secure.MakeReadOnly()
    Register-LoamsSecret -Secret (ConvertFrom-LoamsSecureString -Secure $secure)
    Write-Verbose "DB connection info: host=$dbHost user=$user database=$db source=$source"
    return [pscustomobject]@{ Host = $dbHost; User = $user; Database = $db; Password = $secure; Source = $source }
}

function ConvertTo-LoamsMyCnfValue {
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyString()][string] $Value)
    if ($Value.Contains('"') -and $Value.Contains("'")) { throw 'Value contains both quote characters; supply -DbCredential for a different account.' }
    $escaped = $Value.Replace('\', '\\')
    if ($Value.Contains('"')) { return "'" + $escaped + "'" }
    return '"' + $escaped + '"'
}

# Every run-owned credential file is tracked from before its creation until it is verifiably gone.
$script:LoamsCredentialFiles = New-Object System.Collections.Generic.List[string]

function New-LoamsMysqlDefaultsFile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Directory, [Parameter(Mandatory)] $ConnectionInfo)
    Assert-LoamsMutationAllowed -Action 'write MariaDB client defaults file'
    $path = Join-Path $Directory ('mysql-client-{0}.cnf' -f [guid]::NewGuid().ToString('N'))
    $script:LoamsCredentialFiles.Add($path)
    [IO.File]::WriteAllBytes($path, [byte[]]@())
    Invoke-LoamsExternal -FilePath (Join-Path $env:SystemRoot 'System32\icacls.exe') -ArgumentList @($path, '/inheritance:r', '/grant:r', '*S-1-5-32-544:F', '*S-1-5-18:F') | Out-Null
    $plain = ConvertFrom-LoamsSecureString -Secure $ConnectionInfo.Password
    try {
        $content = "[client]`r`nuser=" + (ConvertTo-LoamsMyCnfValue -Value $ConnectionInfo.User) +
                   "`r`npassword=" + (ConvertTo-LoamsMyCnfValue -Value $plain) +
                   "`r`nhost=" + (ConvertTo-LoamsMyCnfValue -Value $ConnectionInfo.Host) + "`r`n"
        [IO.File]::WriteAllText($path, $content, (New-Object System.Text.UTF8Encoding($false)))
    } finally { $plain = $null; $content = $null }
    return $path
}

function Remove-LoamsFileSecurely {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    $len = (Get-Item -LiteralPath $Path -Force).Length
    [IO.File]::WriteAllBytes($Path, (New-Object byte[] $len))
    Remove-Item -LiteralPath $Path -Force -ErrorAction Stop
}

function Remove-LoamsMysqlDefaultsFile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    if (Test-Path -LiteralPath $Path) { Remove-LoamsFileSecurely -Path $Path }
    [void]$script:LoamsCredentialFiles.Remove($Path)
}

function Get-LoamsCredentialFiles {
    [CmdletBinding()] param()
    return @($script:LoamsCredentialFiles)
}

function Clear-LoamsCredentialFiles {
    [CmdletBinding()] param()
    $failed = @()
    foreach ($p in @($script:LoamsCredentialFiles)) {
        try { Remove-LoamsMysqlDefaultsFile -Path $p } catch { $failed += $p }
    }
    if ($failed.Count -gt 0) { throw ("credential files could not be overwritten and deleted (they contain the DB password): " + ($failed -join ', ')) }
}

function Invoke-LoamsEncryptedDatabaseDump {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MysqldumpExe, [Parameter(Mandatory)] $ConnectionInfo,
        [Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)][string] $RecipientCert,
        [Parameter(Mandatory)][string] $OutFile, [Parameter(Mandatory)][string] $WorkDirectory, [string] $Database = ''
    )
    Assert-LoamsMutationAllowed -Action 'encrypted database dump'
    if (-not $Database) { $Database = $ConnectionInfo.Database }
    $cnf = New-LoamsMysqlDefaultsFile -Directory $WorkDirectory -ConnectionInfo $ConnectionInfo
    try {
        $dumpArgs = @("--defaults-extra-file=$cnf", '--single-transaction', '--quick', '--routines', '--triggers', '--events',
                      '--hex-blob', '--default-character-set=utf8mb4', '--databases', $Database)
        $result = Invoke-LoamsEncryptStream -OpenSslExe $OpenSslExe -RecipientCert $RecipientCert -OutFile $OutFile -Writer {
            param($sink)
            $dumpProc = Start-LoamsRedirectedProcess -FilePath $MysqldumpExe -ArgumentList $dumpArgs -RedirectStdout
            $dumpErr = $dumpProc.StandardError.ReadToEndAsync()
            $src = $dumpProc.StandardOutput.BaseStream
            $chunk = New-Object byte[] 65536
            $tailText = ''
            while (($n = $src.Read($chunk, 0, $chunk.Length)) -gt 0) {
                $sink.Write($chunk, 0, $n)
                $tailText += [Text.Encoding]::UTF8.GetString($chunk, 0, $n)
                if ($tailText.Length -gt 512) { $tailText = $tailText.Substring($tailText.Length - 512) }
            }
            $dumpProc.WaitForExit()
            if ($dumpProc.ExitCode -ne 0) { throw ("mysqldump failed with exit code {0}: {1}" -f $dumpProc.ExitCode, ([string]$dumpErr.Result).Trim()) }
            if ($tailText -notmatch '-- Dump completed') { throw 'Database dump not verified: completion trailer missing.' }
        }
    } finally {
        Remove-LoamsMysqlDefaultsFile -Path $cnf
    }
    Write-LoamsLog -Message "Encrypted database dump verified: $OutFile ($($result.CipherSha256))"
    return [pscustomobject]@{ Path = $OutFile; PlainSha256 = $result.PlainSha256; CipherSha256 = $result.CipherSha256 }
}
```

The `Writer` block runs inside `Invoke-LoamsEncryptStream` and sees `$MysqldumpExe` and `$dumpArgs` through PowerShell's dynamic scoping (both functions are in the same module); any exception it throws is redacted by `Invoke-LoamsEncryptStream`, which also deletes the partial ciphertext. PowerShell strings are immutable, so the plaintext password cannot be zeroed in memory: the controls are shortest lifetime, never logged, never on a command line, and the defaults file overwritten before deletion (recorded in the runbook).

- [ ] **Step 5: Implement `deploy/server/LoamsHost/LoamsHost.Database.ps1`**

```powershell
# MariaDB verification on a scratch schema only (owner change D15): startup, reads, writes, backup + restore.

$script:LoamsVerifyNamePattern = '^loams_s1b_verify_[0-9a-z_]+$'
$script:LoamsSystemSchemas = @('mysql', 'performance_schema', 'sys', 'test', 'phpmyadmin')

function Get-LoamsDatabaseBackupReadiness {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $MariaDbLayout, [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Processes,
        [Parameter(Mandatory)][AllowEmptyString()][string] $ApplicationDatabase, [Parameter(Mandatory)][string] $ServiceName
    )
    if (@($Processes).Count -gt 0) { return [pscustomobject]@{ Ready = $true; State = 'running'; Reason = 'MariaDB is running: the application database will be dumped' } }
    $schemas = @(Get-ChildItem -LiteralPath $MariaDbLayout.DataDir -Directory -Force -ErrorAction Stop | ForEach-Object { $_.Name })
    $data = @($schemas | Where-Object { $script:LoamsSystemSchemas -notcontains $_.ToLowerInvariant() })
    if ($data.Count -gt 0) {
        return [pscustomobject]@{ Ready = $false; State = 'stopped-with-data'
            Reason = "MariaDB is not running but holds data (schema folders: $($data -join ', ')). Start it (Start-Service $ServiceName, or the XAMPP Control Panel), confirm attendance works, then re-run -Report and -Converge. S1b never starts the database itself before the backup and never skips the backup." }
    }
    return [pscustomobject]@{ Ready = $false; State = 'no-application-database'
        Reason = "No application database '$ApplicationDatabase' exists in $($MariaDbLayout.DataDir). Import the LOAMS database and start MariaDB first: post-change validation must prove PHP and database reads." }
}

function Invoke-LoamsMysqlQuery {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $MysqlExe, [Parameter(Mandatory)][string] $DefaultsFile, [Parameter(Mandatory)][string] $Sql)
    $r = Invoke-LoamsExternal -FilePath $MysqlExe -ArgumentList @("--defaults-extra-file=$DefaultsFile", '--batch', '--skip-column-names', '-e', $Sql)
    return ([string]$r.StdOut).Trim()
}

function Copy-LoamsDbSchemaPipe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MysqldumpExe, [Parameter(Mandatory)][string] $MysqlExe, [Parameter(Mandatory)][string] $DefaultsFile,
        [Parameter(Mandatory)][string] $Source, [Parameter(Mandatory)][string] $Target
    )
    Assert-LoamsMutationAllowed -Action "copy schema $Source to $Target"
    $dump = Start-LoamsRedirectedProcess -FilePath $MysqldumpExe -ArgumentList @("--defaults-extra-file=$DefaultsFile", '--single-transaction', $Source) -RedirectStdout
    $load = Start-LoamsRedirectedProcess -FilePath $MysqlExe -ArgumentList @("--defaults-extra-file=$DefaultsFile", $Target) -RedirectStdin
    $dumpErr = $dump.StandardError.ReadToEndAsync()
    $loadErr = $load.StandardError.ReadToEndAsync()
    try { $dump.StandardOutput.BaseStream.CopyTo($load.StandardInput.BaseStream) } finally { $load.StandardInput.Close() }
    $dump.WaitForExit(); $load.WaitForExit()
    if ($dump.ExitCode -ne 0) { throw (Protect-LoamsText -Text ("mysqldump of $Source failed: " + ([string]$dumpErr.Result).Trim())) }
    if ($load.ExitCode -ne 0) { throw (Protect-LoamsText -Text ("restore into $Target failed: " + ([string]$loadErr.Result).Trim())) }
}

function Invoke-LoamsDbVerification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MysqlExe, [Parameter(Mandatory)][string] $MysqldumpExe,
        [Parameter(Mandatory)] $ConnectionInfo, [Parameter(Mandatory)][string] $WorkDirectory,
        [Parameter(Mandatory)][string] $Schema, [Parameter(Mandatory)][string] $RestoreSchema
    )
    Assert-LoamsMutationAllowed -Action 'MariaDB verification (scratch schema only)'
    foreach ($n in @($Schema, $RestoreSchema)) { if ($n -cnotmatch $script:LoamsVerifyNamePattern) { throw "'$n' is not a LOAMS verification schema name." } }
    $s = $Schema; $t = $RestoreSchema
    if ($ConnectionInfo.Database -in @($s, $t)) { throw "The application database may not be named $s or $t." }
    $steps = New-Object System.Collections.Generic.List[object]
    $run = { param([string] $name, [scriptblock] $body)
        try { $detail = & $body; $steps.Add([pscustomobject]@{ Name = $name; Ok = $true; Detail = [string]$detail }); return $true }
        catch { $steps.Add([pscustomobject]@{ Name = $name; Ok = $false; Detail = (Protect-LoamsText -Text $_.Exception.Message) }); return $false } }
    $cnf = New-LoamsMysqlDefaultsFile -Directory $WorkDirectory -ConnectionInfo $ConnectionInfo
    try {
        $ok = & $run 'read' { Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql 'SELECT VERSION()' }
        if ($ok) {
            $ok = & $run 'write' {
                $n = Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql ("DROP DATABASE IF EXISTS $s; DROP DATABASE IF EXISTS $t; CREATE DATABASE $s; " +
                    "CREATE TABLE $s.probe (id INT AUTO_INCREMENT PRIMARY KEY, note VARCHAR(32) NOT NULL) ENGINE=InnoDB; " +
                    "INSERT INTO $s.probe (note) VALUES ('s1b-a'),('s1b-b'),('s1b-c'); SELECT COUNT(*) FROM $s.probe;")
                if ($n -ne '3') { throw "scratch write returned $n rows, expected 3" }
                $n }
        }
        if ($ok) {
            [void](& $run 'backup-restore' {
                Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql "CREATE DATABASE $t;" | Out-Null
                Copy-LoamsDbSchemaPipe -MysqldumpExe $MysqldumpExe -MysqlExe $MysqlExe -DefaultsFile $cnf -Source $s -Target $t
                $n = Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql "SELECT COUNT(*) FROM $t.probe;"
                if ($n -ne '3') { throw "restored row count $n, expected 3" }
                $n })
        }
    } finally {
        [void](& $run 'cleanup' { Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql "DROP DATABASE IF EXISTS $s; DROP DATABASE IF EXISTS $t;" })
        Remove-LoamsMysqlDefaultsFile -Path $cnf
    }
    $passed = (@($steps | Where-Object { -not $_.Ok }).Count -eq 0)
    Write-LoamsLog -Message ("MariaDB verification: " + (($steps | ForEach-Object { "$($_.Name)=$(if ($_.Ok) { 'ok' } else { 'FAILED' })" }) -join ', '))
    return [pscustomobject]@{ Passed = $passed; Steps = @($steps) }
}

function Remove-LoamsVerifySchemas {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MysqlExe, [Parameter(Mandatory)] $ConnectionInfo,
        [Parameter(Mandatory)][string] $WorkDirectory, [Parameter(Mandatory)][AllowEmptyCollection()][string[]] $Schemas
    )
    Assert-LoamsMutationAllowed -Action 'drop MariaDB verification schemas'
    foreach ($n in $Schemas) { if ($n -cnotmatch $script:LoamsVerifyNamePattern) { throw "Refusing to drop '$n': not a LOAMS verification schema name." } }
    if (@($Schemas).Count -eq 0) { return }
    $cnf = New-LoamsMysqlDefaultsFile -Directory $WorkDirectory -ConnectionInfo $ConnectionInfo
    try {
        Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql ((@($Schemas) | ForEach-Object { "DROP DATABASE IF EXISTS $_;" }) -join ' ') | Out-Null
        $left = Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql ("SELECT SCHEMA_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME IN ('" + (@($Schemas) -join "','") + "')")
        if ($left) { throw "verification schemas still present after DROP: $left" }
    } finally {
        Remove-LoamsMysqlDefaultsFile -Path $cnf
    }
    Write-LoamsLog -Message ("Dropped verification schemas: " + (@($Schemas) -join ', '))
}
```

- [ ] **Step 6: Run to verify they pass**

Same command as Step 3. Expected: PASS, 23 tests (Secrets 13, Database 10). Set `expected-test-count.txt` to `205` and run the suite runner → `PASSED: 205 tests`.

- [ ] **Step 7: Commit**

Via the project `commit` skill — subject: `feat(deploy): stream an encrypted DB dump and verify MariaDB on a scratch schema`

---

### Task 14: Encrypted backup set, prior-state capture, full restore, verified off-host copy, 30-day retention

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Backup.ps1`
- Create: `deploy/server/LoamsHost/LoamsHost.Restore.ps1`
- Create: `deploy/server/LoamsHost/LoamsHost.OffHost.ps1`
- Create: `deploy/server/LoamsHost/Restore-LoamsHostBackup.template.ps1` (not dot-sourced: the module loads only `LoamsHost.*.ps1`)
- Create: `deploy/tests/Backup.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `232`

**Interfaces:**
- Consumes: Task 1 helpers; `Get-LoamsServiceSnapshot`, `Set-LoamsServiceAccount`, `Stop-/Start-LoamsWindowsService`, `Start-LoamsControlPanelHttpd`, `Unregister-LoamsApacheService` (Task 8); `Unregister-LoamsMariaDbService`, `Start-LoamsControlPanelMysqld` (Task 10); `Get-LoamsEventSourceState`, `Unregister-LoamsEventSource` (Task 7); `Get-LoamsScheduledTaskXml`, `Get-LoamsTaskDefinitionHash`, `Get-LoamsLayoutState`, `Get-LoamsToolsState` (Task 11); `Invoke-LoamsEncryptStream`, `Test-LoamsCmsFile`, `Write-LoamsBackupArchive` (Task 12); `Invoke-LoamsEncryptedDatabaseDump` (Task 13); ACL profiles (Tasks 6, 10).
- Produces (`LoamsHost.Backup.ps1`):
  - `Set-LoamsAdminOnlyAcl -Path [-Directory]` (mutating).
  - `Get-LoamsBackupItems -XamppRoot -ApiRoot -MyIni -ProgramDataRoot` → `@{ Source; Name }[]` (Apache conf, `php.ini`, `my.ini`, API folder incl. `config.php` and photos except `logs\`, `ProgramData\LOAMS\server` except `logs\`).
  - `Save-LoamsPriorHostState -BackupDir -ProgramDataRoot` → the object written to `rollback\loams-state.json`: `{ eventSource {Registered, LogName}, layout [{path, exists}], retention {taskName, taskExists, taskHash}, tools [{path, type ('dir'|'file'), sha256}] }` — the whole `tools\` tree, directories included; also writes `rollback\retention-task.xml` (when the task exists) and `rollback\tools\…` (copies of every pre-existing tools file).
  - `New-LoamsBackupSet -Layout -MariaDbLayout -ApacheServiceName -MariaDbServiceName -ApacheSituation -MariaDbSituation -BackupRoot -ProgramDataRoot -ConnectionInfo -AclProfile -OpenSslExe -Recipient [-ReportPath]` → `[pscustomobject]@{ Path; Verified=$true; Services; AclSaves; PriorState; RestoreScript; ManifestPath; SumsSha256 }`; throws `"Backup at '<dir>' failed verification: ..."` (no host change has happened yet).
  - `Write-LoamsBackupSums -Path` → SHA-256 of the written `SHA256SUMS.json`; `Test-LoamsBackupSet -Path -OpenSslExe` → `{ Verified; Problems }`; `Restore-LoamsAclSave -BackupPath -AclSave` (mutating).
  - Retention: `New-LoamsRetentionTaskXml -ScriptPath` → task XML (SYSTEM `S-1-5-18`, daily 03:30); `Register-LoamsScheduledTaskXml -TaskName -Xml` / `Remove-LoamsScheduledTask -TaskName` (mutating wrappers, errors **not** suppressed); `Register-LoamsBackupRetentionTask -ProgramDataRoot [-ModuleSource] [-TaskName]`; `Get-LoamsRetentionTaskState -ProgramDataRoot [-TaskName]`; `Restore-LoamsRetentionTaskState -ProgramDataRoot -BackupPath -Prior` (restores exactly the captured task definition and `tools\` tree — removes files and then directories added by convergence, recreates prior directories, restores prior files — then **verifies** both and throws on any difference); `Invoke-LoamsBackupRetention -BackupRoot [-RetentionDays 30] [-NowUtc]`.
- Produces (`LoamsHost.Restore.ps1`):
  - `Set-LoamsServiceStartMode -ServiceName -StartMode <'Auto'|'Manual'|'Disabled'>` (mutating: `sc.exe config <name> start= auto|demand|disabled`).
  - `Test-LoamsRestoreNeedsElevation -Manifest -State` → `[bool]` — true when any step would touch services, ACLs, the event log, a scheduled task or the database (then the restore requires elevation; a restore of filesystem-only state does not).
  - `Invoke-LoamsHostRestore -BackupPath` → `[pscustomobject]@{ Outcome='Restored'|'RecoveryRequired'; Steps=[pscustomobject[]] (Name, Ok, Detail); FailedSteps=[string[]] }` — every step (including reading and checking the backup) runs inside the step machinery; any failed step ⇒ `RecoveryRequired`. The last step drops this run's verification schemas if their folders still exist (credentials from the deployed `config.php`).
- Produces (`LoamsHost.OffHost.ps1`): `Get-LoamsDestinationKind`, `Copy-LoamsSetTree`, `Export-LoamsBackupSet`, `Set-LoamsOffHostAttestation`, `Test-LoamsOffHostVerification` (unchanged contract, below).

**Mutation inventory — everything `-Converge` can change, and how each is captured, undone and verified (17 mutations):**

| # | Mutation | Checkpoint | Captured before change | Automatic undo | Restore script step | Verified by |
|---|---|---|---|---|---|---|
| 1 | Apache service created (Control Panel / fresh) | `ApacheServiceRegistration` | `rollback\service-apache.json` (`Exists=false`, situation) | uninstall service | `remove service Apache2.4 registered by converge` | fingerprint `apacheService` |
| 2 | Control-Panel `httpd` stopped | `ApacheServiceRegistration` | situation `ControlPanel` | restart detached | `start Apache2.4` (detached) | operator check (B6) |
| 3 | MariaDB service created | `MariaDbServiceRegistration` | `rollback\service-mariadb.json` | `mysqld --remove` | `remove service mysql registered by converge` | fingerprint `mariaDbService` |
| 4 | Control-Panel `mysqld` shut down | `MariaDbServiceRegistration` | situation `ControlPanel` | restart `--standalone` | `start mysql` (detached) | operator check (B6, C4) |
| 5 | Apache service account | `ApacheServiceAccount` | snapshot `StartName` + `sc qc` + `reg export` | `sc config obj=` original | `restore account of Apache2.4` | fingerprint `apacheService` |
| 6 | MariaDB service account | `MariaDbServiceAccount` | same for MariaDB | same | `restore account of mysql` | fingerprint `mariaDbService` |
| 7 | Service start types | registration | snapshot `StartMode` | via 1/3 | `restore start type of …` | fingerprint (StartMode) |
| 8 | Service running state | account checkpoints | snapshot `State` | restart if it was running | `stop …` / `start …` | validation suite |
| 9 | Apache-side ACLs (XAMPP tree, logs, tmp, uploads) | `Acl` | `rollback\acl-NN.txt` per path (no `/t`) | `icacls /restore` | `restore ACL <path>` | fingerprint `aclSddl` |
| 10 | MariaDB ACLs (tree, data, log dirs, tmpdir) | `MariaDbAcl` | same | same | same | fingerprint `aclSddl` |
| 11 | `ProgramData\LOAMS` layout created (`server\…`, `compat-guard.log`, `tls\private`, `tools`) | `ServerLayout` | `loams-state.json` `layout` (exists?) | delete created paths | `remove ProgramData layout created by converge` | fingerprint `loamsState` (`layout:*`) |
| 12 | ACLs on pre-existing `ProgramData\LOAMS` paths | `Acl` | `acl-NN.txt` (only for paths that existed) | `icacls /restore` | `restore ACL <path>` | fingerprint `loamsState` (`layout:*` SDDL) |
| 13 | `LOAMS-Transport` event source | `EventSource` | `loams-state.json` `eventSource` | remove only if created | `restore event source` | fingerprint `loamsState` (`eventSource`) |
| 14 | `LOAMS Backup Retention` task (create or overwrite) | `BackupRetentionTask` | `rollback\retention-task.xml` + definition hash | re-register prior XML or remove | `restore retention task and tools` | fingerprint `loamsState` (`retentionTask`) |
| 15 | Deployed tools (`ProgramData\LOAMS\tools\LoamsHost\…`, retention script) | `BackupRetentionTask` | `rollback\tools\…` copies + hashes | restore copies, delete new files | same step | fingerprint `loamsState` (`tool:*`) |
| 16 | MariaDB scratch schemas `loams_s1b_verify_<set id>` and `…_r` | `MariaDbVerification` | names derived from the backup-set id and recorded in `backup-manifest.json` `verifySchemas` (they never exist before) | `Remove-LoamsVerifySchemas` for exactly those names (idempotent; failure ⇒ `RECOVERY REQUIRED`) | `drop verification schemas left by converge` (only if their folders exist) | fingerprint `loamsState` `verifySchemas` + post-change check `mariaDb.leftoverVerifySchemas` |
| 17 | Run-owned MariaDB client defaults files `mysql-client-*.cnf` (contain the DB password) | any step that talks to MariaDB (backup dump, `MariaDbServiceRegistration`, `MariaDbVerification`, validation, restore) | path registered in the run's credential-file list **before** the file is created | overwritten + deleted by the creating step; the `MariaDbVerification` undo and the end-of-run sweep (`Clear-LoamsCredentialFiles`) retry every tracked file; any failure ⇒ `RECOVERY REQUIRED` naming the files | the restore's own files are tracked and removed the same way | converge outcome: never `Success`/`RolledBack`/`Refused` while a tracked file remains |

Not rolled back by design: the backup set itself and `C:\ProgramData\LOAMS\backups` (they hold the recovery data), the off-host copy, and `compat.ini` (S1b never writes it; S1d does). S1b never writes application data.

- [ ] **Step 1: Write the generated-restore wrapper `deploy/server/LoamsHost/Restore-LoamsHostBackup.template.ps1`**

```powershell
#Requires -Version 5.1
<#
    LOAMS host restore script, copied into every backup set by
    Test-LoamsServerHost.ps1 -Converge. It first verifies this backup set's
    rollback data against SHA256SUMS.json, then runs the module copy stored in
    rollback\LoamsHost (Invoke-LoamsHostRestore), which undoes every mutation
    listed in the plan's mutation inventory. Encrypted data (database, config
    files, photos) is restored only on a recovery workstation (runbook Part D3).
    Run elevated on the same computer.
    Exit 0 = restored; exit 4 = RECOVERY REQUIRED (the failed step is named).
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
function Exit-Recovery {
    param([string] $Step, [string] $Why)
    Write-Host ("RECOVERY REQUIRED - step '{0}' failed: {1}. Follow docs/security/runbooks/stack-upgrade-staging.md Part D2." -f $Step, $Why)
    exit 4
}
try {
    $sumsPath = Join-Path $here 'SHA256SUMS.json'
    if (-not (Test-Path -LiteralPath $sumsPath)) { Exit-Recovery 'verify backup integrity' 'SHA256SUMS.json is missing' }
    $sums = Get-Content -Raw -LiteralPath $sumsPath | ConvertFrom-Json
    $needed = @($sums.files | Where-Object { $_.path -like 'rollback\*' -or $_.path -eq 'backup-manifest.json' })
    if ($needed.Count -eq 0) { Exit-Recovery 'verify backup integrity' 'SHA256SUMS.json lists no rollback data' }
    foreach ($e in $needed) {
        $p = Join-Path $here $e.path
        if (-not (Test-Path -LiteralPath $p)) { Exit-Recovery 'verify backup integrity' "missing $($e.path)" }
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLowerInvariant() -ne $e.sha256) { Exit-Recovery 'verify backup integrity' "changed since the backup was taken: $($e.path)" }
    }
    Import-Module (Join-Path $here 'rollback\LoamsHost\LoamsHost.psm1') -Force
    $r = Invoke-LoamsHostRestore -BackupPath $here
    foreach ($s in $r.Steps) { Write-Host ("{0} {1} {2}" -f $(if ($s.Ok) { 'OK  ' } else { 'FAIL' }), $s.Name, $s.Detail) }
    if ($r.Outcome -ne 'Restored') { Exit-Recovery ($r.FailedSteps -join ', ') 'see FAIL lines above' }
    Write-Host 'Restore completed. Run Test-LoamsServerHost.ps1 -Report and compare its profile hash with the pre-change report.'
    exit 0
} catch {
    Exit-Recovery 'unexpected error' $_.Exception.Message
}
```

- [ ] **Step 2: Write the failing tests `deploy/tests/Backup.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Ossl = Get-LoamsTestOpenSsl
    if (-not (Test-Path -LiteralPath $script:Ossl)) { throw "OpenSSL not found at '$script:Ossl': set LOAMS_OPENSSL (prerequisite; failure, not skip)" }
    $script:Pki = New-LoamsTestRecoveryCert -OpenSslExe $script:Ossl -Directory (Join-Path $TestDrive 'pki')
    $script:Recipient = Test-LoamsRecipientCertificate -Path $script:Pki.Cert -ExpectedSha256 $script:Pki.Sha256
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    Set-Content -Path (Join-Path $script:Root 'htdocs\loams_api\uploads\STUDENT-TEST-0001.jpg') -Value 'JPEGDATA'
    $script:Layout = Get-LoamsXamppLayout -XamppRoot $script:Root
    $script:DbLayout = Get-LoamsMariaDbLayout -XamppRoot $script:Root
    $script:Pd = Join-Path $TestDrive 'pd\LOAMS'
    $script:Profile = @(Get-LoamsAclProfile -XamppRoot $script:Root -ProgramDataRoot $script:Pd) + @(Get-LoamsMariaDbAclProfile -XamppRoot $script:Root -Layout $script:DbLayout)
    $script:Conn = [pscustomobject]@{ Host = 'localhost'; User = 'loams_test'; Database = 'wits_test'; Password = (New-Object Security.SecureString); Source = 'credential' }
    $script:OldXml = '<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"><RegistrationInfo><Date>2026-01-01</Date></RegistrationInfo><Actions><Exec><Command>old-retention.exe</Command></Exec></Actions></Task>'
    # Fake scheduled-task store: real functions run, only the Task Scheduler boundary is replaced.
    function Use-TaskStore { param([hashtable] $Initial = @{}, [switch] $RemoveFails)
        $global:LoamsTTasks = $Initial.Clone(); $global:LoamsTRemoveFails = [bool]$RemoveFails
        Mock -ModuleName LoamsHost Get-LoamsScheduledTaskXml { if ($global:LoamsTTasks.ContainsKey($TaskName)) { $global:LoamsTTasks[$TaskName] } else { $null } }
        Mock -ModuleName LoamsHost Register-LoamsScheduledTaskXml { $global:LoamsTTasks[$TaskName] = $Xml }
        Mock -ModuleName LoamsHost Remove-LoamsScheduledTask { if ($global:LoamsTRemoveFails) { throw 'Unregister-ScheduledTask: Access is denied' }; $global:LoamsTTasks.Remove($TaskName) }
    }
    function Use-BackupMocks { param([bool] $DbExists = $true, [switch] $DumpFails, [bool] $EventRegistered = $false)
        $global:LoamsTDbExists = $DbExists; $global:LoamsTDumpFails = [bool]$DumpFails; $global:LoamsTEventRegistered = $EventRegistered
        Use-TaskStore
        Mock -ModuleName LoamsHost Get-LoamsServiceSnapshot {
            $exists = $true; if ($ServiceName -eq 'mysql') { $exists = $global:LoamsTDbExists }
            [pscustomobject]@{ Exists = $exists; Name = $ServiceName; StartName = 'LocalSystem'; StartMode = 'Auto'; PathName = 'x'; State = 'Running'; AccountKind = 'LocalSystem' }
        }
        Mock -ModuleName LoamsHost Get-LoamsEventSourceState { [pscustomobject]@{ Source = 'LOAMS-Transport'; Registered = $global:LoamsTEventRegistered; LogName = 'Application'; ApplicationLogSddl = ''; ServiceCanWrite = $null } }
        Mock -ModuleName LoamsHost Invoke-LoamsExternal {
            if ($ArgumentList -contains '/save') { Set-Content -Path $ArgumentList[2] -Value 'acl-data' }
            if ($FilePath -like '*reg.exe') { Set-Content -Path $ArgumentList[2] -Value 'Windows Registry Editor Version 5.00' }
            [pscustomobject]@{ ExitCode = 0; StdOut = 'SERVICE_NAME: x'; StdErr = '' }
        } -ParameterFilter { $FilePath -notlike '*openssl*' }
        Mock -ModuleName LoamsHost Invoke-LoamsEncryptedDatabaseDump {
            if ($global:LoamsTDumpFails) { throw 'mysqldump failed with exit code 2: <REDACTED>' }
            $r = Invoke-LoamsEncryptStream -OpenSslExe $OpenSslExe -RecipientCert $RecipientCert -OutFile $OutFile -Writer {
                param($s) $b = [Text.Encoding]::UTF8.GetBytes("-- synthetic dump`n-- Dump completed"); $s.Write($b, 0, $b.Length) }
            [pscustomobject]@{ Path = $r.OutFile; PlainSha256 = $r.PlainSha256; CipherSha256 = $r.CipherSha256 }
        }
    }
    function Invoke-Backup {
        New-LoamsBackupSet -Layout $script:Layout -MariaDbLayout $script:DbLayout -ApacheServiceName 'Apache2.4' -MariaDbServiceName 'mysql' `
            -ApacheSituation 'ServiceNonVirtualAccount' -MariaDbSituation 'ServiceNonVirtualAccount' `
            -BackupRoot (Join-Path $TestDrive ('b' + [guid]::NewGuid().ToString('N'))) -ProgramDataRoot $script:Pd -ConnectionInfo $script:Conn `
            -AclProfile $script:Profile -OpenSslExe $script:Ossl -Recipient $script:Recipient -ReportPath 'pre.json'
    }
    function Get-PlainTextOfSet { param([string] $Path)
        (Get-ChildItem -Path $Path -Recurse -File | Where-Object { $_.Extension -ne '.p7m' } | ForEach-Object { [IO.File]::ReadAllText($_.FullName) }) -join "`n"
    }
}
AfterAll { Remove-Variable -Name LoamsTDbExists, LoamsTDumpFails, LoamsTEventRegistered, LoamsTTasks, LoamsTRemoveFails -Scope Global -ErrorAction SilentlyContinue }

Describe 'New-LoamsBackupSet' {
    BeforeEach { Set-LoamsMode -Mode Converge }
    AfterEach { Set-LoamsMode -Mode Report; Set-LoamsLogPath -Path '' }
    It 'creates a timestamped admin-only set with rollback data, a module copy, encrypted parts and logs' {
        Use-BackupMocks
        $b = Invoke-Backup
        (Split-Path -Leaf $b.Path) | Should -Match '^host-\d{8}T\d{6}Z$'
        foreach ($p in @('rollback\loams-state.json', 'rollback\LoamsHost\LoamsHost.psm1', 'encrypted\database.p7m', 'encrypted\files.p7m', 'logs\converge.log', 'SHA256SUMS.json', 'Restore-LoamsHostBackup.ps1')) {
            Test-Path (Join-Path $b.Path $p) | Should -BeTrue
        }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $ArgumentList[0] -eq $b.Path -and ($ArgumentList -join ' ') -match '/inheritance:r /grant:r \*S-1-5-32-544:\(OI\)\(CI\)F \*S-1-5-18:\(OI\)\(CI\)F' }
    }
    It 'captures both service configurations' {
        Use-BackupMocks
        Invoke-Backup | Out-Null
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $FilePath -like '*sc.exe' -and ($ArgumentList -join ' ') -eq 'qc Apache2.4' }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 1 -Exactly -ParameterFilter { $FilePath -like '*sc.exe' -and ($ArgumentList -join ' ') -eq 'qc mysql' }
    }
    It 'skips sc.exe/reg.exe for a MariaDB that has no service (Control Panel)' {
        Use-BackupMocks -DbExists $false
        Invoke-Backup | Out-Null
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0 -ParameterFilter { ($ArgumentList -join ' ') -eq 'qc mysql' }
    }
    It 'saves each profile path''s own ACL without /t (no file names of photos)' {
        Use-BackupMocks
        Invoke-Backup | Out-Null
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -ParameterFilter { $ArgumentList[0] -eq $script:DbLayout.DataDir -and $ArgumentList[1] -eq '/save' }
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsExternal -Times 0 -ParameterFilter { $ArgumentList -contains '/save' -and $ArgumentList -contains '/t' }
    }
    It 'captures the prior LOAMS state: event source, existing retention task and tools files' {
        Use-BackupMocks -EventRegistered $true
        $global:LoamsTTasks['LOAMS Backup Retention'] = $script:OldXml
        New-Item -ItemType Directory -Force -Path (Join-Path $script:Pd 'tools') | Out-Null
        Set-Content -Path (Join-Path $script:Pd 'tools\Invoke-LoamsBackupRetention.ps1') -Value 'old-script'
        try {
            $b = Invoke-Backup
            $state = Get-Content -Raw (Join-Path $b.Path 'rollback\loams-state.json') | ConvertFrom-Json
            $state.eventSource.Registered | Should -BeTrue
            $state.retention.taskExists | Should -BeTrue
            Get-Content -Raw (Join-Path $b.Path 'rollback\retention-task.xml') | Should -Match 'old-retention.exe'
            Get-Content (Join-Path $b.Path 'rollback\tools\Invoke-LoamsBackupRetention.ps1') | Should -Be 'old-script'
        } finally { Remove-Item -Recurse -Force (Join-Path $script:Pd 'tools') }
    }
    It 'encrypts files so that only the recovery key restores them, with no plaintext secret on disk' {
        Use-BackupMocks
        $b = Invoke-Backup
        Test-LoamsCmsFile -OpenSslExe $script:Ossl -Path (Join-Path $b.Path 'encrypted\files.p7m') | Should -BeTrue
        $hits = Get-ChildItem -Path $b.Path -Recurse -File | Where-Object { [IO.File]::ReadAllText($_.FullName).Contains('S1b-test-db-pass') }
        @($hits).Count | Should -Be 0
        $dest = Join-Path $TestDrive ('restore-' + [guid]::NewGuid().ToString('N'))
        Set-LoamsMode -Mode Report
        Expand-LoamsEncryptedArchive -OpenSslExe $script:Ossl -InFile (Join-Path $b.Path 'encrypted\files.p7m') -RecipientCert $script:Pki.Cert -RecoveryKey $script:Pki.Key -Destination $dest | Out-Null
        Get-Content -Raw (Join-Path $dest 'loams_api\config.php') | Should -Match 'S1b-test-db-pass'
    }
    It 'keeps student file names out of every plaintext file of the set' {
        Use-BackupMocks
        $b = Invoke-Backup
        Get-PlainTextOfSet -Path $b.Path | Should -Not -Match 'STUDENT-TEST-0001'
    }
    It 'throws when the database dump fails' {
        Use-BackupMocks -DumpFails
        { Invoke-Backup } | Should -Throw -ExpectedMessage '*mysqldump failed*'
    }
    It 'refuses in Report mode' {
        Set-LoamsMode -Mode Report
        Use-BackupMocks
        { Invoke-Backup } | Should -Throw -ExpectedMessage 'LOAMS-READONLY*'
    }
    It 'detects tampered ciphertext' {
        Use-BackupMocks
        $b = Invoke-Backup
        Add-Content -Path (Join-Path $b.Path 'encrypted\files.p7m') -Value 'x'
        $v = Test-LoamsBackupSet -Path $b.Path -OpenSslExe $script:Ossl
        $v.Verified | Should -BeFalse
        ($v.Problems -join ' ') | Should -Match 'files.p7m'
    }
}

Describe 'Restore (generated script and Invoke-LoamsHostRestore)' {
    BeforeAll {
        Set-LoamsMode -Mode Converge
        Use-BackupMocks
        $script:Set = (Invoke-Backup).Path
        Set-LoamsMode -Mode Report
        function Copy-Set { $c = Join-Path $TestDrive ('copy-' + [guid]::NewGuid().ToString('N')); Copy-Item -LiteralPath $script:Set -Destination $c -Recurse; return $c }
        function Run-RestoreScript { param([string] $SetPath)
            $out = & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File (Join-Path $SetPath 'Restore-LoamsHostBackup.ps1') 2>&1
            [pscustomobject]@{ ExitCode = $LASTEXITCODE; Text = ($out | Out-String) }
        }
        function Use-RestoreMocks { param([switch] $AccountFails)
            $global:LoamsTAccountFails = [bool]$AccountFails
            Mock -ModuleName LoamsHost Test-LoamsElevated { $true }
            Mock -ModuleName LoamsHost Stop-LoamsWindowsService { }
            Mock -ModuleName LoamsHost Start-LoamsWindowsService { }
            Mock -ModuleName LoamsHost Set-LoamsServiceAccount { if ($global:LoamsTAccountFails) { throw 'sc.exe config failed' } }
            Mock -ModuleName LoamsHost Set-LoamsServiceStartMode { }
            Mock -ModuleName LoamsHost Unregister-LoamsEventSource { }
            Mock -ModuleName LoamsHost Get-LoamsEventSourceState { [pscustomobject]@{ Source = 'LOAMS-Transport'; Registered = $true; LogName = 'Application'; ApplicationLogSddl = ''; ServiceCanWrite = $null } }
        }
    }
    AfterAll { Remove-Variable -Name LoamsTAccountFails -Scope Global -ErrorAction SilentlyContinue }
    It 'exits 4 with RECOVERY REQUIRED when a rollback file is missing (generated script, child process)' {
        $c = Copy-Set
        Remove-Item -LiteralPath (Join-Path $c 'rollback\service-apache.json')
        $r = Run-RestoreScript -SetPath $c
        $r.ExitCode | Should -Be 4
        $r.Text | Should -Match "RECOVERY REQUIRED - step 'verify backup integrity' failed: missing rollback\\service-apache.json"
    }
    It 'exits 4 with RECOVERY REQUIRED when a rollback file was altered (generated script, child process)' {
        $c = Copy-Set
        Add-Content -LiteralPath (Join-Path $c 'rollback\acl-01.txt') -Value 'tampered'
        $r = Run-RestoreScript -SetPath $c
        $r.ExitCode | Should -Be 4
        $r.Text | Should -Match 'changed since the backup was taken: rollback\\acl-01.txt'
    }
    It 'runs the generated wrapper end to end in a child process: imports the backup''s module copy, restores and exits 0' {
        # Harmless fixture: no services (names that do not exist), no ACLs, event source and task untouched,
        # only filesystem state inside TestDrive - so the real wrapper and module copy run unmocked and unelevated.
        $set = Join-Path $TestDrive 'child-ok-set'; $pd = Join-Path $TestDrive 'child-ok-pd'; $data = Join-Path $TestDrive 'child-ok-data'
        foreach ($d in @((Join-Path $set 'rollback\tools\sub'), (Join-Path $set 'logs'), (Join-Path $pd 'tools'), $data)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
        Set-Content -Path (Join-Path $set 'rollback\tools\sub\keep.txt') -Value 'prior'
        $moduleDir = Join-Path $PSScriptRoot '..\server\LoamsHost'
        Copy-Item -Path (Join-Path $moduleDir '*') -Destination (New-Item -ItemType Directory -Path (Join-Path $set 'rollback\LoamsHost')).FullName -Recurse
        Copy-Item -LiteralPath (Join-Path $moduleDir 'Restore-LoamsHostBackup.template.ps1') -Destination (Join-Path $set 'Restore-LoamsHostBackup.ps1')
        $sha = (Get-FileHash -Algorithm SHA256 (Join-Path $set 'rollback\tools\sub\keep.txt')).Hash.ToLowerInvariant()
        $none = 'LoamsTestNoSvc' + [guid]::NewGuid().ToString('N').Substring(0, 8)
        $state = [ordered]@{ programDataRoot = $pd; eventSource = [ordered]@{ Registered = $true; LogName = 'Application' }
            layout = @([ordered]@{ path = (Join-Path $pd 'server'); exists = $false })
            retention = [ordered]@{ taskName = "LOAMS Test Restore $none"; taskExists = $false; taskHash = 'absent' }
            tools = @([ordered]@{ path = 'sub'; type = 'dir'; sha256 = '' }, [ordered]@{ path = 'sub\keep.txt'; type = 'file'; sha256 = $sha }) }
        [IO.File]::WriteAllText((Join-Path $set 'rollback\loams-state.json'), ($state | ConvertTo-Json -Depth 5))
        $snap = [ordered]@{ Exists = $false; Name = ''; StartName = ''; StartMode = ''; PathName = ''; State = ''; AccountKind = 'None' }
        $manifest = [ordered]@{ backupVersion = 3; computerName = $env:COMPUTERNAME; programDataRoot = $pd; apiRoot = $pd; httpdExe = 'x'; mysqldExe = 'x'; mysqlExe = 'x'; myIni = 'x'; dataDir = $data
            services = [ordered]@{ apache = [ordered]@{ role = 'apache'; name = "${none}a"; situation = 'FreshInstall'; snapshot = $snap }
                                   mariadb = [ordered]@{ role = 'mariadb'; name = "${none}m"; situation = 'FreshInstall'; snapshot = $snap } }
            aclSaves = @(); verifySchemas = @('loams_s1b_verify_child', 'loams_s1b_verify_child_r'); preChangeReport = 'pre.json' }
        [IO.File]::WriteAllText((Join-Path $set 'backup-manifest.json'), ($manifest | ConvertTo-Json -Depth 6))
        Write-LoamsBackupSums -Path $set | Out-Null
        # The "converged" state to undo: a changed file, an added file, an added directory, a created layout folder.
        New-Item -ItemType Directory -Force -Path (Join-Path $pd 'tools\sub'), (Join-Path $pd 'tools\added-dir'), (Join-Path $pd 'server') | Out-Null
        Set-Content -Path (Join-Path $pd 'tools\sub\keep.txt') -Value 'changed'
        Set-Content -Path (Join-Path $pd 'tools\added.txt') -Value 'added by converge'
        $r = Run-RestoreScript -SetPath $set
        $r.ExitCode | Should -Be 0 -Because $r.Text
        $r.Text | Should -Match 'Restore completed'
        Get-Content -LiteralPath (Join-Path $pd 'tools\sub\keep.txt') | Should -Be 'prior'
        Test-Path (Join-Path $pd 'tools\added.txt') | Should -BeFalse
        Test-Path (Join-Path $pd 'tools\added-dir') | Should -BeFalse
        Test-Path (Join-Path $pd 'server') | Should -BeFalse
    }
    It 'restores every captured mutation and reports Restored' {
        Use-RestoreMocks
        Use-TaskStore -Initial @{ 'LOAMS Backup Retention' = '<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"><Actions><Exec><Command>new.exe</Command></Exec></Actions></Task>' }
        New-Item -ItemType Directory -Force -Path (Join-Path $script:Pd 'server\logs') | Out-Null
        $r = Invoke-LoamsHostRestore -BackupPath $script:Set
        $r.Outcome | Should -Be 'Restored'
        Should -Invoke -ModuleName LoamsHost Set-LoamsServiceAccount -Times 1 -Exactly -ParameterFilter { $ServiceName -eq 'Apache2.4' -and $Account -eq 'LocalSystem' }
        Should -Invoke -ModuleName LoamsHost Set-LoamsServiceAccount -Times 1 -Exactly -ParameterFilter { $ServiceName -eq 'mysql' -and $Account -eq 'LocalSystem' }
        Should -Invoke -ModuleName LoamsHost Unregister-LoamsEventSource -Times 1 -Exactly
        $global:LoamsTTasks.ContainsKey('LOAMS Backup Retention') | Should -BeFalse
        Test-Path (Join-Path $script:Pd 'server') | Should -BeFalse
        Get-LoamsMode | Should -Be 'Report'
    }
    It 'reports RecoveryRequired and names the failed step' {
        Use-RestoreMocks -AccountFails
        Use-TaskStore
        $r = Invoke-LoamsHostRestore -BackupPath $script:Set
        $r.Outcome | Should -Be 'RecoveryRequired'
        $r.FailedSteps | Should -Contain 'restore account of Apache2.4'
    }
}

Describe 'Off-host copy' {
    BeforeAll {
        Set-LoamsMode -Mode Converge
        Use-BackupMocks
        $script:Set = (Invoke-Backup).Path
        Set-LoamsMode -Mode Report
    }
    It 'treats a UNC path to this computer as local' {
        Get-LoamsDestinationKind -Path "\\$env:COMPUTERNAME\backups" | Should -Be 'LocalFixed'
        Get-LoamsDestinationKind -Path '\\localhost\c$\x' | Should -Be 'LocalFixed'
    }
    It 'refuses a local fixed disk' {
        Mock -ModuleName LoamsHost Get-LoamsDestinationKind { 'LocalFixed' }
        { Export-LoamsBackupSet -BackupPath $script:Set -Destination (Join-Path $TestDrive 'local') } | Should -Throw -ExpectedMessage '*not an off-host location*'
    }
    It 'copies the set, verifies every hash on the destination and records it' {
        Mock -ModuleName LoamsHost Get-LoamsDestinationKind { 'Removable' }
        $media = Join-Path $TestDrive 'media1'; New-Item -ItemType Directory -Path $media | Out-Null
        $rec = Export-LoamsBackupSet -BackupPath $script:Set -Destination $media -Operator 'Test Operator'
        $rec.allMatch | Should -BeTrue
        $rec.method | Should -Be 'hash-verified-export'
        (Test-LoamsOffHostVerification -BackupPath $script:Set).Verified | Should -BeTrue
    }
    It 'fails when the destination copy differs' {
        Mock -ModuleName LoamsHost Get-LoamsDestinationKind { 'Removable' }
        Mock -ModuleName LoamsHost Copy-LoamsSetTree { Copy-Item -LiteralPath $Source -Destination $Destination -Recurse; Add-Content -Path (Join-Path $Destination 'encrypted\database.p7m') -Value 'corrupt' }
        $media = Join-Path $TestDrive 'media2'; New-Item -ItemType Directory -Path $media | Out-Null
        { Export-LoamsBackupSet -BackupPath $script:Set -Destination $media } | Should -Throw -ExpectedMessage '*verification failed*'
    }
    It 'accepts an operator attestation only with the correct typed SHA-256' {
        $sums = (Get-FileHash -Algorithm SHA256 (Join-Path $script:Set 'SHA256SUMS.json')).Hash.ToLowerInvariant()
        (Set-LoamsOffHostAttestation -BackupPath $script:Set -Operator 'Test Operator' -Location 'institution vault (placeholder)' -TypedSumsSha256 $sums.ToUpperInvariant()).method | Should -Be 'operator-attested'
    }
    It 'rejects an operator attestation with the wrong SHA-256' {
        { Set-LoamsOffHostAttestation -BackupPath $script:Set -Operator 'Test Operator' -Location 'vault' -TypedSumsSha256 ('ab' * 32) } | Should -Throw -ExpectedMessage '*does not match*'
    }
    It 'invalidates the off-host record when the set changes afterwards' {
        Set-Content -Path (Join-Path $script:Set 'SHA256SUMS.json') -Value '{"files":[]}'
        (Test-LoamsOffHostVerification -BackupPath $script:Set).Verified | Should -BeFalse
    }
}

Describe 'Backup retention task and its rollback' {
    BeforeEach { Set-LoamsMode -Mode Converge }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'deletes sets older than 30 days and always keeps the newest' {
        $root = Join-Path $TestDrive 'retention'
        foreach ($n in @('host-20260801T000000Z', 'host-20260920T000000Z', 'host-20261001T000000Z')) { New-Item -ItemType Directory -Force -Path (Join-Path $root $n) | Out-Null }
        $deleted = Invoke-LoamsBackupRetention -BackupRoot $root -RetentionDays 30 -NowUtc ([DateTime]::new(2026, 10, 5, 0, 0, 0, [DateTimeKind]::Utc))
        ($deleted -join ',') | Should -Be 'host-20260801T000000Z'
        Test-Path (Join-Path $root 'host-20261001T000000Z') | Should -BeTrue
        Test-Path (Join-Path $root 'host-20260920T000000Z') | Should -BeTrue
    }
    It 'registers a daily SYSTEM task that runs the deployed retention script' {
        Use-TaskStore
        $r = Register-LoamsBackupRetentionTask -ProgramDataRoot (Join-Path $TestDrive 'pdr')
        Test-Path $r.Script | Should -BeTrue
        $xml = $global:LoamsTTasks['LOAMS Backup Retention']
        $xml | Should -Match '<UserId>S-1-5-18</UserId>'
        $xml | Should -Match '<DaysInterval>1</DaysInterval>'
        $xml | Should -Match ([regex]::Escape($r.Script))
    }
    It 'restores a pre-existing task and tools exactly after convergence overwrote them' {
        $pd = Join-Path $TestDrive 'pd-existing'
        New-Item -ItemType Directory -Force -Path (Join-Path $pd 'tools\LoamsHost') | Out-Null
        Set-Content -Path (Join-Path $pd 'tools\Invoke-LoamsBackupRetention.ps1') -Value 'old-script'
        Set-Content -Path (Join-Path $pd 'tools\LoamsHost\LoamsHost.psm1') -Value 'old-module'
        Use-TaskStore -Initial @{ 'LOAMS Backup Retention' = $script:OldXml }
        Mock -ModuleName LoamsHost Get-LoamsEventSourceState { [pscustomobject]@{ Registered = $false; LogName = '' } }
        $bk = Join-Path $TestDrive 'bk-existing'
        $prior = Save-LoamsPriorHostState -BackupDir $bk -ProgramDataRoot $pd
        Register-LoamsBackupRetentionTask -ProgramDataRoot $pd | Out-Null
        $global:LoamsTTasks['LOAMS Backup Retention'] | Should -Not -Be $script:OldXml
        Restore-LoamsRetentionTaskState -ProgramDataRoot $pd -BackupPath $bk -Prior $prior
        $global:LoamsTTasks['LOAMS Backup Retention'] | Should -Be $script:OldXml
        Get-Content (Join-Path $pd 'tools\Invoke-LoamsBackupRetention.ps1') | Should -Be 'old-script'
        Get-Content (Join-Path $pd 'tools\LoamsHost\LoamsHost.psm1') | Should -Be 'old-module'
        Test-Path (Join-Path $pd 'tools\LoamsHost\LoamsHost.Common.ps1') | Should -BeFalse
    }
    It 'restores a pre-existing EMPTY tools folder exactly, pruning directories convergence created' {
        $pd = Join-Path $TestDrive 'pd-empty-tools'
        New-Item -ItemType Directory -Force -Path (Join-Path $pd 'tools') | Out-Null
        Use-TaskStore
        Mock -ModuleName LoamsHost Get-LoamsEventSourceState { [pscustomobject]@{ Registered = $false; LogName = '' } }
        $bk = Join-Path $TestDrive 'bk-empty-tools'
        $prior = Save-LoamsPriorHostState -BackupDir $bk -ProgramDataRoot $pd
        @($prior.tools).Count | Should -Be 0
        Register-LoamsBackupRetentionTask -ProgramDataRoot $pd | Out-Null
        Test-Path (Join-Path $pd 'tools\LoamsHost') | Should -BeTrue
        Restore-LoamsRetentionTaskState -ProgramDataRoot $pd -BackupPath $bk -Prior $prior
        Test-Path (Join-Path $pd 'tools') | Should -BeTrue
        @(Get-ChildItem -LiteralPath (Join-Path $pd 'tools') -Force).Count | Should -Be 0
        $global:LoamsTTasks.ContainsKey('LOAMS Backup Retention') | Should -BeFalse
    }
    It 'surfaces a rollback failure instead of suppressing it' {
        $pd = Join-Path $TestDrive 'pd-fresh'
        Use-TaskStore -RemoveFails
        Mock -ModuleName LoamsHost Get-LoamsEventSourceState { [pscustomobject]@{ Registered = $false; LogName = '' } }
        $bk = Join-Path $TestDrive 'bk-fresh'
        $prior = Save-LoamsPriorHostState -BackupDir $bk -ProgramDataRoot $pd
        Register-LoamsBackupRetentionTask -ProgramDataRoot $pd | Out-Null
        { Restore-LoamsRetentionTaskState -ProgramDataRoot $pd -BackupPath $bk -Prior $prior } | Should -Throw -ExpectedMessage '*Access is denied*'
    }
}
```

- [ ] **Step 3: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Backup.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `New-LoamsBackupSet` not recognised.

- [ ] **Step 4: Implement `deploy/server/LoamsHost/LoamsHost.Backup.ps1`**

```powershell
# Encrypted, admin-only backup set taken before any host change, plus prior-state capture and retention.

$script:LoamsRegExe = Join-Path $env:SystemRoot 'System32\reg.exe'
$script:LoamsSumsExclude = @('SHA256SUMS.json', 'offhost-verification.json', 'recovery-required.json')

function Set-LoamsAdminOnlyAcl {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [switch] $Directory)
    Assert-LoamsMutationAllowed -Action "protect $Path"
    $inh = ''
    if ($Directory) { $inh = '(OI)(CI)' }
    Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList @($Path, '/inheritance:r', '/grant:r', "*S-1-5-32-544:${inh}F", "*S-1-5-18:${inh}F") | Out-Null
}

function Get-LoamsBackupItems {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string] $ApiRoot, [Parameter(Mandatory)][string] $MyIni, [Parameter(Mandatory)][string] $ProgramDataRoot)
    $items = @()
    $tree = {
        param([string] $base, [string] $prefix, [string[]] $exclude)
        if (-not (Test-Path -LiteralPath $base)) { return }
        $ex = @($exclude | ForEach-Object { (Join-Path $base $_).TrimEnd('\') + '\' })
        foreach ($f in (Get-ChildItem -LiteralPath $base -Recurse -File -Force)) {
            $full = $f.FullName
            if ($ex | Where-Object { $full.StartsWith($_, [StringComparison]::OrdinalIgnoreCase) }) { continue }
            @{ Source = $full; Name = $prefix + '/' + ($full.Substring($base.TrimEnd('\').Length + 1) -replace '\\', '/') }
        }
    }
    $items += @(& $tree (Join-Path $XamppRoot 'apache\conf') 'apache-conf' @())
    $items += @{ Source = (Join-Path $XamppRoot 'php\php.ini'); Name = 'php/php.ini' }
    if (Test-Path -LiteralPath $MyIni) { $items += @{ Source = $MyIni; Name = 'mariadb/my.ini' } }
    $items += @(& $tree $ApiRoot 'loams_api' @('logs'))
    $items += @(& $tree (Join-Path $ProgramDataRoot 'server') 'programdata-server' @('logs'))
    return , $items
}

function Write-LoamsBackupSums {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    $root = (Resolve-Path -LiteralPath $Path).ProviderPath.TrimEnd('\') + '\'
    $files = foreach ($f in (Get-ChildItem -LiteralPath $root -Recurse -File -Force | Sort-Object FullName)) {
        $rel = $f.FullName.Substring($root.Length)
        if ($rel -like 'logs\*' -or $script:LoamsSumsExclude -contains $rel) { continue }
        [ordered]@{ path = $rel; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $f.FullName).Hash.ToLowerInvariant() }
    }
    $sumsPath = Join-Path $root 'SHA256SUMS.json'
    [IO.File]::WriteAllText($sumsPath, ([ordered]@{ algorithm = 'SHA-256'; files = @($files) } | ConvertTo-Json -Depth 4), (New-Object System.Text.UTF8Encoding($false)))
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $sumsPath).Hash.ToLowerInvariant()
}

function Get-LoamsRetentionTaskState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot, [string] $TaskName = $script:LoamsRetentionTaskName)
    $xml = Get-LoamsScheduledTaskXml -TaskName $TaskName
    return [pscustomobject]@{
        taskName = $TaskName; taskExists = ($null -ne $xml); taskHash = (Get-LoamsTaskDefinitionHash -Xml $xml); taskXml = $xml
        tools = @(Get-LoamsToolsState -ProgramDataRoot $ProgramDataRoot)
    }
}

function Save-LoamsPriorHostState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupDir, [Parameter(Mandatory)][string] $ProgramDataRoot)
    Assert-LoamsMutationAllowed -Action 'capture prior host state'
    $rb = Join-Path $BackupDir 'rollback'
    New-Item -ItemType Directory -Force -Path $rb | Out-Null
    $es = Get-LoamsEventSourceState
    $ret = Get-LoamsRetentionTaskState -ProgramDataRoot $ProgramDataRoot
    if ($ret.taskExists) { [IO.File]::WriteAllText((Join-Path $rb 'retention-task.xml'), $ret.taskXml, (New-Object System.Text.UTF8Encoding($false))) }
    foreach ($t in @($ret.tools | Where-Object { $_.type -eq 'file' })) {
        $dest = Join-Path $rb ('tools\' + $t.path)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        Copy-Item -LiteralPath (Join-Path (Join-Path $ProgramDataRoot 'tools') $t.path) -Destination $dest
    }
    $state = [ordered]@{
        programDataRoot = $ProgramDataRoot
        eventSource = [ordered]@{ Registered = [bool]$es.Registered; LogName = [string]$es.LogName }
        layout = @(Get-LoamsLayoutState -ProgramDataRoot $ProgramDataRoot)
        retention = [ordered]@{ taskName = $ret.taskName; taskExists = $ret.taskExists; taskHash = $ret.taskHash }
        tools = @($ret.tools)
    }
    [IO.File]::WriteAllText((Join-Path $rb 'loams-state.json'), ($state | ConvertTo-Json -Depth 5), (New-Object System.Text.UTF8Encoding($false)))
    return (Get-Content -Raw -LiteralPath (Join-Path $rb 'loams-state.json') | ConvertFrom-Json)
}

function New-LoamsBackupSet {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $Layout, [Parameter(Mandatory)] $MariaDbLayout,
        [Parameter(Mandatory)][string] $ApacheServiceName, [Parameter(Mandatory)][string] $MariaDbServiceName,
        [Parameter(Mandatory)][string] $ApacheSituation, [Parameter(Mandatory)][string] $MariaDbSituation,
        [Parameter(Mandatory)][string] $BackupRoot, [Parameter(Mandatory)][string] $ProgramDataRoot,
        [Parameter(Mandatory)] $ConnectionInfo, [Parameter(Mandatory)][object[]] $AclProfile,
        [Parameter(Mandatory)][string] $OpenSslExe, [Parameter(Mandatory)] $Recipient, [string] $ReportPath = ''
    )
    Assert-LoamsMutationAllowed -Action 'create backup set'
    if (-not (Test-Path -LiteralPath $BackupRoot)) { New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null }
    $dir = Join-Path $BackupRoot ('host-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
    New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
    Set-LoamsAdminOnlyAcl -Path $dir -Directory
    foreach ($sub in @('rollback', 'encrypted', 'logs')) { New-Item -ItemType Directory -Path (Join-Path $dir $sub) | Out-Null }
    Set-LoamsLogPath -Path (Join-Path $dir 'logs\converge.log')
    Write-LoamsLog -Message "Backup set: $dir (recipient $($Recipient.Sha256))"

    $services = [ordered]@{}
    foreach ($svc in @(@{ role = 'apache'; name = $ApacheServiceName; situation = $ApacheSituation }, @{ role = 'mariadb'; name = $MariaDbServiceName; situation = $MariaDbSituation })) {
        $snap = Get-LoamsServiceSnapshot -ServiceName $svc.name
        [IO.File]::WriteAllText((Join-Path $dir "rollback\service-$($svc.role).json"), ($snap | ConvertTo-Json))
        if ($snap.Exists) {
            $qc = Invoke-LoamsExternal -FilePath $script:LoamsSc -ArgumentList @('qc', $svc.name)
            [IO.File]::WriteAllText((Join-Path $dir "rollback\service-$($svc.role)-qc.txt"), [string]$qc.StdOut)
            Invoke-LoamsExternal -FilePath $script:LoamsRegExe -ArgumentList @('export', "HKLM\SYSTEM\CurrentControlSet\Services\$($svc.name)", (Join-Path $dir "rollback\service-$($svc.role).reg"), '/y') | Out-Null
        }
        $services[$svc.role] = [ordered]@{ role = $svc.role; name = $svc.name; situation = $svc.situation; snapshot = $snap }
    }

    $aclSaves = @(); $i = 0
    foreach ($path in @($AclProfile | Where-Object { $_.Mode -ne 'VerifyOnly' } | ForEach-Object { $_.Path } | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $i++
        $file = 'rollback\acl-{0:D2}.txt' -f $i
        # No /t: only the object itself; children's inherited ACEs follow it on restore, and no file names are recorded.
        Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList @($path, '/save', (Join-Path $dir $file), '/c') | Out-Null
        $aclSaves += [pscustomobject]@{ path = $path; parent = (Split-Path -Parent $path); file = $file }
    }
    $prior = Save-LoamsPriorHostState -BackupDir $dir -ProgramDataRoot $ProgramDataRoot
    # Run-specific MariaDB scratch schema names (mutation inventory row 16), derived from this set's id.
    $runId = (Split-Path -Leaf $dir).Substring(5).ToLowerInvariant()
    $verifySchemas = @("loams_s1b_verify_$runId", "loams_s1b_verify_${runId}_r")
    Copy-Item -Path (Join-Path $PSScriptRoot '*') -Destination (New-Item -ItemType Directory -Path (Join-Path $dir 'rollback\LoamsHost')).FullName -Recurse

    $dump = Invoke-LoamsEncryptedDatabaseDump -MysqldumpExe $MariaDbLayout.MysqldumpExe -ConnectionInfo $ConnectionInfo -OpenSslExe $OpenSslExe `
        -RecipientCert $Recipient.Path -OutFile (Join-Path $dir 'encrypted\database.p7m') -WorkDirectory (Join-Path $dir 'logs')
    $items = Get-LoamsBackupItems -XamppRoot $Layout.Root -ApiRoot $Layout.ApiRoot -MyIni $MariaDbLayout.MyIni -ProgramDataRoot $ProgramDataRoot
    $box = @{ count = 0 }
    $files = Invoke-LoamsEncryptStream -OpenSslExe $OpenSslExe -RecipientCert $Recipient.Path -OutFile (Join-Path $dir 'encrypted\files.p7m') -Writer {
        param($sink) $box.count = Write-LoamsBackupArchive -Stream $sink -Items $items }

    $restore = Join-Path $dir 'Restore-LoamsHostBackup.ps1'
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Restore-LoamsHostBackup.template.ps1') -Destination $restore
    $manifest = [ordered]@{
        backupVersion = 3; createdUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'); computerName = $env:COMPUTERNAME
        xamppRoot = $Layout.Root; programDataRoot = $ProgramDataRoot; httpdExe = $Layout.HttpdExe; mysqldExe = $MariaDbLayout.MysqldExe; myIni = $MariaDbLayout.MyIni
        services = $services; aclSaves = $aclSaves
        encrypted = @(
            [ordered]@{ file = 'encrypted\database.p7m'; kind = 'database'; database = $ConnectionInfo.Database; plainSha256 = $dump.PlainSha256; cipherSha256 = $dump.CipherSha256 },
            [ordered]@{ file = 'encrypted\files.p7m'; kind = 'files'; entryCount = $box.count; plainSha256 = $files.PlainSha256; cipherSha256 = $files.CipherSha256 })
        recipientCertSha256 = $Recipient.Sha256
        opensslSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $OpenSslExe).Hash.ToLowerInvariant()
        preChangeReport = $ReportPath; retentionDays = 30
        apiRoot = $Layout.ApiRoot; mysqlExe = $MariaDbLayout.MysqlExe; dataDir = $MariaDbLayout.DataDir; verifySchemas = $verifySchemas
    }
    $manifestPath = Join-Path $dir 'backup-manifest.json'
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 6), (New-Object System.Text.UTF8Encoding($false)))
    $sumsSha = Write-LoamsBackupSums -Path $dir

    $v = Test-LoamsBackupSet -Path $dir -OpenSslExe $OpenSslExe
    if (-not $v.Verified) { throw "Backup at '$dir' failed verification: $($v.Problems -join '; ')" }
    Write-LoamsLog -Message "Backup verified: $($box.count) archived files, $(@($aclSaves).Count) ACL saves, SHA256SUMS $sumsSha"
    return [pscustomobject]@{ Path = $dir; Verified = $true; Services = $services; AclSaves = $aclSaves; PriorState = $prior; VerifySchemas = $verifySchemas; RestoreScript = $restore; ManifestPath = $manifestPath; SumsSha256 = $sumsSha }
}

function Test-LoamsBackupSet {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path, [Parameter(Mandatory)][string] $OpenSslExe)
    $problems = @()
    $sumsPath = Join-Path $Path 'SHA256SUMS.json'
    $mp = Join-Path $Path 'backup-manifest.json'
    if (-not (Test-Path -LiteralPath $sumsPath) -or -not (Test-Path -LiteralPath $mp)) { return [pscustomobject]@{ Verified = $false; Problems = @('SHA256SUMS.json or backup-manifest.json missing') } }
    $sums = Get-Content -Raw -LiteralPath $sumsPath | ConvertFrom-Json
    foreach ($e in @($sums.files)) {
        $p = Join-Path $Path $e.path
        if (-not (Test-Path -LiteralPath $p)) { $problems += "missing $($e.path)"; continue }
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLowerInvariant() -ne $e.sha256) { $problems += "hash mismatch $($e.path)" }
    }
    $m = Get-Content -Raw -LiteralPath $mp | ConvertFrom-Json
    foreach ($e in @($m.encrypted)) {
        $p = Join-Path $Path $e.file
        if (-not (Test-Path -LiteralPath $p)) { $problems += "missing $($e.file)"; continue }
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLowerInvariant() -ne $e.cipherSha256) { $problems += "ciphertext hash mismatch $($e.file)" }
        if (-not (Test-LoamsCmsFile -OpenSslExe $OpenSslExe -Path $p)) { $problems += "not a CMS EnvelopedData file: $($e.file)" }
    }
    foreach ($a in @($m.aclSaves)) {
        $p = Join-Path $Path $a.file
        if (-not (Test-Path -LiteralPath $p) -or (Get-Item -LiteralPath $p).Length -eq 0) { $problems += "ACL save empty or missing: $($a.file)" }
    }
    foreach ($role in @('apache', 'mariadb')) {
        if ($m.services.$role.snapshot.Exists) {
            foreach ($n in @("rollback\service-$role.reg", "rollback\service-$role-qc.txt")) { if (-not (Test-Path -LiteralPath (Join-Path $Path $n))) { $problems += "missing $n" } }
        }
    }
    foreach ($n in @('rollback\loams-state.json', 'rollback\LoamsHost\LoamsHost.psm1', 'Restore-LoamsHostBackup.ps1')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $n))) { $problems += "missing $n" }
    }
    return [pscustomobject]@{ Verified = ($problems.Count -eq 0); Problems = $problems }
}

function Restore-LoamsAclSave {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupPath, [Parameter(Mandatory)] $AclSave)
    Assert-LoamsMutationAllowed -Action "restore ACL $($AclSave.path)"
    Invoke-LoamsExternal -FilePath $script:LoamsIcacls -ArgumentList @($AclSave.parent, '/restore', (Join-Path $BackupPath $AclSave.file), '/c') | Out-Null
    Write-LoamsLog -Message "ACL restored: $($AclSave.path)"
}

function Invoke-LoamsBackupRetention {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupRoot, [int] $RetentionDays = 30, [datetime] $NowUtc = [DateTime]::UtcNow)
    Assert-LoamsMutationAllowed -Action 'backup retention'
    $sets = @(Get-ChildItem -LiteralPath $BackupRoot -Directory -Filter 'host-*' -ErrorAction SilentlyContinue | ForEach-Object {
        $created = [DateTime]::MinValue
        if ([DateTime]::TryParseExact($_.Name.Substring(5), "yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]'AssumeUniversal,AdjustToUniversal', [ref]$created)) {
            [pscustomobject]@{ Name = $_.Name; Path = $_.FullName; Created = $created }
        }
    } | Sort-Object Created -Descending)
    $deleted = @()
    foreach ($s in ($sets | Select-Object -Skip 1)) {
        if (($NowUtc - $s.Created).TotalDays -gt $RetentionDays) {
            Remove-Item -LiteralPath $s.Path -Recurse -Force
            $deleted += $s.Name
        }
    }
    Write-LoamsLog -Message ("Backup retention ({0} days): deleted {1}; kept {2}" -f $RetentionDays, (@($deleted) -join ', '), (@($sets).Count - @($deleted).Count))
    return , $deleted
}

function New-LoamsRetentionTaskXml {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ScriptPath)
    $ps = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $taskArgs = [Security.SecurityElement]::Escape("-NoProfile -ExecutionPolicy Bypass -File `"$ScriptPath`"")
    return @"
<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo><Description>Deletes LOAMS host backup sets older than 30 days (keeps the newest).</Description></RegistrationInfo>
  <Triggers><CalendarTrigger><StartBoundary>2026-01-01T03:30:00</StartBoundary><Enabled>true</Enabled><ScheduleByDay><DaysInterval>1</DaysInterval></ScheduleByDay></CalendarTrigger></Triggers>
  <Principals><Principal id="Author"><UserId>S-1-5-18</UserId><RunLevel>HighestAvailable</RunLevel></Principal></Principals>
  <Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><ExecutionTimeLimit>PT1H</ExecutionTimeLimit><Enabled>true</Enabled></Settings>
  <Actions Context="Author"><Exec><Command>$([Security.SecurityElement]::Escape($ps))</Command><Arguments>$taskArgs</Arguments></Exec></Actions>
</Task>
"@
}

function Register-LoamsScheduledTaskXml {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $TaskName, [Parameter(Mandatory)][string] $Xml)
    Assert-LoamsMutationAllowed -Action "register scheduled task $TaskName"
    Register-ScheduledTask -TaskName $TaskName -Xml $Xml -Force -ErrorAction Stop | Out-Null
}

function Remove-LoamsScheduledTask {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $TaskName)
    Assert-LoamsMutationAllowed -Action "remove scheduled task $TaskName"
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction Stop
}

function Register-LoamsBackupRetentionTask {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot, [string] $ModuleSource = $PSScriptRoot, [string] $TaskName = $script:LoamsRetentionTaskName)
    Assert-LoamsMutationAllowed -Action "deploy retention task $TaskName"
    $tools = Join-Path $ProgramDataRoot 'tools'
    $dest = Join-Path $tools 'LoamsHost'
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    Copy-Item -Path (Join-Path $ModuleSource '*') -Destination $dest -Recurse -Force
    $script = Join-Path $tools 'Invoke-LoamsBackupRetention.ps1'
    $body = @(
        '#Requires -Version 5.1',
        '$ErrorActionPreference = ''Stop''',
        'Import-Module (Join-Path $PSScriptRoot ''LoamsHost\LoamsHost.psm1'') -Force',
        'Set-LoamsLogPath -Path (Join-Path $PSScriptRoot ''backup-retention.log'')',
        'Set-LoamsMode -Mode Converge',
        'Invoke-LoamsBackupRetention -BackupRoot (Join-Path (Split-Path -Parent $PSScriptRoot) ''backups'') -RetentionDays 30 | Out-Null'
    ) -join "`r`n"
    [IO.File]::WriteAllText($script, $body, (New-Object System.Text.UTF8Encoding($true)))
    Register-LoamsScheduledTaskXml -TaskName $TaskName -Xml (New-LoamsRetentionTaskXml -ScriptPath $script)
    Write-LoamsLog -Message "Registered scheduled task '$TaskName' (SYSTEM, daily 03:30)"
    return [pscustomobject]@{ TaskName = $TaskName; Script = $script }
}

function Restore-LoamsRetentionTaskState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot, [Parameter(Mandatory)][string] $BackupPath, [Parameter(Mandatory)] $Prior)
    Assert-LoamsMutationAllowed -Action 'restore retention task and tools'
    $tools = Join-Path $ProgramDataRoot 'tools'
    $priorTools = @($Prior.tools)
    $priorKeys = @($priorTools | ForEach-Object { "$($_.type)|$($_.path)" })
    $now = @(Get-LoamsToolsState -ProgramDataRoot $ProgramDataRoot)
    # 1. files added by convergence, 2. directories added by convergence (deepest first, now empty) - exact tree restore.
    foreach ($e in @($now | Where-Object { $_.type -eq 'file' })) {
        if ($priorKeys -notcontains "file|$($e.path)") { Remove-Item -LiteralPath (Join-Path $tools $e.path) -Force -ErrorAction Stop }
    }
    foreach ($e in @($now | Where-Object { $_.type -eq 'dir' } | Sort-Object { $_.path.Length } -Descending)) {
        if ($priorKeys -notcontains "dir|$($e.path)") { [IO.Directory]::Delete((Join-Path $tools $e.path)) }
    }
    foreach ($e in @($priorTools | Where-Object { $_.type -eq 'dir' })) { New-Item -ItemType Directory -Force -Path (Join-Path $tools $e.path) | Out-Null }
    foreach ($e in @($priorTools | Where-Object { $_.type -eq 'file' })) {
        $target = Join-Path $tools $e.path
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath (Join-Path $BackupPath ('rollback\tools\' + $e.path)) -Destination $target -Force -ErrorAction Stop
    }
    $name = $Prior.retention.taskName
    if ($Prior.retention.taskExists) {
        Register-LoamsScheduledTaskXml -TaskName $name -Xml ([IO.File]::ReadAllText((Join-Path $BackupPath 'rollback\retention-task.xml')))
    } elseif ($null -ne (Get-LoamsScheduledTaskXml -TaskName $name)) {
        Remove-LoamsScheduledTask -TaskName $name
    }
    $nowState = Get-LoamsRetentionTaskState -ProgramDataRoot $ProgramDataRoot -TaskName $name
    $want = (@($priorTools | ForEach-Object { "$($_.type)|$($_.path)=$($_.sha256)" }) | Sort-Object) -join ';'
    $have = (@($nowState.tools | ForEach-Object { "$($_.type)|$($_.path)=$($_.sha256)" }) | Sort-Object) -join ';'
    if ($nowState.taskHash -ne $Prior.retention.taskHash -or $want -ne $have) {
        throw "retention task or tools not restored exactly (task $($nowState.taskHash) vs $($Prior.retention.taskHash))"
    }
    Write-LoamsLog -Message 'Retention task and tools restored to their prior state'
}
```

- [ ] **Step 5: Implement `deploy/server/LoamsHost/LoamsHost.Restore.ps1`**

```powershell
# Full restore of every mutation in the inventory (Task 14 table). Used by the generated restore script.

function Set-LoamsServiceStartMode {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ServiceName, [Parameter(Mandatory)][string] $StartMode)
    Assert-LoamsServiceName -ServiceName $ServiceName
    Assert-LoamsMutationAllowed -Action "set start type of $ServiceName"
    $map = @{ Auto = 'auto'; Manual = 'demand'; Disabled = 'disabled' }
    if (-not $map.ContainsKey($StartMode)) { throw "Unknown start mode '$StartMode'." }
    Invoke-LoamsExternal -FilePath $script:LoamsSc -ArgumentList @('config', $ServiceName, 'start=', $map[$StartMode]) | Out-Null
}

function Test-LoamsRestoreNeedsElevation {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Manifest, [Parameter(Mandatory)] $State)
    foreach ($s in @($Manifest.services.apache, $Manifest.services.mariadb)) {
        if ($s.snapshot.Exists -or $s.situation -eq 'ControlPanel' -or (Get-LoamsServiceSnapshot -ServiceName $s.name).Exists) { return $true }
    }
    if (@($Manifest.aclSaves).Count -gt 0) { return $true }
    if (-not $State.eventSource.Registered -and (Get-LoamsEventSourceState).Registered) { return $true }
    if ($State.retention.taskExists -or $null -ne (Get-LoamsScheduledTaskXml -TaskName $State.retention.taskName)) { return $true }
    if (@(Get-LoamsVerifySchemaFolders -DataDir $Manifest.dataDir | Where-Object { @($Manifest.verifySchemas) -contains $_ }).Count -gt 0) { return $true }
    return $false
}

function Invoke-LoamsHostRestore {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupPath)
    $steps = New-Object System.Collections.Generic.List[object]
    $run = { param([string] $name, [scriptblock] $body)
        try { & $body | Out-Null; $steps.Add([pscustomobject]@{ Name = $name; Ok = $true; Detail = '' }); return $true }
        catch { $steps.Add([pscustomobject]@{ Name = $name; Ok = $false; Detail = (Protect-LoamsText -Text $_.Exception.Message) }); return $false } }
    $box = @{}
    $previousMode = Get-LoamsMode
    Set-LoamsMode -Mode Converge
    try {
        $ok = & $run 'read backup manifest' {
            $box.m = Get-Content -Raw -LiteralPath (Join-Path $BackupPath 'backup-manifest.json') -ErrorAction Stop | ConvertFrom-Json
            $box.state = Get-Content -Raw -LiteralPath (Join-Path $BackupPath 'rollback\loams-state.json') -ErrorAction Stop | ConvertFrom-Json
            if ($box.m.computerName -ne $env:COMPUTERNAME) { throw "backup was taken on '$($box.m.computerName)', not '$env:COMPUTERNAME'" }
            if ((Test-LoamsRestoreNeedsElevation -Manifest $box.m -State $box.state) -and -not (Test-LoamsElevated)) { throw 'not elevated: this restore changes services, ACLs, the event log, scheduled tasks or the database; run it as Administrator' }
        }
        if ($ok) {
            $m = $box.m; $state = $box.state
            $svcs = @($m.services.apache, $m.services.mariadb)
            foreach ($s in $svcs) {
                [void](& $run "stop $($s.name)" { $now = Get-LoamsServiceSnapshot -ServiceName $s.name; if ($now.Exists -and $now.State -eq 'Running') { Stop-LoamsWindowsService -ServiceName $s.name -TimeoutSec 120 } })
            }
            foreach ($s in $svcs) {
                if ($s.snapshot.Exists) {
                    [void](& $run "restore account of $($s.name)" { Set-LoamsServiceAccount -ServiceName $s.name -Account $s.snapshot.StartName })
                    [void](& $run "restore start type of $($s.name)" { Set-LoamsServiceStartMode -ServiceName $s.name -StartMode $s.snapshot.StartMode })
                } else {
                    [void](& $run "remove service $($s.name) registered by converge" {
                        if ((Get-LoamsServiceSnapshot -ServiceName $s.name).Exists) {
                            if ($s.role -eq 'apache') { Unregister-LoamsApacheService -HttpdExe $m.httpdExe -ServiceName $s.name }
                            else { Unregister-LoamsMariaDbService -MysqldExe $m.mysqldExe -ServiceName $s.name }
                        } })
                }
            }
            foreach ($a in @($m.aclSaves)) { [void](& $run "restore ACL $($a.path)" { Restore-LoamsAclSave -BackupPath $BackupPath -AclSave $a }) }
            [void](& $run 'restore event source' {
                if (-not $state.eventSource.Registered -and (Get-LoamsEventSourceState).Registered) { Unregister-LoamsEventSource } })
            [void](& $run 'restore retention task and tools' { Restore-LoamsRetentionTaskState -ProgramDataRoot $m.programDataRoot -BackupPath $BackupPath -Prior $state })
            [void](& $run 'remove ProgramData layout created by converge' {
                foreach ($l in @(@($state.layout) | Sort-Object { $_.path.Length } -Descending)) {
                    if (-not $l.exists -and (Test-Path -LiteralPath $l.path)) { Remove-Item -LiteralPath $l.path -Recurse -Force -ErrorAction Stop }
                } })
            foreach ($s in @($m.services.mariadb, $m.services.apache)) {
                [void](& $run "start $($s.name)" {
                    if ($s.snapshot.Exists) { if ($s.snapshot.State -eq 'Running') { Start-LoamsWindowsService -ServiceName $s.name -TimeoutSec 120 } }
                    elseif ($s.situation -eq 'ControlPanel') {
                        if ($s.role -eq 'apache') { Start-LoamsControlPanelHttpd -HttpdExe $m.httpdExe } else { Start-LoamsControlPanelMysqld -MysqldExe $m.mysqldExe -MyIni $m.myIni }
                    } })
            }
            [void](& $run 'drop verification schemas left by converge' {
                $left = @(Get-LoamsVerifySchemaFolders -DataDir $m.dataDir | Where-Object { @($m.verifySchemas) -contains $_ })
                if ($left.Count -gt 0) {
                    $conn = Get-LoamsDbConnectionInfo -ApiRoot $m.apiRoot
                    Remove-LoamsVerifySchemas -MysqlExe $m.mysqlExe -ConnectionInfo $conn -WorkDirectory (Join-Path $BackupPath 'logs') -Schemas $left
                } })
        }
    } finally {
        Set-LoamsMode -Mode $previousMode
    }
    $failed = @($steps | Where-Object { -not $_.Ok } | ForEach-Object { $_.Name })
    $outcome = 'Restored'
    if ($failed.Count -gt 0) { $outcome = 'RecoveryRequired' }
    Write-LoamsLog -Message ("Restore {0}: {1}" -f $outcome, (($steps | ForEach-Object { "$($_.Name)=$(if ($_.Ok) { 'ok' } else { 'FAILED' })" }) -join ', '))
    return [pscustomobject]@{ Outcome = $outcome; Steps = @($steps); FailedSteps = $failed }
}
```

The restore stops both services first, restores accounts/registrations, ACLs, event source, retention task and tools, removes the created layout (never `C:\ProgramData\LOAMS` itself or `backups\`, which hold this backup), then starts MariaDB before Apache. Every action is a named step; nothing is suppressed; any failure ⇒ `RecoveryRequired` (the wrapper exits 4).

- [ ] **Step 6: Implement `deploy/server/LoamsHost/LoamsHost.OffHost.ps1`**

```powershell
# One independently protected copy outside the server (owner change D14). Never the machine being migrated.

function Get-LoamsDestinationKind {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)
    if ($Path -match '^\\\\([^\\]+)\\') {
        $hostPart = $Matches[1].ToLowerInvariant()
        if ($hostPart -in @('localhost', '127.0.0.1', '::1', '.', $env:COMPUTERNAME.ToLowerInvariant())) { return 'LocalFixed' }
        return 'Network'
    }
    $root = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Path))
    if (-not $root) { return 'Unknown' }
    $drive = New-Object System.IO.DriveInfo($root)
    switch ([string]$drive.DriveType) {
        'Network' { return 'Network' }
        'Removable' { return 'Removable' }
        'Fixed' {
            $disk = Get-Partition -DriveLetter $root.Substring(0, 1) -ErrorAction SilentlyContinue | Get-Disk -ErrorAction SilentlyContinue
            if ($disk -and [string]$disk.BusType -eq 'USB') { return 'UsbFixed' }
            return 'LocalFixed'
        }
        default { return 'Unknown' }
    }
}

function Copy-LoamsSetTree {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Source, [Parameter(Mandatory)][string] $Destination)
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse
}

function Write-LoamsOffHostRecord {
    param([string] $BackupPath, [System.Collections.IDictionary] $Record)
    [IO.File]::WriteAllText((Join-Path $BackupPath 'offhost-verification.json'), ($Record | ConvertTo-Json -Depth 3), (New-Object System.Text.UTF8Encoding($false)))
}

function Export-LoamsBackupSet {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupPath, [Parameter(Mandatory)][string] $Destination, [string] $Operator = $env:USERNAME)
    $kind = Get-LoamsDestinationKind -Path $Destination
    if ($kind -in @('LocalFixed', 'Unknown')) { throw "Destination '$Destination' ($kind) is not an off-host location: use removable media, a USB disk or an approved network share." }
    $target = Join-Path $Destination (Split-Path -Leaf $BackupPath)
    if (Test-Path -LiteralPath $target) { throw "Refusing to overwrite $target" }
    $sumsSha = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $BackupPath 'SHA256SUMS.json')).Hash.ToLowerInvariant()
    Copy-LoamsSetTree -Source $BackupPath -Destination $target
    $problems = @()
    $targetSums = Join-Path $target 'SHA256SUMS.json'
    if (-not (Test-Path -LiteralPath $targetSums) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $targetSums).Hash.ToLowerInvariant() -ne $sumsSha) { $problems += 'SHA256SUMS.json differs on the destination' }
    $sums = Get-Content -Raw -LiteralPath (Join-Path $BackupPath 'SHA256SUMS.json') | ConvertFrom-Json
    foreach ($e in @($sums.files)) {
        $p = Join-Path $target $e.path
        if (-not (Test-Path -LiteralPath $p) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLowerInvariant() -ne $e.sha256) { $problems += $e.path }
    }
    if ($problems.Count -gt 0) { throw "Off-host copy verification failed for: $($problems -join ', ')" }
    $record = [ordered]@{
        method = 'hash-verified-export'; destination = $Destination; destinationKind = $kind; verifiedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
        fileCount = @($sums.files).Count; sumsSha256 = $sumsSha; allMatch = $true; operator = $Operator
    }
    Write-LoamsOffHostRecord -BackupPath $BackupPath -Record $record
    Write-LoamsLog -Message "Off-host copy verified at $Destination ($kind), $(@($sums.files).Count) files"
    return [pscustomobject]$record
}

function Set-LoamsOffHostAttestation {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupPath, [Parameter(Mandatory)][string] $Operator, [Parameter(Mandatory)][string] $Location, [Parameter(Mandatory)][string] $TypedSumsSha256)
    $sumsSha = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $BackupPath 'SHA256SUMS.json')).Hash.ToLowerInvariant()
    if (($TypedSumsSha256 -replace '\s', '').ToLowerInvariant() -ne $sumsSha) {
        throw 'The typed SHA-256 does not match this backup set''s SHA256SUMS.json: verify the off-host copy again (runbook Part D4).'
    }
    $record = [ordered]@{
        method = 'operator-attested'; destination = $Location; destinationKind = 'attested'; verifiedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
        fileCount = $null; sumsSha256 = $sumsSha; allMatch = $true; operator = $Operator
    }
    Write-LoamsOffHostRecord -BackupPath $BackupPath -Record $record
    Write-LoamsLog -Message "Off-host copy attested by '$Operator' at '$Location'"
    return [pscustomobject]$record
}

function Test-LoamsOffHostVerification {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BackupPath)
    $p = Join-Path $BackupPath 'offhost-verification.json'
    if (-not (Test-Path -LiteralPath $p)) { return [pscustomobject]@{ Verified = $false; Reason = 'no off-host copy recorded' } }
    $r = Get-Content -Raw -LiteralPath $p | ConvertFrom-Json
    $current = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $BackupPath 'SHA256SUMS.json')).Hash.ToLowerInvariant()
    if (-not $r.allMatch) { return [pscustomobject]@{ Verified = $false; Reason = 'off-host copy did not match' } }
    if ($r.sumsSha256 -ne $current) { return [pscustomobject]@{ Verified = $false; Reason = 'backup set changed after the off-host copy was verified' } }
    return [pscustomobject]@{ Verified = $true; Reason = "$($r.method) at $($r.destination)" }
}
```

- [ ] **Step 7: Run to verify it passes**

Same command as Step 3. Expected: PASS, 27 tests. Set `expected-test-count.txt` to `232` and run the suite runner → `PASSED: 232 tests`.

- [ ] **Step 8: Commit**

Via the project `commit` skill — subjects (two concerns): `feat(deploy): take an encrypted, verified backup set with full prior-state restore and 30-day retention` and `feat(deploy): require a hash-verified off-host copy of every backup set`

---

### Task 15: Checkpoint engine — rollback and RECOVERY REQUIRED

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Checkpoints.ps1`
- Create: `deploy/tests/Checkpoints.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `241`

**Interfaces:**
- Consumes: `Write-LoamsLog`, `Protect-LoamsText` (Task 1).
- Produces:
  - `New-LoamsCheckpoint -Name <string> -Do <scriptblock> -Verify <scriptblock> -Undo <scriptblock> -ManualRestore <string> [-Context <hashtable>]` → `[pscustomobject]`. The engine calls each block as `& $block $Context`, so plan code passes state through `-Context` instead of `GetNewClosure()` (a closure would move the block into a dynamic module and bypass the module scope that Pester mocks).
  - `Invoke-LoamsCheckpointPlan -Checkpoints <object[]> -StatePath <string>` → `[pscustomobject]@{ Outcome='Success'|'RolledBack'|'RecoveryRequired'; FailedCheckpoint=[string]; Failure=[string]; Completed=[string[]]; RollbackFailures=[pscustomobject[]] (Checkpoint, Error); ManualSteps=[string[]] }`. Writes `StatePath` (JSON) before each checkpoint and at the end.

**Semantics (spec §4 "recoverable, not atomic"):**
1. Checkpoints run in order; each runs `Do`, then `Verify` must return exactly `$true`.
2. On the first failure, **no later checkpoint runs**. The failed checkpoint's own `Undo` runs first (it may be partially applied, so every `Undo` is idempotent), then the completed checkpoints' `Undo` in reverse order.
3. Rollback is best-effort through every step; if **any** `Undo` throws, the outcome is `RecoveryRequired`, naming the failed checkpoint, every rollback failure and the `ManualRestore` text of each checkpoint that was being rolled back. `Success` is impossible after a failure.
4. All messages are redacted before they are stored or logged.

- [ ] **Step 1: Write the failing tests `deploy/tests/Checkpoints.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    function New-Cp { param([string] $Name, [switch] $FailDo, [switch] $FailVerify, [switch] $FailUndo)
        # GetNewClosure binds a new scope, so capture the shared call list as a local.
        $n = $Name; $fd = [bool]$FailDo; $fv = [bool]$FailVerify; $fu = [bool]$FailUndo; $calls = $script:Calls
        New-LoamsCheckpoint -Name $n -ManualRestore "manual restore for $n" `
            -Do { $calls.Add("do:$n"); if ($fd) { throw "do failed in $n" } }.GetNewClosure() `
            -Verify { -not $fv }.GetNewClosure() `
            -Undo { $calls.Add("undo:$n"); if ($fu) { throw "undo failed in $n" } }.GetNewClosure()
    }
}

Describe 'Invoke-LoamsCheckpointPlan' {
    BeforeEach {
        $script:Calls = New-Object System.Collections.Generic.List[string]
        $script:State = Join-Path $TestDrive ([guid]::NewGuid().ToString('N') + '.json')
        Clear-LoamsSecrets
    }
    It 'succeeds when every checkpoint passes' {
        $r = Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A), (New-Cp B)) -StatePath $script:State
        $r.Outcome | Should -Be 'Success'
        ($r.Completed -join ',') | Should -Be 'A,B'
        ($script:Calls -join ',') | Should -Be 'do:A,do:B'
    }
    It 'passes the checkpoint Context to Do, Verify and Undo' {
        $ctx = @{ seen = @() }
        $cp = New-LoamsCheckpoint -Name 'Ctx' -ManualRestore 'm' -Context $ctx `
            -Do { param($c) $c.seen += 'do' } -Verify { param($c) $c.seen += 'verify'; $false } -Undo { param($c) $c.seen += 'undo' }
        Invoke-LoamsCheckpointPlan -Checkpoints @($cp) -StatePath $script:State | Out-Null
        ($ctx.seen -join ',') | Should -Be 'do,verify,undo'
    }
    It 'rolls back the failed checkpoint first, then completed ones in reverse' {
        $r = Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A), (New-Cp B), (New-Cp C -FailDo), (New-Cp D)) -StatePath $script:State
        $r.Outcome | Should -Be 'RolledBack'
        $r.FailedCheckpoint | Should -Be 'C'
        ($script:Calls -join ',') | Should -Be 'do:A,do:B,do:C,undo:C,undo:B,undo:A'
    }
    It 'treats a failed verification as a failure' {
        $r = Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A), (New-Cp B -FailVerify)) -StatePath $script:State
        $r.Outcome | Should -Be 'RolledBack'
        $r.FailedCheckpoint | Should -Be 'B'
        $r.Failure | Should -Match 'verification failed'
    }
    It 'never runs checkpoints after a failure' {
        Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A -FailDo), (New-Cp B)) -StatePath $script:State | Out-Null
        $script:Calls | Should -Not -Contain 'do:B'
    }
    It 'yields RECOVERY REQUIRED when an undo fails, and never Success' {
        $r = Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A -FailUndo), (New-Cp B -FailDo)) -StatePath $script:State
        $r.Outcome | Should -Be 'RecoveryRequired'
        $r.FailedCheckpoint | Should -Be 'B'
        $r.RollbackFailures[0].Checkpoint | Should -Be 'A'
        ($script:Calls -join ',') | Should -Be 'do:A,do:B,undo:B,undo:A'
    }
    It 'lists manual restore steps for every checkpoint being rolled back' {
        $r = Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A -FailUndo), (New-Cp B -FailDo)) -StatePath $script:State
        ($r.ManualSteps -join ' | ') | Should -Be 'B: manual restore for B | A: manual restore for A'
    }
    It 'records the final outcome in the state file' {
        Invoke-LoamsCheckpointPlan -Checkpoints @((New-Cp A), (New-Cp B -FailDo)) -StatePath $script:State | Out-Null
        $s = Get-Content -Raw $script:State | ConvertFrom-Json
        $s.outcome | Should -Be 'RolledBack'
        $s.failedCheckpoint | Should -Be 'B'
    }
    It 'redacts secrets in failure messages and the state file' {
        Register-LoamsSecret -Secret 'S1b-test-db-pass'
        $cp = New-LoamsCheckpoint -Name 'X' -ManualRestore 'm' -Do { throw 'boom S1b-test-db-pass' } -Verify { $true } -Undo { }
        $r = Invoke-LoamsCheckpointPlan -Checkpoints @($cp) -StatePath $script:State
        $r.Failure | Should -Not -Match 'S1b-test-db-pass'
        Get-Content -Raw $script:State | Should -Not -Match 'S1b-test-db-pass'
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Checkpoints.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `New-LoamsCheckpoint` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.Checkpoints.ps1`**

```powershell
# Generic recoverable-not-atomic checkpoint engine (spec §4 steps 3-6).

function New-LoamsCheckpoint {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $Name, [Parameter(Mandatory)][scriptblock] $Do,
        [Parameter(Mandatory)][scriptblock] $Verify, [Parameter(Mandatory)][scriptblock] $Undo,
        [Parameter(Mandatory)][string] $ManualRestore, [hashtable] $Context = @{}
    )
    return [pscustomobject]@{ Name = $Name; Do = $Do; Verify = $Verify; Undo = $Undo; ManualRestore = $ManualRestore; Context = $Context }
}

function Save-LoamsCheckpointState {
    param([string] $StatePath, [hashtable] $State)
    $State.updatedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    $json = Protect-LoamsText -Text ($State | ConvertTo-Json -Depth 5)
    [IO.File]::WriteAllText($StatePath, $json, (New-Object System.Text.UTF8Encoding($false)))
}

function Invoke-LoamsCheckpointPlan {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object[]] $Checkpoints, [Parameter(Mandatory)][string] $StatePath)
    $completed = New-Object System.Collections.Generic.List[object]
    $failed = $null
    $failure = ''
    foreach ($cp in $Checkpoints) {
        Save-LoamsCheckpointState -StatePath $StatePath -State @{ outcome = 'Running'; current = $cp.Name; completed = @($completed | ForEach-Object Name) }
        Write-LoamsLog -Message "Checkpoint start: $($cp.Name)"
        try {
            & $cp.Do $cp.Context
            $ok = & $cp.Verify $cp.Context
            if ($ok -ne $true) { throw "verification failed for checkpoint '$($cp.Name)'" }
            $completed.Add($cp)
            Write-LoamsLog -Message "Checkpoint passed: $($cp.Name)"
        } catch {
            $failed = $cp
            $failure = Protect-LoamsText -Text $_.Exception.Message
            Write-LoamsLog -Level Error -Message "Checkpoint FAILED: $($cp.Name): $failure"
            break
        }
    }
    $names = @($completed | ForEach-Object Name)
    if ($null -eq $failed) {
        Save-LoamsCheckpointState -StatePath $StatePath -State @{ outcome = 'Success'; completed = $names }
        return [pscustomobject]@{ Outcome = 'Success'; FailedCheckpoint = ''; Failure = ''; Completed = $names; RollbackFailures = @(); ManualSteps = @() }
    }

    $toUndo = @($failed)
    for ($i = $completed.Count - 1; $i -ge 0; $i--) { $toUndo += $completed[$i] }
    $rollbackFailures = @()
    foreach ($cp in $toUndo) {
        try {
            & $cp.Undo $cp.Context
            Write-LoamsLog -Message "Rolled back: $($cp.Name)"
        } catch {
            $msg = Protect-LoamsText -Text $_.Exception.Message
            $rollbackFailures += [pscustomobject]@{ Checkpoint = $cp.Name; Error = $msg }
            Write-LoamsLog -Level Error -Message "ROLLBACK FAILED: $($cp.Name): $msg"
        }
    }
    $outcome = 'RolledBack'
    $manual = @()
    if ($rollbackFailures.Count -gt 0) {
        $outcome = 'RecoveryRequired'
        $manual = @($toUndo | ForEach-Object { "$($_.Name): $($_.ManualRestore)" })
    }
    Save-LoamsCheckpointState -StatePath $StatePath -State @{
        outcome = $outcome; failedCheckpoint = $failed.Name; failure = $failure; completed = $names
        rollbackFailures = $rollbackFailures; manualSteps = $manual
    }
    return [pscustomobject]@{
        Outcome = $outcome; FailedCheckpoint = $failed.Name; Failure = $failure; Completed = $names
        RollbackFailures = $rollbackFailures; ManualSteps = $manual
    }
}
```

- [ ] **Step 4: Run to verify it passes**

Same command as Step 2. Expected: PASS, 9 tests. Set `expected-test-count.txt` to `241` and run the suite runner → `PASSED: 241 tests`.

- [ ] **Step 5: Commit**

Via the project `commit` skill — subject: `feat(deploy): add checkpoint engine with rollback and RECOVERY REQUIRED`

---

---

### Task 16: Staging authorization, operator confirmation, report acknowledgement + drift re-check, post-change validation suite

**Owner changes covered:** D2 (the staging marker is no longer sole proof: approved machine identity **and** typed per-run confirmation), D9 (the 72-hour acknowledged report is valid only together with a fresh live re-check right before the first change), D13 (validation beyond `get_branding.php`: Apache, PHP execution, DB connectivity, legacy-critical reads — **no attendance writes**).

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Authorization.ps1`
- Create: `deploy/server/LoamsHost/LoamsHost.Validation.ps1`
- Create: `deploy/tests/Authorization.Tests.ps1`
- Create: `deploy/tests/Validation.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `261`

**Interfaces:**
- Consumes: `Test-LoamsAdminOnlyAcl`, `Get-LoamsAclSddl` (Task 6); `Compare-LoamsHostFingerprint` (Task 9); `Get-LoamsServiceSnapshot` (Task 8); `New-LoamsMysqlDefaultsFile`, `Remove-LoamsMysqlDefaultsFile` (Task 13); `Invoke-LoamsMysqlQuery` (Task 13).
- Produces (`LoamsHost.Authorization.ps1`):
  - `Get-LoamsMachineIdentity` → `[pscustomobject]@{ ComputerName; MachineGuid }` (`HKLM:\SOFTWARE\Microsoft\Cryptography\MachineGuid`, readable without elevation).
  - `Read-LoamsOperatorInput -Prompt <string>` → `[string]` (wrapper over `Read-Host`; mocked in tests).
  - `Confirm-LoamsOperatorHost` → `[bool]` — the operator must type this computer's name exactly (case-sensitive); asked on **every** `-Converge` run, staging or production.
  - `Test-LoamsStagingAuthorization -ProgramDataRoot <string>` → `[pscustomobject]@{ Authorized; Reason }` — requires (1) `<ProgramDataRoot>\STAGING-HOST.marker` with an admin-only ACL, **and** (2) `<ProgramDataRoot>\approved-staging-hosts.json` with an admin-only ACL listing this machine's `machineGuid` **and** `computerName`. The file lives only on staging hosts and is never committed (machine identifiers are host-specific).
  - `Test-LoamsAcknowledgedReport -Path <string> -CurrentReport <pscustomobject> [-MaxAgeHours 72] [-ClockSkewMinutes 5]` → `[pscustomobject]@{ Ok; Reason; Report }` — same computer, run elevated, ≤ 72 h old, **not dated more than 5 minutes in the future** (clock-skew tolerance), carries a fingerprint, and `profileHash` equals the current report's.
  - `Test-LoamsPreChangeDrift -Acknowledged <pscustomobject> -Live <pscustomobject>` → `[pscustomobject]@{ NoDrift; Differences=[string[]] }` — compares the acknowledged report's fingerprint with a **live** fingerprint taken immediately before the first change (service configurations and accounts, config-file hashes, loaded-module hashes, XAMPP-side ACLs).
- Produces (`LoamsHost.Validation.ps1`):
  - `Invoke-LoamsHttpGet -Url <string>` → `[pscustomobject]@{ StatusCode; ContentType; Body; Error }` (GET only, no redirects followed).
  - `Assert-LoamsReadOnlyEndpoint -Url <string>` → throws for any attendance or state-changing endpoint (`student_login.php`, `rfid_login.php`, `guest_login.php`, `turnstile.php`, `turnstile_pull.php`, `reset_visits.php`).
  - `Invoke-LoamsPostConvergeValidation -BaseUrl <string> -ApiRoot <string> -ApacheServiceName <string> -MariaDbServiceName <string> -MysqlExe <string> -ConnectionInfo <pscustomobject> -WorkDirectory <string>` → `[pscustomobject]@{ Passed; Checks=[pscustomobject[]] (Name, Ok, Detail) }`. Checks: `apache-running`, `mariadb-running`, `php-smoke` (`get_branding.php` → 200 + JSON), `php-db-read:<endpoint>` for `get_departments.php`, `get_years.php`, `get_courses.php` (each → 200 + JSON; these are the unauthenticated, SELECT-only reads the legacy client uses for registration lists — confirmed by source inspection), `legacy-static-photo` (`uploads/default.jpg` must exist in the deployed web root **and** be served as HTTP 200 `image/*`; it is the legacy client's photo fallback, it is not in the repository, so the deployment must provide it — a missing file fails validation), `db-read-query` (`SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = '<db>'` > 0). No request writes data; legacy admin actions that need the admin key are verified manually on staging (runbook B6).

- [ ] **Step 1: Write the failing tests `deploy/tests/Authorization.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    # Mock bodies run in the module scope, so the SDDL fixtures are literals inside each mock.
    function New-StagingRoot { param([string] $Guid = '11111111-2222-3333-4444-555555555555', [string] $Computer = $env:COMPUTERNAME, [switch] $NoMarker)
        $root = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $root | Out-Null
        if (-not $NoMarker) { Set-Content -Path (Join-Path $root 'STAGING-HOST.marker') -Value 'staging' }
        @{ version = 1; hosts = @(@{ computerName = $Computer; machineGuid = $Guid; approvedBy = 'Test Owner'; approvedOn = '2026-10-05' }) } | ConvertTo-Json -Depth 4 |
            Set-Content -Path (Join-Path $root 'approved-staging-hosts.json')
        return $root
    }
    function New-AckFile { param([string] $Computer = $env:COMPUTERNAME, [string] $Hash = 'h1', [datetime] $When = [DateTime]::UtcNow, [bool] $Elevated = $true)
        $p = Join-Path $TestDrive ([guid]::NewGuid().ToString('N') + '.json')
        @{ computerName = $Computer; profileHash = $Hash; generatedUtc = $When.ToString('yyyy-MM-ddTHH:mm:ssZ'); elevated = $Elevated
           fingerprint = @{ computerName = $Computer; apacheService = 'a'; mariaDbService = 'm'; configHashes = @('C:\x\httpd.conf=aa'); aclSddl = @(); modules = @('c:\x\php8ts.dll=ab') } } |
            ConvertTo-Json -Depth 5 | Set-Content -Path $p
        return $p
    }
    $script:Current = [pscustomobject]@{ computerName = $env:COMPUTERNAME; profileHash = 'h1' }
    function Fp { param([string] $Svc = 'a', [string] $Conf = 'aa', [string] $Mod = 'ab')
        [pscustomobject]@{ computerName = $env:COMPUTERNAME; apacheService = $Svc; mariaDbService = 'm'; configHashes = @("C:\x\httpd.conf=$Conf"); aclSddl = @(); modules = @("c:\x\php8ts.dll=$Mod") }
    }
}

Describe 'Test-LoamsStagingAuthorization' {
    BeforeEach { Mock -ModuleName LoamsHost Get-LoamsMachineIdentity { [pscustomobject]@{ ComputerName = $env:COMPUTERNAME; MachineGuid = '11111111-2222-3333-4444-555555555555' } } }
    It 'authorizes a marked, listed host whose files are admin-only' {
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x1200a9;;;BU)' }
        (Test-LoamsStagingAuthorization -ProgramDataRoot (New-StagingRoot)).Authorized | Should -BeTrue
    }
    It 'refuses a host whose machine GUID is not listed' {
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x1200a9;;;BU)' }
        $r = Test-LoamsStagingAuthorization -ProgramDataRoot (New-StagingRoot -Guid '99999999-0000-0000-0000-000000000000')
        $r.Authorized | Should -BeFalse
        $r.Reason | Should -Match 'not listed'
    }
    It 'refuses when the approved-hosts file is writable by non-administrators' {
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { if ($Path -like '*approved-staging-hosts.json') { 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;0x1301bf;;;BU)' } else { 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x1200a9;;;BU)' } }
        (Test-LoamsStagingAuthorization -ProgramDataRoot (New-StagingRoot)).Reason | Should -Match 'approved-staging-hosts.json is not admin-only'
    }
    It 'refuses without the staging marker' {
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;;FA;;;BA)(A;;FA;;;SY)(A;;0x1200a9;;;BU)' }
        (Test-LoamsStagingAuthorization -ProgramDataRoot (New-StagingRoot -NoMarker)).Reason | Should -Match 'marker'
    }
}

Describe 'Confirm-LoamsOperatorHost' {
    It 'accepts the exact computer name' {
        Mock -ModuleName LoamsHost Read-LoamsOperatorInput { $env:COMPUTERNAME }
        Confirm-LoamsOperatorHost | Should -BeTrue
    }
    It 'rejects anything else' {
        Mock -ModuleName LoamsHost Read-LoamsOperatorInput { 'yes' }
        Confirm-LoamsOperatorHost | Should -BeFalse
    }
}

Describe 'Test-LoamsAcknowledgedReport' {
    It 'accepts a fresh elevated report of this computer with the same profile hash' {
        $r = Test-LoamsAcknowledgedReport -Path (New-AckFile) -CurrentReport $script:Current
        $r.Ok | Should -BeTrue
        $r.Report.fingerprint.apacheService | Should -Be 'a'
    }
    It 'refuses a report from another computer' {
        (Test-LoamsAcknowledgedReport -Path (New-AckFile -Computer 'OTHER-PC') -CurrentReport $script:Current).Reason | Should -Match 'not this computer'
    }
    It 'refuses a report older than 72 hours' {
        (Test-LoamsAcknowledgedReport -Path (New-AckFile -When ([DateTime]::UtcNow.AddDays(-4))) -CurrentReport $script:Current).Reason | Should -Match 'older than 72'
    }
    It 'refuses a report dated in the future beyond the 5-minute clock-skew tolerance' {
        (Test-LoamsAcknowledgedReport -Path (New-AckFile -When ([DateTime]::UtcNow.AddMinutes(30))) -CurrentReport $script:Current).Reason | Should -Match 'future'
    }
    It 'refuses a report that was not run elevated' {
        (Test-LoamsAcknowledgedReport -Path (New-AckFile -Elevated $false) -CurrentReport $script:Current).Reason | Should -Match 'elevated'
    }
}

Describe 'Test-LoamsPreChangeDrift' {
    BeforeAll { $script:Ack = Get-Content -Raw (New-AckFile) | ConvertFrom-Json }
    It 'reports no drift when the live host matches' {
        (Test-LoamsPreChangeDrift -Acknowledged $script:Ack -Live (Fp)).NoDrift | Should -BeTrue
    }
    It 'detects a service configuration or account change' {
        $r = Test-LoamsPreChangeDrift -Acknowledged $script:Ack -Live (Fp -Svc 'b')
        $r.NoDrift | Should -BeFalse
        ($r.Differences -join ' ') | Should -Match 'apacheService'
    }
    It 'detects a changed configuration file' {
        ((Test-LoamsPreChangeDrift -Acknowledged $script:Ack -Live (Fp -Conf 'bb')).Differences -join ' ') | Should -Match 'configHashes: changed'
    }
    It 'detects a changed loaded module' {
        ((Test-LoamsPreChangeDrift -Acknowledged $script:Ack -Live (Fp -Mod 'cd')).Differences -join ' ') | Should -Match 'modules: changed'
    }
}
```

- [ ] **Step 2: Write the failing tests `deploy/tests/Validation.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Conn = [pscustomobject]@{ Host = 'localhost'; User = 'loams_test'; Database = 'wits_test'; Password = (New-Object Security.SecureString); Source = 'credential' }
    function Use-ValidationMocks { param([switch] $DbFails, [switch] $DepartmentsNotJson)
        $global:LoamsTDbFails = [bool]$DbFails; $global:LoamsTBadDept = [bool]$DepartmentsNotJson
        Mock -ModuleName LoamsHost Get-LoamsServiceSnapshot { [pscustomobject]@{ Exists = $true; Name = $ServiceName; State = 'Running' } }
        Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } -ParameterFilter { $FilePath -like '*icacls.exe' }
        Mock -ModuleName LoamsHost Invoke-LoamsMysqlQuery { if ($global:LoamsTDbFails) { throw 'ERROR 2003: cannot connect' }; '12' }
        Mock -ModuleName LoamsHost Invoke-LoamsHttpGet {
            if ($Url -like '*default.jpg') { return [pscustomobject]@{ StatusCode = 200; ContentType = 'image/jpeg'; Body = ''; Error = '' } }
            if ($global:LoamsTBadDept -and $Url -like '*get_departments.php') { return [pscustomobject]@{ StatusCode = 200; ContentType = 'text/html'; Body = '<b>Fatal error</b>'; Error = '' } }
            [pscustomobject]@{ StatusCode = 200; ContentType = 'application/json'; Body = '[]'; Error = '' }
        }
    }
    function Validate {
        Invoke-LoamsPostConvergeValidation -BaseUrl 'http://127.0.0.1/loams_api/' -ApiRoot (Join-Path $script:Root 'htdocs\loams_api') -ApacheServiceName 'Apache2.4' `
            -MariaDbServiceName 'mysql' -MysqlExe 'C:\x\mysql.exe' -ConnectionInfo $script:Conn -WorkDirectory $TestDrive
    }
}
AfterAll { Remove-Variable -Name LoamsTDbFails, LoamsTBadDept -Scope Global -ErrorAction SilentlyContinue }

Describe 'Invoke-LoamsPostConvergeValidation' {
    BeforeEach { Set-LoamsMode -Mode Converge }
    AfterEach { Set-LoamsMode -Mode Report }
    It 'passes when Apache, PHP, the database and the legacy reads all work' {
        Use-ValidationMocks
        $r = Validate
        $r.Passed | Should -BeTrue
        ($r.Checks.Name -join ',') | Should -Be 'apache-running,mariadb-running,php-smoke,php-db-read:get_departments.php,php-db-read:get_years.php,php-db-read:get_courses.php,legacy-static-photo,db-read-query'
    }
    It 'fails when the read-only database query fails' {
        Use-ValidationMocks -DbFails
        $r = Validate
        $r.Passed | Should -BeFalse
        ($r.Checks | Where-Object Name -eq 'db-read-query').Ok | Should -BeFalse
    }
    It 'fails when PHP returns something other than JSON' {
        Use-ValidationMocks -DepartmentsNotJson
        ((Validate).Checks | Where-Object Name -eq 'php-db-read:get_departments.php').Ok | Should -BeFalse
    }
    It 'fails when the legacy photo fallback file is not deployed' {
        Use-ValidationMocks
        $api = Join-Path $TestDrive 'api-nophoto'; New-Item -ItemType Directory -Force -Path (Join-Path $api 'uploads') | Out-Null
        $r = Invoke-LoamsPostConvergeValidation -BaseUrl 'http://127.0.0.1/loams_api/' -ApiRoot $api -ApacheServiceName 'Apache2.4' -MariaDbServiceName 'mysql' -MysqlExe 'C:\x\mysql.exe' -ConnectionInfo $script:Conn -WorkDirectory $TestDrive
        ($r.Checks | Where-Object Name -eq 'legacy-static-photo').Ok | Should -BeFalse
        $r.Passed | Should -BeFalse
    }
    It 'only ever issues GETs to read-only endpoints' {
        Use-ValidationMocks
        Validate | Out-Null
        Should -Invoke -ModuleName LoamsHost Invoke-LoamsHttpGet -Times 0 -ParameterFilter { $Url -match 'student_login|rfid_login|guest_login|turnstile|reset_visits' }
        { Assert-LoamsReadOnlyEndpoint -Url 'http://127.0.0.1/loams_api/rfid_login.php' } | Should -Throw -ExpectedMessage '*attendance*'
    }
}
```

- [ ] **Step 3: Run to verify they fail**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Authorization.Tests.ps1,deploy\tests\Validation.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Test-LoamsStagingAuthorization` / `Invoke-LoamsPostConvergeValidation` not recognised.

- [ ] **Step 4: Implement `deploy/server/LoamsHost/LoamsHost.Authorization.ps1`**

```powershell
# Who may change this host, and is it still the host that was reviewed? (owner changes D2, D9)

function Get-LoamsMachineIdentity {
    [CmdletBinding()] param()
    $guid = (Get-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Cryptography' -Name MachineGuid -ErrorAction Stop).MachineGuid
    return [pscustomobject]@{ ComputerName = $env:COMPUTERNAME; MachineGuid = [string]$guid }
}

function Read-LoamsOperatorInput {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Prompt)
    return (Read-Host -Prompt $Prompt)
}

function Confirm-LoamsOperatorHost {
    [CmdletBinding()] param()
    $typed = Read-LoamsOperatorInput -Prompt "Type this computer's name exactly ($env:COMPUTERNAME) to authorize changes on it"
    return ([string]$typed -ceq $env:COMPUTERNAME)
}

function Test-LoamsStagingAuthorization {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $ProgramDataRoot)
    $deny = { param($why) [pscustomobject]@{ Authorized = $false; Reason = $why } }
    $marker = Join-Path $ProgramDataRoot 'STAGING-HOST.marker'
    if (-not (Test-Path -LiteralPath $marker)) { return (& $deny 'staging marker STAGING-HOST.marker not present') }
    if (-not (Test-LoamsAdminOnlyAcl -Sddl (Get-LoamsAclSddl -Path $marker))) { return (& $deny 'staging marker is not admin-only') }
    $list = Join-Path $ProgramDataRoot 'approved-staging-hosts.json'
    if (-not (Test-Path -LiteralPath $list)) { return (& $deny 'approved-staging-hosts.json not present') }
    if (-not (Test-LoamsAdminOnlyAcl -Sddl (Get-LoamsAclSddl -Path $list))) { return (& $deny 'approved-staging-hosts.json is not admin-only') }
    try { $hosts = @((Get-Content -Raw -LiteralPath $list | ConvertFrom-Json).hosts) } catch { return (& $deny 'approved-staging-hosts.json is not valid JSON') }
    $me = Get-LoamsMachineIdentity
    $match = @($hosts | Where-Object { [string]$_.machineGuid -eq $me.MachineGuid -and [string]$_.computerName -eq $me.ComputerName })
    if ($match.Count -eq 0) { return (& $deny "this machine ($($me.ComputerName)) is not listed in approved-staging-hosts.json") }
    return [pscustomobject]@{ Authorized = $true; Reason = "approved staging host (approved by $($match[0].approvedBy) on $($match[0].approvedOn))" }
}

function Test-LoamsAcknowledgedReport {
    [CmdletBinding()]
    param([AllowEmptyString()][string] $Path, [Parameter(Mandatory)] $CurrentReport, [int] $MaxAgeHours = 72, [int] $ClockSkewMinutes = 5)
    $fail = { param($why) [pscustomobject]@{ Ok = $false; Reason = $why; Report = $null } }
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        return (& $fail "acknowledged report '$Path' not found: run -Report first and review it (the first run on any gate PC is report-only)")
    }
    try { $ack = Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json } catch { return (& $fail "acknowledged report '$Path' is not valid JSON") }
    if ([string]$ack.computerName -ne [string]$CurrentReport.computerName) { return (& $fail "acknowledged report is from '$($ack.computerName)', not this computer") }
    if ($ack.elevated -ne $true) { return (& $fail 'acknowledged report was not run elevated: its loaded-module evidence is incomplete') }
    if ($null -eq $ack.fingerprint) { return (& $fail 'acknowledged report has no host fingerprint (re-run -Report)') }
    $g = $ack.generatedUtc
    if ($g -is [DateTime]) { $when = $g.ToUniversalTime() }
    else { $when = [DateTime]::ParseExact([string]$g, "yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]'AssumeUniversal,AdjustToUniversal') }
    if ($when -gt [DateTime]::UtcNow.AddMinutes($ClockSkewMinutes)) { return (& $fail "acknowledged report is dated in the future (more than $ClockSkewMinutes minutes ahead): check the clocks and run -Report again") }
    if (([DateTime]::UtcNow - $when).TotalHours -gt $MaxAgeHours) { return (& $fail "acknowledged report is older than $MaxAgeHours h: run -Report again and review it") }
    if ([string]$ack.profileHash -ne [string]$CurrentReport.profileHash) { return (& $fail 'host changed since the acknowledged report (profile hash differs): run -Report again and review it') }
    return [pscustomobject]@{ Ok = $true; Reason = ''; Report = $ack }
}

function Test-LoamsPreChangeDrift {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Acknowledged, [Parameter(Mandatory)] $Live)
    $diff = @(Compare-LoamsHostFingerprint -Expected $Acknowledged.fingerprint -Actual $Live)
    return [pscustomobject]@{ NoDrift = ($diff.Count -eq 0); Differences = $diff }
}
```

- [ ] **Step 5: Implement `deploy/server/LoamsHost/LoamsHost.Validation.ps1`**

```powershell
# Post-convergence validation: read-only checks only (owner change D13). Never writes attendance data.

$script:LoamsValidationReads = @('get_departments.php', 'get_years.php', 'get_courses.php')
$script:LoamsForbiddenEndpoints = @('student_login.php', 'rfid_login.php', 'guest_login.php', 'turnstile.php', 'turnstile_pull.php', 'reset_visits.php')

function Assert-LoamsReadOnlyEndpoint {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Url)
    $leaf = ([Uri]$Url).Segments[-1]
    if ($script:LoamsForbiddenEndpoints -contains $leaf) { throw "Validation must not call the attendance/state-changing endpoint $leaf." }
}

function Invoke-LoamsHttpGet {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Url)
    Assert-LoamsReadOnlyEndpoint -Url $Url
    try {
        $r = Invoke-WebRequest -Uri $Url -Method Get -UseBasicParsing -TimeoutSec 15 -MaximumRedirection 0 -ErrorAction Stop
        return [pscustomobject]@{ StatusCode = [int]$r.StatusCode; ContentType = [string]$r.Headers['Content-Type']; Body = [string]$r.Content; Error = '' }
    } catch {
        return [pscustomobject]@{ StatusCode = 0; ContentType = ''; Body = ''; Error = $_.Exception.Message }
    }
}

function Invoke-LoamsPostConvergeValidation {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $BaseUrl, [Parameter(Mandatory)][string] $ApiRoot,
        [Parameter(Mandatory)][string] $ApacheServiceName, [Parameter(Mandatory)][string] $MariaDbServiceName,
        [Parameter(Mandatory)][string] $MysqlExe, [Parameter(Mandatory)] $ConnectionInfo, [Parameter(Mandatory)][string] $WorkDirectory
    )
    $checks = New-Object System.Collections.Generic.List[object]
    $check = { param([string] $name, [scriptblock] $body)
        try { $detail = & $body; $checks.Add([pscustomobject]@{ Name = $name; Ok = $true; Detail = [string]$detail }) }
        catch { $checks.Add([pscustomobject]@{ Name = $name; Ok = $false; Detail = (Protect-LoamsText -Text $_.Exception.Message) }) } }
    $base = $BaseUrl.TrimEnd('/') + '/'
    $json = { param([string] $endpoint)
        $r = Invoke-LoamsHttpGet -Url ($base + $endpoint)
        if ($r.StatusCode -ne 200) { throw "$endpoint returned HTTP $($r.StatusCode) $($r.Error)" }
        try { $null = $r.Body | ConvertFrom-Json } catch { throw "$endpoint did not return JSON (PHP or DB error)" }
        "HTTP 200 JSON" }

    & $check 'apache-running' { $s = Get-LoamsServiceSnapshot -ServiceName $ApacheServiceName; if ($s.State -ne 'Running') { throw "Apache service is $($s.State)" }; 'Running' }
    & $check 'mariadb-running' { $s = Get-LoamsServiceSnapshot -ServiceName $MariaDbServiceName; if ($s.State -ne 'Running') { throw "MariaDB service is $($s.State)" }; 'Running' }
    & $check 'php-smoke' { & $json 'get_branding.php' }
    foreach ($ep in $script:LoamsValidationReads) { & $check "php-db-read:$ep" { & $json $ep } }
    & $check 'legacy-static-photo' {
        if (-not (Test-Path -LiteralPath (Join-Path $ApiRoot 'uploads\default.jpg'))) { throw 'uploads\default.jpg (the legacy photo fallback) is not deployed in the web root: the deployment must provide it (runbook B0/F)' }
        $r = Invoke-LoamsHttpGet -Url ($base + 'uploads/default.jpg')
        if ($r.StatusCode -ne 200 -or $r.ContentType -notlike 'image/*') { throw "uploads/default.jpg returned HTTP $($r.StatusCode) $($r.ContentType)" }
        'HTTP 200 image' }
    & $check 'db-read-query' {
        if ($ConnectionInfo.Database -notmatch '^[A-Za-z0-9_]+$') { throw 'unexpected database name' }
        $cnf = New-LoamsMysqlDefaultsFile -Directory $WorkDirectory -ConnectionInfo $ConnectionInfo
        try {
            $n = Invoke-LoamsMysqlQuery -MysqlExe $MysqlExe -DefaultsFile $cnf -Sql ("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = '{0}'" -f $ConnectionInfo.Database)
        } finally { Remove-LoamsMysqlDefaultsFile -Path $cnf }
        if ([int]$n -le 0) { throw 'application database has no tables' }
        "$n tables" }

    $passed = (@($checks | Where-Object { -not $_.Ok }).Count -eq 0)
    Write-LoamsLog -Message ('Post-change validation: ' + (($checks | ForEach-Object { "$($_.Name)=$(if ($_.Ok) { 'ok' } else { 'FAILED' })" }) -join ', '))
    return [pscustomobject]@{ Passed = $passed; Checks = @($checks) }
}
```

`db-read-query` creates the admin-only client defaults file, so the suite runs in Converge mode (inside the `Validate` checkpoint, Task 17).

- [ ] **Step 6: Run to verify they pass**

Same command as Step 3. Expected: PASS, 20 tests (Authorization 15, Validation 5). Set `expected-test-count.txt` to `261` and run the suite runner → `PASSED: 261 tests`.

- [ ] **Step 7: Commit**

Via the project `commit` skill — subject: `feat(deploy): authorize staging hosts, re-check drift before changes, and validate read-only after`

---

### Task 17: `-Converge` — gates, encrypted backup + off-host copy, drift re-check, checkpoint plan (Apache + MariaDB), outcome, entry script

**Files:**
- Create: `deploy/server/LoamsHost/LoamsHost.Converge.ps1`
- Modify: `deploy/server/Test-LoamsServerHost.ps1` (add the `Converge` parameter set — full file below)
- Create: `deploy/tests/Converge.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `305`

**Interfaces:**
- Consumes: everything from Tasks 1–16.
- Produces:
  - `Get-LoamsConvergePreflight -Report -Gate -Ack [-Staging] [-Additional <string[]>]` → `[pscustomobject]@{ Ok; Problems }`. The prerequisites passed in include, for every mode except `-BackupOnly`, `Test-LoamsReportLocation -ReportPath` (Task 11): the final report location is validated **before** any backup or checkpoint.
  - `Test-LoamsScheduledTaskPresent -TaskName <string>` → `[bool]` (via `Get-LoamsScheduledTaskXml`, Task 11).
  - `Test-LoamsPostChangeReportClean -Report <pscustomobject> [-Staging]` → `[pscustomobject]@{ Clean; Reasons }` — clean = elevated, Apache and MariaDB `Converged`, both ACL profiles compliant, event source registered, module inventory complete, and (production) report outcome `Success` / (staging) no runtime finding other than `ManifestNotFinal`.
  - `New-LoamsConvergePlan -Report -Backup -Manifest -ManifestPath -XamppRoot -ProgramDataRoot -BaseUrl -ConnectionInfo [-Staging]` → checkpoints, in order: `ApacheServiceRegistration` (Apache FreshInstall/ControlPanel only), `MariaDbServiceRegistration` (MariaDB FreshInstall/ControlPanel only), `ServerLayout`, `Acl`, `MariaDbAcl`, `EventSource`, `BackupRetentionTask`, `MariaDbServiceAccount`, `MariaDbVerification`, `ApacheServiceAccount`, `Validate`.
  - `Test-LoamsProductionPlan -Plan <object[]> [-Staging]` → `[string[]]` mandatory checkpoint names missing from the plan: every plan needs `Validate`; production also needs `Acl`, `MariaDbAcl`, `MariaDbServiceAccount`, `MariaDbVerification`, `ApacheServiceAccount`.
  - `Write-LoamsConvergeOutcome -Result -BackupPath -RestoreScript`.
  - `Invoke-LoamsHostConverge -XamppRoot -ManifestPath -ServiceName -MariaDbServiceName -ProgramDataRoot -BackupRoot -AcknowledgedReport -ReportPath -RecoveryCertificate -RecoveryCertSha256 [-OffHostDestination <path>] [-OffHostAttestation] [-Staging] [-BackupOnly] [-DbCredential] [-BaseUrl]` → `[pscustomobject]@{ Outcome='Refused'|'Success'|'RolledBack'|'RecoveryRequired'; Problems; BackupPath; Result }`.

**Order of operations (spec §4 converge steps 1–6, with the owner's changes):**
1. **Read-only phase** (mode `Report`): fresh report → manifest status gate (non-`approved` refused unless `-Staging` **and** `Test-LoamsStagingAuthorization`) → acknowledged report (same computer, elevated, ≤ 72 h, not in the future, same `profileHash`) → prerequisites (pinned OpenSSL contained and matching; recovery certificate fingerprint = `-RecoveryCertSha256`; an off-host target given and not local; **database backup readiness** — MariaDB running, D19) → preflight (Apache **and** MariaDB situations convergeable; runtime `Match` unless staging). Any problem ⇒ `Refused`, nothing changed.
2. **Operator authorization:** the operator types this computer's name (every run, staging and production). Wrong ⇒ `Refused`.
3. **Backup** (mode `Converge`): encrypted, verified backup set (Task 14). Failure ⇒ `Refused`, no host change.
4. **Off-host copy:** `-OffHostDestination` ⇒ hash-verified export; `-OffHostAttestation` ⇒ operator types name, location and the SHA-256 of `SHA256SUMS.json` measured on the off-host copy. Then `Test-LoamsOffHostVerification` must pass. Otherwise ⇒ `Refused` (backup kept, host unchanged). `-BackupOnly` stops here (runbook Part B before a manual stack swap — D1).
5. **Drift re-check immediately before the first change:** live fingerprint vs the acknowledged report's fingerprint (service configuration/accounts, config hashes, loaded modules, XAMPP-side ACLs). Any difference ⇒ `Refused`, no host change.
6. **Plan check:** every plan must contain `Validate`; without `-Staging`, a plan missing any mandatory checkpoint (incl. `MariaDbAcl`, `MariaDbServiceAccount`, `MariaDbVerification`) is refused before execution; the secure MariaDB ACL is therefore a hard production gate.
7. **Checkpoints** via `Invoke-LoamsCheckpointPlan` (Task 15). `MariaDbServiceAccount` switches the account and verifies the service runs; `MariaDbVerification` runs startup/read/write/backup-restore checks on this run's scratch schemas and its **undo drops exactly those schemas** (failure ⇒ `RECOVERY REQUIRED`); `BackupRetentionTask` rolls back to the **captured prior** task definition and `tools\` tree (Task 14), never by blind deletion; `Validate` runs the read-only validation suite and requires a **clean post-change report** (`Test-LoamsPostChangeReportClean`, incl. no leftover verification schemas). Any failure rolls back; any rollback failure is `RECOVERY REQUIRED`.
8. **Outcome (decided):** a failure to write the final report after a validated success is handled deterministically: outcome `SuccessReportNotSaved`, exit code 6, clear message naming the path (the evidence copy in the backup set is reported). After every run, every tracked credential file must be gone (`Clear-LoamsCredentialFiles`); otherwise the outcome is `RecoveryRequired` (`CredentialCleanup`), never `Success`, `RolledBack` or `Refused`.
   Rollback rule:  a dirty post-change report fails the real `Validate` checkpoint ⇒ the engine rolls everything back ⇒ `RolledBack` (exit 5) with the dirty report **kept as evidence** in `<backup>\logs\post-change-report.json`; if any undo fails ⇒ `RecoveryRequired` (exit 4, `recovery-required.json`, manual steps, restore script). `Success` (exit 0) only when every checkpoint passed, and then the clean post-change report is saved to `-ReportPath` as well. An engine `Success` without a clean `Validate` report is treated as an invariant breach (`RecoveryRequired`, failed checkpoint `PostChangeReport`) — unreachable while every plan contains `Validate`. Never a success message otherwise.

MariaDB and Apache restarts in steps 7 are short attendance outages; the runbook schedules convergence inside the maintenance window (spec §7 step 3).

- [ ] **Step 1: Write the failing tests `deploy/tests/Converge.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    Import-Module (Join-Path $PSScriptRoot '..\server\LoamsHost\LoamsHost.psm1') -Force
    . (Join-Path $PSScriptRoot 'TestHelpers.ps1')
    $script:Root = New-LoamsFakeXampp -Root (Join-Path $TestDrive 'xampp')
    $script:Pd = Join-Path $TestDrive 'pd\LOAMS'
    $script:Entry = Join-Path $PSScriptRoot '..\server\Test-LoamsServerHost.ps1'
    $script:Approved = ConvertTo-LoamsManifestObject -Manifest (New-LoamsTestManifest -XamppRoot $script:Root)
    $script:Draft = ConvertTo-LoamsManifestObject -Manifest (New-LoamsTestManifest -XamppRoot $script:Root -Status draft)
    $script:Conn = [pscustomobject]@{ Host = 'localhost'; User = 'loams_test'; Database = 'wits_test'; Password = (New-Object Security.SecureString); Source = 'credential' }
    $script:Fp = [pscustomobject]@{ computerName = $env:COMPUTERNAME.ToUpperInvariant(); apacheService = 'a'; mariaDbService = 'm'; configHashes = @('C:\x\httpd.conf=aa'); aclSddl = @(); modules = @() }

    function New-TestReport { param([string] $Situation = 'ServiceNonVirtualAccount', [bool] $Stop = $false, [string] $DbSituation = 'ServiceNonVirtualAccount', [bool] $DbStop = $false,
                                    [bool] $Elevated = $true, [string] $Runtime = 'Match', [string] $Hash = 'h1')
        $cls = { param($sit, $stop, $name, $acct) [pscustomobject]@{ Situation = $sit; StopAndReport = $stop; Convergeable = (-not $stop); ServiceName = $name; CurrentAccount = $acct; TargetAccount = "NT SERVICE\$name"; PhpSapi = 'apache2handler'; Reasons = @("test reason $sit") } }
        [pscustomobject]@{
            computerName = $env:COMPUTERNAME; generatedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'); elevated = $Elevated
            manifest = [pscustomobject]@{ path = 'm.json'; valid = $true; status = 'approved'; manifestVersion = '1.0'; errors = @() }
            layout = (Get-LoamsXamppLayout -XamppRoot $script:Root)
            processes = @([pscustomobject]@{ ProcessId = 10; ParentProcessId = 4; ExecutablePath = 'x' })
            classification = (& $cls $Situation $Stop 'Apache2.4' 'LocalSystem')
            mariaDb = [pscustomobject]@{ layout = (Get-LoamsMariaDbLayout -XamppRoot $script:Root); processes = @([pscustomobject]@{ ProcessId = 20; ParentProcessId = 4; ExecutablePath = 'y' }); classification = (& $cls $DbSituation $DbStop 'mysql' 'LocalSystem') }
            runtime = [pscustomobject]@{ Status = $Runtime; Findings = @(); Checked = 1 }
            fingerprint = $script:Fp; profileHash = $Hash; outcome = 'Success'
        }
    }
    function Write-Ack { param([string] $Hash = 'h1')
        $p = Join-Path $TestDrive ([guid]::NewGuid().ToString('N') + '.json')
        @{ computerName = $env:COMPUTERNAME; profileHash = $Hash; generatedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'); elevated = $true; fingerprint = $script:Fp } |
            ConvertTo-Json -Depth 5 | Set-Content -Path $p
        return $p
    }
    function Set-ConvergeMocks { param($Report = (New-TestReport), $Manifest = $script:Approved, [bool] $StagingAuthorized = $false)
        $global:LoamsTReport = $Report; $global:LoamsTManifest = $Manifest; $global:LoamsTStagingAuth = $StagingAuthorized
        $global:LoamsTLiveFp = $script:Fp
        $global:LoamsTBackupDir = Join-Path $TestDrive ('bk' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path (Join-Path $global:LoamsTBackupDir 'logs') -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $script:Pd 'reports') -Force | Out-Null
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)' }
        Mock -ModuleName LoamsHost New-LoamsHostReport { $global:LoamsTReport }
        Mock -ModuleName LoamsHost Save-LoamsHostReport { }
        Mock -ModuleName LoamsHost Read-LoamsStackManifest { $global:LoamsTManifest }
        Mock -ModuleName LoamsHost Test-LoamsStagingAuthorization {
            if ($global:LoamsTStagingAuth) { [pscustomobject]@{ Authorized = $true; Reason = 'approved staging host' } }
            else { [pscustomobject]@{ Authorized = $false; Reason = 'this machine is not listed in approved-staging-hosts.json' } } }
        Mock -ModuleName LoamsHost Get-LoamsPinnedOpenSsl { 'C:\fake\openssl.exe' }
        Mock -ModuleName LoamsHost Test-LoamsRecipientCertificate { [pscustomobject]@{ Path = 'C:\fake\recovery.crt'; Sha256 = ('ab' * 32); Subject = 'CN=Test'; NotAfter = '2030-01-01T00:00:00Z' } }
        Mock -ModuleName LoamsHost Get-LoamsDestinationKind { 'Removable' }
        Mock -ModuleName LoamsHost Confirm-LoamsOperatorHost { $true }
        Mock -ModuleName LoamsHost Get-LoamsDbConnectionInfo { [pscustomobject]@{ Host = 'localhost'; User = 'u'; Database = 'd'; Password = (New-Object Security.SecureString); Source = 'credential' } }
        Mock -ModuleName LoamsHost Get-LoamsBridgeTaskAccount { '' }
        Mock -ModuleName LoamsHost New-LoamsBackupSet { [pscustomobject]@{ Path = $global:LoamsTBackupDir; Verified = $true; AclSaves = @(); Services = $null; SumsSha256 = 's'; RestoreScript = (Join-Path $global:LoamsTBackupDir 'Restore-LoamsHostBackup.ps1') } }
        Mock -ModuleName LoamsHost Export-LoamsBackupSet { [pscustomobject]@{ method = 'hash-verified-export'; allMatch = $true } }
        Mock -ModuleName LoamsHost Test-LoamsOffHostVerification { [pscustomobject]@{ Verified = $true; Reason = 'hash-verified-export' } }
        Mock -ModuleName LoamsHost Get-LoamsLiveFingerprint { $global:LoamsTLiveFp }
        Mock -ModuleName LoamsHost New-LoamsConvergePlan {
            $cps = @(@('Acl', 'MariaDbAcl', 'MariaDbServiceAccount', 'MariaDbVerification', 'ApacheServiceAccount') | ForEach-Object { New-LoamsCheckpoint -Name $_ -ManualRestore 'none' -Do { } -Verify { $true } -Undo { } })
            $cps += New-LoamsCheckpoint -Name 'Validate' -ManualRestore 'none' -Context @{ PostReport = $null } -Do { } -Verify { param($c) $c.PostReport = [pscustomobject]@{ outcome = 'Success' }; $true } -Undo { }
            $cps }
        Mock -ModuleName LoamsHost Test-LoamsPostChangeReportClean { [pscustomobject]@{ Clean = $true; Reasons = @() } }
    }
    function Converge { param([string] $Ack, [switch] $Staging, [switch] $BackupOnly, [string] $OffHost = 'E:\loams-offhost', [switch] $Attest, [string] $ReportPath = (Join-Path $script:Pd 'reports\post.json'))
        $offArgs = @{}
        if ($Attest) { $offArgs['OffHostAttestation'] = $true } elseif ($OffHost) { $offArgs['OffHostDestination'] = $OffHost }
        Invoke-LoamsHostConverge -XamppRoot $script:Root -ManifestPath 'm.json' -ProgramDataRoot $script:Pd -BackupRoot (Join-Path $TestDrive 'backups') `
            -AcknowledgedReport $Ack -ReportPath $ReportPath -RecoveryCertificate 'C:\fake\recovery.crt' -RecoveryCertSha256 ('ab' * 32) `
            -Staging:$Staging -BackupOnly:$BackupOnly @offArgs 6>&1
    }
    function Get-Outcome { param($All) @($All | Where-Object { $_ -isnot [System.Management.Automation.InformationRecord] })[-1] }
    function Get-Text { param($All) ($All | Where-Object { $_ -is [System.Management.Automation.InformationRecord] } | ForEach-Object { $_.MessageData.ToString() }) -join "`n" }
    function Problems { param($All) ((Get-Outcome $All).Problems -join ' ') }
}
AfterAll { Remove-Variable -Name LoamsTReport, LoamsTManifest, LoamsTStagingAuth, LoamsTBackupDir, LoamsTLiveFp, LoamsTSuite, LoamsTPost, LoamsTTasks, LoamsTRemoveFails, LoamsTSql, LoamsTDropFailures, LoamsTProbeUndone, LoamsTLocked, LoamsTLockFailures, LoamsTHttpd, LoamsTMysqld, LoamsTMyIni, LoamsTAccount, LoamsTDbAccount, LoamsTModules -Scope Global -ErrorAction SilentlyContinue }

Describe 'Converge gates (nothing changes when refused)' {
    It 'refuses a draft manifest without -Staging' {
        Set-ConvergeMocks -Manifest $script:Draft
        (Get-Outcome (Converge -Ack (Write-Ack))).Outcome | Should -Be 'Refused'
        Should -Invoke -ModuleName LoamsHost New-LoamsBackupSet -Times 0
        Should -Invoke -ModuleName LoamsHost Confirm-LoamsOperatorHost -Times 0
    }
    It 'refuses -Staging on a host that is not an authorized staging host' {
        Set-ConvergeMocks -Manifest $script:Draft -StagingAuthorized $false
        Problems (Converge -Ack (Write-Ack) -Staging) | Should -Match 'not listed'
    }
    It 'allows -Staging with a draft manifest on an authorized staging host' {
        Set-ConvergeMocks -Manifest $script:Draft -StagingAuthorized $true -Report (New-TestReport -Runtime 'Unknown')
        (Get-Outcome (Converge -Ack (Write-Ack) -Staging)).Outcome | Should -Be 'Success'
    }
    It 'refuses without a prior report (first run is report-only)' {
        Set-ConvergeMocks
        Problems (Converge -Ack (Join-Path $TestDrive 'missing.json')) | Should -Match 'report-only'
    }
    It 'refuses when the host changed since the acknowledged report' {
        Set-ConvergeMocks
        Problems (Converge -Ack (Write-Ack -Hash 'other')) | Should -Match 'profile hash differs'
    }
    It 'refuses when not elevated' {
        Set-ConvergeMocks -Report (New-TestReport -Elevated $false)
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'not elevated'
    }
    It 'refuses an Apache stop-and-report situation' {
        Set-ConvergeMocks -Report (New-TestReport -Situation 'MissingModSsl' -Stop $true)
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'MissingModSsl'
    }
    It 'refuses a MariaDB stop-and-report situation' {
        Set-ConvergeMocks -Report (New-TestReport -DbSituation 'NoXamppMariaDb' -DbStop $true)
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'MariaDB situation NoXamppMariaDb'
    }
    It 'refuses a stack that does not match the approved manifest' {
        Set-ConvergeMocks -Report (New-TestReport -Runtime 'Mismatch')
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'upgrade per runbook'
    }
    It 'refuses (no backup attempted) when MariaDB is stopped but holds data - readiness not mocked' {
        $data = Join-Path $script:Root 'mysql\data\wits_test'; New-Item -ItemType Directory -Force -Path $data | Out-Null
        try {
            $rep = New-TestReport; $rep.mariaDb.processes = @()
            Set-ConvergeMocks -Report $rep
            Problems (Converge -Ack (Write-Ack)) | Should -Match 'holds data'
            Should -Invoke -ModuleName LoamsHost New-LoamsBackupSet -Times 0
        } finally { Remove-Item -LiteralPath $data -Recurse -Force }
    }
    It 'refuses a fresh host whose MariaDB has no application database - readiness not mocked' {
        $rep = New-TestReport; $rep.mariaDb.processes = @()
        Set-ConvergeMocks -Report $rep
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'No application database'
        Should -Invoke -ModuleName LoamsHost New-LoamsBackupSet -Times 0
    }
    It 'refuses when the recovery certificate does not match the pinned fingerprint' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Test-LoamsRecipientCertificate { throw 'Recovery certificate fingerprint 00 does not match the expected fingerprint' }
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'fingerprint'
    }
    It 'refuses without an off-host target' {
        Set-ConvergeMocks
        Problems (Converge -Ack (Write-Ack) -OffHost '') | Should -Match 'off-host'
    }
    It 'refuses an off-host destination on this machine' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Get-LoamsDestinationKind { 'LocalFixed' }
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'not an off-host location'
    }
    It 'refuses before any backup or checkpoint when -ReportPath is outside the protected reports folder' {
        Set-ConvergeMocks
        $r = Get-Outcome (Converge -Ack (Write-Ack) -ReportPath (Join-Path $TestDrive 'elsewhere\post.json'))
        $r.Outcome | Should -Be 'Refused'
        ($r.Problems -join ' ') | Should -Match 'must be a file directly inside the protected reports folder'
        Should -Invoke -ModuleName LoamsHost New-LoamsBackupSet -Times 0
        Should -Invoke -ModuleName LoamsHost New-LoamsConvergePlan -Times 0
        Should -Invoke -ModuleName LoamsHost Confirm-LoamsOperatorHost -Times 0
        Should -Invoke -ModuleName LoamsHost Export-LoamsBackupSet -Times 0
    }
    It 'refuses before any backup when the reports folder ACL is not the protected allowlist' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Get-LoamsAclSddl { 'O:BAG:SYD:AI(A;OICIID;FA;;;BA)(A;OICIID;FA;;;SY)(A;OICIID;0x1200a9;;;BU)' }
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'reports folder ACL: unexpected principal S-1-5-32-545'
        Should -Invoke -ModuleName LoamsHost New-LoamsBackupSet -Times 0
    }
    It 'refuses when the operator does not type the computer name' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Confirm-LoamsOperatorHost { $false }
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'computer name'
        Should -Invoke -ModuleName LoamsHost New-LoamsBackupSet -Times 0
    }
    It 'refuses with no host change when the backup cannot be verified' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost New-LoamsBackupSet { throw "Backup at 'x' failed verification: hash mismatch" }
        (Get-Outcome (Converge -Ack (Write-Ack))).Outcome | Should -Be 'Refused'
        Should -Invoke -ModuleName LoamsHost New-LoamsConvergePlan -Times 0
    }
    It 'refuses with no host change when the off-host copy is not verified' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Test-LoamsOffHostVerification { [pscustomobject]@{ Verified = $false; Reason = 'off-host copy did not match' } }
        $r = Get-Outcome (Converge -Ack (Write-Ack))
        $r.Outcome | Should -Be 'Refused'
        $r.BackupPath | Should -Be $global:LoamsTBackupDir
        Should -Invoke -ModuleName LoamsHost New-LoamsConvergePlan -Times 0
    }
    It 'refuses with no host change when the host drifted after the acknowledged report' {
        Set-ConvergeMocks
        $global:LoamsTLiveFp = [pscustomobject]@{ computerName = $env:COMPUTERNAME.ToUpperInvariant(); apacheService = 'changed'; mariaDbService = 'm'; configHashes = @('C:\x\httpd.conf=aa'); aclSddl = @(); modules = @() }
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'drifted.*apacheService'
        Should -Invoke -ModuleName LoamsHost New-LoamsConvergePlan -Times 0
    }
    It 'refuses a production plan that lacks the MariaDB checkpoints, before executing anything' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost New-LoamsConvergePlan { @(New-LoamsCheckpoint -Name 'Acl' -ManualRestore 'none' -Do { } -Verify { $true } -Undo { }) }
        Problems (Converge -Ack (Write-Ack)) | Should -Match 'MariaDbAcl'
        Test-Path (Join-Path $global:LoamsTBackupDir 'logs\converge-state.json') | Should -BeFalse
    }
}

Describe 'Converge execution' {
    It '-BackupOnly takes the encrypted backup, verifies the off-host copy and changes nothing else' {
        Set-ConvergeMocks
        $r = Get-Outcome (Converge -Ack (Write-Ack) -BackupOnly)
        $r.Outcome | Should -Be 'Success'
        Should -Invoke -ModuleName LoamsHost Export-LoamsBackupSet -Times 1 -Exactly
        Should -Invoke -ModuleName LoamsHost New-LoamsConvergePlan -Times 0
    }
    It 'reports success and saves the post-change report' {
        Set-ConvergeMocks
        $all = Converge -Ack (Write-Ack)
        (Get-Outcome $all).Outcome | Should -Be 'Success'
        (Get-Text $all) | Should -Match 'CONVERGE SUCCEEDED'
        Should -Invoke -ModuleName LoamsHost Save-LoamsHostReport -Times 1 -Exactly -ParameterFilter { $Path -like '*post.json' }
        Should -Invoke -ModuleName LoamsHost Save-LoamsHostReport -Times 1 -Exactly -ParameterFilter { $Path -like '*logs\post-change-report.json' }
        Get-LoamsMode | Should -Be 'Report'
    }
    It 'handles a final-report save failure after success deterministically (SuccessReportNotSaved, exit 6, no throw)' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Save-LoamsHostReport { if ($Path -like '*reports\post.json') { throw 'disk full' } }
        $all = Converge -Ack (Write-Ack)
        $r = Get-Outcome $all
        $r.Outcome | Should -Be 'SuccessReportNotSaved'
        Get-LoamsExitCode -Outcome $r.Outcome | Should -Be 6
        (Get-Text $all) | Should -Match "post-change report could not be saved to '.*post\.json': disk full"
    }
    It 'never reports success while a run-owned credential file remains' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Clear-LoamsCredentialFiles { throw 'credential files could not be overwritten and deleted (they contain the DB password): C:\x\mysql-client-1.cnf' }
        $all = Converge -Ack (Write-Ack)
        (Get-Outcome $all).Outcome | Should -Be 'RecoveryRequired'
        (Get-Text $all) | Should -Not -Match 'CONVERGE SUCCEEDED'
        (Get-Text $all) | Should -Match 'RECOVERY REQUIRED - failed checkpoint: CredentialCleanup'
    }
    It 'records an operator attestation when -OffHostAttestation is used' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Read-LoamsOperatorInput { 'typed value' }
        Mock -ModuleName LoamsHost Set-LoamsOffHostAttestation { [pscustomobject]@{ method = 'operator-attested'; allMatch = $true } }
        (Get-Outcome (Converge -Ack (Write-Ack) -Attest)).Outcome | Should -Be 'Success'
        Should -Invoke -ModuleName LoamsHost Set-LoamsOffHostAttestation -Times 1 -Exactly
        Should -Invoke -ModuleName LoamsHost Export-LoamsBackupSet -Times 0
    }
    It 'rolls back automatically when a checkpoint fails' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost New-LoamsConvergePlan { @(
            (New-LoamsCheckpoint -Name 'Acl' -ManualRestore 'restore acl' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'MariaDbAcl' -ManualRestore 'restore db acl' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'MariaDbServiceAccount' -ManualRestore 'restore db account' -Do { throw 'sc.exe failed' } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'MariaDbVerification' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'ApacheServiceAccount' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'Validate' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { })) }
        $all = Converge -Ack (Write-Ack)
        (Get-Outcome $all).Outcome | Should -Be 'RolledBack'
        (Get-Text $all) | Should -Match "CONVERGE FAILED at checkpoint 'MariaDbServiceAccount'"
    }
    It 'declares RECOVERY REQUIRED when rollback fails and never reports success' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost New-LoamsConvergePlan { @(
            (New-LoamsCheckpoint -Name 'Acl' -ManualRestore 'icacls restore' -Do { } -Verify { $true } -Undo { throw 'icacls /restore failed' }),
            (New-LoamsCheckpoint -Name 'MariaDbAcl' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'MariaDbServiceAccount' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'MariaDbVerification' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'ApacheServiceAccount' -ManualRestore 'sc.exe config' -Do { throw 'sc.exe failed' } -Verify { $true } -Undo { }),
            (New-LoamsCheckpoint -Name 'Validate' -ManualRestore 'x' -Do { } -Verify { $true } -Undo { })) }
        $all = Converge -Ack (Write-Ack)
        (Get-Outcome $all).Outcome | Should -Be 'RecoveryRequired'
        $text = Get-Text $all
        $text | Should -Match 'RECOVERY REQUIRED - failed checkpoint: ApacheServiceAccount'
        $text | Should -Not -Match 'CONVERGE SUCCEEDED'
        $doc = Get-Content -Raw (Join-Path $global:LoamsTBackupDir 'recovery-required.json') | ConvertFrom-Json
        $doc.failedCheckpoint | Should -Be 'ApacheServiceAccount'
        ($doc.manualSteps -join ' ') | Should -Match 'icacls restore'
        Should -Invoke -ModuleName LoamsHost Save-LoamsHostReport -Times 0
    }
    It 'rolls back on a dirty post-change report, keeps that report as evidence and never reports success' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost New-LoamsConvergePlan {
            $cps = @(@('Acl', 'MariaDbAcl', 'MariaDbServiceAccount', 'MariaDbVerification', 'ApacheServiceAccount') | ForEach-Object { New-LoamsCheckpoint -Name $_ -ManualRestore 'none' -Do { } -Verify { $true } -Undo { } })
            $cps += New-LoamsCheckpoint -Name 'Validate' -ManualRestore 'none' -Context @{ PostReport = $null } -Do { } -Verify { param($c) $c.PostReport = [pscustomobject]@{ outcome = 'StopAndReport' }; $false } -Undo { }
            $cps }
        $all = Converge -Ack (Write-Ack)
        (Get-Outcome $all).Outcome | Should -Be 'RolledBack'
        (Get-Text $all) | Should -Not -Match 'CONVERGE SUCCEEDED'
        (Get-Text $all) | Should -Match "CONVERGE FAILED at checkpoint 'Validate'"
        Should -Invoke -ModuleName LoamsHost Save-LoamsHostReport -Times 1 -Exactly -ParameterFilter { $Path -like '*logs\post-change-report.json' }
        Should -Invoke -ModuleName LoamsHost Save-LoamsHostReport -Times 0 -ParameterFilter { $Path -like '*post.json' }
    }
    It 'keeps the DB password out of output, log and transcript even when the dump fails' {
        Set-ConvergeMocks
        Mock -ModuleName LoamsHost Get-LoamsDbConnectionInfo {
            Register-LoamsSecret -Secret 'S1b-test-db-pass'
            [pscustomobject]@{ Host = 'localhost'; User = 'loams_test'; Database = 'wits_test'; Password = (ConvertTo-SecureString 'S1b-test-db-pass' -AsPlainText -Force); Source = 'config.php' } }
        Mock -ModuleName LoamsHost New-LoamsBackupSet { throw (Protect-LoamsText -Text 'mysqldump failed with exit code 1: Access denied using password S1b-test-db-pass') }
        $log = Join-Path $TestDrive 'pw.log'; $tx = Join-Path $TestDrive 'pw-transcript.txt'
        Set-LoamsLogPath -Path $log
        Start-Transcript -Path $tx | Out-Null
        try { $all = Converge -Ack (Write-Ack) } finally { Stop-Transcript | Out-Null }
        (Get-Outcome $all).Outcome | Should -Be 'Refused'
        ($all | Out-String) | Should -Not -Match 'S1b-test-db-pass'
        Get-Content -Raw $tx | Should -Not -Match 'S1b-test-db-pass'
        if (Test-Path $log) { Get-Content -Raw $log | Should -Not -Match 'S1b-test-db-pass' }
    }
}

Describe 'New-LoamsConvergePlan' {
    BeforeAll {
        $script:Prior = [pscustomobject]@{ retention = [pscustomobject]@{ taskName = 'LOAMS Backup Retention'; taskExists = $false; taskHash = 'absent' }; tools = @() }
        $script:Backup = [pscustomobject]@{ Path = $TestDrive; AclSaves = @(); PriorState = $script:Prior; VerifySchemas = @('loams_s1b_verify_testrun', 'loams_s1b_verify_testrun_r') }
        New-Item -ItemType Directory -Force -Path (Join-Path $TestDrive 'logs') | Out-Null
        $script:PlanManifest = Join-Path $TestDrive 'm-plan.json'
        Save-LoamsTestManifest -Manifest (New-LoamsTestManifest -XamppRoot $script:Root) -Path $script:PlanManifest
        $script:ApprovedModulePaths = @(foreach ($c in $script:Approved.server.components) { foreach ($mod in $c.modules) { Join-Path $script:Root ($mod.relativePath -replace '/', '\') } })
        function Plan { param($Report, [switch] $Staging, [string] $Pd = $script:Pd)
            New-LoamsConvergePlan -Report $Report -Backup $script:Backup -Manifest $script:Approved -ManifestPath $script:PlanManifest -XamppRoot $script:Root -ProgramDataRoot $Pd `
                -BaseUrl 'http://127.0.0.1/loams_api/' -ConnectionInfo $script:Conn -Staging:$Staging
        }
        function Use-SchemaMocks { param([int] $DropFailures)
            $global:LoamsTSql = New-Object System.Collections.Generic.List[string]; $global:LoamsTDropFailures = $DropFailures
            Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } -ParameterFilter { $FilePath -like '*icacls.exe' }
            Mock -ModuleName LoamsHost Copy-LoamsDbSchemaPipe { }
            Mock -ModuleName LoamsHost Invoke-LoamsMysqlQuery {
                $global:LoamsTSql.Add($Sql)
                if ($Sql -match '^DROP' -and $Sql -notmatch 'CREATE' -and $global:LoamsTDropFailures -gt 0) { $global:LoamsTDropFailures--; throw 'ERROR 1010 (HY000): Error dropping database' }
                if ($Sql -match 'SCHEMA_NAME') { return '' }
                if ($Sql -match 'COUNT\(\*\)') { return '3' }
                return '10.4.28-MariaDB' }
        }
        function New-PostReport { param([string] $Outcome = 'Success', [string] $Status = 'Match', [object[]] $Findings = @(), [bool] $Complete = $true, [string[]] $Leftover = @())
            [pscustomobject]@{ elevated = $true; outcome = $Outcome; inventoryComplete = $Complete
                classification = [pscustomobject]@{ Situation = 'Converged' }; acl = [pscustomobject]@{ Compliant = $true }
                mariaDb = [pscustomobject]@{ classification = [pscustomobject]@{ Situation = 'Converged' }; acl = [pscustomobject]@{ Compliant = $true }; leftoverVerifySchemas = $Leftover }
                eventSource = [pscustomobject]@{ Registered = $true }
                runtime = [pscustomobject]@{ Status = $Status; Findings = $Findings } }
        }
        function Use-ValidateMocks { param([bool] $SuitePasses = $true, $PostReport = (New-PostReport))
            $global:LoamsTSuite = $SuitePasses; $global:LoamsTPost = $PostReport
            Mock -ModuleName LoamsHost Get-LoamsServiceSnapshot { [pscustomobject]@{ Exists = $true; State = 'Running'; StartName = "NT SERVICE\$ServiceName" } }
            Mock -ModuleName LoamsHost Invoke-LoamsPostConvergeValidation { [pscustomobject]@{ Passed = $global:LoamsTSuite; Checks = @() } }
            Mock -ModuleName LoamsHost New-LoamsHostReport { $global:LoamsTPost }
        }
    }
    BeforeEach { Mock -ModuleName LoamsHost Get-LoamsBridgeTaskAccount { '' } }
    It 'orders the checkpoints and registers services only for Control Panel / fresh hosts' {
        ((Plan -Report (New-TestReport -Situation 'ControlPanel' -DbSituation 'ControlPanel')).Name -join ',') |
            Should -Be 'ApacheServiceRegistration,MariaDbServiceRegistration,ServerLayout,Acl,MariaDbAcl,EventSource,BackupRetentionTask,MariaDbServiceAccount,MariaDbVerification,ApacheServiceAccount,Validate'
        ((Plan -Report (New-TestReport)).Name -join ',') |
            Should -Be 'ServerLayout,Acl,MariaDbAcl,EventSource,BackupRetentionTask,MariaDbServiceAccount,MariaDbVerification,ApacheServiceAccount,Validate'
    }
    It 'ApacheServiceAccount undo restores the original account and restarts the service' {
        Set-LoamsMode -Mode Converge
        try {
            Mock -ModuleName LoamsHost Get-LoamsServiceSnapshot { [pscustomobject]@{ Exists = $true; State = 'Running'; StartName = 'NT SERVICE\Apache2.4' } }
            Mock -ModuleName LoamsHost Stop-LoamsWindowsService { }
            Mock -ModuleName LoamsHost Start-LoamsWindowsService { }
            Mock -ModuleName LoamsHost Set-LoamsServiceAccount { }
            $sa = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'ApacheServiceAccount'
            $sa.Context.ApacheWasRunning = $true
            & $sa.Undo $sa.Context
            Should -Invoke -ModuleName LoamsHost Set-LoamsServiceAccount -Times 1 -Exactly -ParameterFilter { $ServiceName -eq 'Apache2.4' -and $Account -eq 'LocalSystem' }
            Should -Invoke -ModuleName LoamsHost Start-LoamsWindowsService -Times 1 -Exactly -ParameterFilter { $ServiceName -eq 'Apache2.4' }
        } finally { Set-LoamsMode -Mode Report }
    }
    It 'drops exactly this run''s verification schemas on rollback after a failed verification cleanup (real checkpoint, real engine)' {
        Set-LoamsMode -Mode Converge
        try {
            Use-SchemaMocks -DropFailures 1
            $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'MariaDbVerification'
            $r = Invoke-LoamsCheckpointPlan -Checkpoints @($v) -StatePath (Join-Path $TestDrive 'mv1-state.json')
            $r.Outcome | Should -Be 'RolledBack'
            $r.FailedCheckpoint | Should -Be 'MariaDbVerification'
            @($global:LoamsTSql | Where-Object { $_ -match '^DROP' -and $_ -notmatch 'CREATE' })[-1] | Should -Be 'DROP DATABASE IF EXISTS loams_s1b_verify_testrun; DROP DATABASE IF EXISTS loams_s1b_verify_testrun_r;'
        } finally { Set-LoamsMode -Mode Report }
    }
    It 'declares RECOVERY REQUIRED when the verification schemas cannot be dropped on rollback (real checkpoint, real engine)' {
        Set-LoamsMode -Mode Converge
        try {
            Use-SchemaMocks -DropFailures 2
            $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'MariaDbVerification'
            $r = Invoke-LoamsCheckpointPlan -Checkpoints @($v) -StatePath (Join-Path $TestDrive 'mv2-state.json')
            $r.Outcome | Should -Be 'RecoveryRequired'
            $r.RollbackFailures[0].Checkpoint | Should -Be 'MariaDbVerification'
        } finally { Set-LoamsMode -Mode Report }
    }
    It 'an undeletable defaults file from the verification yields RECOVERY REQUIRED, never RolledBack (real checkpoint, real engine)' {
        Set-LoamsMode -Mode Converge
        try {
            Use-SchemaMocks -DropFailures 0
            # Only the deletion boundary is mocked: the first credential file it sees stays locked.
            $global:LoamsTLocked = $null; $global:LoamsTLockFailures = 99
            Mock -ModuleName LoamsHost Remove-LoamsFileSecurely {
                if ($null -eq $global:LoamsTLocked) { $global:LoamsTLocked = $Path }
                if ($Path -eq $global:LoamsTLocked -and $global:LoamsTLockFailures -gt 0) { $global:LoamsTLockFailures--; throw 'The process cannot access the file because it is being used by another process' }
                [IO.File]::Delete($Path) }
            $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'MariaDbVerification'
            $r = Invoke-LoamsCheckpointPlan -Checkpoints @($v) -StatePath (Join-Path $TestDrive 'cred1-state.json')
            $r.Outcome | Should -Be 'RecoveryRequired'
            $r.RollbackFailures[0].Checkpoint | Should -Be 'MariaDbVerification'
            $r.RollbackFailures[0].Error | Should -Match 'credential files could not be overwritten and deleted'
            Get-LoamsCredentialFiles | Should -Contain $global:LoamsTLocked
            Test-Path $global:LoamsTLocked | Should -BeTrue
        } finally {
            if ($global:LoamsTLocked -and (Test-Path $global:LoamsTLocked)) { [IO.File]::Delete($global:LoamsTLocked) }
            Clear-LoamsCredentialFiles
            Set-LoamsMode -Mode Report
        }
    }
    It 'a transiently locked defaults file is removed by the undo and the outcome is RolledBack with nothing left (real checkpoint, real engine)' {
        Set-LoamsMode -Mode Converge
        try {
            Use-SchemaMocks -DropFailures 0
            $global:LoamsTLocked = $null; $global:LoamsTLockFailures = 1
            Mock -ModuleName LoamsHost Remove-LoamsFileSecurely {
                if ($null -eq $global:LoamsTLocked) { $global:LoamsTLocked = $Path }
                if ($Path -eq $global:LoamsTLocked -and $global:LoamsTLockFailures -gt 0) { $global:LoamsTLockFailures--; throw 'The process cannot access the file because it is being used by another process' }
                [IO.File]::Delete($Path) }
            $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'MariaDbVerification'
            $r = Invoke-LoamsCheckpointPlan -Checkpoints @($v) -StatePath (Join-Path $TestDrive 'cred2-state.json')
            $r.Outcome | Should -Be 'RolledBack'
            Test-Path $global:LoamsTLocked | Should -BeFalse
            (@(Get-LoamsCredentialFiles) -contains $global:LoamsTLocked) | Should -BeFalse
        } finally { Set-LoamsMode -Mode Report }
    }
    It 'a dirty post-change report fails the real Validate checkpoint and the real engine rolls back' {
        Set-LoamsMode -Mode Converge
        try {
            # Only system boundaries are mocked: CIM/processes/externals/registry (Set-LoamsReportMocks), HTTP, MySQL client, icacls.
            Set-LoamsReportMocks -XamppRoot $script:Root -ModulePaths $script:ApprovedModulePaths
            Mock -ModuleName LoamsHost Invoke-LoamsExternal { [pscustomobject]@{ ExitCode = 0; StdOut = ''; StdErr = '' } } -ParameterFilter { $FilePath -like '*icacls.exe' }
            Mock -ModuleName LoamsHost Invoke-LoamsHttpGet {
                if ($Url -like '*default.jpg') { return [pscustomobject]@{ StatusCode = 200; ContentType = 'image/jpeg'; Body = ''; Error = '' } }
                [pscustomobject]@{ StatusCode = 200; ContentType = 'application/json'; Body = '[]'; Error = '' } }
            Mock -ModuleName LoamsHost Invoke-LoamsMysqlQuery { '12' }
            $global:LoamsTProbeUndone = $false
            $probe = New-LoamsCheckpoint -Name 'Probe' -ManualRestore 'none' -Do { } -Verify { $true } -Undo { $global:LoamsTProbeUndone = $true }
            $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'Validate'
            $r = Invoke-LoamsCheckpointPlan -Checkpoints @($probe, $v) -StatePath (Join-Path $TestDrive 'validate-state.json')
            $r.Outcome | Should -Be 'RolledBack'
            $r.FailedCheckpoint | Should -Be 'Validate'
            $global:LoamsTProbeUndone | Should -BeTrue
            $v.Context.PostReport | Should -Not -BeNullOrEmpty
            $clean = Test-LoamsPostChangeReportClean -Report $v.Context.PostReport
            $clean.Clean | Should -BeFalse
            ($clean.Reasons -join ' ') | Should -Match 'ACL profile not compliant'
        } finally { Set-LoamsMode -Mode Report }
    }
    It 'Validate fails when verification schemas were left behind' {
        Use-ValidateMocks -PostReport (New-PostReport -Leftover @('loams_s1b_verify_testrun'))
        $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'Validate'
        (& $v.Verify $v.Context) | Should -BeFalse
    }
    It 'Validate fails in production when the post-change report is not Success (e.g. Incomplete or a stack mismatch)' {
        Use-ValidateMocks -PostReport (New-PostReport -Outcome 'Incomplete' -Complete $false)
        $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'Validate'
        (& $v.Verify $v.Context) | Should -BeFalse
        $v.Context.PostReport.outcome | Should -Be 'Incomplete'
    }
    It 'Validate fails when the read-only validation suite fails' {
        Use-ValidateMocks -SuitePasses $false
        $v = (Plan -Report (New-TestReport)) | Where-Object Name -eq 'Validate'
        (& $v.Verify $v.Context) | Should -BeFalse
    }
    It 'staging Validate fails on a HashMismatch even with a draft manifest' {
        $f = @([pscustomobject]@{ Kind = 'HashMismatch'; Path = 'C:\xampp\php\php8ts.dll' }, [pscustomobject]@{ Kind = 'ManifestNotFinal'; Path = '' })
        Use-ValidateMocks -PostReport (New-PostReport -Outcome 'StopAndReport' -Status 'Unknown' -Findings $f)
        $v = (Plan -Report (New-TestReport) -Staging) | Where-Object Name -eq 'Validate'
        (& $v.Verify $v.Context) | Should -BeFalse
    }
    It 'staging Validate passes when the only runtime finding is ManifestNotFinal' {
        Use-ValidateMocks -PostReport (New-PostReport -Outcome 'StopAndReport' -Status 'Unknown' -Findings @([pscustomobject]@{ Kind = 'ManifestNotFinal'; Path = '' }))
        $v = (Plan -Report (New-TestReport) -Staging) | Where-Object Name -eq 'Validate'
        (& $v.Verify $v.Context) | Should -BeTrue
    }
    It 'rolls the retention checkpoint back to the captured prior state, and a failing rollback yields RECOVERY REQUIRED' {
        Set-LoamsMode -Mode Converge
        try {
            $global:LoamsTTasks = @{}; $global:LoamsTRemoveFails = $true
            Mock -ModuleName LoamsHost Get-LoamsScheduledTaskXml { if ($global:LoamsTTasks.ContainsKey($TaskName)) { $global:LoamsTTasks[$TaskName] } else { $null } }
            Mock -ModuleName LoamsHost Register-LoamsScheduledTaskXml { $global:LoamsTTasks[$TaskName] = $Xml }
            Mock -ModuleName LoamsHost Remove-LoamsScheduledTask { if ($global:LoamsTRemoveFails) { throw 'Unregister-ScheduledTask: Access is denied' }; $global:LoamsTTasks.Remove($TaskName) }
            $ret = (Plan -Report (New-TestReport) -Pd (Join-Path $TestDrive 'pd-ret17')) | Where-Object Name -eq 'BackupRetentionTask'
            $fail = New-LoamsCheckpoint -Name 'Next' -ManualRestore 'none' -Do { throw 'later failure' } -Verify { $true } -Undo { }
            $r = Invoke-LoamsCheckpointPlan -Checkpoints @($ret, $fail) -StatePath (Join-Path $TestDrive 'ret17-state.json')
            $r.Outcome | Should -Be 'RecoveryRequired'
            $r.RollbackFailures[0].Checkpoint | Should -Be 'BackupRetentionTask'
            $global:LoamsTRemoveFails = $false
            $r2 = Invoke-LoamsCheckpointPlan -Checkpoints @($ret, $fail) -StatePath (Join-Path $TestDrive 'ret17-state2.json')
            $r2.Outcome | Should -Be 'RolledBack'
            $global:LoamsTTasks.ContainsKey('LOAMS Backup Retention') | Should -BeFalse
        } finally { Set-LoamsMode -Mode Report }
    }
}

Describe 'Test-LoamsServerHost.ps1 parameters' {
    It 'requires -AcknowledgedReport and -RecoveryCertSha256 with -Converge' {
        $cmd = Get-Command $script:Entry
        $cmd.Parameters['AcknowledgedReport'].ParameterSets['Converge'].IsMandatory | Should -BeTrue
        $cmd.Parameters['RecoveryCertSha256'].ParameterSets['Converge'].IsMandatory | Should -BeTrue
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\Converge.Tests.ps1 -Output Detailed"
```

Expected: FAIL — `Invoke-LoamsHostConverge` not recognised.

- [ ] **Step 3: Implement `deploy/server/LoamsHost/LoamsHost.Converge.ps1`**

```powershell
# -Converge: report-first, authorized, encrypted backup + verified off-host copy, drift re-check,
# checkpointed Apache + MariaDB identity/ACL convergence, read-only validation (spec §4 + owner changes).

$script:LoamsMandatoryProductionCheckpoints = @('Acl', 'MariaDbAcl', 'MariaDbServiceAccount', 'MariaDbVerification', 'ApacheServiceAccount', 'Validate')
$script:LoamsMandatoryStagingCheckpoints = @('Validate')

function Get-LoamsConvergePreflight {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Report, [Parameter(Mandatory)] $Gate, [Parameter(Mandatory)] $Ack, [switch] $Staging, [string[]] $Additional = @())
    $p = @()
    if (-not $Report.elevated) { $p += 'not elevated: run as Administrator' }
    if (-not $Report.manifest.valid) { $p += 'manifest invalid: ' + (@($Report.manifest.errors) -join '; ') }
    if (-not $Gate.Allowed) { $p += $Gate.Reason }
    if (-not $Ack.Ok) { $p += $Ack.Reason }
    $c = $Report.classification
    if ($c.StopAndReport) { $p += "Apache situation $($c.Situation) requires stop and report: " + (@($c.Reasons) -join '; ') }
    $d = $Report.mariaDb.classification
    if ($d.StopAndReport) { $p += "MariaDB situation $($d.Situation) requires stop and report: " + (@($d.Reasons) -join '; ') }
    if (-not $Staging -and @($Report.processes).Count -gt 0 -and $Report.runtime.Status -ne 'Match') {
        $p += "runtime stack is $($Report.runtime.Status) against the manifest: upgrade per runbook Part B (staged) before converging"
    }
    $p += @($Additional | Where-Object { $_ })
    return [pscustomobject]@{ Ok = ($p.Count -eq 0); Problems = $p }
}

function Test-LoamsScheduledTaskPresent {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $TaskName)
    return ($null -ne (Get-LoamsScheduledTaskXml -TaskName $TaskName))
}

function Test-LoamsPostChangeReportClean {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Report, [switch] $Staging)
    $r = @()
    if (-not $Report.elevated) { $r += 'post-change report was not run elevated' }
    if ($Report.classification.Situation -ne 'Converged') { $r += "Apache is $($Report.classification.Situation), not Converged" }
    if ($Report.mariaDb.classification.Situation -ne 'Converged') { $r += "MariaDB is $($Report.mariaDb.classification.Situation), not Converged" }
    if ($null -eq $Report.acl -or -not $Report.acl.Compliant) { $r += 'Apache ACL profile not compliant' }
    if ($null -eq $Report.mariaDb.acl -or -not $Report.mariaDb.acl.Compliant) { $r += 'MariaDB ACL profile not compliant' }
    if (-not $Report.eventSource.Registered) { $r += 'LOAMS-Transport event source not registered' }
    if (@($Report.mariaDb.leftoverVerifySchemas).Count -gt 0) { $r += 'MariaDB verification schemas left behind: ' + (@($Report.mariaDb.leftoverVerifySchemas) -join ', ') }
    if (-not $Report.inventoryComplete) { $r += 'loaded-module inventory incomplete' }
    if ($Staging) {
        $bad = @(@($Report.runtime.Findings) | Where-Object { $_.Kind -ne 'ManifestNotFinal' })
        if ($bad.Count -gt 0) { $r += ('runtime findings: ' + (($bad | ForEach-Object { "$($_.Kind) $($_.Path)" }) -join '; ')) }
    } elseif ($Report.outcome -ne 'Success') {
        $r += "post-change report outcome is $($Report.outcome)"
    }
    return [pscustomobject]@{ Clean = ($r.Count -eq 0); Reasons = $r }
}

function Test-LoamsProductionPlan {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object[]] $Plan, [switch] $Staging)
    $names = @($Plan | ForEach-Object { $_.Name })
    $required = $script:LoamsMandatoryProductionCheckpoints
    if ($Staging) { $required = $script:LoamsMandatoryStagingCheckpoints }
    return , @($required | Where-Object { $names -notcontains $_ })
}

function New-LoamsConvergePlan {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] $Report, [Parameter(Mandatory)] $Backup, [Parameter(Mandatory)] $Manifest, [Parameter(Mandatory)][string] $ManifestPath,
        [Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string] $ProgramDataRoot,
        [Parameter(Mandatory)][string] $BaseUrl, [Parameter(Mandatory)] $ConnectionInfo, [switch] $Staging
    )
    $a = $Report.classification
    $d = $Report.mariaDb.classification
    $dbLayout = $Report.mariaDb.layout
    $apacheProfile = Get-LoamsAclProfile -XamppRoot $XamppRoot -ProgramDataRoot $ProgramDataRoot -ServiceName $a.ServiceName -BridgeAccount (Get-LoamsBridgeTaskAccount)
    $dbProfile = Get-LoamsMariaDbAclProfile -XamppRoot $XamppRoot -Layout $dbLayout -ServiceName $d.ServiceName
    $dbPaths = @($dbProfile | ForEach-Object { $_.Path })
    $ctx = @{
        ServiceName = $a.ServiceName; TargetAccount = $a.TargetAccount; OriginalAccount = $a.CurrentAccount; Situation = $a.Situation
        HttpdExe = $Report.layout.HttpdExe; Processes = @($Report.processes); ApiRoot = $Report.layout.ApiRoot
        DbServiceName = $d.ServiceName; DbTargetAccount = $d.TargetAccount; DbOriginalAccount = $d.CurrentAccount; DbSituation = $d.Situation
        DbLayout = $dbLayout; DbProcesses = @($Report.mariaDb.processes)
        XamppRoot = $XamppRoot; ProgramDataRoot = $ProgramDataRoot; BaseUrl = ($BaseUrl.TrimEnd('/') + '/')
        BackupPath = $Backup.Path; WorkDirectory = (Join-Path $Backup.Path 'logs')
        ApacheAclSaves = @($Backup.AclSaves | Where-Object { $dbPaths -notcontains $_.path })
        DbAclSaves = @($Backup.AclSaves | Where-Object { $dbPaths -contains $_.path })
        Manifest = $Manifest; ManifestPath = $ManifestPath; Staging = [bool]$Staging; ConnectionInfo = $ConnectionInfo
        PriorState = $Backup.PriorState; PostReport = $null; VerifySchemas = @($Backup.VerifySchemas); DbVerification = $null
        ApacheProfile = $apacheProfile; ApacheSid = (Get-LoamsServiceSid -ServiceName $a.ServiceName)
        DbProfile = $dbProfile; DbSid = (Get-LoamsServiceSid -ServiceName $d.ServiceName)
        CreatedPaths = @(); EventSourceCreated = $false; ApacheWasRunning = $false; DbWasRunning = $false
    }
    $orig = { param($acct) if ($acct) { $acct } else { 'LocalSystem' } }
    $plan = @()
    if ($a.Situation -in @('FreshInstall', 'ControlPanel')) {
        $plan += New-LoamsCheckpoint -Name 'ApacheServiceRegistration' -Context $ctx `
            -ManualRestore "Elevated: '$($ctx.HttpdExe)' -k uninstall -n $($ctx.ServiceName); then start Apache from the XAMPP Control Panel." `
            -Do { param($x)
                if ($x.Situation -eq 'ControlPanel') { Stop-LoamsControlPanelProcesses -Processes $x.Processes }
                Register-LoamsApacheService -HttpdExe $x.HttpdExe -ServiceName $x.ServiceName } `
            -Verify { param($x) (Get-LoamsServiceSnapshot -ServiceName $x.ServiceName).Exists } `
            -Undo { param($x)
                $s = Get-LoamsServiceSnapshot -ServiceName $x.ServiceName
                if ($s.Exists) {
                    if ($s.State -eq 'Running') { Stop-LoamsWindowsService -ServiceName $x.ServiceName }
                    Unregister-LoamsApacheService -HttpdExe $x.HttpdExe -ServiceName $x.ServiceName
                }
                if ($x.Situation -eq 'ControlPanel') { Start-LoamsControlPanelHttpd -HttpdExe $x.HttpdExe } }
    }
    if ($d.Situation -in @('FreshInstall', 'ControlPanel')) {
        $plan += New-LoamsCheckpoint -Name 'MariaDbServiceRegistration' -Context $ctx `
            -ManualRestore "Elevated: '$($dbLayout.MysqldExe)' --remove $($ctx.DbServiceName); then start MySQL from the XAMPP Control Panel." `
            -Do { param($x)
                if ($x.DbSituation -eq 'ControlPanel') {
                    $cnf = New-LoamsMysqlDefaultsFile -Directory $x.WorkDirectory -ConnectionInfo $x.ConnectionInfo
                    try { Stop-LoamsControlPanelMysqld -MysqladminExe $x.DbLayout.MysqladminExe -DefaultsFile $cnf -Processes $x.DbProcesses -Port $x.DbLayout.Port -DataDir $x.DbLayout.DataDir }
                    finally { Remove-LoamsMysqlDefaultsFile -Path $cnf }
                }
                Register-LoamsMariaDbService -MysqldExe $x.DbLayout.MysqldExe -ServiceName $x.DbServiceName -MyIni $x.DbLayout.MyIni } `
            -Verify { param($x) (Get-LoamsServiceSnapshot -ServiceName $x.DbServiceName).Exists } `
            -Undo { param($x)
                $s = Get-LoamsServiceSnapshot -ServiceName $x.DbServiceName
                if ($s.Exists) {
                    if ($s.State -eq 'Running') { Stop-LoamsWindowsService -ServiceName $x.DbServiceName -TimeoutSec 120 }
                    Unregister-LoamsMariaDbService -MysqldExe $x.DbLayout.MysqldExe -ServiceName $x.DbServiceName
                }
                if ($x.DbSituation -eq 'ControlPanel') { Start-LoamsControlPanelMysqld -MysqldExe $x.DbLayout.MysqldExe -MyIni $x.DbLayout.MyIni } }
    }
    $plan += New-LoamsCheckpoint -Name 'ServerLayout' -Context $ctx `
        -ManualRestore "Delete only the folders/files under '$ProgramDataRoot' that converge.log lists as created by this run." `
        -Do { param($x) $x.CreatedPaths = @(Initialize-LoamsServerLayout -ProgramDataRoot $x.ProgramDataRoot) } `
        -Verify { param($x) (Test-Path -LiteralPath (Join-Path $x.ProgramDataRoot 'server\logs\compat-guard.log')) -and (Test-Path -LiteralPath (Join-Path $x.ProgramDataRoot 'server\tls\private')) } `
        -Undo { param($x)
            Assert-LoamsMutationAllowed -Action 'remove created layout'
            foreach ($p in @($x.CreatedPaths | Sort-Object Length -Descending)) { if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force } }
            $x.CreatedPaths = @() }
    $plan += New-LoamsCheckpoint -Name 'Acl' -Context $ctx `
        -ManualRestore "For each Apache-side rollback\acl-NN.txt in '$($Backup.Path)': icacls <parent> /restore <file> /c (Restore-LoamsHostBackup.ps1 does this)." `
        -Do { param($x) foreach ($e in $x.ApacheProfile) { Set-LoamsAclEntry -Entry $e } } `
        -Verify { param($x) (Test-LoamsAclCompliance -AclProfile $x.ApacheProfile -ServiceSid $x.ApacheSid).Compliant } `
        -Undo { param($x) foreach ($s in $x.ApacheAclSaves) { Restore-LoamsAclSave -BackupPath $x.BackupPath -AclSave $s } }
    $plan += New-LoamsCheckpoint -Name 'MariaDbAcl' -Context $ctx `
        -ManualRestore "For each MariaDB rollback\acl-NN.txt in '$($Backup.Path)': icacls <parent> /restore <file> /c." `
        -Do { param($x) foreach ($e in $x.DbProfile) { Set-LoamsAclEntry -Entry $e } } `
        -Verify { param($x) (Test-LoamsAclCompliance -AclProfile $x.DbProfile -ServiceSid $x.DbSid).Compliant } `
        -Undo { param($x) foreach ($s in $x.DbAclSaves) { Restore-LoamsAclSave -BackupPath $x.BackupPath -AclSave $s } }
    $plan += New-LoamsCheckpoint -Name 'EventSource' -Context $ctx `
        -ManualRestore 'Only if converge.log says this run created it: Remove-EventLog -Source LOAMS-Transport (elevated).' `
        -Do { param($x) $x.EventSourceCreated = (Register-LoamsEventSource).Created } `
        -Verify { param($x) (Get-LoamsEventSourceState).Registered } `
        -Undo { param($x) if ($x.EventSourceCreated) { Unregister-LoamsEventSource; $x.EventSourceCreated = $false } }
    $plan += New-LoamsCheckpoint -Name 'BackupRetentionTask' -Context $ctx `
        -ManualRestore "Run the backup set's Restore-LoamsHostBackup.ps1 (step 'restore retention task and tools'), or by hand: re-register rollback\retention-task.xml (or remove the task if the set has none) and restore rollback\tools\ over $ProgramDataRoot\tools." `
        -Do { param($x) Register-LoamsBackupRetentionTask -ProgramDataRoot $x.ProgramDataRoot | Out-Null } `
        -Verify { param($x) Test-LoamsScheduledTaskPresent -TaskName 'LOAMS Backup Retention' } `
        -Undo { param($x) Restore-LoamsRetentionTaskState -ProgramDataRoot $x.ProgramDataRoot -BackupPath $x.BackupPath -Prior $x.PriorState }
    $plan += New-LoamsCheckpoint -Name 'MariaDbServiceAccount' -Context $ctx `
        -ManualRestore "Elevated: sc.exe config $($ctx.DbServiceName) obj= `"$(& $orig $ctx.DbOriginalAccount)`" password= `"`"; then Start-Service $($ctx.DbServiceName)." `
        -Do { param($x)
            $s = Get-LoamsServiceSnapshot -ServiceName $x.DbServiceName
            $x.DbWasRunning = ($s.State -eq 'Running')
            if ($x.DbWasRunning) { Stop-LoamsWindowsService -ServiceName $x.DbServiceName -TimeoutSec 120 }
            Set-LoamsServiceAccount -ServiceName $x.DbServiceName -Account $x.DbTargetAccount
            Start-LoamsWindowsService -ServiceName $x.DbServiceName -TimeoutSec 120 } `
        -Verify { param($x)
            $s = Get-LoamsServiceSnapshot -ServiceName $x.DbServiceName
            ($s.StartName -eq $x.DbTargetAccount) -and ($s.State -eq 'Running') } `
        -Undo { param($x)
            $s = Get-LoamsServiceSnapshot -ServiceName $x.DbServiceName
            if (-not $s.Exists) { return }
            if ($s.State -eq 'Running') { Stop-LoamsWindowsService -ServiceName $x.DbServiceName -TimeoutSec 120 }
            Set-LoamsServiceAccount -ServiceName $x.DbServiceName -Account (& $orig $x.DbOriginalAccount)
            if ($x.DbWasRunning -and $x.DbSituation -notin @('FreshInstall', 'ControlPanel')) { Start-LoamsWindowsService -ServiceName $x.DbServiceName -TimeoutSec 120 } }
    $plan += New-LoamsCheckpoint -Name 'MariaDbVerification' -Context $ctx `
        -ManualRestore "With MariaDB running, elevated: mysql.exe --defaults-extra-file=<admin-only file> -e `"$((@($ctx.VerifySchemas) | ForEach-Object { "DROP DATABASE IF EXISTS $_;" }) -join ' ')`" (or run the backup set's Restore-LoamsHostBackup.ps1)." `
        -Do { param($x)
            $x.DbVerification = Invoke-LoamsDbVerification -MysqlExe $x.DbLayout.MysqlExe -MysqldumpExe $x.DbLayout.MysqldumpExe -ConnectionInfo $x.ConnectionInfo `
                -WorkDirectory $x.WorkDirectory -Schema $x.VerifySchemas[0] -RestoreSchema $x.VerifySchemas[1] } `
        -Verify { param($x) [bool]$x.DbVerification.Passed } `
        -Undo { param($x)
            try { Remove-LoamsVerifySchemas -MysqlExe $x.DbLayout.MysqlExe -ConnectionInfo $x.ConnectionInfo -WorkDirectory $x.WorkDirectory -Schemas @($x.VerifySchemas) }
            finally { Clear-LoamsCredentialFiles } }
    $plan += New-LoamsCheckpoint -Name 'ApacheServiceAccount' -Context $ctx `
        -ManualRestore "Elevated: sc.exe config $($ctx.ServiceName) obj= `"$(& $orig $ctx.OriginalAccount)`" password= `"`"; then Start-Service $($ctx.ServiceName)." `
        -Do { param($x)
            $s = Get-LoamsServiceSnapshot -ServiceName $x.ServiceName
            $x.ApacheWasRunning = ($s.State -eq 'Running')
            if ($x.ApacheWasRunning) { Stop-LoamsWindowsService -ServiceName $x.ServiceName }
            Set-LoamsServiceAccount -ServiceName $x.ServiceName -Account $x.TargetAccount
            Start-LoamsWindowsService -ServiceName $x.ServiceName } `
        -Verify { param($x)
            $s = Get-LoamsServiceSnapshot -ServiceName $x.ServiceName
            ($s.StartName -eq $x.TargetAccount) -and ($s.State -eq 'Running') -and ((Invoke-LoamsHttpGet -Url ($x.BaseUrl + 'get_branding.php')).StatusCode -eq 200) } `
        -Undo { param($x)
            $s = Get-LoamsServiceSnapshot -ServiceName $x.ServiceName
            if (-not $s.Exists) { return }
            if ($s.State -eq 'Running') { Stop-LoamsWindowsService -ServiceName $x.ServiceName }
            Set-LoamsServiceAccount -ServiceName $x.ServiceName -Account (& $orig $x.OriginalAccount)
            if ($x.ApacheWasRunning -and $x.Situation -notin @('FreshInstall', 'ControlPanel')) { Start-LoamsWindowsService -ServiceName $x.ServiceName } }
    $plan += New-LoamsCheckpoint -Name 'Validate' -Context $ctx -ManualRestore 'Validation only; nothing to undo.' `
        -Do { param($x) } `
        -Verify { param($x)
            foreach ($svc in @(@{ n = $x.ServiceName; t = $x.TargetAccount }, @{ n = $x.DbServiceName; t = $x.DbTargetAccount })) {
                $s = Get-LoamsServiceSnapshot -ServiceName $svc.n
                if ($s.StartName -ne $svc.t -or $s.State -ne 'Running') { Write-LoamsLog -Level Error -Message "Validate: $($svc.n) not running as $($svc.t)"; return $false }
            }
            $suite = Invoke-LoamsPostConvergeValidation -BaseUrl $x.BaseUrl -ApiRoot $x.ApiRoot -ApacheServiceName $x.ServiceName -MariaDbServiceName $x.DbServiceName `
                -MysqlExe $x.DbLayout.MysqlExe -ConnectionInfo $x.ConnectionInfo -WorkDirectory $x.WorkDirectory
            if (-not $suite.Passed) { Write-LoamsLog -Level Error -Message 'Validate: read-only validation suite failed'; return $false }
            # The full post-change report gates the outcome: identities, both ACL profiles, event source, module inventory, runtime.
            $x.PostReport = New-LoamsHostReport -XamppRoot $x.XamppRoot -ManifestPath $x.ManifestPath -ProgramDataRoot $x.ProgramDataRoot -ServiceName $x.ServiceName -MariaDbServiceName $x.DbServiceName
            $clean = Test-LoamsPostChangeReportClean -Report $x.PostReport -Staging:$x.Staging
            foreach ($reason in $clean.Reasons) { Write-LoamsLog -Level Error -Message "Validate: $reason" }
            return $clean.Clean } `
        -Undo { param($x) }
    return , $plan
}

function Write-LoamsConvergeOutcome {
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Result, [Parameter(Mandatory)][string] $BackupPath, [string] $RestoreScript = '')
    switch ($Result.Outcome) {
        'Success' { Write-LoamsLog -Message ("CONVERGE SUCCEEDED - checkpoints: " + (@($Result.Completed) -join ', ')) }
        'SuccessReportNotSaved' {
            Write-LoamsLog -Level Warn -Message ("CONVERGE SUCCEEDED but the post-change report could not be saved to '{0}': {1}. Exit code 6. Evidence copy in the backup set: {2}." -f $Result.ReportPath, $Result.Failure, $(if ($Result.EvidenceSaved) { 'logs\post-change-report.json' } else { 'NOT saved' }))
        }
        'RolledBack' {
            Write-LoamsLog -Level Warn -Message ("CONVERGE FAILED at checkpoint '{0}': {1}. Every applied step was rolled back automatically; the host is in its pre-change state. Backup: {2}" -f $Result.FailedCheckpoint, $Result.Failure, $BackupPath)
        }
        'RecoveryRequired' {
            $doc = [ordered]@{
                state = 'RECOVERY REQUIRED'; failedCheckpoint = $Result.FailedCheckpoint; failure = $Result.Failure
                rollbackFailures = @($Result.RollbackFailures); manualSteps = @($Result.ManualSteps)
                restoreScript = $RestoreScript; backupPath = $BackupPath
                runbook = 'docs/security/runbooks/stack-upgrade-staging.md Part D'
            }
            [IO.File]::WriteAllText((Join-Path $BackupPath 'recovery-required.json'), (Protect-LoamsText -Text ($doc | ConvertTo-Json -Depth 4)), (New-Object System.Text.UTF8Encoding($false)))
            Write-LoamsLog -Level Error -Message "RECOVERY REQUIRED - failed checkpoint: $($Result.FailedCheckpoint) ($($Result.Failure))"
            foreach ($f in @($Result.RollbackFailures)) { Write-LoamsLog -Level Error -Message "  rollback of '$($f.Checkpoint)' failed: $($f.Error)" }
            Write-LoamsLog -Level Error -Message 'Manual restore steps:'
            foreach ($m in @($Result.ManualSteps)) { Write-LoamsLog -Level Error -Message "  - $m" }
            Write-LoamsLog -Level Error -Message "Restore script: $RestoreScript ; details: $(Join-Path $BackupPath 'recovery-required.json')"
            Write-LoamsLog -Level Error -Message 'The host is NOT in a known-good state. Start the manual-attendance contingency now.'
        }
    }
}

function Invoke-LoamsHostConverge {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $XamppRoot, [Parameter(Mandatory)][string] $ManifestPath,
        [string] $ServiceName = 'Apache2.4', [string] $MariaDbServiceName = 'mysql',
        [Parameter(Mandatory)][string] $ProgramDataRoot, [Parameter(Mandatory)][string] $BackupRoot,
        [Parameter(Mandatory)][AllowEmptyString()][string] $AcknowledgedReport, [Parameter(Mandatory)][string] $ReportPath,
        [Parameter(Mandatory)][string] $RecoveryCertificate, [Parameter(Mandatory)][string] $RecoveryCertSha256,
        [string] $OffHostDestination = '', [switch] $OffHostAttestation,
        [switch] $Staging, [switch] $BackupOnly, [pscredential] $DbCredential,
        [string] $BaseUrl = 'http://127.0.0.1/loams_api/'
    )
    $refuse = { param([string[]] $problems, [string] $backupPath = '')
        Write-LoamsLog -Level Error -Message 'CONVERGE REFUSED - no change was made to the host:'
        foreach ($p in $problems) { Write-LoamsLog -Level Error -Message "  - $p" }
        [pscustomobject]@{ Outcome = 'Refused'; Problems = @($problems); BackupPath = $backupPath; Result = $null } }

    Set-LoamsMode -Mode Report
    $report = New-LoamsHostReport -XamppRoot $XamppRoot -ManifestPath $ManifestPath -ProgramDataRoot $ProgramDataRoot -ServiceName $ServiceName -MariaDbServiceName $MariaDbServiceName
    $manifest = $null
    $gate = [pscustomobject]@{ Allowed = $false; Reason = 'manifest invalid' }
    if ($report.manifest.valid) {
        $manifest = Read-LoamsStackManifest -Path $ManifestPath
        $isStaging = $false; $stagingReason = ''
        if ($Staging) { $auth = Test-LoamsStagingAuthorization -ProgramDataRoot $ProgramDataRoot; $isStaging = $auth.Authorized; $stagingReason = $auth.Reason }
        $gate = Test-LoamsManifestStatusGate -Manifest $manifest -Staging:$Staging -IsStagingHost $isStaging
        if (-not $gate.Allowed -and $Staging -and -not $isStaging) { $gate = [pscustomobject]@{ Allowed = $false; Reason = "$($gate.Reason): $stagingReason" } }
    }
    $ack = Test-LoamsAcknowledgedReport -Path $AcknowledgedReport -CurrentReport $report

    $prereq = @()
    $openssl = $null; $recipient = $null
    if ($manifest) { try { $openssl = Get-LoamsPinnedOpenSsl -Manifest $manifest -XamppRoot $XamppRoot } catch { $prereq += $_.Exception.Message } }
    try { $recipient = Test-LoamsRecipientCertificate -Path $RecoveryCertificate -ExpectedSha256 $RecoveryCertSha256 } catch { $prereq += $_.Exception.Message }
    if (-not $OffHostDestination -and -not $OffHostAttestation) {
        $prereq += 'no off-host copy target: pass -OffHostDestination <removable, USB or approved network path> or -OffHostAttestation; the only recovery copy must never stay on this machine'
    } elseif ($OffHostDestination) {
        $kind = Get-LoamsDestinationKind -Path $OffHostDestination
        if ($kind -in @('LocalFixed', 'Unknown')) { $prereq += "-OffHostDestination '$OffHostDestination' ($kind) is not an off-host location" }
    }
    if ($report.mariaDb.layout.IsXampp -and -not $report.mariaDb.classification.StopAndReport) {
        try {
            $dbName = Get-LoamsPhpDefine -Text (Get-Content -Raw -LiteralPath (Join-Path $report.layout.ApiRoot 'config.php') -ErrorAction Stop) -Name 'DB_NAME'
            $ready = Get-LoamsDatabaseBackupReadiness -MariaDbLayout $report.mariaDb.layout -Processes @($report.mariaDb.processes) -ApplicationDatabase ([string]$dbName) -ServiceName $report.mariaDb.classification.ServiceName
            if (-not $ready.Ready) { $prereq += $ready.Reason }
        } catch {
            $prereq += "database backup readiness could not be checked: $($_.Exception.Message)"
        }
    }
    if (-not $BackupOnly) {
        # The final report location is checked before any backup or checkpoint, never after the host has changed.
        $reportLoc = Test-LoamsReportLocation -Path $ReportPath -ProgramDataRoot $ProgramDataRoot
        if (-not $reportLoc.Ok) { $prereq += $reportLoc.Problems }
    }
    $pre = Get-LoamsConvergePreflight -Report $report -Gate $gate -Ack $ack -Staging:$Staging -Additional $prereq
    if (-not $pre.Ok) { return (& $refuse $pre.Problems) }
    if (-not (Confirm-LoamsOperatorHost)) { return (& $refuse @('the operator did not confirm this computer name')) }

    Set-LoamsMode -Mode Converge
    try {
        $dbLayout = $report.mariaDb.layout
        $aclProfile = @(Get-LoamsAclProfile -XamppRoot $XamppRoot -ProgramDataRoot $ProgramDataRoot -ServiceName $report.classification.ServiceName -BridgeAccount (Get-LoamsBridgeTaskAccount)) +
                      @(Get-LoamsMariaDbAclProfile -XamppRoot $XamppRoot -Layout $dbLayout -ServiceName $report.mariaDb.classification.ServiceName)
        try {
            $conn = Get-LoamsDbConnectionInfo -ApiRoot $report.layout.ApiRoot -DbCredential $DbCredential
            $backup = New-LoamsBackupSet -Layout $report.layout -MariaDbLayout $dbLayout -ApacheServiceName $report.classification.ServiceName `
                -MariaDbServiceName $report.mariaDb.classification.ServiceName -ApacheSituation $report.classification.Situation `
                -MariaDbSituation $report.mariaDb.classification.Situation -BackupRoot $BackupRoot -ProgramDataRoot $ProgramDataRoot `
                -ConnectionInfo $conn -AclProfile $aclProfile -OpenSslExe $openssl -Recipient $recipient -ReportPath $AcknowledgedReport
        } catch {
            $msg = Protect-LoamsText -Text $_.Exception.Message
            $sweepError = $null
            try { Clear-LoamsCredentialFiles } catch { $sweepError = $_.Exception.Message }
            if ($sweepError) {
                Write-LoamsLog -Level Error -Message "RECOVERY REQUIRED - the backup failed and run-owned credential files remain: $sweepError"
                return [pscustomobject]@{ Outcome = 'RecoveryRequired'; Problems = @("backup not verified: $msg", $sweepError); BackupPath = ''; Result = $null }
            }
            return (& $refuse @("backup not verified: $msg"))
        }
        try {
            if ($OffHostDestination) {
                Export-LoamsBackupSet -BackupPath $backup.Path -Destination $OffHostDestination | Out-Null
            } else {
                $op = Read-LoamsOperatorInput -Prompt 'Operator name or role recording the off-host copy'
                $loc = Read-LoamsOperatorInput -Prompt 'Approved off-host location (description, no secrets)'
                $typed = Read-LoamsOperatorInput -Prompt "SHA-256 of SHA256SUMS.json measured on the off-host copy of $(Split-Path -Leaf $backup.Path)"
                Set-LoamsOffHostAttestation -BackupPath $backup.Path -Operator $op -Location $loc -TypedSumsSha256 $typed | Out-Null
            }
            $off = Test-LoamsOffHostVerification -BackupPath $backup.Path
            if (-not $off.Verified) { throw $off.Reason }
        } catch {
            return (& $refuse @("off-host copy not verified: $($_.Exception.Message)") $backup.Path)
        }
        if ($BackupOnly) {
            Write-LoamsLog -Message "BACKUP ONLY - encrypted backup at $($backup.Path), off-host copy verified; no host change was made."
            return [pscustomobject]@{ Outcome = 'Success'; Problems = @(); BackupPath = $backup.Path; Result = $null }
        }
        $live = Get-LoamsLiveFingerprint -XamppRoot $XamppRoot -ProgramDataRoot $ProgramDataRoot -ApacheServiceName $report.classification.ServiceName -MariaDbServiceName $report.mariaDb.classification.ServiceName `
            -MariaDbLayout $dbLayout -AclPaths (Get-LoamsFingerprintAclPaths -AclProfile $aclProfile -ProgramDataRoot $ProgramDataRoot)
        $drift = Test-LoamsPreChangeDrift -Acknowledged $ack.Report -Live $live
        if (-not $drift.NoDrift) { return (& $refuse (@('host drifted since the acknowledged report:') + $drift.Differences) $backup.Path) }

        $plan = New-LoamsConvergePlan -Report $report -Backup $backup -Manifest $manifest -ManifestPath $ManifestPath -XamppRoot $XamppRoot -ProgramDataRoot $ProgramDataRoot -BaseUrl $BaseUrl -ConnectionInfo $conn -Staging:$Staging
        $missing = Test-LoamsProductionPlan -Plan $plan -Staging:$Staging
        if ($missing.Count -gt 0) { return (& $refuse @("plan lacks mandatory checkpoints: $($missing -join ', ')") $backup.Path) }
        $result = Invoke-LoamsCheckpointPlan -Checkpoints $plan -StatePath (Join-Path $backup.Path 'logs\converge-state.json')
        $validate = @($plan | Where-Object { $_.Name -eq 'Validate' }) | Select-Object -First 1
        $post = $null
        if ($null -ne $validate -and $validate.Context.ContainsKey('PostReport')) { $post = $validate.Context.PostReport }
        Set-LoamsMode -Mode Report
        $evidenceSaved = $false
        if ($null -ne $post) {
            # Kept for every outcome: after a rollback it is the evidence of why Validate failed. A save failure never escapes.
            $evidence = Join-Path $backup.Path 'logs\post-change-report.json'
            try {
                Save-LoamsHostReport -Report $post -Path $evidence
                $evidenceSaved = $true
                Write-LoamsLog -Message "Post-change report saved as evidence: $evidence (outcome $($post.outcome))"
            } catch {
                Write-LoamsLog -Level Error -Message "Post-change report could not be saved as evidence to ${evidence}: $(Protect-LoamsText -Text $_.Exception.Message)"
            }
        }
        if ($result.Outcome -eq 'Success') {
            if ($null -eq $post) { $clean = [pscustomobject]@{ Clean = $false; Reasons = @('the Validate checkpoint produced no post-change report') } }
            else { $clean = Test-LoamsPostChangeReportClean -Report $post -Staging:$Staging }
            if ($clean.Clean) {
                try {
                    Save-LoamsHostReport -Report $post -Path $ReportPath -ProgramDataRoot $ProgramDataRoot
                    Write-LoamsLog -Message "Post-change report: $ReportPath"
                } catch {
                    # Deterministic: the host is converged and validated; only the report file is missing (exit 6).
                    $result = [pscustomobject]@{
                        Outcome = 'SuccessReportNotSaved'; FailedCheckpoint = ''; Failure = (Protect-LoamsText -Text $_.Exception.Message)
                        Completed = $result.Completed; RollbackFailures = @(); ManualSteps = @(); ReportPath = $ReportPath; EvidenceSaved = $evidenceSaved
                    }
                }
            } else {
                # Invariant breach (unreachable while every plan contains Validate): never report success.
                $result = [pscustomobject]@{
                    Outcome = 'RecoveryRequired'; FailedCheckpoint = 'PostChangeReport'; Failure = ($clean.Reasons -join '; ')
                    Completed = $result.Completed; RollbackFailures = @()
                    ManualSteps = @("PostChangeReport: the changes were applied but no clean post-change report exists; validate manually (runbook Part C) or restore with $($backup.RestoreScript) (runbook Part D2).")
                }
            }
        }
        # End-of-run sweep: no run-owned credential file may outlive the run, whatever the outcome.
        $sweepError = $null
        try { Clear-LoamsCredentialFiles } catch { $sweepError = $_.Exception.Message }
        if ($sweepError) {
            $result = [pscustomobject]@{
                Outcome = 'RecoveryRequired'; FailedCheckpoint = 'CredentialCleanup'; Failure = $sweepError
                Completed = $result.Completed; RollbackFailures = @([pscustomobject]@{ Checkpoint = 'CredentialCleanup'; Error = $sweepError })
                ManualSteps = @("CredentialCleanup: overwrite and delete the listed mysql-client-*.cnf files now (they contain the DB password), then re-run -Report.")
            }
        }
        Write-LoamsConvergeOutcome -Result $result -BackupPath $backup.Path -RestoreScript $backup.RestoreScript
        return [pscustomobject]@{ Outcome = $result.Outcome; Problems = @(); BackupPath = $backup.Path; Result = $result }
    } finally {
        Set-LoamsMode -Mode Report
        Clear-LoamsSecrets
        Set-LoamsLogPath -Path ''
    }
}
```

- [ ] **Step 4: Replace `deploy/server/Test-LoamsServerHost.ps1` with the final entry script**

```powershell
#Requires -Version 5.1
<#
.SYNOPSIS
    LOAMS server host check and convergence (spec S1 section 4).
.DESCRIPTION
    -Report (default) is read-only.
    -Converge changes the host (Apache and MariaDB service registration and
    NT SERVICE virtual accounts, least-privilege ACLs incl. the MariaDB data
    directory, LOAMS-Transport event source, backup retention task) only after:
    an approved manifest (or -Staging on an authorized staging host), a prior
    elevated -Report from this computer whose fingerprint still matches
    (-AcknowledgedReport, <= 72 h), the pinned OpenSSL and recovery certificate,
    typed confirmation of the computer name, an encrypted verified backup, a
    verified off-host copy, and a fresh drift re-check. Recoverable, not
    atomic: automatic rollback, or RECOVERY REQUIRED with manual steps.
    Exit codes: 0 Success, 1 Refused, 2 StopAndReport, 3 Incomplete,
    4 RecoveryRequired, 5 RolledBack, 6 SuccessReportNotSaved.
#>
[CmdletBinding(DefaultParameterSetName = 'Report')]
param(
    [Parameter(ParameterSetName = 'Report')][switch] $Report,
    [Parameter(ParameterSetName = 'Converge', Mandatory)][switch] $Converge,
    [string] $XamppRoot = 'C:\xampp',
    [string] $ManifestPath = (Join-Path $PSScriptRoot '..\stack\loams-stack-manifest.json'),
    [string] $ServiceName = 'Apache2.4',
    [string] $MariaDbServiceName = 'mysql',
    [string] $ProgramDataRoot = (Join-Path $env:ProgramData 'LOAMS'),
    [string] $ReportPath = '',
    [Parameter(ParameterSetName = 'Converge', Mandatory)][string] $AcknowledgedReport,
    [Parameter(ParameterSetName = 'Converge', Mandatory)][string] $RecoveryCertSha256,
    [Parameter(ParameterSetName = 'Converge')][string] $RecoveryCertificate = (Join-Path $env:ProgramData 'LOAMS\backup-recipient\loams-backup-recovery.crt'),
    [Parameter(ParameterSetName = 'Converge')][string] $BackupRoot = (Join-Path $env:ProgramData 'LOAMS\backups'),
    [Parameter(ParameterSetName = 'Converge')][string] $OffHostDestination = '',
    [Parameter(ParameterSetName = 'Converge')][switch] $OffHostAttestation,
    [Parameter(ParameterSetName = 'Converge')][switch] $Staging,
    [Parameter(ParameterSetName = 'Converge')][switch] $BackupOnly,
    [Parameter(ParameterSetName = 'Converge')][pscredential] $DbCredential,
    [Parameter(ParameterSetName = 'Converge')][string] $BaseUrl = 'http://127.0.0.1/loams_api/'
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'LoamsHost\LoamsHost.psm1') -Force
if (-not $ReportPath) {
    $ReportPath = Join-Path (Join-Path $ProgramDataRoot 'reports') ('loams-host-report-{0}-{1}.json' -f $env:COMPUTERNAME, [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
}
if ($PSCmdlet.ParameterSetName -eq 'Converge') {
    $result = Invoke-LoamsHostConverge -XamppRoot $XamppRoot -ManifestPath $ManifestPath -ServiceName $ServiceName -MariaDbServiceName $MariaDbServiceName `
        -ProgramDataRoot $ProgramDataRoot -BackupRoot $BackupRoot -AcknowledgedReport $AcknowledgedReport -ReportPath $ReportPath `
        -RecoveryCertificate $RecoveryCertificate -RecoveryCertSha256 $RecoveryCertSha256 -OffHostDestination $OffHostDestination `
        -OffHostAttestation:$OffHostAttestation -Staging:$Staging -BackupOnly:$BackupOnly -DbCredential $DbCredential -BaseUrl $BaseUrl
    exit (Get-LoamsExitCode -Outcome $result.Outcome)
}
$result = Invoke-LoamsHostReport -XamppRoot $XamppRoot -ManifestPath $ManifestPath -ProgramDataRoot $ProgramDataRoot -ServiceName $ServiceName -MariaDbServiceName $MariaDbServiceName -ReportPath $ReportPath
exit (Get-LoamsExitCode -Outcome $result.outcome)
```

- [ ] **Step 5: Run to verify it passes**

Same command as Step 2. Expected: PASS, 44 tests. Set `expected-test-count.txt` to `305` and run the suite runner → `PASSED: 305 tests` (Report.Tests still passes: the default parameter set is unchanged).

- [ ] **Step 6: Commit**

Via the project `commit` skill — subject: `feat(deploy): add gated, encrypted-backup, drift-checked, recoverable -Converge for Apache and MariaDB`

---

### Task 18: Staging identity probe and the staging upgrade runbook

Unit tests cannot prove what the **real** `NT SERVICE\Apache2.4` and `NT SERVICE\mysql` tokens can do, how PHP's real file-open calls behave under `(AD,S)`, that the Event Log accepts the Apache account's writes, or that an encrypted backup really restores. Those are staging verifications with exact commands (runbook Parts B and C), run by a person on an authorized staging host; this plan does not claim the Pester suite proves them. The authoritative Layer 7 check remains S1d's `Test-LoamsDeployment.ps1`.

**Files:**
- Create: `deploy/server/staging/LoamsIdentityProbe.php`
- Create: `docs/security/runbooks/stack-upgrade-staging.md`
- Create: `deploy/tests/StagingArtifacts.Tests.ps1`
- Modify: `deploy/tests/expected-test-count.txt` → `308`

**Interfaces:**
- Consumes: Tasks 1–17.
- Produces: the probe's `results.json` contract `{ probeVersion, timeUtc, whoami, groups, appendOpen, appendWrite, filePutAppend, truncateRefused, deleteRefused, renameRefused, apiConfigWriteRefused, httpdConfWriteRefused, compatIniWriteRefused, keyDirReadable, backupsListRefused, mariaDbDataListRefused, mariaDbDataReadRefused, uploadsWritable, eventLogWrite }`; the runbook (Parts A–F) referenced by every tool message.

- [ ] **Step 1: Write the failing tests `deploy/tests/StagingArtifacts.Tests.ps1`**

```powershell
#Requires -Version 5.1
BeforeAll {
    $script:Probe = Join-Path $PSScriptRoot '..\server\staging\LoamsIdentityProbe.php'
    $script:Runbook = Join-Path $PSScriptRoot '..\..\docs\security\runbooks\stack-upgrade-staging.md'
    $script:Php = $env:LOAMS_PHP
    if (-not $script:Php) { $script:Php = 'C:\xampp\php\php.exe' }
}

Describe 'Staging identity probe' {
    It 'is valid PHP (php -l); a missing PHP binary is a failure, not a skip' {
        Test-Path -LiteralPath $script:Php | Should -BeTrue -Because 'set LOAMS_PHP to a PHP 8 CLI to lint the probe'
        $out = & $script:Php -l $script:Probe 2>&1
        $LASTEXITCODE | Should -Be 0 -Because ($out | Out-String)
    }
    It 'is never placed under the web root and declares itself staging-only' {
        (Resolve-Path $script:Probe).ProviderPath | Should -Not -Match '\\(htdocs|deliverables)\\'
        Get-Content -Raw $script:Probe | Should -Match 'STAGING ONLY'
    }
}

Describe 'Staging upgrade runbook' {
    It 'contains every required part' {
        $text = Get-Content -Raw $script:Runbook
        foreach ($h in @('## Part A', '## Part B', '## Part C', '## Part D', '## Part E', '## Part F')) { $text | Should -Match ([regex]::Escape($h)) }
    }
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
powershell -NoProfile -Command "Import-Module Pester -MinimumVersion 5.5; Invoke-Pester -Path deploy\tests\StagingArtifacts.Tests.ps1 -Output Detailed"
```

Expected: FAIL — probe and runbook do not exist.

- [ ] **Step 3: Implement `deploy/server/staging/LoamsIdentityProbe.php`**

```php
<?php
/**
 * LOAMS S1b identity probe -- STAGING ONLY.
 *
 * NOT an HTTP endpoint (spec: no new diagnostic HTTP endpoint). Apache starts it
 * as a piped logger, i.e. as a child of httpd running under the Apache service's
 * own token (NT SERVICE\Apache2.4), so it observes exactly what PHP code running
 * as that identity can do. Never copy this file under htdocs or into a release.
 *
 * Install/remove only via docs/security/runbooks/stack-upgrade-staging.md Part C1.
 * It runs its checks once (done.flag), writes results.json next to itself, then
 * drains the log pipe until Apache closes it.
 */
declare(strict_types=1);

$probeDir = __DIR__;                       // <ProgramDataRoot>\staging-probe
$root     = dirname($probeDir);            // <ProgramDataRoot>, e.g. C:\ProgramData\LOAMS
$xampp    = dirname(dirname(PHP_BINARY));  // <XamppRoot>, from <XamppRoot>\php\php.exe
$flag     = $probeDir . DIRECTORY_SEPARATOR . 'done.flag';
$results  = $probeDir . DIRECTORY_SEPARATOR . 'results.json';

if (!file_exists($flag)) {
    $compat = $root . '\\server\\logs\\compat-guard.log';
    $r = [
        'probeVersion' => 2,
        'timeUtc'      => gmdate('Y-m-d\TH:i:s\Z'),
        'whoami'       => trim((string)shell_exec('whoami')),
        'groups'       => trim((string)shell_exec('whoami /groups /fo csv /nh')),
    ];

    // Append-only must hold with PHP's real open modes (spec section 4).
    $h = @fopen($compat, 'ab');
    $r['appendOpen']  = ($h !== false);
    $r['appendWrite'] = false;
    if ($h !== false) {
        $r['appendWrite'] = (fwrite($h, '{"probe":"S1b","mode":"fopen-ab"}' . "\n") !== false);
        fclose($h);
    }
    $r['filePutAppend'] = (@file_put_contents($compat, '{"probe":"S1b","mode":"file_put_contents-append"}' . "\n", FILE_APPEND) !== false);

    // These MUST be refused. On staging a "false" here is a finding (and may have altered the log).
    $r['truncateRefused'] = (@fopen($compat, 'wb') === false);
    $r['renameRefused']   = !@rename($compat, $compat . '.moved');
    $r['deleteRefused']   = !@unlink($compat);

    // Code, config and compat switches are not writable; backups and MariaDB data are not readable.
    $r['apiConfigWriteRefused']  = (@fopen($xampp . '\\htdocs\\loams_api\\config.php', 'ab') === false);
    $r['httpdConfWriteRefused']  = (@fopen($xampp . '\\apache\\conf\\httpd.conf', 'ab') === false);
    $r['compatIniWriteRefused']  = (@fopen($root . '\\server\\compat.ini', 'ab') === false);
    $r['keyDirReadable']         = is_readable($root . '\\server\\tls\\private');
    $r['backupsListRefused']     = (@scandir($root . '\\backups') === false);
    $r['mariaDbDataListRefused'] = (@scandir($xampp . '\\mysql\\data') === false);
    $r['mariaDbDataReadRefused'] = (@fopen($xampp . '\\mysql\\data\\ibdata1', 'rb') === false);

    // The inventoried write location still works for the real upload path.
    $probeFile = $xampp . '\\htdocs\\loams_api\\uploads\\s1b-probe-' . getmypid() . '.tmp';
    $r['uploadsWritable'] = (@file_put_contents($probeFile, 'probe') !== false);
    @unlink($probeFile);

    // Event Log: PHP's syslog() writes to the Windows Event Log using the openlog() ident as the source.
    $r['eventLogWrite'] = openlog('LOAMS-Transport', LOG_PID, LOG_USER) && syslog(LOG_WARNING, 'LOAMS S1b staging identity probe');
    closelog();

    @file_put_contents($results, json_encode($r, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    @touch($flag);
}

// Piped logger contract: keep reading until Apache closes the pipe.
while (($line = fgets(STDIN)) !== false) {
    // discard access-log lines
}
```

- [ ] **Step 4: Write `docs/security/runbooks/stack-upgrade-staging.md`**

````markdown
# Runbook — LOAMS server stack: staging validation, upgrade, backups and approval

**Spec:** `docs/superpowers/specs/2026-10-05-loams-s1-transport-security-design.md` §4 (Compatibility manifest, `Test-LoamsServerHost.ps1`, Service identity & least privilege), §5 (key custody), §7 (S1b, cutover steps 1–3), §8 (R6, R11).
**Tools:** `deploy/server/Test-LoamsServerHost.ps1`, `deploy/stack/loams-stack-manifest.json`, `deploy/server/staging/LoamsIdentityProbe.php`, module `deploy/server/LoamsHost`.
**Rule zero:** nothing here runs on the production gate PC before S1f authorises cutover. Production MUST NOT be upgraded directly without staging validation. `-Converge` refuses any manifest that is not `approved` (only an authorized staging host may use `-Staging` with a `draft` or `staging-validated` manifest).

Placeholders: `<XamppRoot>` (normally `C:\xampp`), `<ProgramDataRoot>` (normally `C:\ProgramData\LOAMS`), `<repo>` (checkout of the tagged commit), `<offhost>` (removable/USB media or an approved network share — never a disk of the host), `<server-LAN-IP>`, `<controller-IP>`, `<ADMIN_KEY>` (staging test key only). Never paste real student data, real keys, real IPs, machine GUIDs or personal paths into committed evidence.

## Part A — Component selection, recording and the recovery key (human procedure)

Agents and scripts never invent versions or hashes. A named owner role performs this per manifest version. The manifest stays `status: "draft"` until exact versions, packages and hashes are selected **and** tested on staging.

### A1. Apache (server) and the pinned OpenSSL tool

1. Open `https://www.apachelounge.com/download/` (HTTPS only). Choose the Apache 2.4.x Win64 build whose notes state it is built with **OpenSSL 3.x**, with the VC runtime matching the chosen PHP build (A2).
2. Download the ZIP and the publisher's checksum (and PGP signature if published). Verify and record:
   ```powershell
   Import-Module <repo>\deploy\server\LoamsHost\LoamsHost.psm1 -Force
   Test-LoamsPublisherChecksum -ArchivePath .\httpd-<ver>-win64-VS17.zip -Published '<sha256 text exactly as published>'
   ```
   `Match` MUST be `True`; otherwise stop. Record `Computed` in `server.components[apache].archive.sha256`, plus `archive.fileName`, `version`, `server.apacheVersion` and `source.verification`.
3. The bundle's `bin\openssl.exe` is the **only** OpenSSL used by the backup tooling: record it in `server.tools` with `New-LoamsManifestModuleEntry -Path '<extracted>\Apache24\bin\openssl.exe' -Root '<extracted>\Apache24\..'` (relativePath `apache/bin/openssl.exe` once placed under `<XamppRoot>`).

### A2. PHP (server)

1. Open `https://windows.php.net/download/` (HTTPS only). Choose a supported PHP 8.x **Thread Safe x64** build for the same Visual C++ runtime family as Apache.
2. Verify with the SHA-256 published on that page (`Test-LoamsPublisherChecksum`); record `version`, `server.phpVersion`, `archive.*`, `source.verification`.
3. **OpenSSL inside `httpd`:** Windows resolves a DLL dependency to an already-loaded module with the same base name, so PHP may use Apache's `libssl-3-x64.dll`. The manifest records the modules **actually loaded** (A4); both builds MUST be compatibility-tested together on staging. Never copy individual DLLs between bundles.
4. Record the MariaDB version shipped by the XAMPP bundle in the evidence file (MariaDB is not replaced by S1b; its identity and ACLs are hardened).

### A3. Client TLS runtime

1. Qt Maintenance Tool → add the **OpenSSL 3 toolkit** component (candidate source in spec §3; record the exact component name and version shown). Locate `libssl-3-x64.dll` and `libcrypto-3-x64.dll`.
2. `qopensslbackend.dll` comes from the Qt 6.11.1 MinGW kit: `<Qt>\6.11.1\mingw_64\plugins\tls\qopensslbackend.dll`.
3. Record each with `New-LoamsManifestModuleEntry`. The Maintenance Tool hands out no archive: record the package name/version in `archive.fileName` and the SHA-256 of a ZIP of exactly the recorded files (`Compress-Archive`, `Get-FileHash`) in `archive.sha256`, and say so in `source.verification`. S1e consumes this section.

### A4. Record the server modules actually loaded (staging, after B4)

```powershell
# Elevated, on the staging host, with the new stack running.
Import-Module <repo>\deploy\server\LoamsHost\LoamsHost.psm1 -Force
$inv = Get-LoamsLoadedModuleInventory -ProcessIds @(Get-LoamsHttpdProcesses | ForEach-Object ProcessId)
$inv.Complete   # MUST be True
ConvertTo-LoamsManifestModuleEntries -Inventory $inv -XamppRoot '<XamppRoot>' | ConvertTo-Json -Depth 3
```

Every loaded `libssl*`/`libcrypto*` MUST appear (otherwise `-Report` flags `UnlistedCrypto`). Cross-check each hash against the file inside the verified archive; a loaded module not from a verified archive is a stop.

### A5. Status transitions

| Status | Set when | Who | `-Converge` |
|---|---|---|---|
| `draft` | any `REPLACE_ME` remains, or Parts B/C not yet passed | anyone | staging only (`-Staging` on an authorized staging host) |
| `staging-validated` | Parts B + C passed; evidence `docs/security/evidence/<yyyy-mm-dd>-stack-v<ver>-staging.md` committed and referenced in `approval.stagingEvidence` | implementer | staging only |
| `approved` | owner reviewed evidence + rollback and restore drills; sets `approvedBy` (role), `approvedOn` | owner only | production allowed (S1f window) |

### A6. Backup recovery key pair (offline, once; same custody as the CA key)

On the offline CA machine (spec §5), never on the server:

```powershell
# Passphrase is typed interactively; never on a command line or in a file.
& '<pinned openssl.exe>' req -x509 -newkey rsa:3072 -keyout loams-backup-recovery.key -out loams-backup-recovery.crt -days 1825 `
    -subj '/CN=LOAMS Backup Recovery' -addext 'keyUsage=critical,keyEncipherment' -addext 'extendedKeyUsage=emailProtection'
(Get-FileHash -Algorithm SHA256 -InputStream ([IO.MemoryStream]::new((New-Object Security.Cryptography.X509Certificates.X509Certificate2 '.\loams-backup-recovery.crt').RawData))).Hash.ToLowerInvariant()
```

- Store the **private key** encrypted, offline, with at least two copies on separate media/locations under the CA-key custody and passphrase rules (named roles). It never touches the server or a client.
- Copy only `loams-backup-recovery.crt` to each server at `<ProgramDataRoot>\backup-recipient\loams-backup-recovery.crt` (folder admin-only: `icacls <folder> /inheritance:r /grant:r *S-1-5-32-544:(OI)(CI)F *S-1-5-18:(OI)(CI)F`).
- Print the fingerprint from the second command and pass it out of band to the operator; it is the `-RecoveryCertSha256` value. A fingerprint read from the server alone is not trusted.
- Rotate with the CA (5-year lifetime); keep old recovery keys as long as any backup encrypted to them is retained.

## Part B — Staging upgrade procedure

### B0. Staging host

- Windows 11 PC/VM with the gate PC's XAMPP layout and versions, the deployed legacy `WITS.exe` binary copy, the bridge, and a **synthetic** dataset (never the production DB).
- **Provision the report folder (elevated, once, before the first `-Report`).** `-Report` is read-only and never creates directories; it fails with `... -Report never creates directories ...` if the target folder is missing, and nothing writes into `C:\ProgramData\LOAMS\reports` unless the folder passes the protected allowlist. This administrative step is not part of `-Report`. It replaces the folder's whole security descriptor (no inherited and no leftover explicit entries such as `Users`), then verifies it and stops on any mismatch:
  ```powershell
  Import-Module <repo>\deploy\server\LoamsHost\LoamsHost.psm1 -Force
  Set-LoamsMode -Mode Converge
  try { Initialize-LoamsReportFolder -ProgramDataRoot 'C:\ProgramData\LOAMS' } finally { Set-LoamsMode -Mode Report }
  $check = Test-LoamsProtectedFolderAcl -Sddl (Get-LoamsAclSddl -Path 'C:\ProgramData\LOAMS\reports')
  $check
  if (-not $check.Ok) { throw 'STOP: the reports folder ACL does not match the allowlist (Administrators + SYSTEM FullControl, protected)' }
  ```
  Expected: `Ok = True`, no problems (protected DACL; exactly `S-1-5-32-544` and `S-1-5-18` with FullControl). Any problem ⇒ STOP and investigate. `-Converge` re-checks the same allowlist before any backup and refuses otherwise.
- Mark and approve it (elevated):
  ```powershell
  $pd = 'C:\ProgramData\LOAMS'; New-Item -ItemType Directory -Force $pd | Out-Null
  Set-Content -Path "$pd\STAGING-HOST.marker" -Value 'LOAMS staging host'
  (Get-ItemProperty HKLM:\SOFTWARE\Microsoft\Cryptography -Name MachineGuid).MachineGuid   # copy into the file below
  @{ version = 1; hosts = @(@{ computerName = $env:COMPUTERNAME; machineGuid = '<machine-guid-from-above>'; approvedBy = '<owner role>'; approvedOn = '<yyyy-mm-dd>' }) } |
      ConvertTo-Json -Depth 4 | Set-Content "$pd\approved-staging-hosts.json"
  foreach ($f in @("$pd\STAGING-HOST.marker", "$pd\approved-staging-hosts.json")) {
      icacls $f /inheritance:r /grant:r *S-1-5-32-544:F *S-1-5-18:F; icacls $f /setowner *S-1-5-32-544 }
  ```
  These two files stay on the staging host only (never committed). The production gate PC must never carry either (Part F).
- The deployed web root MUST contain `loams_api\uploads\default.jpg` (the legacy client's photo fallback; it is not in the repository). Post-change validation fails without it.
- MariaDB must be **running** with the application database before any `-Converge` (backup readiness, D19): S1b never starts the database itself before the backup and never skips the backup.
- Install the recovery certificate (A6) and have `<offhost>` media ready.

### B1. Baseline report and DB engines

```powershell
cd <repo>
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\server\Test-LoamsServerHost.ps1 -Report -ReportPath C:\ProgramData\LOAMS\reports\pre-upgrade.json
& '<XamppRoot>\mysql\bin\mysql.exe' -u root -p -e "SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='wits_app';"
```

Every table MUST be InnoDB for `--single-transaction` to be consistent; a non-InnoDB table is a stop (convert on staging first, in a reviewed change). Review the report: Apache and MariaDB situations, accounts, loaded modules (expect `UnlistedCrypto` for OpenSSL 1.1.1t on the current bundle).

### B2. Encrypted backup + off-host copy before touching binaries

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\server\Test-LoamsServerHost.ps1 -Converge -BackupOnly -Staging `
    -AcknowledgedReport C:\ProgramData\LOAMS\reports\pre-upgrade.json -RecoveryCertSha256 '<fingerprint from A6>' -OffHostDestination '<offhost>'
```

Type the computer name when asked. Exit 0 and `BACKUP ONLY - encrypted backup at ..., off-host copy verified`. The DB password comes from `config.php` or a prompt; it is never typed on a command line.

### B3. Side-by-side stack swap (maintenance window on staging)

1. Extract the verified archives to `<XamppRoot>\apache.v<ver>` and `<XamppRoot>\php.v<ver>`.
2. Port configuration, never binaries: `httpd.conf`, `extra\httpd-ssl.conf`, `extra\httpd-xampp.conf` (keep `LoadModule php_module` pointing at the new `php8apache2_4.dll`), `php.ini` (diff against the new `php.ini-production`; keep `upload_tmp_dir`, `session.save_path`, `error_log`, `extension_dir`, and the extensions in `server.requiredPhpExtensions`).
3. Stop Apache; rename `apache` → `apache.prev`, `apache.v<ver>` → `apache`; same for `php`.
4. `& '<XamppRoot>\apache\bin\httpd.exe' -t` → `Syntax OK`; start Apache.
5. Any failure: stop Apache, rename the `.prev` folders back, start Apache, run B4 and compare the profile hash with `pre-upgrade.json`.

### B4. Post-swap verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\server\Test-LoamsServerHost.ps1 -Report -ReportPath C:\ProgramData\LOAMS\reports\post-upgrade.json
```

Record modules and the OpenSSL tool (A1.3, A4). Re-run `-Report`: no `Missing`, `HashMismatch` or `UnlistedCrypto` (status `Unknown` only because of `ManifestNotFinal` while `draft`).

### B5. Identity convergence on staging (Apache + MariaDB)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File deploy\server\Test-LoamsServerHost.ps1 -Converge -Staging `
    -AcknowledgedReport C:\ProgramData\LOAMS\reports\post-upgrade.json -RecoveryCertSha256 '<fingerprint from A6>' -OffHostDestination '<offhost>'
```

Exit 0 = `CONVERGE SUCCEEDED`. Exit 1 = refused (nothing changed; read the listed problems, e.g. drift). Exit 5 = rolled back (read `<backup>\logs\converge.log`, fix, repeat from B4). Exit 4 = Part D2 immediately. `logs\converge.log` must show `MariaDB verification: read=ok, write=ok, backup-restore=ok, cleanup=ok` and `Post-change validation: ...=ok` for every check.

### B6. Integration validation (record each result in the evidence file)

| # | Check | How (staging, synthetic data) | Pass |
|---|---|---|---|
| 1 | Admin authentication | 2.0 admin login with the staging `<ADMIN_KEY>`; legacy `WITS.exe` admin login | both succeed; wrong key refused |
| 2 | Attendance endpoints | kiosk student login, guest login | one visit row each |
| 3 | RFID login | synthetic card on the USB reader in 2.0 and in legacy `WITS.exe` | visit + student shown with photo |
| 4 | Turnstile events | bench controller (or `deliverables/loams_api/tests/turnstile_integration_test.php`) to `turnstile.php`; bridge display in `WITS.exe` | event recorded once; display shown |
| 5 | Reporting | 2.0 reports incl. `api.php/reports/data`, exports | figures match the synthetic dataset |
| 6 | File uploads | register a student with a photo; bulk ZIP import | files under `uploads\`; nowhere else |
| 7 | Legacy `WITS.exe` actual behaviour | the exact deployed binary: login, RFID, photos, every admin action it performs | identical to pre-upgrade |
| 8 | MariaDB | service runs as `NT SERVICE\mysql` (C4); restart survives; `mysql_error.log` keeps being written | yes |
| 9 | Logs | Apache, PHP, API logs keep being written | new lines present |

Any failure ⇒ roll back (B7) and do not mark the manifest `staging-validated`.

### B7. Rollback rehearsal and restore drill (mandatory before `staging-validated`)

1. Stop Apache; rename `apache`/`php` back to the `.prev` folders.
2. Run the restore script from the B2 backup folder, elevated: `& '<backup>\Restore-LoamsHostBackup.ps1'` → exit 0 (services and ACLs restored).
3. `-Report` again: `profileHash` equals `pre-upgrade.json`.
4. **Encrypted restore drill** on the recovery workstation with the offline key (Part D3): decrypt `database.p7m` and expand `files.p7m` from the **off-host** copy, restore the dump into a scratch database on staging (`loams_restore_drill`), and compare row counts of every table with the synthetic source; compare restored file hashes (verified by `Expand-LoamsEncryptedArchive`). Drop `loams_restore_drill` and securely delete the drill plaintext afterwards.
5. Measure and record the restore time (S1f needs it for the rollback deadline).

## Part C — Staging verifications unit tests cannot prove

### C1. Apache identity probe (real token, real PHP file-open behaviour, Event Log)

After B5, elevated:

```powershell
$pd = 'C:\ProgramData\LOAMS'; $probe = Join-Path $pd 'staging-probe'
New-Item -ItemType Directory -Force $probe | Out-Null
icacls $probe /inheritance:r /grant:r '*S-1-5-32-544:(OI)(CI)F' '*S-1-5-18:(OI)(CI)F' "NT SERVICE\Apache2.4:(OI)(CI)M"
Copy-Item <repo>\deploy\server\staging\LoamsIdentityProbe.php $probe
Set-Content -Path '<XamppRoot>\apache\conf\extra\loams-staging-probe.conf' -Value 'CustomLog "|C:/xampp/php/php.exe -n -f C:/ProgramData/LOAMS/staging-probe/LoamsIdentityProbe.php" common'
Add-Content -Path '<XamppRoot>\apache\conf\httpd.conf' -Value 'Include conf/extra/loams-staging-probe.conf'
& '<XamppRoot>\apache\bin\httpd.exe' -t
Restart-Service Apache2.4
Start-Sleep -Seconds 5
Get-Content (Join-Path $probe 'results.json')
Get-WinEvent -FilterHashtable @{ LogName = 'Application'; ProviderName = 'LOAMS-Transport' } -MaxEvents 5 | Format-List TimeCreated, Id, Message
```

| Field | Expected | If not |
|---|---|---|
| `whoami` | `nt service\apache2.4` | probe invalid — stop |
| `appendOpen`, `appendWrite`, `filePutAppend` | `true` | `(AD,S)` too narrow for PHP: widen per D8 in a reviewed change (never `WD`/`DE`/`M`), or S1d uses the Event Log fallback |
| `truncateRefused`, `renameRefused`, `deleteRefused` | `true` | ACL too broad — stop |
| `apiConfigWriteRefused`, `httpdConfWriteRefused`, `compatIniWriteRefused` | `true` | code/config writable — stop |
| `keyDirReadable` | `true` | fix before S1c |
| `backupsListRefused`, `mariaDbDataListRefused`, `mariaDbDataReadRefused` | `true` | backups or raw DB files exposed to PHP — stop |
| `uploadsWritable` | `true` | uploads would break — fix the profile |
| `eventLogWrite` + an event from `Get-WinEvent` | `true` + present | S1d's fallback channel unavailable — stop and escalate |

Remove the probe (MUST NOT remain):

```powershell
(Get-Content '<XamppRoot>\apache\conf\httpd.conf') | Where-Object { $_ -ne 'Include conf/extra/loams-staging-probe.conf' } | Set-Content '<XamppRoot>\apache\conf\httpd.conf'
Remove-Item '<XamppRoot>\apache\conf\extra\loams-staging-probe.conf'
& '<XamppRoot>\apache\bin\httpd.exe' -t; Restart-Service Apache2.4
Remove-Item -Recurse -Force $probe
```

### C2. Effective permissions spot-check

```powershell
icacls '<XamppRoot>'; icacls '<XamppRoot>\htdocs\loams_api\config.php'; icacls '<XamppRoot>\mysql\data'
icacls C:\ProgramData\LOAMS\server\logs\compat-guard.log; icacls C:\ProgramData\LOAMS\server\tls\private; icacls C:\ProgramData\LOAMS\backups
```

`mysql\data` shows only Administrators, SYSTEM and `NT SERVICE\mysql:(OI)(CI)(M)` — no `Users`, no `Authenticated Users`; `compat-guard.log` shows `NT SERVICE\Apache2.4:(AD,S)`; `tls\private` only Administrators and `NT SERVICE\Apache2.4:(OI)(CI)(R)`; `backups` only Administrators and SYSTEM; no `Authenticated Users:(M)` on `<XamppRoot>`.

### C3. Loaded modules

Elevated `-Report` against the filled manifest: `Runtime modules ... Match` (or `Unknown` solely because of `ManifestNotFinal` while `draft`).

### C4. MariaDB identity, startup, reads/writes, backup/restore

```powershell
Get-CimInstance Win32_Service -Filter "Name='mysql'" | Select-Object Name, StartName, State
Get-CimInstance Win32_Process -Filter "Name='mysqld.exe'" | Invoke-CimMethod -MethodName GetOwner | Select-Object Domain, User
Restart-Service mysql; Get-Content '<XamppRoot>\mysql\data\mysql_error.log' -Tail 20
Select-String -Path '<backup>\logs\converge.log' -Pattern 'MariaDB verification'
```

Expected: `StartName = NT SERVICE\mysql`, owner `NT SERVICE` / `mysql`, a clean restart in the error log, and `read=ok, write=ok, backup-restore=ok, cleanup=ok`. The verification used only this run's scratch schemas `loams_s1b_verify_<set id>` / `…_r` (listed in the backup set's `backup-manifest.json` `verifySchemas`, dropped afterwards; `-Report` shows `verifySchemas=none` in the fingerprint); no application or attendance table was touched.

## Part D — Rollback, recovery, backups

### D1. Automatic rollback

`-Converge` rolls back every applied checkpoint in reverse when any checkpoint or `Validate` fails and exits 5 with `CONVERGE FAILED at checkpoint '<name>'`; the host is in its pre-change state.

### D2. RECOVERY REQUIRED (exit 4)

1. Start the manual-attendance contingency immediately.
2. Open `<backup>\recovery-required.json` (failed checkpoint, failed rollback steps, manual steps).
3. Elevated: `& '<backup>\Restore-LoamsHostBackup.ps1'` (services + ACLs). Exit 0 → step 5.
4. If it reports `RECOVERY REQUIRED`, apply the listed manual steps one by one (`sc.exe config …`, `icacls <parent> /restore <file> /c`, `httpd -k uninstall -n …`, `mysqld --remove …`).
5. `-Report` and compare `profileHash` with the pre-change report (the fingerprint includes service configuration, ACLs, event source, retention task, deployed tools and the `ProgramData\LOAMS` layout); never declare success until it matches and B6 rows 2–4, 7 and 8 pass.
6. Leftover verification schemas are dropped by the restore script's step `drop verification schemas left by converge`. If that step fails, drop the names listed in `backup-manifest.json` `verifySchemas` by hand (MariaDB running): `& '<XamppRoot>\mysql\bin\mysql.exe' --defaults-extra-file=<admin-only file> -e "DROP DATABASE IF EXISTS <name>; DROP DATABASE IF EXISTS <name>_r;"`, then confirm `-Report` shows `verifySchemas=none`.

The generated `Restore-LoamsHostBackup.ps1` first checks every rollback file against `SHA256SUMS.json` and then runs the module copy stored in the backup set; a missing or altered file, or any failed step, ends with exit 4 and `RECOVERY REQUIRED - step '<name>' failed`. It never deletes `C:\ProgramData\LOAMS` or `backups\` (they hold the recovery data).

### D3. Restoring encrypted data (recovery workstation only)

Needs the offline recovery key; never bring the key to the production server.

```powershell
Import-Module <repo>\deploy\server\LoamsHost\LoamsHost.psm1 -Force
# Files (config, API files, photos, ProgramData\LOAMS\server): extracted and hash-checked per entry.
Expand-LoamsEncryptedArchive -OpenSslExe '<pinned openssl.exe>' -InFile '<offhost>\host-<stamp>\encrypted\files.p7m' `
    -RecipientCert '<offline>\loams-backup-recovery.crt' -RecoveryKey '<offline>\loams-backup-recovery.key' -Destination '<restore-folder>'
# Database dump (plaintext only inside <restore-folder> on the recovery workstation).
Invoke-LoamsDecryptFile -OpenSslExe '<pinned openssl.exe>' -InFile '<offhost>\host-<stamp>\encrypted\database.p7m' `
    -RecipientCert '<offline>\loams-backup-recovery.crt' -RecoveryKey '<offline>\loams-backup-recovery.key' -OutFile '<restore-folder>\database.sql'
(Get-FileHash -Algorithm SHA256 '<restore-folder>\database.sql').Hash.ToLowerInvariant()   # must equal encrypted[database].plainSha256 in backup-manifest.json
```

A DB restore discards attendance recorded after the dump: follow spec §7 reconciliation (idempotent reconciliation of `library_visits`, `turnstile_events` and dependent state such as `students.visits`). Restore with `mysql.exe --defaults-extra-file=<admin-only file>` — never `-p<password>`. Delete `<restore-folder>` securely when done.

### D4. Off-host copy (the only recovery copy is never the machine being migrated)

- Preferred: `-OffHostDestination <offhost>` — the tool copies the set and re-hashes every file **on the destination**, then records `offhost-verification.json`.
- Approved location the tool cannot write to (e.g. an institutional vault copied by IT): `-OffHostAttestation`. Before answering the prompt, on the off-host copy run `(Get-FileHash -Algorithm SHA256 '<copy>\SHA256SUMS.json').Hash` **and** check every file against `SHA256SUMS.json` there; type that SHA-256. The tool refuses a mismatch and never accepts silence.
- Destinations on any fixed disk of the host, or UNC paths naming the host itself, are refused.

### D5. Retention and deletion

- `LOAMS Backup Retention` (SYSTEM, daily 03:30) deletes host backup sets older than **30 days**, always keeping the newest; it logs set names and dates only (`<ProgramDataRoot>\tools\backup-retention.log`) — never passwords or student records.
- Off-host copies follow the same 30-day policy; the owner role deletes them and records the deletion in the evidence log.
- A set referenced by an open incident is moved to the incident folder (admin-only) before it ages out.

### D6. Rolling back to an older stack

Restores known vulnerabilities (R11): time-limited exposure with restricted access, a dated remediation plan in the evidence file, and a new pass through Part B.

## Part E — Patch review and approval of new stack versions (v1.1, v1.2, …)

1. Trigger: a security advisory for Apache httpd, PHP, OpenSSL or the XAMPP MariaDB affecting the pinned versions, or the 6-monthly review.
2. Bump `manifestVersion` and `server.name` (`LOAMS Server Stack v1.1`), set `status: draft`, redo Part A for changed components.
3. Run Parts B and C on staging, including B7 rollback and restore drills.
4. `staging-validated` with a new evidence file; owner review → `approved`.
5. Production only through the then-current maintenance runbook, starting report-only (Part F). Never upgrade production directly.

## Part F — Production gate PC (S1f and later)

- First run on any gate PC is report-only: `Test-LoamsServerHost.ps1 -Report` (elevated); compare with the staging-validated profile (cutover step 1).
- Confirm `C:\ProgramData\LOAMS\STAGING-HOST.marker` and `approved-staging-hosts.json` do **not** exist.
- Confirm `loams_api\uploads\default.jpg` is deployed and MariaDB is running.
- Provision `C:\ProgramData\LOAMS\reports` exactly as in B0 (`Initialize-LoamsReportFolder` + `Test-LoamsProtectedFolderAcl`, STOP on mismatch) before the first `-Report` (it never creates directories).
- Install the recovery certificate (A6) and confirm its fingerprint out of band; have `<offhost>` media ready.
- `-Converge` only inside the S1f window, with an `approved` manifest, the step-1 report as `-AcknowledgedReport`, `-RecoveryCertSha256`, and `-OffHostDestination` (or `-OffHostAttestation`). The secure MariaDB ACL and identity checkpoints are mandatory: a production run cannot report success without them.
- Apache and MariaDB restarts interrupt attendance for seconds; the window plan covers it.
- Residual risk R6 (recorded caution): the virtual account limits damage but cannot protect the TLS key from compromised PHP inside Apache; the compromise runbook (S1c) applies.
````

- [ ] **Step 5: Run to verify it passes**

Same command as Step 2. Expected: PASS, 3 tests. Set `expected-test-count.txt` to `308` and run the suite runner → `PASSED: 308 tests`.

- [ ] **Step 6: Commit**

Via the project `commit` skill — subjects (two concerns): `feat(deploy): add staging-only Apache identity probe (piped logger)` and `docs(security): add stack upgrade, backup and approval runbook`

---

### Task 19: Human component selection, recovery key and staging dry run (records the manifest)

This task is **performed by people** with an authorized staging host and the offline CA machine (runbook Parts A–C). It cannot be done by an agent on the dev box, and its outputs (real versions, hashes, the recovery certificate fingerprint) MUST NOT be guessed. It may run after Tasks 1–18 are merged; it blocks S1c (which needs the manifest-pinned OpenSSL) and S1f. Until it completes, the committed manifest stays `draft` and production convergence is refused.

**Files:**
- Modify: `deploy/stack/loams-stack-manifest.json` (real values; `status` → `staging-validated`)
- Create: `docs/security/evidence/<yyyy-mm-dd>-stack-v1.0-staging.md` (synthetic data only; host names, IPs and machine GUIDs as placeholders)

**Interfaces:**
- Consumes: runbook Parts A–D; `Test-LoamsPublisherChecksum`, `New-LoamsManifestModuleEntry`, `ConvertTo-LoamsManifestModuleEntries`, `Expand-LoamsEncryptedArchive`, `Invoke-LoamsDecryptFile`, `Test-LoamsServerHost.ps1`.
- Produces: a schema-valid manifest with no `REPLACE_ME`, `approval.stagingEvidence` pointing at the evidence file; the recovery certificate fingerprint (custody record, not committed); the owner later sets `approved`.

- [ ] **Step 1: Recovery key pair** — runbook A6 on the offline CA machine; two encrypted offline copies of the key; fingerprint confirmed out of band; certificate installed on the staging host.
- [ ] **Step 2: Select and verify components** — runbook A1 (Apache + `openssl.exe` tool), A2 (PHP), A3 (client OpenSSL 3 + `qopensslbackend`). Every `Test-LoamsPublisherChecksum` result `Match = True`.
- [ ] **Step 3: Staging host approval** — runbook B0 (marker + `approved-staging-hosts.json`, both admin-only).
- [ ] **Step 4: Staging upgrade** — runbook B1–B4, then record loaded modules (A4) into the manifest.
- [ ] **Step 5: Validate the manifest locally**

```powershell
powershell -NoProfile -Command "Import-Module .\deploy\server\LoamsHost\LoamsHost.psm1 -Force; (Test-LoamsStackManifest -Manifest (Get-Content -Raw deploy\stack\loams-stack-manifest.json | ConvertFrom-Json)).Errors"
```

Expected while still `draft`: no output.

- [ ] **Step 6: Identity convergence + probes** — runbook B5, C1, C2, C3, C4; every expected value met (or a reviewed D8 widening / escalation recorded).
- [ ] **Step 7: Integration list, rollback rehearsal and restore drill** — runbook B6 (all rows pass) and B7 (restore script exit 0, `profileHash` equal to `pre-upgrade.json`, encrypted restore drill from the **off-host** copy with matching row counts and file hashes, restore time measured).
- [ ] **Step 8: Write the evidence file** — date, manifest version, component versions and hashes, MariaDB version, B6 results, C1 `results.json`, C4 output, restore-drill results, restore time, deviations. Set `approval.stagingEvidence` and `status: staging-validated`.
- [ ] **Step 9: Run the suite** — `powershell -NoProfile -ExecutionPolicy Bypass -File deploy\tests\Invoke-LoamsPesterSuite.ps1` → `PASSED: 308 tests` (the committed-manifest tests now validate the real values).
- [ ] **Step 10: Commit** — via the project `commit` skill: `chore(stack): record LOAMS Server Stack v1.0 (staging-validated)`. Owner approval (`status: approved`, `approvedBy`, `approvedOn`) is a separate, owner-made commit: `chore(stack): approve LOAMS Server Stack v1.0`.

---

## Acceptance Tests

1. **Pester suite (Windows PowerShell 5.1, Pester ≥ 5.5, PHP CLI and an `openssl.exe` present):**
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File deploy\tests\Invoke-LoamsPesterSuite.ps1
   ```
   Expected: `LOAMS Pester suite PASSED: 308 tests (minimum 308)`, exit 0 — 0 failed, 0 skipped, 0 not run. Per file (counted mechanically from the `It` blocks in this plan): Common 14, Manifest 28, ManifestRecord 7, Detection 11, RuntimeModules 11, Acl 22, EventSource 9, ServiceIdentity 11, Classification 17, MariaDb 17, Report 18, Crypto 17, Secrets 13, Database 10, Backup 27, Checkpoints 9, Authorization 15, Validation 5, Converge 44, StagingArtifacts 3.
2. **PowerShell 5.1 compatibility:** the suite runs under `powershell.exe` 5.1 (not `pwsh`). Additionally:
   ```powershell
   Get-ChildItem deploy -Recurse -Include *.ps1,*.psm1 | Select-String -Pattern '\?\?|\?\.|ConvertFrom-Json\s+-AsHashtable|Test-Json|\s&&\s|\s\|\|\s' | Select-Object Path, LineNumber, Line
   Get-ChildItem deploy -Recurse -Include *.ps1,*.psm1 | Where-Object { $b = [IO.File]::ReadAllBytes($_.FullName); -not ($b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) }
   ```
   Expected: no output from either.
3. **Read-only proof of `-Report` on this dev box** (Task 11 Step 7): `icacls C:\xampp`, `icacls C:\xampp\mysql\data` and the `Apache2.4`/`mysql` `Win32_Service` StartName/StartMode/State are identical before and after; `C:\ProgramData\LOAMS` still absent; `LOAMS-Transport` registry key still absent. Expected exit: 3 non-elevated, 2 elevated.
4. **Secret and data hygiene:** `git grep -n -I 'S1b-test-db-pass' -- deploy docs/security` matches only `deploy/tests/`; `git grep -n -I 'DB_PASS' -- deploy` matches only `deploy/tests/` and the parser call in `LoamsHost.Secrets.ps1`; `git grep -n -E '\b([0-9]{1,3}\.){3}[0-9]{1,3}\b' -- deploy docs/security` shows only `127.0.0.1`; no machine GUID or `approved-staging-hosts.json` is committed.
5. **Staging verification checklist** (human, runbook; recorded in the evidence file — not proven by unit tests):
   - [ ] A6 recovery key pair created offline, two encrypted copies, fingerprint confirmed out of band.
   - [ ] B0 staging host authorized (marker + approved machine identity); B1 all tables InnoDB.
   - [ ] B2 `-BackupOnly`: encrypted set + hash-verified off-host copy (exit 0); no plaintext dump or file copy in the set (spot-check: `Select-String -Path <set>\* -Pattern 'DB_PASS' -SimpleMatch -List` finds nothing).
   - [ ] B4/C3 elevated `-Report`: no `Missing` / `HashMismatch` / `UnlistedCrypto`.
   - [ ] B5 `-Converge -Staging` exit 0; Apache runs as `NT SERVICE\Apache2.4`, MariaDB as `NT SERVICE\mysql`; converge log shows MariaDB verification and validation all `ok`.
   - [ ] C1 probe: `whoami` = `nt service\apache2.4`; append works; truncate/rename/delete refused; code/config/compat writes refused; backups and MariaDB data not readable; uploads writable; `LOAMS-Transport` event visible.
   - [ ] C2 ACL spot-check incl. `mysql\data` without `Users`/`Authenticated Users`.
   - [ ] C4 MariaDB identity, clean restart, scratch-schema verification.
   - [ ] B6 integration rows 1–9 pass incl. deployed legacy `WITS.exe` actual behaviour.
   - [ ] B7 rollback rehearsal (restore exit 0, `profileHash` restored) and encrypted restore drill from the off-host copy (row counts and file hashes match); restore time recorded.
   - [ ] Forced-failure drills: (a) `-Converge -Staging -BaseUrl http://127.0.0.1:9/loams_api/` (nothing listens) → `ApacheServiceAccount` verification fails → exit 5 and a fresh `-Report` shows the pre-change `profileHash`; (b) edit `httpd.conf` (comment line) between `-Report` and `-Converge` → exit 1 "host drifted" with no change; (c) run a copy of a backup set's `Restore-LoamsHostBackup.ps1` with one `rollback\acl-NN.txt` removed → exit 4 `RECOVERY REQUIRED`.
   - [ ] Retention task present (`Get-ScheduledTask 'LOAMS Backup Retention'`, principal SYSTEM); re-running `-Converge` over an existing task and rolling back leaves the original definition (compare `Export-ScheduledTask` before/after).
   - [ ] `loams_api\uploads\default.jpg` deployed and served; a Control-Panel `mysqld` (if that is the starting situation) is shut down within the timeout with port 3306 released.

## Rollback Considerations

- **S1b ships tooling and documentation only.** Merging changes no running system; nothing touches the production gate PC before S1f (invariant 8). The legacy `WITS.exe`, the bridge and the deployed PHP API are untouched.
- **Converge rollback design:** read-only gates, typed confirmation, encrypted verified backup and verified off-host copy, and a live drift re-check all happen before the first change; then named checkpoints with idempotent `Undo` executed in reverse (failed checkpoint first); automatic rollback ⇒ exit 5; any failed undo ⇒ `RECOVERY REQUIRED` (exit 4, `recovery-required.json`, manual steps, generated `Restore-LoamsHostBackup.ps1`); success is never reported after partial restoration. The application database is never modified by S1b convergence (MariaDB verification uses only the scratch schemas `loams_s1b_verify*`); restoring encrypted data is manual on a recovery workstation with the offline key, followed by spec §7 reconciliation.
- **Restore coverage:** every row of the Task 14 mutation inventory has an automatic undo and a restore-script step; the backup set carries the module copy and prior state it needs, and the fingerprint covers every row, so `-Report` proves restoration.
- **MariaDB-specific:** the MariaDB checkpoints restore the original service account (SYSTEM/Administrators keep full control of the data directory throughout) and the saved ACL of each MariaDB path; a Control-Panel `mysqld` is restarted the Control-Panel way on undo after its service registration is removed.
- **Stack upgrade rollback** is runbook-driven (B3 step 5, B7): `.prev` folders side by side, restore script, `profileHash` comparison. Rolling back to the old stack re-exposes known vulnerabilities (R11) — time-limited, dated remediation (runbook D6).
- **Backups:** the only recovery copy is never the migrated machine (off-host copy verified first); decryption needs the offline recovery key, so the key's custody and two-copy rule are part of the rollback capability.
- **Reverting the code:** every task is its own Conventional Commit; revert with `git revert <sha>` (newest first) via the project `commit` skill, or revert the squash-merge commit of the S1b PR. No data migration or persisted state needs undoing; a converged staging host is restored with its backup set's restore script.
- **Recorded caution / residual risks:** R6 — the TLS key folder is readable by the Apache identity, so compromised PHP can read it; the virtual account reduces damage only. R11 — rollback temporarily restores a vulnerable stack.

## Completion Criteria

- [ ] Tasks 1–18 merge-ready: suite `PASSED: 308 tests`, 0 failed / skipped / not run, on Windows PowerShell 5.1.
- [ ] Task 11 Step 7 read-only proof recorded (dev box unchanged, incl. `mysql\data` ACL and both services).
- [ ] `/claude-review` (project workflow) — or `/codex-review` if the owner prefers — reaches **APPROVE** (≤ 3 rounds); Critical/Important findings fixed.
- [ ] Project `create-pr` gate passes with exactly the three agents `dry-checker`, `security-reviewer`, `general-code-reviewer` (diff-scoped); security-reviewer findings on secret handling, encryption, off-host copy, ACLs (incl. MariaDB) and the probe resolved.
- [ ] Manifest committed as `draft` by Tasks 1–18; tests prove `-Converge` refuses it without `-Staging` on an authorized staging host, and refuses a production plan lacking the MariaDB checkpoints.
- [ ] Task 19 done before S1c relies on the manifest OpenSSL and before S1f: manifest `staging-validated` with committed evidence, then owner `approved`.
- [ ] Staging dry run (runbook B + C, incl. B7 rollback rehearsal, encrypted restore drill and the forced-failure drills) recorded **before S1f**.
- [ ] No secrets, real IPs, machine GUIDs, personal paths or real student data in any committed file (Acceptance 4); no Claude/Anthropic co-author trailer; all commits via the project `commit` skill.
- [ ] PR opened via the project `create-pr`; merge is the owner's call (`/merge-pr`).

## Spec Coverage

| Spec requirement / owner change (S1b-relevant) | Where |
|---|---|
| §4 manifest `deploy/stack/loams-stack-manifest.json`, versioned, server "LOAMS Server Stack v1.0" + client section | Task 2 |
| §4 each component: version, source, archive SHA-256, expected loaded module names + SHA-256 | Task 2 (schema), Task 3 (helpers), Task 19 (real values) |
| §4 schema-checked in the gate; single source of truth for `Test-LoamsServerHost.ps1` and Installer 2.0 | Task 2 (validator + committed-manifest tests), Task 1 runner (D7), Tasks 5/11/12/17 consume it |
| §4 pinning ≠ freezing: patch review, staging validation, checksums, manifest bump, tested rollback | Runbook A5, B7, E (Task 18) |
| §4 stack requirements (no direct prod upgrade; backups of Apache/PHP config, API files, DB, certificates, compat settings; integration list; verified checksums; no binary mixing; failed upgrade must not leave attendance down / DB partial) | Tasks 12–14 (encrypted backup set), Task 5 (`UnlistedCrypto`), runbook A/B/D |
| §4 `-Report` default read-only; `-Converge` | Task 11 (read-only test), Task 17 |
| §4 runtime verification from loaded `httpd` modules incl. PHP's OpenSSL, never `openssl.exe` | Task 5, Task 11 |
| §4 no new diagnostic HTTP endpoint | Task 16 (existing read-only endpoints), Task 18 (piped-logger probe) |
| §4 situations table (fresh PC, Control Panel, other name/LocalSystem, FastCGI, stop-and-report) | Task 9 (shared algorithm), Task 10 (MariaDB), Task 17 (registration checkpoints) |
| §4 first run on any gate PC report-only | Tasks 16–17 (`-AcknowledgedReport`), runbook Part F |
| §4 recoverable-not-atomic steps 1–6, RECOVERY REQUIRED, never success after partial restore | Tasks 14, 15, 17 |
| §4 `NT SERVICE\Apache2.4` after verifying service name and XAMPP compatibility | Tasks 8, 9, 17 |
| §4 ACL table incl. compat log `FILE_APPEND_DATA`, key Apache + Administrators only, modify list from inventory | Task 6 |
| §4 append-only tested with PHP's real file-open behaviour; Event Log fallback | Task 18 probe + runbook C1, D8 |
| §4 Apache/PHP cannot modify compat switches, code, provisioning scripts | Task 6, probe checks |
| §4 safe migration: back up service config + ACLs, test, document rollback | Tasks 14, 17, runbook D |
| §4 `LOAMS-Transport` event source registered + Apache account can write | Task 7, Task 17 (`EventSource`), runbook C1 |
| §4 recorded caution / §8 R6 | Rollback Considerations, runbook Part F |
| §1 invariant 7 pinned, verified runtime | Tasks 2, 5, 11, 17 (`Validate`) |
| §1 invariant 8 / §7 no production deployment before S1f | Global Constraints, runbook rule zero + Part F |
| §6 Layer 6 (Pester 5: manifest schema; secrets never in command lines/transcripts/verbose/exceptions/logs) | Tasks 1, 2, 13, 17 |
| §6 Layer 7 S1b-owned parts (append-only ACL, event source writable, loaded DLLs match, service-account effective permissions) | Task 18 runbook C1–C4 (manual staging), Tasks 5/11 (DLLs); full Layer 7 tool is S1d |
| §7 S1b slice row | Tasks 1–19 |
| §7 slice conditions (passing tests, security review, documented rollback) | Acceptance Tests, Completion Criteria, Rollback Considerations |
| §7 cutover steps 1–3 tooling | Tasks 11, 14, 17; runbook B, F |
| §8 R11 | Runbook D6, Rollback Considerations |
| Client section: OpenSSL 3 DLLs + `qopensslbackend` (Qt Maintenance Tool toolkit candidate) | Task 2, runbook A3, Task 19 |
| Owner D15 — MariaDB identity + data/log ACLs in S1b, scratch-schema verification, staging first, production gate | Tasks 9, 10, 13 (`Invoke-LoamsDbVerification`), 17 (`MariaDbAcl`, `MariaDbServiceAccount`, `Test-LoamsProductionPlan`), runbook C2/C4 |
| Owner D14 — encrypted backups (pinned OpenSSL, offline recovery key, streamed, no plaintext on disk), off-host verified copy, integrity + restore drill, 30-day retention, never the only copy on the migrated machine | Tasks 12, 13, 14, 17; runbook A6, B2, B7, D3–D5 |
| Owner D2 — staging marker plus approved machine identity plus typed confirmation | Task 16, Task 17, runbook B0/F |
| Owner D9 — 72-hour acknowledgement plus fresh drift re-check before the first change | Tasks 9 (fingerprint), 11 (live fingerprint), 16, 17 |
| Owner D13 — validation of Apache, PHP execution, DB connectivity, legacy-critical reads, no attendance writes | Task 16, Task 17 (`Validate`) |
| Owner — manifest unapproved for production until selected and tested; production refuses non-approved | Global Constraints, Task 2 status gate, Task 17, Task 19 |
| Codex R1-1 — manifest array contract; tests fail on the PS 5.1 nested-array bug | Task 2 (`Get-LoamsPropList` + contract tests), Task 5 (two-component test), Task 12 (multi-tool selection test) |
| Codex R1-2 — retention rollback restores the captured prior task/tools, never suppresses errors | Task 14 (`Save-LoamsPriorHostState`, `Restore-LoamsRetentionTaskState` + tests), Task 17 (checkpoint undo + RECOVERY REQUIRED integration test) |
| Codex R1-3 — restore covers every mutation, validates inputs inside the step machinery, exercised by executing the generated script; fingerprint includes LOAMS state | Task 14 (16-row inventory, `Invoke-LoamsHostRestore`, child-process tests), Tasks 9/11 (`loamsState`) |
| Codex R1-4 — FreshInstall / stopped-DB backup behaviour | Task 13 (D19 readiness + tests), Task 17 (unmocked readiness integration tests) |
| Codex R1-5 — verified `mysqld` shutdown | Task 10 (PID/port/data-file verification + tests), Task 17 caller |
| Codex R1-6 — post-change report gates `Success`; staging rejects HashMismatch/UnlistedCrypto/incomplete inventory | Task 17 (`Test-LoamsPostChangeReportClean`, Validate, post-engine gate + tests) |
| Codex R1-7 — legacy photo fallback must exist | Task 16 (failing check + test), runbook B0/F |
| Codex R1-8 — counts mechanically recounted | every task's count lines, Acceptance 1, Task 19, Self-review |
| Codex R1-9 — contained tool paths (validator + execution, junction) | Task 2 (`Test-LoamsManifestRelativePath` + tests), Task 12 (`Resolve-LoamsContainedPath` + tests) |
| Codex R1-10 — future-dated acknowledged reports | Task 16 (5-minute skew + test) |
| Codex R2-1 — verification-schema cleanup undo, restore step, fingerprint, post-check, inventory row 16 | Task 13 (`Remove-LoamsVerifySchemas` + tests), Task 11 (`verifySchemas`, `leftoverVerifySchemas` + test), Task 14 (manifest `verifySchemas`, restore step), Task 17 (`MariaDbVerification` + two real-engine tests, leftover post-check test) |
| Codex R2-2 — `reports` provisioned in B0; `-Report` fails closed | Task 11 (`Save-LoamsHostReport` check + test), runbook B0/F |
| Codex R2-3 — dirty post-change report outcome via the real `Validate` path | Task 17 (outcome text, evidence save, real Validate + engine test, converge evidence test) |
| Codex R2-4 — exact `tools\` tree capture/restore incl. directories | Task 11 (`Get-LoamsToolsState` dirs), Task 14 (`Restore-LoamsRetentionTaskState` + empty-folder test) |
| Codex R2-5 — child-process success test of the generated restore wrapper | Task 14 (TestDrive fixture, real wrapper + module copy, exit 0 + restored state) |
| Codex R3-1 — report location validated in preflight; deterministic final-save failure | Task 11 (`Test-LoamsReportLocation` + tests), Task 17 (preflight, `SuccessReportNotSaved` + two refusal tests and a save-failure test), Task 1 (exit code 6) |
| Codex R3-2 — tracked credential files; leftovers ⇒ `RecoveryRequired` | Task 13 (`Remove-LoamsFileSecurely`, `Clear-LoamsCredentialFiles` + tests), Task 14 (row 17), Task 17 (undo + end-of-run sweep; two real checkpoint/engine tests mocking only the deletion boundary; converge sweep test) |
| Codex R3-3 — protected allowlist ACL for the reports folder, verified programmatically | Task 6 (`Test-LoamsProtectedFolderAcl` + tests), Task 11 (`Initialize-LoamsReportFolder`, Save check + tests), runbook B0/F |

## Self-review (revision 5)

- **Spec/owner coverage:** every row above maps to a task with tests or to a runbook step explicitly marked as staging verification.
- **Codex round 3:** confirmed in the plan text first (`Save-LoamsHostReport -Path $ReportPath` ran only after all checkpoints, outside the outcome handling; `Invoke-LoamsDbVerification` removed its defaults file in `finally` and a failure there was untracked while the undo created a different file; B0's `icacls /inheritance:r /grant:r` kept unrelated explicit ACEs); fixes and tests are in the Spec Coverage rows `Codex R3-*`.
- **Codex round 2:** each finding was confirmed in the plan text first (cleanup failure ended `RolledBack` with schemas left; `Save-LoamsHostReport` cannot create `reports`; the old dirty-report test mocked the whole plan; tools directories were not tracked; both child tests exited before the module import); fixes and tests are listed in the Spec Coverage rows `Codex R2-*`.
- **Codex round 1:** each finding was checked against the plan before fixing (finding 1 reproduced in Windows PowerShell 5.1: `@(f)` where `f` returns `, $array` has `Count` 1); the fixes and their tests are listed in the Spec Coverage rows `Codex R1-*`.
- **Placeholders:** none in plan steps; `REPLACE_ME` appears only in the committed draft manifest by design (schema-enforced, refused for production).
- **Interface consistency checked:** `Get-LoamsServiceSituation` (Tasks 9, 10); `Stop-LoamsWindowsService` / `Start-LoamsWindowsService` / `Stop-LoamsControlPanelProcesses` (Task 8, used in Task 17); `Get-LoamsLiveFingerprint` + `Get-LoamsFingerprintAclPaths` (Task 11, used in Task 17); `Invoke-LoamsEncryptStream` (Task 12, used in Tasks 13, 14); `Invoke-LoamsEncryptedDatabaseDump` (Task 13, used in Task 14); `New-LoamsBackupSet -Recipient` (Task 14) receives `Test-LoamsRecipientCertificate` output (Task 12); `Test-LoamsAcknowledgedReport` returns `.Report` consumed by `Test-LoamsPreChangeDrift` (Task 16); checkpoint `-Context` engine (Task 15) used by every plan checkpoint (Task 17); `Invoke-LoamsHostRestore` reads `services.<role>.{name, situation, snapshot}`, `aclSaves`, `programDataRoot`, `httpdExe`, `mysqldExe`, `myIni` from `backup-manifest.json` and `eventSource`, `layout`, `retention`, `tools` from `rollback\loams-state.json`, all written by Task 14; `New-LoamsConvergePlan -ManifestPath` and `Get-LoamsLiveFingerprint -ProgramDataRoot` updated at every call site (Tasks 11, 17 and their tests); `Stop-LoamsControlPanelMysqld -Port -DataDir` passed from `DbLayout` (Task 17).
- **Counts:** recounted mechanically (regex over the `It` blocks of every test file in this plan); cumulative `expected-test-count.txt` values 14, 42, 49, 60, 71, 93, 102, 113, 130, 147, 165, 182, 205, 232, 241, 261, 305, 308 match the per-file totals in Acceptance 1.
