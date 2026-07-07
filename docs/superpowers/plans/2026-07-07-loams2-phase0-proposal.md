# LOAMS 2.0 — Phase 0 Modernization Proposal — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce the Phase 0 deliverable — a single, comprehensive, evidence-grounded LOAMS 2.0 modernization proposal document (the 20 sections the owner brief requires) — and stop at a hard owner-approval gate. **No application code is written in this plan.**

**Architecture:** This plan builds a *document*, not software. Each task is a research-then-write cycle: a worker inspects the actual repository and the Claude Design reference files, writes an assigned cluster of proposal sections with `file:line` citations for every factual claim, verifies the section against a grounding checklist, and commits. Sections accrue into one Markdown file. The final task writes the executive summary and runs a whole-document consistency pass. The "test" analog for a research document is a grounding/consistency checklist run per task (real citations, required sub-points present, no contradiction with the locked spec decisions, no placeholder text).

**Tech Stack:** Markdown (the deliverable); the subject matter under analysis is Qt 6.11 / C++17 / CMake (Widgets today → Qt Quick/QML target), a PHP 8 / MySQL backend under `deliverables/loams_api/`, and the Claude Design HTML mockups.

## Global Constraints

Every task's requirements implicitly include this section. Copy these verbatim into each worker's brief.

- **This is a documentation deliverable. Write ZERO application code.** Do not create, modify, or delete any `.cpp`, `.h`, `.qml`, `.ui`, `.php`, `.sql`, `CMakeLists.txt`, or resource file. The only file this plan writes is the proposal Markdown document (and this is enforced in review). The owner brief is explicit: "Do not start implementing the migration immediately."
- **Known pre-existing, unrelated working-tree drift at plan start:** `qt-app/adminwindow.ui` has the owner's own in-progress edit, and `uploads/` is an untracked runtime artifact directory. Neither is ever touched by this plan. Every task's "verify grounding" step excludes exactly these two paths by name when checking that only the proposal doc changed — don't broaden that exclusion to anything else.
- **Every factual claim about the current codebase MUST cite `file:line` (or a filename for whole-file claims).** Generic advice is a failure. The owner brief: "reference specific modules/classes/services/files instead of generic advice." A claim a reviewer cannot trace to a real line is a defect.
- **Do not relitigate the locked decisions** in `docs/superpowers/specs/2026-07-07-loams2-qtquick-design.md` §2. The proposal *elaborates within* them (scope = existing modules only; Phase 1 branding engine retained; one app / two surfaces; brand engine + light/dark; targeted backend hardening; migration Strategy A; business logic in C++; Phase 0 → owner approval → phased implementation). If evidence genuinely contradicts a locked decision, flag it in a clearly-marked "⚠ Decision tension" callout — do not silently override.
- **Stay within 2.0 scope.** Inventory, Borrow/Return, and AI Assistant are OUT of 2.0. They appear in the proposal only as *forward-compatibility seams* (where the architecture must not close a door), never as work to be built now.
- **The design reference files are the visual source of truth** and live OUTSIDE this repo at `../Library System UI Modernization/` (`Admin Dashboard.dc.html`, `Library Kiosk v2.dc.html`, `Kiosk Style Options.dc.html`). They are light-theme only; dark is derived.
- **Security hygiene (binding project rule):** no secrets, admin keys, backend credentials, internal URLs, real student PII, or personal machine paths (`C:\Users\...`) anywhere in the document. Use placeholders (`<ADMIN_KEY>`, `http://example.com/endpoint.php`). When quoting `apiconfig.h`'s base URL, present it as an illustrative localhost example, not a live target.
- **Commit convention:** Conventional Commits, subject prefix `docs(phase0):`. Commit via the `commit` skill when running interactively; the pre-specified subjects in each task are the intended messages.
- **The proposal must end recommending that nothing be implemented until the owner approves it** — Phase 0 is a hard stop by design.

**Deliverable file:** `docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md`

**The 20 required sections (owner brief) → task map:**

| # | Section | Task |
|---|---|---|
| 1 | Executive summary | Task 7 |
| 2 | Repository architecture assessment | Task 1 |
| 3 | Technical debt assessment | Task 1 |
| 4 | Design assessment | Task 2 |
| 5 | Product UX assessment | Task 2 |
| 6 | Qt Widgets → Qt Quick migration strategy | Task 6 |
| 7 | Proposed project architecture | Task 3 |
| 8 | Proposed folder structure | Task 3 |
| 9 | Feature / module organization | Task 3 |
| 10 | ViewModel architecture | Task 3 |
| 11 | QML component library | Task 4 |
| 12 | Design system | Task 4 |
| 13 | Theme system | Task 4 |
| 14 | Navigation architecture | Task 4 |
| 15 | Animation guidelines | Task 4 |
| 16 | Backend reuse plan | Task 5 |
| 17 | Backend refactoring plan | Task 5 |
| 18 | Screen-by-screen redesign recommendations | Task 6 |
| 19 | Risk analysis | Task 6 |
| 20 | Phased implementation roadmap | Task 6 |

