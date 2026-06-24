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

**Do the conversion in Qt Designer**, not by hand-editing XML. The admin form is ~2584
lines with ~61 absolute-geometry rects; hand-editing risks malformed forms and accidental
`objectName` loss. Designer's Lay Out / Break Layout / Morph operations preserve object
names and emit valid `.ui` XML. The structural test (below) is the backstop that the
conversion did what was intended. One caveat for review hygiene: Designer rewrites the whole
file (and may flip line endings); keep each page's commit reviewable by checking the
semantic diff with `git diff --ignore-all-space` and confirming no `name="…"` was dropped.

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
- **Database / Reporting / Visitor** — add a top-level layout to each page (`QVBoxLayout`
  / `QHBoxLayout` / `QGridLayout` as the existing visual arrangement dictates); dissolve the
  `…LayoutWidget` artifact containers and lift their inner layouts so the page layout owns
  them; designate tables / charts / preview panels (e.g. `visitorTable`, `chartsPreviewBox`)
  as the **Expanding** members and keep narrow form fields at natural size.
- **Student Search** — same as above, **with one carve-out**: `searchOverlay` (see
  "Overlay widgets" below) must NOT be added to the page layout.
- **Re-check main/guest** — sweep `mainwindow.ui` and `guestwindow.ui` for any remaining
  absolute-geometry children inside frames; convert any found. No-op if already clean.

### Overlay widgets — do not lay them out (Critical carve-out)

`studentSearchPage` contains `searchOverlay` (`adminwindow.ui:2309`, `native="true"`, abs
geometry `0,91,631×451`) — a full-page busy overlay that is `raise()`d, shown/hidden, and
whose `width()/height()` are read at runtime to center a spinner
(`adminWindow::showSearchOverlay`, `adminwindow.cpp:~3616-3648`), driven by ~20
`showTemporaryOverlay(ui->studentSearchPage, …)` calls. **If it is placed in the page
layout it stops covering the page and the spinner mis-centers → broken search UX.**

Rule: overlay-style widgets stay **direct children of the page but are NOT added to its
layout** — they remain free-floating, raised siblings positioned by the existing runtime
code. The page layout manages only the real content widgets. Before converting any page,
identify such overlays (look for `native="true"` siblings that the `.cpp` `raise()`s or
whose geometry it reads) and exclude them.

Forward-note (out of scope, optional `.cpp` follow-up): `searchOverlay` is currently fixed
at `631×451`, so it does not track the page as it grows. Making it fill the reflowed page
responsively would need a `resizeEvent` in `adminwindow.cpp` — deliberately deferred to keep
this a `.ui`-only change. Not a regression: the overlay is no worse than today.

## Invariant: preserve every objectName

This is a **layout-only** restructure, never a rename. `adminwindow.cpp`,
`mainwindow.cpp`, `guestwindow.cpp`, and the per-page stylesheets bind to widgets by name
string (e.g. `QWidget#reportingPage`, `QGroupBox`, slot wiring on `updateBtn`,
`applyChangesBtn`, `visitorTable`, `schoolName`, `searchOverlay`, …). Renaming or dropping
any widget silently breaks slots and styling. Every existing `objectName` must survive the
conversion — enforced both by `findChild` lookups and the mechanical name-set diff (see
Testing).

## Testing (TDD)

Extend the existing `tst_responsive_ui` `QUiLoader`-based contract test (red → green):

- **Red first**, against the current `.ui` files. Assert **per surface**, not uniformly —
  some assertions are pre-satisfied today and must be placed where they actually fail:
  - **Page-level layout** for the four pages that lack one (`databasePage`,
    `reportingPage`, `studentSearchPage`, `visitorPage`): non-null `page->layout()`.
    *Note:* `generalPage` already has `gridLayout`, so its real red is at the **frame**
    level, not the page level — assert frame layouts there, not a redundant page check.
  - **Frame-level layout**: each absolutely-positioned frame / group box gains a non-null
    `layout()` — `adminFrame`, `securityFrame`, `libraryFrame`, `settingsFrame` on General;
    and any frame on the other pages whose children are converted.
  - **Expanding members**: assert the designated content widgets carry a horizontal
    size policy of specifically **`QSizePolicy::Expanding`** (e.g. `visitorTable`,
    `chartsPreviewBox`). A generic "growing" check would NOT go red — `QFrame`/`QGroupBox`
    already default to `Preferred` — so require `Expanding` on the named members to make it
    a real guard.
  - **objectName rename guard, two layers:** (a) every enumerated critical `objectName`
    still resolves via `findChild` after load; plus (b) a **mechanical** check — the sorted
    set of `name="…"` attributes after conversion is a superset of the committed set, i.e.
    **zero deletions** (catches names the enumerated list forgot). The overlay carve-out
    means `searchOverlay` must still resolve via `findChild` after Student Search converts.
- Convert the `.ui` files until the suite is **green**.

Automated tests prove the **structure** (layouts exist, names preserved, Expanding members
expand). They do **not** prove visual fidelity or correct centering — that is confirmed only
by running `WITS.exe` at multiple window sizes (fullscreen / maximized / restored /
minimal), the same manual step as Task 5. The Student Search busy-overlay must be
exercised manually too, since the test can't see the runtime spinner.

## Guardrails

- **`.ui`-only + test file.** This change should require **zero** edits to
  `adminwindow.cpp` / `mainwindow.cpp` / `guestwindow.cpp`. The reason it holds: the runtime
  geometry calls in `adminwindow.cpp` (the search spinner, `showSearchOverlay`, the
  `showTemporaryOverlay` widgets) all target widgets that are **never added to a layout**, so
  the new layouts don't fight them — *provided* the overlay carve-out above is honored. The
  one **expected exception** to "stop if a `.cpp` edit seems necessary" is overlay handling;
  everything else needing a `.cpp` edit is a signal to stop and reassess (and never touch
  RFID hunks).
- **minimumSize for graceful reflow.** Today only `adminFrame` carries a `minimumSize`.
  Without sensible minimums on frames/pages, reflow can *clip* content at small window sizes
  instead of staying coherent — undercutting success-criterion #2. Set modest `minimumSize`
  values on the converted frames so the small window reflows rather than truncates.
- **Commit scope.** Commit only the responsive `.ui` files and the test file, via the
  `commit` skill. Never stage the RFID working-tree changes
  (`rfid_login.php`, `adminwindow.cpp`, `guestwindow.cpp`, `mainwindow.cpp`,
  `tests/CMakeLists.txt`, `apiconfig.h`, `tst_apiconfig.cpp`). Also discard the unrelated
  Qt Designer churn already sitting in the working-tree `adminwindow.ui` (CRLF re-encode +
  a stray `currentIndex` change) before starting, so the conversion begins from clean
  committed `.ui`.
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
5. The Student Search busy-overlay (`searchOverlay`) still covers the page and its spinner
   centers correctly when triggered — verified by exercising a search manually.
