# LOAMS 2.0 — Phase 4b-iv-a: Time Analytics — "When?" (Design Spec)

**Date:** 2026-08-25
**Status:** Design approved (owner). Ready for `superpowers:writing-plans`.
**Depends on:** 4b-i (Reporting preview, merged PR #40), 4b-ii (export, merged PR #41), 4b-iii-a (on-screen analytics, merged PR #42), 4b-iii-b (export analytics, merged PR #43). Backend: **adds one new endpoint** (`get_report_time_data.php`); no change to `get_report_data.php`.

---

## 1. Problem & goal

4b-iii closed out the question ladder **How much? → Who? → Which? → Details**, but explicitly deferred the fifth rung: **When?** (§11/§12 of the 4b-iii spec). The reason was structural, not a priority call: `get_report_data.php` returns per-student visit **counts**, which is `library_visits.login_time` already aggregated away — there is no time series left to rank once that query has run. Answering "when" requires a **second, purpose-built aggregation** over `login_time` at the database layer.

**Goal:** surface WHEN students visit — a peak-hours breakdown (24 hourly buckets) and a busiest-days breakdown (Monday–Sunday) — on the Reporting dashboard, honoring the exact same filters as the rest of the report (department, course, date range, semester).

**Non-goal / explicit scope guard:** this slice is on-screen only. No PDF/Excel rendering of time analytics (that is 4b-iv-b), no hour×day heatmap, no per-course/per-department time breakdowns, no timezone handling. `login_time` is server-local, exactly as every other datetime in the app — this slice does not introduce timezone awareness.

## 2. Scope & slicing

| Slice | Contents | This spec |
|---|---|---|
| **4b-iv-a** | New backend endpoint (`get_report_time_data.php`) + on-screen "When?" dashboard section | ✅ THIS slice |
| **4b-iv-b** | The same time analytics rendered into the PDF/Excel exports | ⛔ DEFERRED, not this slice |

| Explicitly out of scope (this phase entirely) |
|---|
| Hour × day-of-week heatmap |
| Per-course / per-department time breakdowns |
| Timezone handling (login_time stays server-local) |

## 3. Backend — new endpoint `get_report_time_data.php`

A sibling of `get_report_data.php`, not a modification of it. `get_report_data.php` keeps its existing per-student-row contract untouched; this endpoint answers a different question over the same underlying table.

- **Method:** POST.
- **Request body:** the SAME JSON filter shape as `get_report_data.php`: `{department, course, durationType, start, end, year, semester}`.
- **Filter/WHERE logic is REUSED verbatim** from `get_report_data.php`:
  - `department` optional — empty string or `"all"`/`"all departments"` (case-insensitive) means no department filter.
  - `course` optional — empty string or `"all"`/`"all courses"` means no course filter.
  - `durationType` of `day` / `custom` / `month` → `DATE(v.login_time) BETWEEN start AND end`.
  - `durationType` of `semester` → the server's existing Philippine academic-year windows: first/`"1"` = Jun 1–Oct 31; second/`"2"` = Nov 1–(year+1) Mar 31; `summer` = Apr 1–May 31. Requires `year > 0`.
  - All filter values are bound with `bind_param` (parameterized), matching `get_report_data.php`'s existing safety posture — no string-concatenated SQL.
- **Join:** `library_visits v` INNER JOIN `students s` ON `s.school_id = v.student_id`, so the student-table department/course filters apply correctly and visits from since-deleted or unmatched student rows never appear. Base clause `WHERE 1=1` with the filter conditions appended conditionally, same pattern as `get_report_data.php`.
- **Two aggregations, computed from the same filtered join:**
  - **byHour:** `GROUP BY HOUR(v.login_time)`. PHP densifies the sparse SQL result to a **24-element array**, index 0..23 (0 = midnight hour, 23 = 11 PM hour), any bucket with no matching rows = `0`.
  - **byWeekday:** `GROUP BY DAYOFWEEK(v.login_time)`. PHP densifies to a **7-element array**, 0-based index `i` corresponding to MySQL `DAYOFWEEK` value `i+1` — i.e. `[0]` = Sunday, `[1]` = Monday, … `[6]` = Saturday. This mapping is documented in-code as a comment on the densification block: `DAYOFWEEK 1=Sunday … 7=Saturday`.
- **Response shape** (always fully dense — an empty result set for the filtered range still returns 24 zeros and 7 zeros, never a short array):
  ```json
  {"status": "success", "byHour": [0,0,3,...], "byWeekday": [12,40,38,...]}
  ```
  `byHour` always has length 24. `byWeekday` always has length 7, `[0]` = Sunday.
- **Errors** mirror the existing endpoints' shape: `{"status": "error", "message": "..."}`. Consistent with existing endpoints, the raw `$conn->error` is echoed into `message` on a query failure — this is not a new information-disclosure surface, it matches current practice across the reporting endpoints; broader error-message hardening is a deferred Phase-6 concern, not introduced or fixed in this slice.
- **Security posture:** parameterized query (no injection surface beyond what `get_report_data.php` already carries). The response contains aggregate counts only — no student name, ID, course, or department per row — making it **strictly less sensitive** than the existing roster endpoint. Cleartext HTTP is the app's existing constraint everywhere and is not newly introduced by this endpoint.
- **Deployment:** commit the new PHP file to `deliverables/loams_api/` AND deploy it to `C:/xampp/htdocs/loams_api/`, mirroring the 4b-i all-departments deploy pattern — it goes live on the local XAMPP target ahead of PR merge, which is an accepted, already-established pattern in this project. Before deploying: back up the existing XAMPP target directory, run `php -l` against the new file, then verify with `curl` (see §8) before considering the endpoint ready for the client to call.

## 4. Client architecture — the layering rule

Same load-bearing rule as 4b-iii §3, applied to a new data path: **the aggregator is pure and presentation-agnostic; the backend owns DB semantics + densification; core owns domain normalization; the ViewModel owns presentation state; QML owns presentation only.**

```
get_report_time_data.php response (dense byHour[24], byWeekday[7], Sunday-first)
        │
        ▼
ReportController::fetchTimeAnalytics   — parse + CONTRACT VALIDATION (length, type)
        │
        ▼
TimeAnalytics::compute (core, pure)    — Sun→Mon reorder, peak detection, hasData
        │
        ▼
ReportingViewModel                     — presentation state (labels, bar models, error string)
        │
        ▼
ReportingScreen.qml — "When do students visit?" section
```

- `core/timeanalytics.{h,cpp}` lives in **witscore**, alongside `reportanalytics.{h,cpp}` from 4b-iii. It is `static`, network-free, unit-tested directly, and — per the MVVM rule — is never called from QML; only the ViewModel touches it.
- The backend is responsible for densification (sparse SQL `GROUP BY` result → fixed-length array with zero-filled gaps). The core aggregator **consumes already-dense arrays** — it does not re-densify raw SQL rows. This mirrors 4b-iii's rule that `ReportAnalytics::compute` requires already-normalized input rather than re-deriving normalization itself.
- Contract validation (exact-length checks) happens at the `ReportController` parse boundary, **before** anything reaches the pure aggregator — the aggregator's own defensive length checks (§4.2) are a second, independent safety net, not the primary gate.

### 4.1 `ReportController`

- New method `fetchTimeAnalytics(const QJsonObject &filters)` — POSTs to `get_report_time_data.php`, mirroring the existing `fetchReportRows` request/response plumbing (same `QNetworkAccessManager` usage, same JSON-body construction from `filters`).
- New signal `timeAnalyticsReady(QList<int> byHour, QList<int> byWeekday)`, emitted only on a validated success (see contract rule below).
- Failures — network failure, non-success `status`, or a validation failure — reuse the **existing** error signal(s) that `fetchReportRows` already reports through (`reportError` / `loadError`, whichever `fetchReportRows` currently emits); no new error signal type is introduced for this request.
- **Parse-boundary contract validation (loud, non-negotiable):** a response is only treated as valid if ALL of the following hold:
  - `status == "success"`.
  - `byHour` is present and has **exactly** 24 elements.
  - `byWeekday` is present and has **exactly** 7 elements.
  - Each count parses robustly as a non-negative integer, accepting either a JSON number or a numeric string (defensive against either encoding from PHP).
  - Any violation — wrong length, missing field, non-numeric entry, `status == "error"` — is treated as an error: the error signal is emitted, `timeAnalyticsReady` is **never** emitted, and nothing malformed is ever handed to the aggregator.

### 4.2 Core aggregator — `core/timeanalytics.{h,cpp}`

```cpp
struct TimeAnalytics {
    QList<int> hourly;               // 24 entries, index = hour 0..23 (as received)
    QList<int> weekdayMonFirst;      // 7 entries, index 0=Mon .. 6=Sun (reordered from Sun-first input)
    int        peakHour = 0;         // 0..23, hour bucket with the highest count
    int        peakHourCount = 0;
    int        peakWeekdayMonFirst = 0; // 0=Mon .. 6=Sun
    int        peakWeekdayCount = 0;
    bool       hasData = false;      // true iff any input count > 0
};

// byHour: dense 24-element array, index = hour 0..23.
// byWeekdaySunFirst: dense 7-element array, [0] = Sunday .. [6] = Saturday (the wire format).
static TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour,
                                             const QList<int> &byWeekdaySunFirst);
```

Responsibilities — and ONLY these:
1. **Reorder** the weekday array from Sunday-first (wire format) to Monday-first (`weekdayMonFirst`) for presentation convenience downstream.
2. **Find the peak hour** — the index in `byHour` with the highest count — and record it plus its count.
3. **Find the peak weekday** — the index in the Monday-first array with the highest count — and record it plus its count.
4. **Tie-break rule (documented, deterministic):** when multiple buckets share the maximum count, the **earliest/first bucket wins** — i.e. the lowest hour index, or the earliest weekday in Monday-first order. This applies identically to both the hourly and weekday peak searches.
5. **Set `hasData`** = true iff any single count across either input array is greater than zero. (Both inputs will always agree on this in practice, since they aggregate the same visit rows — hasData is not computed per-array.)

Explicitly **not** this function's job:
- No densification of raw SQL rows — the backend already returns dense arrays; `compute` trusts that shape once past `ReportController`'s validation.
- No locale/format-string work of any kind — `compute` deals only in **indices and counts**. Turning `peakHour == 14` into the string "2–3 PM", or `peakWeekdayMonFirst == 2` into "Wednesday", is the ViewModel's job (§5), not core's — core stays presentation-agnostic exactly like `ReportAnalytics::compute` in 4b-iii.

**Defensive behavior:** if either input array is not exactly the expected length (24 for `byHour`, 7 for `byWeekdaySunFirst`), `compute` returns a `TimeAnalytics` with `hasData = false` and does not attempt to index into the malformed array — this is a second, independent safety net beneath the `ReportController` contract validation in §4.1, not a substitute for it.

## 5. ViewModel — `ReportingViewModel`

### 5.1 Parallel fetch, single-operation guard

- On **Generate**, the ViewModel fires **both** `fetchReportRows` and `fetchTimeAnalytics` **in parallel** — they are two child requests belonging to **one** logical Generate operation, not two independent operations.
- The single-in-flight guard protects **one Generate operation**, not one HTTP request. A second Generate call is a no-op while the current operation is still running (i.e. while either child request is outstanding).
- Two internal flags track completion: `reportRowsSettled` and `timeAnalyticsSettled`, where **settled means success OR failure** — a flag flips to true the moment its request resolves either way, not only on success.
- The operation **finalizes** (loading indicator off, `canGenerate` re-enabled) only when `reportRowsSettled && timeAnalyticsSettled` — i.e. only once both child requests have resolved, in either order.

### 5.2 Outcome state table — graceful degradation

| Rows result | Time result | Reporting outcome |
|---|---|---|
| Success | Success | Full report + "When?" section shows data |
| Success | Error | Full report renders normally; "When?" section shows its own inline time-error state |
| Error | Success | Existing primary Generate error path fires (report does not render); "When?" is not shown |
| Error | Error | Existing primary Generate error path fires (report does not render) |

**Governing principle:** the primary (rows) fetch is **fatal** to the whole reporting operation on failure — this is unchanged from today. The secondary (time) fetch's failure is **localized** to the "When?" section only. This asymmetry is intentional and must never be inverted: a time-analytics failure must never block or blank out the primary report, and a primary-report failure must never be silently masked by a successful time fetch.

### 5.3 Export gating unaffected

`canExport` remains gated on the **primary report rows only** — unchanged from 4b-iii. Time analytics is not part of the export in this slice (that's 4b-iv-b), so a time-fetch error must never block PDF/Excel export, and a time-fetch success is not a precondition for export either.

### 5.4 Cached state and exposed properties

The ViewModel caches the computed `m_timeAnalytics` (a `TimeAnalytics`) and exposes, all with `NOTIFY`:

- Two bar-chart data models feeding the peak-hours and busiest-days charts — reuse the existing `BarsModel` that `LBarChart` already consumes if its shape fits (24-entry and 7-entry series); if it does not fit cleanly, a lightweight equivalent model is acceptable. **This is called out explicitly as an implementation-time decision to confirm against the real `LBarChart`/`BarsModel` component** — the design commits to reuse-where-it-fits, not to inventing a new model type unconditionally.
- `busiestHourLabel` (`QString`) — human-readable 12-hour range derived from `peakHour`, e.g. `"2–3 PM"`.
- `busiestDayLabel` (`QString`) — full weekday name derived from `peakWeekdayMonFirst`, e.g. `"Wednesday"`.
- `hasTimeData` (`bool`) — mirrors `TimeAnalytics::hasData`.
- `timeError` (`QString`) — empty when the time fetch succeeded (or hasn't run yet); set to a user-facing message when the time fetch failed, per §5.2's degradation rule.

**Formatting boundary:** all human-readable formatting — the 12-hour range string, the weekday name — happens in the ViewModel's presentation layer, never in `core/timeanalytics.cpp`. Core hands over indices (`peakHour`, `peakWeekdayMonFirst`); the ViewModel is the only place that knows how to turn `14` into `"2–3 PM"` or `2` into `"Wednesday"`.

## 6. UI — `ReportingScreen.qml`

- A new **"When do students visit?"** section, built as an `LCard`, placed inside the scrollable body (the existing `reportScroll` `Flickable`), positioned **below** the existing KPI band, rankings, and course chart from 4b-iii. Like the rest of the analytics dashboard, it is gated on `hasResult`.
- **Peak-hours chart:** a 24-bar `LBarChart`, one bar per hour (0..23), x-axis labels shown at roughly every-3-hour intervals to avoid label crowding. **All 24 bars are always shown** — the chart is never trimmed to "operating hours" or to only non-zero buckets; the dense 24-element series is rendered in full. Caption reads `"Busiest: 2–3 PM"` bound to `busiestHourLabel`.
- **Busiest-days chart:** a 7-bar `LBarChart` in **Monday–Sunday** order (matching `weekdayMonFirst`/`peakWeekdayMonFirst`). Caption reads `"Busiest: Wednesday"` bound to `busiestDayLabel`.
- **Section states** (independent of the rest of the report — the section's own state reflects only the time request's outcome):
  - **Loading:** the section is dimmed / shows a spinner while the time-analytics request is in flight (rows may already have resolved and be rendering above it).
  - **Success + data:** both charts render with their captions.
  - **Success + all-zero (`hasTimeData == false`):** an empty state, `"No visit activity in this range"`, replaces the charts.
  - **Time-error (`timeError` set):** an inline `"Couldn't load visit times"` message replaces the charts, **while the rest of the report (KPIs, rankings, course chart) continues to render normally above it** — per §5.2, this failure is localized.
- **Theming:** every visual token comes from `Theme.qml` — zero raw hex anywhere in the new section. All labels/captions use `qsTr` and render as `Text.PlainText` (cleartext-HTTP-derived-data rule, same as the rest of Reporting).

## 7. Data-semantics summary (cross-reference)

| Concern | Rule |
|---|---|
| Weekday wire format | `byWeekday[0]` = Sunday … `byWeekday[6]` = Saturday (MySQL `DAYOFWEEK` 1..7, 0-based array) |
| Weekday presentation format | `weekdayMonFirst[0]` = Monday … `weekdayMonFirst[6]` = Sunday |
| Hour format | `byHour[0]` = midnight hour … `byHour[23]` = 11 PM hour, unchanged index convention end-to-end |
| Densification | Backend's job — `compute` never densifies |
| Peak tie-break | Earliest/first bucket wins, both for hour and weekday |
| Empty range | Backend still returns fully dense 24-zero / 7-zero arrays, never a short/omitted array |
| Timezone | None — `login_time` is server-local throughout, unchanged from the rest of the app |

## 8. Error handling & manual verification

- **Client-side:** see §4.1's contract validation and §5.2's degradation table — these are the complete error-handling story for this slice; there is no additional retry logic introduced.
- **Backend has no PHP unit-test harness in-repo.** Verification is manual, via `curl`, before and after deployment:
  1. Back up the existing `C:/xampp/htdocs/loams_api/` target directory.
  2. `php -l get_report_time_data.php` — confirm no syntax errors.
  3. `curl` with an empty-department filter body — confirm `byHour` has 24 entries and `byWeekday` has 7 entries, both integers.
  4. `curl` with a specific department filter — confirm the join/filter narrows the counts as expected relative to the empty-department call.
  5. `curl` against a date range known to contain visit data — confirm non-zero buckets land where expected (spot-check against a manual look at `library_visits` for that range).
  6. `curl` against a date range known to contain **no** visits — confirm the response is `status: success` with all-zero 24/7 arrays, not an error and not a short array.
  7. Only once all of the above pass, deploy to `C:/xampp/htdocs/loams_api/` per §3's deployment note.

## 9. Testing

- **`tst_timeanalytics` (pure, Qt Test):**
  - Sunday-first → Monday-first reorder produces the correct 7-element array.
  - Peak-hour detection picks the correct index and count.
  - Peak-weekday detection picks the correct index (Monday-first) and count.
  - Tie-break: two or more buckets sharing the max count → earliest/first bucket wins, for both hour and weekday searches.
  - All-zero input (both arrays all zeros) → `hasData == false`.
  - Defensive: `byHour` not length 24, or `byWeekdaySunFirst` not length 7 → `hasData == false`, no crash / no out-of-bounds access.
  - Peak counts (`peakHourCount`, `peakWeekdayCount`) are correct alongside the peak indices.
- **`ReportController` parse-boundary test:**
  - A valid, well-formed response produces the correct 24/7 arrays via `timeAnalyticsReady`.
  - A wrong-length `byHour` or `byWeekday` → error signal fires, `timeAnalyticsReady` does not.
  - Malformed/non-numeric entries → error signal fires.
  - String-encoded counts (e.g. `"12"` instead of `12`) parse correctly as the numeric-string-tolerant path, not as an error.
- **`ReportingViewModel` test:**
  - The Generate operation finalizes (loading off, `canGenerate` re-enabled) only when **both** `reportRowsSettled` and `timeAnalyticsSettled` are true, regardless of which settles first.
  - All four combinations from the §5.2 outcome table are exercised, with particular attention to the two graceful-degradation cases: rows-success/time-error (report renders, "When?" shows its inline error) and rows-error/time-success (primary error path fires, "When?" not shown).
  - Bar-chart models are populated correctly from a successful time fetch.
  - `busiestHourLabel` / `busiestDayLabel` produce the correct formatted strings for known peak indices.
  - The all-zero / empty-state path (`hasTimeData == false`) is covered.
  - `canExport` is unaffected by the time-analytics outcome in every combination.
- **`tst_qml_admin` QuickTest** for `ReportingScreen`:
  - The "When?" section renders both charts when time data is present.
  - The all-zero empty state renders.
  - The inline time-error state renders (and the rest of the report is still visible above it).
  - The section is gated off entirely with no result (`hasResult == false`).
  - **Known gotcha (carried forward from 4b-iii-b):** the reporting QuickTest fixture sits inside a clipping `Flickable`. Adding a new section increases total content height, so the reporting-instance height and the host-root height must both be bumped so the taller screen's clickable band actually fits within the visible/interactive area — same class of fix as 4b-iii-b Task 5. Do not skip this or clicks on the new section's states will silently miss.
- **Backend:** no in-repo PHP harness — covered entirely by the manual `curl` + deploy procedure in §8.
- Synthetic test data only in all fixtures — **no real student PII**, consistent with the project-wide security-hygiene rule.

## 10. Build infrastructure

- Add `core/timeanalytics.h` and `core/timeanalytics.cpp` to witscore's `qt-app/core/CMakeLists.txt` — the bare-entry source list, **not** `qt-app/CMakeLists.txt` (the top-level executable target).
- Add a new `tst_timeanalytics` test target in `qt-app/tests/CMakeLists.txt`, registered via `wits_add_qttest()`.
- **Undefined-reference trap (carried forward from the 4b-iii-b `reportanalytics` lesson):** if `tst_timeanalytics` (or any test target that needs `TimeAnalytics::compute`) is structured to compile source files directly rather than linking the `witscore` library — the same pattern `tst_reportrenderer` uses — then `timeanalytics.cpp`/`timeanalytics.h` must be added directly to that target's `SOURCES`, not assumed to be pulled in transitively. Confirm which pattern the new test target actually uses before assuming witscore linkage is sufficient.
- `tst_qml_admin`'s CMake wiring is unaffected beyond whatever new QML fixture files the QuickTests in §9 require — no new CMake target, just additional test cases/fixtures under the existing target.

## 11. Decision freeze

| Area | Decision |
|---|---|
| Backend | New endpoint `get_report_time_data.php`, sibling to `get_report_data.php`; `get_report_data.php` unchanged |
| Filter logic | Reused verbatim from `get_report_data.php` (department/course/duration/semester rules), parameterized |
| Join | `library_visits v` INNER JOIN `students s` ON `s.school_id = v.student_id` |
| Aggregations | `GROUP BY HOUR(v.login_time)` and `GROUP BY DAYOFWEEK(v.login_time)`, both densified server-side |
| byHour shape | Dense 24-element array, index = hour 0..23 |
| byWeekday shape | Dense 7-element array, `[0]` = Sunday .. `[6]` = Saturday (documented in-code) |
| Empty range | Still returns dense all-zero arrays, `status: success` |
| Response/error shape | Matches existing endpoints' `{status, ...}` / `{status:"error", message}` convention |
| Security | Parameterized query; response is aggregate-only (less sensitive than the roster); no new hardening scope |
| Client contract validation | `ReportController` rejects wrong-length/malformed responses before the aggregator ever sees them |
| Core aggregator | `TimeAnalytics::compute(byHour, byWeekdaySunFirst)` — pure, presentation-agnostic, defensive on bad input length |
| Reorder | Sunday-first (wire) → Monday-first (`weekdayMonFirst`), done in core |
| Peak tie-break | Earliest/first bucket wins (hour and weekday alike), documented |
| Formatting | 12-hour range / weekday name strings are ViewModel work, never core |
| Fetch model | Rows + time fetched in parallel as one Generate operation; single-in-flight guard = one operation, not one request |
| Degradation | Rows failure is fatal to the whole operation; time failure is localized to the "When?" section only |
| Export gating | `canExport` still gated on rows only — time-fetch outcome never blocks export |
| On-screen placement | New "When?" `LCard` below KPIs/rankings/course chart, gated on `hasResult` |
| Charts | 24-bar hourly (all 24 always shown) + 7-bar Mon–Sun weekday, both `LBarChart` |
| Section states | loading / success+data / success+all-zero empty state / inline time-error (independent of rows) |
| Export of time analytics | Deferred to 4b-iv-b — not in this slice |
| Heatmap / per-course / per-dept time / timezone | Out of scope for the whole Phase 4b-iv arc |

## 12. Forward notes / out of scope

- **4b-iv-b** — the same time analytics (peak-hours + busiest-days) rendered into the PDF and Excel exports, extending `ReportRenderer` (`paintReport` / `writeReportToXlsx`) the same way 4b-iii-b extended it for `ReportAnalytics`, preserving export/screen parity from one computed truth.
- **Hour × day-of-week heatmap** — not planned in this phase; would require a third, denser aggregation (24×7 buckets) and a new visualization component.
- **Per-course / per-department time breakdowns** — not planned; would require re-running the time aggregation per breakdown dimension rather than once per report.
- **Timezone handling** — not planned; `login_time` remains server-local everywhere in the app, consistent with existing behavior.

Each future slice runs its own `superpowers:writing-plans` → subagent-driven TDD build → `/claude-review` → `create-pr` cycle, per the project workflow rule.