---

## File Structure

- **Create:** `docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md` — the single deliverable. One responsibility: the complete Phase 0 proposal. Built up section-by-section across tasks; Task 1 scaffolds the full 20-heading skeleton + front matter, each later task fills its assigned headings by replacing that heading's stub.
- **Read-only inputs** (never modified by this plan):
  - Spec: `docs/superpowers/specs/2026-07-07-loams2-qtquick-design.md` (the locked-decision source; the proposal expands it).
  - Prior-effort context: `docs/LOAMS_UI_Modernization_PRD.md`, `docs/superpowers/plans/2026-07-06-brand-theme-engine.md`, `docs/superpowers/reports/2026-07-06-phase1-proofs.md`.
  - Qt app: `qt-app/` (40 top-level `.cpp`/`.h`, 4 `.ui`, `resources/wits.qss`, `CMakeLists.txt`, `tests/CMakeLists.txt` with 12 targets).
  - Backend: `deliverables/loams_api/` (30 PHP files), `deliverables/sql/` (`wits_app.sql`, `branding_config.sql`).
  - Design: `../Library System UI Modernization/*.dc.html` (3 files, outside git).

Because subagent-driven-development runs tasks sequentially with a review gate between each, sequential append to the one document is safe — no parallel write conflict. Task 1 must land the skeleton before any other task runs; encode this ordering when dispatching.

---

## Task 1: Scaffold + Current-State Code Assessment (Sections 2, 3)

Produces the document skeleton and the two evidence-heavy current-state code sections. This is the foundation every later section reasons from, so it goes first and is reviewed strictly for citation quality.

**Files:**
- Create: `docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md`
- Inspect (read-only): all of `qt-app/*.h`, `qt-app/*.cpp`, `qt-app/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`, `qt-app/resources/wits.qss`, the 4 `.ui` files; the spec and prior-effort docs listed in File Structure.

**Interfaces:**
- Consumes: nothing (first task).
- Produces: the on-disk document with all 20 section headings present as `## N. <Title>` with a one-line italic stub `*[to be written in Task X]*` under each unfilled heading; sections 2 and 3 fully written. Later tasks locate their heading and replace the stub. Also produces a stable front-matter block (title, date, status `Draft — pending owner approval`, provenance pointing at the spec, a "Sources of truth" note, and a legend explaining the `⚠ Decision tension` callout convention).

- [ ] **Step 1: Write the document skeleton**

Create the file with: an H1 title `# LOAMS 2.0 — Modernization Proposal (Phase 0)`; a front-matter block (Date 2026-07-07; Status: `Draft — pending owner approval`; Supersedes/Builds-on: the spec at `docs/superpowers/specs/2026-07-07-loams2-qtquick-design.md`; a one-paragraph "How to read this" that states the locked decisions are settled and this document elaborates within them); the 20 section headings in the exact order and titles from the task-map table above, each followed by `*[to be written in Task N]*`. Set §2 and §3's stubs aside for this task.

- [ ] **Step 2: Research the repository architecture (for Section 2)**

Read every controller (`settingscontroller`, `visitorcontroller`, `importcontroller`, `studentcontroller`, `reportcontroller`), `reportrenderer`, both windows (`mainwindow`, `adminwindow`) and `guestwindow`, the RFID pair (`rfidscandetector`, `rfidkeyboardfilter`), the brand engine (`brandtheme`, `brandcolormath.h`, `brandthemedata.h`, `brandingcontroller`), `apiconfig.h`, `theme.h`, the 4 `.ui` files, `CMakeLists.txt`, and `tests/CMakeLists.txt`. For each, note its responsibility, its dependencies, and — critically — the current UI↔logic coupling (which logic already lives in UI-free controllers vs. which still lives inside `QMainWindow`/`QDialog` subclasses).

- [ ] **Step 3: Write Section 2 — Repository architecture assessment**

