# RFID Card-Code Mismatch — Diagnosis & Fix Design

**Status:** Draft for review (design-spec mode)
**Date:** 2026-06-24
**Branch:** `feat/responsive-ui` (RFID integration follow-up)
**Related:** `docs/superpowers/specs/2026-06-23-rfid-integration-design.md` (the feature this extends)

---

## Goal

A registered student who taps their RFID card at the kiosk is logged in and
their visit recorded — instead of getting **"Card not registered. Please see
the librarian."** when the card *is* registered.

## Symptom (observed)

Tapping a known card shows the kiosk error *"Card not registered. Please see
the librarian."* Manual login for the **same student** (typing their school ID
+ admin key) succeeds. One sample card emits the value `0003241370`.

## What is already ruled out

The end-to-end RFID transport works. The kiosk error string is produced by the
client at [`mainwindow.cpp:403`](../../../qt-app/mainwindow.cpp) when it receives
a well-formed JSON object whose `status != "success"`. The *expected* such
response is `{"status":"error","message":"RFID not registered"}`, echoed at
[`rfid_login.php:89`](../../../deliverables/loams_api/rfid_login.php) **only** when

```sql
SELECT * FROM students WHERE code = ? LIMIT 1
```

matches **zero rows** (the `else` branch of the `fetch_assoc` success test at
`:30`). Therefore:

- The request reaches the backend (else: `QNetworkReply` error → *"Network
  error"*, not *"Card not registered"*).
- The `loams_api/` base URL and routing are correct (same `ApiConfig::endpoint`
  that the working manual login uses — [`apiconfig.h:18`](../../../qt-app/apiconfig.h)).
- The response is valid JSON (else: *"Invalid server response"* at
  [`mainwindow.cpp:395`](../../../qt-app/mainwindow.cpp), which was the *earlier*,
  now-fixed `rtrim` parse-error bug — a different defect).

**Caveat — "Card not registered" is not a unique signal.** The client funnels
*every* non-success JSON object into that one error string. In particular the
`catch` block at [`rfid_login.php:97`](../../../deliverables/loams_api/rfid_login.php)
returns `{"status":"error","message":"Internal server error"}` **with no
`http_response_code()` set — i.e. HTTP 200** — so a *registered* card whose
`library_visits` INSERT throws (NOT NULL / type / FK on the visit row) would show
the identical kiosk message. Phase 1 therefore must read the literal response
body: only `"RFID not registered"` confirms the zero-row value mismatch below;
`"Internal server error"` points at the insert/transaction instead.

**Once the body is confirmed to be `"RFID not registered"`, the defect is
narrowed to exactly one thing: the card value sent by the kiosk does not
string-equal any `students.code` in the database.**

## Data flow (with the relevant seams)

```
HID reader (keyboard wedge)
  → RfidScanDetector::feedKey()        qt-app/rfidscandetector.cpp:11   (buffers digits, Enter terminates)
  → RfidKeyboardFilter::eventFilter()  qt-app/rfidkeyboardfilter.cpp:31 (emits rfidScanned(code))
  → MainWindow::handleRfidLogin(code)  qt-app/mainwindow.cpp:359        (POST rfid_id=code)
  → rfid_login.php                     trim($_POST['rfid_id'])          (SELECT ... WHERE code = ?)

Registration side (how students.code is written):
  Admin form ONLY → adminwindow.cpp:1044 addPart("code", codeLineEdit) → register_student.php:13 trim() → INSERT
```

**The admin form is the *only* writer of `students.code`.** The bulk paths do
**not** touch it: `upload_students_zip.php`'s INSERT (`:73-76`) is
`(school_id, name, course, department, year_level, gender, status, photo)` — no
`code` column — and `bulk_update_students.php`'s UPDATE (`:43-44`) sets only
`name, course, year_level, department, gender, status`. A bulk-imported student
therefore has a NULL/empty `code` until an admin fills it in by hand. The single
write path (`register_student.php:13`) and the read path
([`rfid_login.php:11`](../../../deliverables/loams_api/rfid_login.php)) both only
`trim()` — no leading-zero handling, width-padding, or case-folding exists at any
seam.

## Root-cause investigation status (the Iron Law)

Root cause is **not yet confirmed** — and the existing instrumentation cannot
confirm it, because it logs the wrong value:

> [`mainwindow.cpp:372`](../../../qt-app/mainwindow.cpp) prints
> `qDebug() << "POST DATA:" << postData.toString();` **before** line 373 appends
> `rfid_id`. It therefore always prints `POST DATA: ""` and never reveals the
> bytes actually transmitted.

So Phase 1 of this design is to **fix the diagnostic and capture ground truth**
before changing any matching logic. We will not "fix" a mismatch we have not
measured.

### Ranked hypotheses (to be confirmed by Phase 1 evidence)

1. **Data-entry mismatch on the admin form (most likely).** Because the admin
   form is the *only* writer of `code`, the live cause is almost certainly how
   it was filled in: a librarian typed the card's *printed* number (e.g.
   `3241370`, or a serial that differs from the reader's output) into
   `codeLineEdit` instead of letting the reader scan into the field. The stored
   `code` then never equals the bytes the reader emits at the kiosk. (The four
   leading zeros in `0003241370` are a plausible instance — typed without them —
   but the mechanism is hand entry, **not** CSV/Excel import, which never writes
   `code` at all.)
2. **Hidden/extra characters from the reader.** A trailing/leading whitespace,
   a checksum digit, a prefix (e.g. `;`), or a different encoding the reader
   appends — invisible in a normal log, visible in a length/hex dump. Would
   affect the kiosk side even if the stored value was scanned in correctly.
3. **Genuinely unregistered card.** The student exists (school ID works) but
   their `code` column is empty/NULL (e.g. bulk-imported and never filled in) or
   holds a *different* card's value. Operational gap, not an app defect.

These three are not sharply separated — (1) and (3) are points on the same
"how was `code` populated" axis. Phase 1 (emitted bytes vs. stored bytes)
distinguishes a *format* difference (1/2) from an *empty/wrong* value (3)
definitively, and the answer to open-question #1 pins down the mechanism.

## Design

### Phase 1 — Diagnostic (evidence before fix)

Make the instrumentation capture exactly what is sent and exactly what is
stored, so the mismatch is observable, not inferred.

1. In `handleRfidLogin`, log the *real* code with enough detail to expose
   invisible differences — the raw string, its **length**, and a **hex/latin1
   dump** — and log it *after* it is set, e.g.:

   ```cpp
   qDebug() << "RFID code:" << code
            << "len:" << code.length()
            << "hex:" << code.toLatin1().toHex(' ');
   ```

   Move/replace the broken line at `mainwindow.cpp:372`. (Temporary — see
   Security.)

2. The operator re-scans the known sample card (`0003241370`) and reads back the
   stored value with a one-off, read-only check against the DB, e.g.
   `SELECT id, school_id, code, LENGTH(code) FROM students WHERE school_id = '<that student>';`
   (run by the user — the client cannot reach the DB directly).

3. Compare: scanned string + length + hex **vs** stored `code` + `LENGTH`. The
   exact difference (missing leading zeros, trailing `\r`, extra checksum char,
   case) selects the fix in Phase 2. If they are byte-identical yet still miss,
   escalate — the query/collation is implicated, not the value.

**Exit criterion:** a written statement of the precise byte-level difference
between emitted code and stored code (or "byte-identical" if not a value bug).

### Phase 2 — Fix (gated on Phase 1 evidence)

Pick the **narrowest** change that the evidence supports. Options, with the
recommendation conditional on the confirmed cause:

- **Option A — Canonical normalization at every seam (recommended if the cause
  is leading-zero/whitespace, hypothesis 1/2).** Define one pure normalization
  for RFID codes and apply it at the *three* choke points that handle a `code`
  value so writes and reads agree: kiosk scan (`handleRfidLogin`), admin
  registration (`register_student.php` — the sole writer of `code`), and the
  lookup (`rfid_login.php`). Normalization must be **lossless and reversible in intent**
  — e.g. `trim()` + strip a *known* fixed prefix, or compare on a zero-padded
  fixed width — rather than blindly stripping all leading zeros (which could
  collide two distinct cards). The exact rule is chosen from the measured
  difference, not guessed. Because the detector/normalizer is pure C++, it is
  unit-testable in the existing `wits_tests` target.

  - **A-server-only sub-variant:** if only the lookup can be changed safely
    without a data migration, make `rfid_login.php` match on a normalized
    expression (e.g. compare zero-stripped or width-padded forms on *both*
    sides of the predicate). Document the collation/index impact.

- **Option B — Data backfill (recommended if hypothesis 1 confirmed AND the app
  already sends the canonical full form).** Leave the app alone; run a one-off,
  reviewed SQL migration that re-pads/repairs the `students.code` column to the
  reader's canonical format, and close the recurrence on the **registration
  path** (scan-into-field per the Decision rule) so newly-entered codes match by
  construction. Lower app-code risk, but touches live data — must be backed up
  and reviewed.

- **Option C — Operational only (if hypothesis 3 confirmed).** No app/SQL logic
  change; the card was never linked. Document the registration procedure (scan
  the card *into* the admin code field rather than typing the printed number) so
  the stored `code` always equals what the reader emits. Add validation in the
  admin form that the entered code matches a fresh scan.

**Decision rule:** Phase 1 evidence → exactly one option. Do not implement more
than one. The *recurrence* fix lives on the **registration path**, since that is
the only writer of `code`: make the admin form capture the code by *scanning the
card into the field* (so the stored value is, by construction, exactly what the
reader emits) rather than relying on a hand-typed printed number, and/or validate
the entered code against a live scan. (Note: hardening `upload_students_zip.php` /
`bulk_update_students.php` is *not* applicable here — those paths never write
`code`, so there is no zero-stripping to prevent there. If future bulk import is
meant to populate `code`, that is a separate enhancement, not this fix.)

## Testing

- **Pure unit tests (QtTest, `wits_tests`):** if Option A introduces a
  normalization function, cover it red→green: leading zeros preserved/handled
  per the chosen rule, whitespace trimmed, idempotence
  (`normalize(normalize(x)) == normalize(x)`), and that a known emitted form and
  a known stored form normalize equal. No network — feed strings directly, per
  the existing test harness conventions.
- **PHP side:** no unit harness exists; verify `rfid_login.php` manually with a
  captured payload (`curl`/Postman with `rfid_id=<measured value>`) against a
  test row, asserting `status:success`. Do **not** test against real student
  rows in a way that pollutes `library_visits`.
- **End-to-end:** operator scans the sample card → student displays and a visit
  is recorded; a deliberately-unknown card still shows the *"Card not
  registered"* path (the error path must survive the fix).

## Security / PII constraints (binding)

- The Phase-1 `qDebug` prints a **card code** — that is borderline PII linked to
  a person. It is a **temporary diagnostic only** and MUST be removed before the
  branch is committed/merged. No card codes in persistent logs or in committed
  code. (Matches the project security-hygiene rule: keep card codes out of
  persistent logs.)
- Any SQL run in Phase 1/2 against real data is read-first; any migration
  (Option B) requires a backup and explicit review. Real student rows are PII —
  no real `code`/`school_id`/name values may be pasted into commits, fixtures,
  or this repo.
- Keep using the existing `http://localhost/loams_api/` transport as-is — the
  pre-existing plaintext-localhost pattern is accepted and out of scope here.

## Non-goals

- Barcode scanning (explicitly deferred to a later phase).
- The responsive-UI / full-screen work (separate task/branch).
- The invisible-input-text dark-mode bug (separate task/branch).
- Re-opening the already-fixed `rtrim` parse-error in `rfid_login.php`.

## Open questions (for the reviewer / to resolve in Phase 1)

1. How was the failing student's `code` originally entered — typed from the
   card's printed number, scanned into the admin field, or bulk-imported from a
   spreadsheet? (Selects between hypotheses 1 and 3.)
2. Does the reader emit a fixed-width 10-digit decimal for *all* cards, or does
   width vary per card? (Determines whether "zero-pad to width N" is safe vs.
   "strip leading zeros".)
3. Is `students.code` column type `VARCHAR` or an integer type? An integer
   column cannot store leading zeros at all, which would *force* Option A/B.
   Note: the *existence* of this bug is already evidence the column is textual —
   if `code` were an integer type, `WHERE code = '0003241370'` would coerce the
   string to `3241370` and **match** a row stored as `3241370`, so the mismatch
   could not occur. All binds are `"s"` (string), consistent with `VARCHAR`.
   Confirm the column type to be certain, but expect textual.
