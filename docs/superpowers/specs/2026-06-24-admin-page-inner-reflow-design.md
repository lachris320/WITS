# Admin-Page Inner Reflow — Design

Date: 2026-06-24
Branch: feat/responsive-ui
Follows: docs/superpowers/specs/2026-06-23-responsive-ui-design.md
Status: Approved (design); pending implementation plan.

## Problem

The manual GUI check of `WITS.exe` (Task 5 of the responsive-UI work) surfaced two
layout defects in the **admin window's stacked pages**:

1. **General Page** — the page scales, but the widgets *inside* each frame are not
   centered/aligned correctly. They stay glued to the top-left as the frame grows.
2. **Other pages (Database, Reporting, Student Search, Visitor Logs)** — the frames
   themselves do not scale together with the window; they float at a fixed size and
   position even though their internal content is laid out.

The user's goal: "scale all elements together so that it looks the same on the mini
window" — i.e. the layout should stay coherent and proportionate at any window size.

## Root cause

The responsive-UI branch (commits 984a0ab, 8f75f48, 35b6d77, 107d1f9) added Qt layouts
only at the **outermost** level — the window / central widget and a couple of page
containers. Three inner levels were never converted and still use absolute `geometry`
(fixed `x/y/width/height` rects):

1. **Page level (4 of 5 admin pages).** `databasePage`, `reportingPage`,
   `studentSearchPage`, and `visitorPage` have **no top-level layout**. Their direct
   children (group boxes, tables, `…LayoutWidget` containers) carry absolute geometry,
   e.g. `individualRegistrationBox` pinned at `x=320,y=0, 315×591`; `visitorTable` at
   `x=0,y=70, 631×521`; `horizontalLayoutWidget` at `x=0,y=10, 631×51`.
2. **Container artifacts.** The `…LayoutWidget` / `gridLayoutWidget` widgets are Qt
   Designer artifacts: a layout applied to a manually-sized container rather than to the
   page. The inner layout is fine; the container floats.
3. **Frame-content level (General Page).** `generalPage` *does* have a `QGridLayout`, so
   its four frames reflow — but every widget inside each frame
   (`adminNameLineEdit`, `schoolName`, `lineEdit`, labels, buttons, …) is pinned with an
   absolute rect such as `x=70,y=70, 191×24`. So frame growth does not reach the controls.

Net: space stops propagating partway down the widget tree.

## Decisions (locked during brainstorming)

- **Scaling model: reflow, not zoom.** Use native Qt layouts. Widgets stretch/reposition
  and keep readable sizes down to sensible minimums; this is NOT a proportional photo-zoom
  (no `QGraphicsView`/manual font scaling). Consistent with the rest of the branch.
- **Scope:** all five admin pages, plus a re-check sweep of `mainwindow.ui` and
  `guestwindow.ui` for any leftover absolute-geometry children.
- **Inner behavior: natural size, top-aligned.** As a frame grows, its form fields,
  labels, and buttons keep readable, proportionate sizes and group toward the top; trailing
  spacers absorb the leftover room. Fields do not stretch to absurd widths. (Wide,
  content-hungry elements — tables, charts, preview areas — may be the Expanding members of
  their page.)

## Strategy

Replace absolute `geometry` with nested Qt layouts top-down, at every level, so available
space propagates from the window all the way to each control.

### Reusable per-frame conversion pattern