Cover, with `file:line` citations throughout: the module inventory and layering as it exists today; the single flat `qt_add_executable(WITS ...)` target (`qt-app/CMakeLists.txt:31-53`) and the absence of any library split; the controller extraction already achieved (the five controllers + `reportrenderer` are UI-free) vs. logic still embedded in `mainwindow.cpp`/`adminwindow.cpp`; the injected-`QNetworkAccessManager` convention; the RFID input path (`mainwindow.cpp:95` installs `RfidKeyboardFilter` on `qApp`, a `QApplication`; the filter gates on `QApplication::activeWindow()`/`focusWidget()` at `rfidkeyboardfilter.cpp:25,33-35`); the theming layers (`theme.h` fallback constants + `resources/wits.qss` + the merged brand engine); the test baseline (12 ctest targets — most direct-compile the class under test's `.cpp` alongside the test binary, e.g. `tst_visitorcontroller`, `tst_reportrenderer`, `tst_brandtheme`; `tst_apiconfig` is header-only and `tst_theme`/`tst_responsive_ui` compile no extra business-logic source at all — per `tests/CMakeLists.txt`). State plainly what is already well-positioned for MVVM reuse and what is not. Do NOT propose the target architecture here (that is Section 7) — this section is diagnosis only.

- [ ] **Step 4: Write Section 3 — Technical debt assessment**

Enumerate concrete, cited debt items, each with location + impact + rough remediation cost, e.g.: UI/logic coupling remaining in the window classes; the flat CMake target blocking a shared core; the hardcoded plaintext base URL `http://localhost/loams_api/` in `apiconfig.h:20`; leftover/debug PHP endpoints (`deliverables/loams_api/phpinfo.php`, `testupload.php`, and the status of `hash_admin.php`); any duplication between controllers; the `theme.h` + `wits.qss` + brand-engine three-way theming split; `QSettings` used as a cache but sometimes reasoned about as storage. Rank items (High/Med/Low) by how much they impede the 2.0 migration specifically. Mark anything that is explicitly deferred to the spec's Phase 6 as such.

- [ ] **Step 5: Verify grounding**

Run the checks below. Every one must pass before commit.

```bash
cd "<repo-root>"
# (a) No application-code files were touched by this task (excludes the two known
# pre-existing, unrelated dirty paths from Global Constraints):
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
# (b) No placeholder text left in the two written sections (skeleton stubs for OTHER sections are allowed):
grep -nE 'TBD|TODO|FIXME|XXX' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no debt-markers"
# (c) Spot-check 5 cited file:line refs actually exist and say what the doc claims (manual: open each cited line).
# (d) No secrets/PII/personal paths (matches a single backslash, e.g. C:\Users, or the Mac form):
grep -nE 'C:\\Users|/Users/|[0-9]{2}-[0-9]{4}|ADMIN_KEY *= *[^<]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no obvious secrets/PII/personal-path"
```
Expected: each line prints its `OK:` message (or lists only intended, justified hits). Manually confirm (c): pick 5 `file:line` citations from Sections 2–3 and open them; each must support the sentence citing it.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): scaffold proposal + repo architecture & tech-debt assessment"
```

---

## Task 2: Design & Product-UX Assessment (Sections 4, 5)

The current-state assessment of the *visual/interaction* layer and the *product workflows* — the counterpart to Task 1's code assessment. Requires reading the design reference files and walking the existing screens.

**Files:**
- Modify: the proposal doc (fill §4, §5 stubs).
- Inspect (read-only): the 3 `.dc.html` design files at `../Library System UI Modernization/`; the 4 `.ui` files; `mainwindow.cpp`, `adminwindow.cpp`, `guestwindow.cpp` for current screen flows; `resources/wits.qss` and `theme.h` for current styling; the spec §5/§7 for the already-agreed visual direction.

**Interfaces:**
- Consumes: the skeleton from Task 1 (locate the `## 4.` / `## 5.` stubs).
- Produces: §4 and §5 written; establishes the screen inventory and per-screen pain-point vocabulary that Task 6's screen-by-screen redesign (Section 18) will build on — use consistent screen names (Kiosk login, Live attendance feed, Guest flow, Admin dashboard, Database/students, Reporting, Search, Visit logs, Settings) so Sections 5, 18, and 20 refer to the same set.

- [ ] **Step 1: Study the design references**

Open all three `.dc.html` files. Extract the concrete design language: tokens (confirm brand `#7E1A15`, gold `#E8B10E`, page `#FBF8F3`, card `#FFFFFF`/border `#EFE5D4`, text `#3A2C22`, muted `#8B7A62`, Source Serif 4 + Public Sans, radii 8–18px, the `shade()`/`mix()` derivation JS, dark-on-gold button text); layout structure (kiosk maroon brand panel + feed; admin left sidebar + card grid + tables); motion (durations, `pageIn`/`rowIn`/`barGrow`). Note that the references are light-theme only.

- [ ] **Step 2: Write Section 4 — Design assessment**

Compare the *current* Widgets UI (cite `.ui` files and `wits.qss`) against the design references. Cover: where today's UI already aligns vs. diverges; the gap between `wits.qss`/`theme.h` static styling and the token-driven design system; the visual quality delta (spacing, typography, hierarchy, rounded corners, motion) that "premium desktop" implies; and an explicit reconciliation note wherever the repo and the design conflict (the owner brief asks for this). Reference the spec's §5 design-system decisions as the agreed target so this section is diagnosis, not re-decision.

- [ ] **Step 3: Write Section 5 — Product UX assessment**

Walk each existing screen/workflow (cite the `.ui` file and the driving slots in `mainwindow.cpp`/`adminwindow.cpp`/`guestwindow.cpp`). For each: current workflow, friction points, redundant interactions, missing affordances, and simplification/automation opportunities — but only for in-scope 2.0 modules. Explicitly note where the owner brief's listed modules (Inventory, Borrow/Return, AI Assistant) do **not** exist in the repo, and treat them as out-of-scope forward-seams, not UX work. Keep recommendations at the "what to improve and why" level; the concrete redesign is Section 18.

- [ ] **Step 4: Verify grounding**

```bash
cd "<repo-root>"
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
grep -nE 'TBD|TODO|FIXME|XXX' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no debt-markers"
grep -F '[to be written in Task 2]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: Task 2's assigned sections (4, 5) are filled"
```
Expected: `OK:` on all three. Manually confirm the design tokens quoted in §4 match the `.dc.html` sources (spot-check 3), and that every current-screen claim in §5 cites a real `.ui` file or slot.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): design assessment & product-UX assessment"
```

---

## Task 3: Target Architecture (Sections 7, 8, 9, 10)

The proposed target-state technical design: architecture, folder structure, module organization, and the ViewModel layer. Elaborates the spec's §4 into implementation-ready detail without writing code.

**Files:**
- Modify: the proposal doc (fill §7, §8, §9, §10 stubs).
- Inspect (read-only): spec §4/§6; `qt-app/CMakeLists.txt`, `tests/CMakeLists.txt`; every controller header (for the exact public API a ViewModel would wrap); `reportrenderer.h` (for the Charts/Widgets dependency); `apiconfig.h`.

**Interfaces:**
- Consumes: Task 1's §2 diagnosis (what's already UI-free vs. coupled); Task 2's §5 screen inventory and naming (so the module list in §9 and the ViewModel names in §10 stay aligned with the screen vocabulary already on disk).
- Produces: the canonical directory tree, the `witscore` library boundary, and the ViewModel↔controller mapping that Sections 11–15 (UI systems) and Section 20 (roadmap) both depend on. Name the ViewModels concretely (e.g. `KioskViewModel`, `DashboardViewModel`, `StudentsViewModel`, `VisitLogsViewModel`, `ReportsViewModel`, `SettingsViewModel`, `ThemeViewModel`, plus a `Navigator`) and keep those names identical wherever later sections reference them.

- [ ] **Step 1: Write Section 7 — Proposed project architecture**

Present the shared-core + Qt Quick shell design (spec §4) at implementation depth: the `witscore` static library boundary and its *precise* dependency envelope — business-logic core is Widgets-free, but `reportrenderer` transitively links `Qt::Charts` + `Qt::Widgets` + `QXlsx` because `QChart` derives from `QGraphicsWidget` in Qt 6 (cite `reportrenderer.h` and `tests/CMakeLists.txt:152-157`); the MVVM layering (QML → ViewModels → controllers → network/backend); the rule that ViewModels are the only QML-facing C++; the `Navigator` singleton owning surface/page/modal state; how both the legacy Widgets app and the new Quick app link `witscore` during the parallel-rebuild window; the AUTOMOC/AUTOUIC implications of splitting sources into a library. State the CMake surgery honestly (the current flat target at `CMakeLists.txt:31-53` must be split; each of the 12 test targets keeps its current direct-compile-or-header-only shape, with updated `core/` paths where it compiles a moved source, rather than relinking a monolith).

- [ ] **Step 2: Write Section 8 — Proposed folder structure**

Give the full target tree (expand the spec §4 tree): `qt-app/core/` (moved-not-rewritten controllers, `reportrenderer`, brand engine, `apiconfig`, RFID state machine, data structs), `qt-app/quick/` (`main.cpp`, `viewmodels/`, `models/`, `qml/{AppShell.qml, kiosk/, admin/, components/, theme/}`, `tests/`), the retained `qt-app/tests/` baseline, and the legacy Widgets sources building until Phase 6. Annotate each directory with its one responsibility. Note the `-DBUILD_LEGACY_WIDGETS=ON` default that keeps the rollback alive.

- [ ] **Step 3: Write Section 9 — Feature / module organization**

Define feature-based organization: each in-scope module (kiosk, visitors/dashboard, students/database, reports, search, visit-logs, settings, auth) as a `viewmodel + qml/module + core-service` triple. Show the mapping from existing controller → owning ViewModel → QML module. Specify the forward-seam contract: future modules (inventory, borrow/return, AI) plug in as new triples; nothing in 2.0 may assume the admin sidebar is a closed set (cite where the current sidebar is defined so the "not closed" requirement is concrete).

- [ ] **Step 4: Write Section 10 — ViewModel architecture**

For each named ViewModel, specify: which controller(s) it owns, the `Q_PROPERTY`/`Q_INVOKABLE`/signal surface it exposes to QML, and which `QAbstractListModel` it produces (students, visitors, log rows). Show one worked example end-to-end (e.g. `VisitLogsViewModel` wrapping `VisitorController` — cite the controller's real fetch/filter/export methods — exposing a filtered model + a Today/This-Week segmented-control property). State the threading rule (no blocking the UI thread; async controller signals drive property updates) and the no-live-network test rule for ViewModel unit tests (synthetic payloads, house rule). Confirm `ThemeViewModel` can wrap the free-function `BrandTheme::current()/setCurrent()` (cite `brandtheme.h`) and emit a `changed` signal for QML bindings without modifying `BrandTheme`.

- [ ] **Step 5: Verify grounding & internal name-consistency**

```bash
cd "<repo-root>"
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
grep -nE 'TBD|TODO|FIXME|XXX' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no debt-markers"
grep -F '[to be written in Task 3]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: Task 3's assigned sections (7-10) are filled"
```
Expected: `OK:` on all three. Manually confirm: every controller method cited in §10 exists in the real header; the ViewModel names introduced here are the exact strings later sections will reuse; the directory tree in §8 matches the spec §4 tree (no drift).

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): target architecture, folder structure, module org, ViewModel layer"
```