For a frame whose children are absolutely positioned (e.g. General Page's `securityFrame`):

1. Give the frame a `QVBoxLayout`.
2. **Top:** the bold heading label (e.g. `label_2` "Change Admin Password").
3. **Middle:** a `QFormLayout` (or `QGridLayout` where a row pairs a field with a trailing
   button, e.g. `lineEdit` + `showOldBtn`) for the label/field rows. Set the form's field
   growth policy so fields keep their natural width rather than expanding edge-to-edge.
4. **Bottom:** the action button(s) (`updateBtn`, `applyChangesBtn`, …).
5. **Trailing vertical spacer** below the content — this is what top-aligns the group
   instead of letting the layout stretch rows apart.
6. Keep frame `minimumSize` where present; give frames a growing size policy so the page
   grid distributes space to them.

### Per-surface work

- **General Page (`generalPage`)** — keep its `QGridLayout`; tidy the unused/odd row
  indices and any stray spacer rows so the four frames tile cleanly. Apply the per-frame
  pattern to `adminFrame`, `securityFrame`, `libraryFrame`, `settingsFrame`.
- **Database / Reporting / Student Search / Visitor** — add a top-level layout to each
  page (`QVBoxLayout` / `QHBoxLayout` / `QGridLayout` as the existing visual arrangement
  dictates); dissolve the `…LayoutWidget` artifact containers and lift their inner layouts
  so the page layout owns them; designate tables / charts / preview panels as the Expanding
  members and keep narrow form fields at natural size.
- **Re-check main/guest** — sweep `mainwindow.ui` and `guestwindow.ui` for any remaining
  absolute-geometry children inside frames; convert any found. No-op if already clean.

## Invariant: preserve every objectName

This is a **layout-only** restructure, never a rename. `adminwindow.cpp`,
`mainwindow.cpp`, `guestwindow.cpp`, and the per-page stylesheets bind to widgets by name
string (e.g. `QWidget#reportingPage`, `QGroupBox`, slot wiring on `updateBtn`,
`applyChangesBtn`, `visitorTable`, `schoolName`, …). Renaming or dropping any widget
silently breaks slots and styling. Every existing `objectName` must survive the conversion.

## Testing (TDD)

Extend the existing `tst_responsive_ui` `QUiLoader`-based contract test (red → green):

- **Red first**, against the current `.ui` files, assert:
  - each admin page widget (`generalPage`, `databasePage`, `reportingPage`,
    `studentSearchPage`, `visitorPage`) has a non-null top-level `layout()`;
  - each key frame / group box has a non-null `layout()`;
  - every critical `objectName` still resolves via `findChild` after load (rename guard);
  - frames carry a growing (Expanding/Preferred) horizontal size policy.
- Convert the `.ui` files until the suite is **green**.

Automated tests prove the **structure** (layouts exist, names preserved). They do **not**
prove visual fidelity — that is confirmed only by running `WITS.exe` at multiple window
sizes (fullscreen / maximized / restored / minimal), the same manual step as Task 5.

## Guardrails

- **`.ui`-only + test file.** This change should require **zero** edits to
  `adminwindow.cpp` / `mainwindow.cpp` / `guestwindow.cpp`, which keeps it clear of the
  uncommitted RFID changes coexisting in the working tree. If a `.cpp` edit appears
  necessary, stop and reassess — do not touch RFID hunks.
- **Commit scope.** Commit only the responsive `.ui` files and the test file, via the
  `commit` skill. Never stage the RFID working-tree changes
  (`rfid_login.php`, `adminwindow.cpp`, `guestwindow.cpp`, `mainwindow.cpp`,
  `tests/CMakeLists.txt`, `apiconfig.h`, `tst_apiconfig.cpp`).
- **Stylesheets.** Preserve per-page `styleSheet` blocks and the object names they target.

## Workflow

Per the project rules (`.claude/rules/workflow.md`): subagent-driven TDD. One subagent per
page (or per logical group) owning its red → green → refactor cycle; the main agent
orchestrates, dispatches, and synthesizes. After all pages are green and the build is
clean: run `/claude-review`, fix Critical/Important findings, then the user does the manual
multi-size run of `WITS.exe` before `create-pr`.

## Out of scope

- Proportional/zoom scaling of fonts and controls.
- Renaming widgets or refactoring slot logic.
- The RFID integration changes in the working tree.
- The three Low review follow-ups recorded in the responsive-UI progress ledger
  (row-stretch-equality test, header order/stretch assertion, dead `#label` rule).

## Success criteria

1. Resizing the admin window keeps each page's frames filling the window and their inner
   controls aligned/top-grouped — no top-left glue, no floating fixed-size frames.
2. The small ("mini") window shows the same coherent arrangement as the large window,
   reflowed rather than clipped.
3. `tst_responsive_ui` is green; the full build is clean; all existing object names resolve.
4. `mainwindow.ui` and `guestwindow.ui` confirmed free of stray absolute-geometry children.