---

## Task 4: UI Systems (Sections 11, 12, 13, 14, 15)

The presentation-layer systems: component library, design system, theme system, navigation, animation. Turns the design references + spec §5–§7 into a specified (not coded) QML UI system.

**Files:**
- Modify: the proposal doc (fill §11–§15 stubs).
- Inspect (read-only): the 3 `.dc.html` files; spec §5, §6, §7; `brandtheme.h`, `brandcolormath.h`, `brandthemedata.h` (theme mechanics); `theme.h`, `resources/wits.qss` (what's being replaced).

**Interfaces:**
- Consumes: Task 3's ViewModel names and the `qml/components/` + `qml/theme/` structure from §8; Task 2's screen inventory.
- Produces: the `Theme` singleton property contract and the component-library catalog that Section 18 (screen redesigns) composes from. Fix the component names (`LButton`, `LCard`, `LTable`, `LSegmented`, `LSideNav`, `LToast`, `LDialog`, …) and reuse them verbatim in Section 18.

- [ ] **Step 1: Write Section 12 — Design system** (write before §11, since components consume tokens)

Catalog the tokens from the `.dc.html` files (colors, the `shade()`/`mix()` derivations mapped onto `brandcolormath.h`, spacing scale, radii 8–18px, Source Serif 4 + Public Sans typography scale, elevation). Specify these as the `Theme` singleton's property surface. Reference the spec's "no stray hardcoded colors" grep-audit rule as a standing constraint.

- [ ] **Step 2: Write Section 13 — Theme system**

Specify the `Theme` QML singleton backed by `ThemeViewModel` (from Task 3): every visual property role; the brand-engine integration (`BrandTheme::current()` supplies brand roles; logo import → `regenerateFromLogo` → live re-theme via bindings, no restart; persistence on `branding_config` backend + `QSettings` cache, cache-first startup — cite the shipped mechanics in `brandtheme.h`/`brandingcontroller.h`); light + dark with dark surfaces derived and re-contrast-checked via the engine's WCAG machinery (`MinContrast = 3.0` UI / 4.5 text); theme mode (light/dark/system) as a local `QSettings` key; Manual mode remaining a code hook only (no editor UI in 2.0).

- [ ] **Step 3: Write Section 11 — QML component library**

Catalog each reusable component (`LButton`, `LCard`, `LTable`, `LSegmented`, `LSideNav`, `LToast`, `LDialog`, plus stat tile, dashboard bar, page header, etc. as the designs require). For each: purpose, key properties, which `Theme` tokens it binds, and `Accessible.*` role. State the rule: screens compose components and never restyle primitives locally.

- [ ] **Step 4: Write Section 14 — Navigation architecture**

Specify the `Navigator` singleton: surface switching (kiosk ↔ admin), page stack within admin, modal/dialog state, centralized keyboard shortcuts (sidebar cycling, table focus, Ctrl+K quick search). Make flow testable (Navigator is C++, unit-testable). Tie to the two-surfaces-one-executable topology.

- [ ] **Step 5: Write Section 15 — Animation guidelines**

Specify motion from the design files: durations (150–400 ms), easing, the named transitions (`pageIn`, `rowIn`, `barGrow`), all routed through `Theme.motion.*`, with a global reduce-motion switch. Give per-interaction guidance (row entry in the live feed, bar growth on the dashboard, page transitions) referencing the `.dc.html` motion.

- [ ] **Step 6: Verify grounding & name-consistency**

```bash
cd "<repo-root>"
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
grep -nE 'TBD|TODO|FIXME|XXX' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no debt-markers"
grep -F '[to be written in Task 4]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: Task 4's assigned sections (11-15) are filled"
```
Expected: `OK:` on all three. Manually confirm: theme-engine claims cite real symbols in `brandtheme.h`/`brandingcontroller.h`; component names here are exactly those Section 18 will use; the `ThemeViewModel` referenced matches Task 3's §10 definition.

- [ ] **Step 7: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): design system, theme, component library, navigation, animation"
```

---

## Task 5: Backend Reuse & Refactoring (Sections 16, 17)

The backend plan: what to reuse as-is vs. harden, grounded in the real 30-endpoint PHP surface, within the "targeted hardening" locked decision.

**Files:**
- Modify: the proposal doc (fill §16, §17 stubs).
- Inspect (read-only): all 30 files in `deliverables/loams_api/` (esp. `db.php`, `auth_helper.php`, `config.php`, `admin_login.php`, `rfid_login.php`, `student_login.php`, `guest_login.php`, `update_admin_key.php`, `register_student.php`, `bulk_update_students.php`, `get_visitors.php`, `get_report_data.php`, `get_branding.php`, `save_branding.php`, `phpinfo.php`, `testupload.php`, `hash_admin.php`); `deliverables/sql/wits_app.sql`, `branding_config.sql`; `apiconfig.h`.

**Interfaces:**
- Consumes: Task 1's §3 debt items that touched the backend (base URL, debug endpoints).
- Produces: the endpoint-by-endpoint reuse/harden table and the contract-test-first methodology that Section 20's Phase 6 roadmap entry depends on.

- [ ] **Step 1: Inventory the backend**

Build a table of all 30 endpoints: path, method, auth requirement (public vs. `requireAdminAuth`), inputs, what it touches, and current query style (parameterized vs. string-built). Note the shared helpers (`db.php`, `auth_helper.php`, `config.php`) and the schema in `wits_app.sql`.

- [ ] **Step 2: Write Section 16 — Backend reuse plan**

State what is reused verbatim: the PHP/MySQL stack, the working request/response contracts, the auth pattern (`requireAdminAuth`), the branding endpoints from Phase 1. Cite the endpoints the 2.0 client will call unchanged. Emphasize the locked decision: contracts stay; new endpoints are additive off `apiconfig.h`'s single base URL.

- [ ] **Step 3: Write Section 17 — Backend refactoring plan (targeted hardening)**

Within "targeted hardening" only: list concrete, cited hardening items — any string-built SQL to parameterize (cite the file), auth tightening, removal of debug endpoints (`phpinfo.php`, `testupload.php`) and a decision on `hash_admin.php`, the plaintext `http://localhost` base-URL transport question (cite `apiconfig.h:20`), and palette/JSON validation looseness noted in the Phase 1 roll-up. Prescribe the **contract-tests-first** method: capture current request/response fixtures per endpoint, then harden against them for zero contract drift. Map this work to the spec's Phase 6. Explicitly exclude backend redesign (out of scope).

- [ ] **Step 4: Verify grounding**

```bash
cd "<repo-root>"
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
grep -nE 'TBD|TODO|FIXME|XXX' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no debt-markers"
grep -F '[to be written in Task 5]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: Task 5's assigned sections (16, 17) are filled"
# Confirm the endpoint count claim:
ls deliverables/loams_api/*.php | wc -l   # expect 30
```
Expected: `OK:` on the first three; `30` on the count. Manually confirm each hardening item names a real file and that no proposed change alters an existing contract (reuse vs. harden is clearly separated).

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): backend reuse plan & targeted-hardening refactoring plan"
```

---

## Task 6: Migration, Screen Redesigns, Risk, Roadmap (Sections 6, 18, 19, 20)

The synthesis sections that turn everything prior into an ordered, de-risked plan of action. Written after Tasks 1–5 so it can reference their content.

**Files:**
- Modify: the proposal doc (fill §6, §18, §19, §20 stubs).
- Inspect (read-only): the spec §8–§10 (verification model, roadmap, risks); the already-written Sections 2–5, 7–17 of the proposal (to synthesize, not restate); the 3 `.dc.html` files (for the per-screen redesign targets).

**Interfaces:**
- Consumes: screen inventory (Task 2), ViewModel/component names (Tasks 3–4), backend method (Task 5).
- Produces: the phased roadmap that the executive summary (Task 7) headlines.

- [ ] **Step 1: Write Section 6 — Qt Widgets → Qt Quick migration strategy**

Present Strategy A (parallel rebuild on shared core) as the chosen path with its rationale and the alternatives rejected in brainstorming (in-place port; big-bang rewrite) and why. Cover: incremental phase sequencing, reusable-backend preservation, the UI-replacement approach per surface, ViewModel/model migration, the component-library build-out, theme migration, and the rollback posture (Widgets app stays until parity via `-DBUILD_LEGACY_WIDGETS`). Recommend an implementation order that allows continuous verification (the 12 tests green every phase).

- [ ] **Step 2: Write Section 18 — Screen-by-screen redesign recommendations**

For each in-scope screen (the Task 2 inventory), give a concrete redesign referencing the `.dc.html` design and composing the named components from Section 11: Kiosk (maroon brand panel, pulsing scan affordance, gold LOG IN CTA, clock + hours, live attendance feed with row-in motion, "now signed in" hero, stat tiles, guest flow behind the existing settings toggle — RFID behaves exactly as today *as observable behavior*, even though the filter implementation is re-authored); Admin dashboard (sidebar, card grid, hourly/department bars as custom QML); Database/students; Reporting (reuse `reportrenderer` exports); Search (Ctrl+K); Visit logs (Today/This-Week segmented control, inherited from the old P3 scope, landing natively); Settings (live logo re-theming). For each: what changes, which components/ViewModel, and the parity items that must be preserved.

- [ ] **Step 3: Write Section 19 — Risk analysis**

Expand the spec §10 risks with likelihood/impact/mitigation/owner-check for each: RFID under QML focus (top risk; de-risked by the Phase 1 spike; note the filter is re-authored, not reused); QtCharts-vs-QML styling (custom bars; Charts stays in `reportrenderer` export path with its accepted `Qt::Widgets` link); dark mode has no design reference (derived + budgeted human review); deployment-hardware rendering performance (validate on the actual library PC in Phase 1; software-rendering fallback); PHP hardening regressions (contract fixtures first); two apps in one repo (build flag + delete at Phase 6). Add any new risk surfaced while writing Tasks 1–5. Add the reviewer's forward-note: verify "RFID behaves as today" as *observable behavior* in the parity checklist, not as assumed code reuse.

- [ ] **Step 4: Write Section 20 — Phased implementation roadmap**

Reproduce and elaborate the spec §9 roadmap (Phase 0 hard stop → Phase 1 core extraction + AppShell/Theme + RFID spike → Phase 2 kiosk parity → Phase 3 admin part 1 → Phase 4 admin part 2 → Phase 5 dark mode + a11y → Phase 6 backend hardening + Widgets deletion + final review). For each phase: deliverables, the gate (build + 12 tests + `/claude-review` + human walkthrough), and the parity/verification criteria. Make explicit that Phase 0 (this document) ends at owner approval before Phase 1 begins.

- [ ] **Step 5: Verify grounding & cross-section consistency**

```bash
cd "<repo-root>"
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
grep -nE 'TBD|TODO|FIXME|XXX' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no debt-markers"
grep -F '[to be written in Task 6]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: Task 6's assigned sections (6, 18-20) are filled"
```
Expected: `OK:` on all three. Manually confirm: §18 uses the exact component names from §11 and screen names from §5; §20's phases match the spec §9 numbering (Phase 6 exists; no stray "Phase 5 deletes Widgets"); §19 covers all six spec risks plus any new ones.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): migration strategy, screen redesigns, risk analysis, phased roadmap"
```

---

## Task 7: Executive Summary + Whole-Document Consistency Pass (Section 1)

Written last because it summarizes the finished document. Also the single place responsible for end-to-end consistency and flipping the status to review-ready.

**Files:**
- Modify: the proposal doc (fill §1; whole-document pass; front-matter status).

**Interfaces:**
- Consumes: the complete Sections 2–20.
- Produces: the final review-ready document.

- [ ] **Step 1: Write Section 1 — Executive summary**

One-to-two page synthesis for the owner: what LOAMS 2.0 is, the locked decisions, the strategy (parallel rebuild on shared core), the headline of each assessment (repo, debt, design, UX), the phase roadmap at a glance with the Phase 0 approval gate, the top three risks, and a clear "what we are asking you to approve" statement. It must stand alone.

- [ ] **Step 2: Whole-document consistency pass**

Read the entire document start to finish. Fix: any ViewModel/component/screen name used inconsistently across sections; any section that contradicts a locked decision without a `⚠ Decision tension` callout; any 2.0-scope creep (Inventory/Borrow/AI treated as build-now rather than forward-seam); any remaining skeleton stub `*[to be written…]*`; any duplicated content that should be a cross-reference. Confirm all 20 sections are present, in order, and non-empty.

- [ ] **Step 3: Flip status & add the approval gate**

Update front matter status from `Draft — pending owner approval` to `Complete — awaiting owner review`. Ensure the document's closing explicitly states: no implementation begins until the owner approves this proposal (the Phase 0 hard stop).

- [ ] **Step 4: Verify whole document**

```bash
cd "<repo-root>"
git status --porcelain | grep -v '^ M qt-app/adminwindow\.ui$' | grep -v '^?? uploads/$' | grep -v 'docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal\.md$' || echo "OK: only the proposal doc changed beyond known pre-existing drift"
# No skeleton stubs and no debt-markers remain anywhere:
grep -nE 'TBD|TODO|FIXME|XXX|\[to be written' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: no stubs or debt-markers"
# All 20 top-level sections present:
grep -cE '^## [0-9]+\.' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md   # expect 20
# No secrets/PII/personal paths (matches a single backslash, e.g. C:\Users, or the Mac form):
grep -nE 'C:\\Users|/Users/|ADMIN_KEY *= *[^<]' docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md || echo "OK: clean"
```
Expected: `OK:` messages; the section count prints `20`.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md
git commit -m "docs(phase0): executive summary + whole-document consistency pass; mark review-ready"
```

- [ ] **Step 6: Run the design-spec review gate on the finished proposal**

The proposal is itself a design document. Run `/claude-review` (DESIGN-SPEC mode) on `docs/superpowers/proposals/2026-07-07-loams2-phase0-modernization-proposal.md`, naming the same real files each section cites so the reviewer validates grounding. Fix findings and resubmit until APPROVE or the round cap. Then present the finished, reviewed proposal to the owner for the Phase 0 approval decision — **hard stop; no implementation until approval.**

---

## Self-Review

**1. Spec coverage:** All 20 owner-brief sections are mapped to a task (see the task-map table) and every locked decision in spec §2 has a home (scope → §5/§9/§18; Phase 1 retention → §16/§13; topology → §7/§14; theming → §12/§13; backend → §16/§17; Strategy A → §6; C++ logic → §7/§10; process → §20 + Task 7's gate). The spec's verification model (§8) is folded into §20's per-phase gates. No gap found.

**2. Placeholder scan:** The plan contains no "TBD/implement later/similar to Task N" — each task lists exact files to inspect and the exact sub-points each section must contain. The *deliverable* uses skeleton stubs by design (Task 1 creates them, later tasks replace them, Task 7 verifies none remain); this is a mechanism, not a plan placeholder.

**3. Type/name consistency:** ViewModel names (`KioskViewModel`, `DashboardViewModel`, `StudentsViewModel`, `VisitLogsViewModel`, `ReportsViewModel`, `SettingsViewModel`, `ThemeViewModel`, `Navigator`) are introduced in Task 3 and reused in Tasks 4/6. Component names (`LButton`/`LCard`/`LTable`/`LSegmented`/`LSideNav`/`LToast`/`LDialog`) are fixed in Task 4 §11 and reused in Task 6 §18. Screen names are fixed in Task 2 §5 and reused in §18/§20. Section→task numbering matches the task-map table. Consistent.

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-07-07-loams2-phase0-proposal.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task (Task 1 first, since it scaffolds the document; then 2→7 sequentially so appends don't collide), review each section's grounding between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints for review.

**Which approach?**
