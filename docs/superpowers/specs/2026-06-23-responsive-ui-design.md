# Responsive WITS UI — Design

**Date:** 2026-06-23
**Status:** Approved (pending spec review)
**Scope:** Make the WITS Qt 6 / C++17 desktop UI fill any screen size by removing
fixed-size caps and converting absolute-geometry layouts to Qt layout managers,
while preserving the existing visual design, object names, and signal/slot wiring.

## Problem

The app does not fill the screen and its content does not scale:

1. **`mainwindow.ui`** has a hard `maximumSize` of **1400×846** on the top-level
   `MainWindow`. Even though `mainwindow.cpp` calls `showFullScreen()`, the window
   physically cannot grow past 1400×846, leaving empty space on larger displays.
2. **`mainwindow.ui` uses absolute pixel geometry with no layout managers**
   (85 `geometry` properties, 0 layouts). Even if the window grew, nothing reflows.
3. **`adminwindow.ui`** is *already* layout-based (26 layouts) but is hard-locked to a
   **fixed 760×600** (`minimumSize` == `maximumSize`). Additionally,
   **`adminwindow.cpp:665-668`** positions four dashboard frames with hard-coded
   `setGeometry(...)` at construction, overriding the layouts — this is the real
   reason the admin dashboard does not scale.
4. **`guestwindow.ui`** is a small modal `QDialog`, already laid out; only minor tidy.

## Goals / Non-goals

**Goals**
- MainWindow fills any screen (fullscreen/maximized) and content reflows.
- Admin window becomes resizable and maximizable, dashboard reflows.
- Preserve the existing look/proportions: flexible-width left sidebar, expanding content.
- **Preserve every existing object name** so the `setStyleSheet(R"(...)")` block in
  `mainwindow.cpp` (selectors like `#frame`, `#frame_2`, `#frame_3`, `#stdList`,
  `#schLogo_Image`, `#username`, `#loginBtn`, `#time_label`, `#date_label`, `#label`),
  the `QGraphicsDropShadowEffect` on `ui->widget` / `ui->frame_3`, and
  `refreshRightPanel()`'s label lists keep working unchanged.

**Non-goals**
- No rewrite of the recent-logins area into a `QTableWidget`/model (would break the
  object-name-based wiring in `refreshRightPanel()`).
- No restyling, color, or font changes beyond what layout conversion requires.
- No functional/behavioral change to login, RFID, or networking code.

## Design

### 1. MainWindow top-level (`mainwindow.ui`)
- **Remove** the `maximumSize` (1400×846) property entirely → defaults to unlimited
  (`QWIDGETSIZE_MAX`). Keep `minimumSize` 800×600.
- Add a **`QHBoxLayout`** to `centralwidget` (contents margins 0, spacing 0) with:
  - `frame` (sidebar): horizontal sizePolicy `Preferred`/Fixed-ish, **flexible width
    240–320px** (`minimumWidth` 240, `maximumWidth` 320), vertical `Expanding`,
    layout stretch 0.
  - `frame_2` (content): sizePolicy `Expanding`/`Expanding`, layout stretch 1.

### 2. Sidebar internals (`frame`)
Convert absolute children to a **`QVBoxLayout`**, top→bottom:
`schLogo_Image`, `schoolNameLabel`, `schAddressLabel`, spacer, `username`,
`loginBtn`, `guestLoginBtn`, `line`, `time_label`, `date_label`, expanding stretch,
`poster_Image`.
- Buttons (`loginBtn`, `guestLoginBtn`) wrapped so they keep their current centered,
  non-full-width look (e.g. an inner `QHBoxLayout` with side stretches, or a bounded
  `maximumWidth`).
- `schLogo_Image` and `poster_Image` keep aspect ratio; `updateLogo()` /
  `updatePoster()` already scale to `->size()` and are re-invoked from `resizeEvent()`,
  so images follow the new layout automatically.

### 3. Content area — recent-logins table (`frame_2` → `stdList`)
- `frame_2` gets a `QVBoxLayout` holding the existing `stdList` `QScrollArea`
  (already `widgetResizable=true`).
- `scrollAreaWidgetContents` gets a **`QVBoxLayout`**:
  `[ widget (detail card) ][ frame_3 (header) ][ widget_2 … widget_9 rows ][ stretch ]`.
- **Detail card `widget`**: `HBox[ studentPhoto | VBox[ nameLabel,
  Grid[(label_5,courseLabel),(label_2,yrlevel_label),(label_4,depLabel),
  (label_3,timeDate_Label)] ] ]`. All object names preserved.
- **Each data row `widget_N`** (`widget_2`,`_3`,`_4`,`_5`,`_6`,`_7`,`_8`,`_9`): a
  `QHBoxLayout` of its five labels (`nameLabel_N`, `courseLabel_N`, `yrlevel_label_N`,
  `depLabel_N`, `timeDate_Label_N`) using **identical column stretch factors**
  `name:course:year:dept:time = 24:23:9:19:27` (derived from the original column
  widths 241/231/91/191/271, preserving proportions). A small left margin (~10px)
  matches the original `x=10` inset.
- **`frame_3` header**: gets a `QHBoxLayout` using the **same** `24:23:9:19:27`
  stretches so the header lines up with the data columns. (It currently has no child
  header labels; if column captions are desired they can be added later — out of scope.)
- **`line_6`** (absolute vertical divider at x=810): **dropped** — it does not survive
  a layout cleanly. This is the one intentional minor visual change.

> Note on row object-name numbering: `refreshRightPanel()` iterates the labels
> `nameLabel`, `_2`, `_3`, `_5`, `_7`, `_8`, `_9`, `_11`, `_13` (and the parallel
> course/year/dep/time lists). The conversion MUST NOT rename any of these — only
> re-parent them into per-row `QHBoxLayout`s.

### 4. AdminWindow (`adminwindow.ui` + `adminwindow.cpp`)
- **`.ui`**: remove the fixed lock — set a sensible `minimumSize` (e.g. keep 760×600
  as the minimum) and **remove `maximumSize`**. Audit the 24 child `minimumSize` /
  `maximumSize` caps and relax those on **container frames** that block growth; leave
  sensible caps on small fixed controls (icons, spinners).
- **`.cpp:665-668`**: replace the four `ui->securityFrame/adminFrame/settingsFrame/
  libraryFrame->setGeometry(...)` calls with a **`QGridLayout`** on their parent
  container — `securityFrame` (row 0, col 0), `adminFrame` (row 0, col 1),
  `settingsFrame` (row 1, col 0–1 span), `libraryFrame` (row 2, col 0–1 span) — to
  match the original 2-up / full-width / full-width arrangement, with appropriate
  stretch. Verify no other `setGeometry`/`setFixedSize` in `adminwindow.cpp` re-locks
  the window (the `searchSpinner->setFixedSize(48,48)` and `overlay->resize(...)` at
  3632/3959 are fine and unrelated).
- Result: admin window resizable + maximizable; dashboard reflows.

### 5. GuestWindow (`guestwindow.ui`)
Ensure the dialog's content is fully owned by its layout so it scales with the dialog;
confirm no hard size cap. Minor effort.

### 6. Testing (TDD where feasible)
- New **`tst_responsive_ui`** target in `qt-app/tests/CMakeLists.txt`, following the
  existing pattern (`qt_add_executable`, `WIN32_EXECUTABLE FALSE`,
  link `Qt::Test` + `Qt::Widgets` + `Qt::UiTools`, `add_test`,
  `ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"`).
- Tests use **`QUiLoader`** to load the `.ui` files at runtime and assert the
  responsive contract (no app linkage needed; `mainwindow.ui`/`guestwindow.ui` use only
  stock Qt widget classes):
  - `mainwindow.ui`: top widget `maximumSize() == QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)`
    (no cap); `centralwidget`'s layout is non-null; a representative row (`widget_2`)
    has a non-null layout; `frame`'s `maximumWidth()` is bounded (≤ 320) and
    `frame_2`'s horizontal size policy is `Expanding`.
  - `guestwindow.ui`: loads and top dialog has a non-null layout, no max cap.
  - These assertions fail against the current `.ui` (red), pass after conversion (green).
- The `.ui` files are made available to the test at runtime (copy into the test build
  dir via CMake, or compile their path in) — chosen by the implementing task.
- **AdminWindow is verified manually, not auto-tested.** Its fix is cpp-side
  (`setGeometry`→`QGridLayout`) and instantiating `AdminWindow` in a unit test pulls in
  charts, network, and QXlsx — too heavy and fragile to be worth a test. This is stated
  honestly rather than claiming coverage that does not exist.

### 7. Verification
- Configure + `cmake --build` with the Qt 6.11.1 MinGW kit (Ninja + `CMAKE_PREFIX_PATH`,
  tools not on PATH — see project build memory). No new warnings/errors.
- `ctest --output-on-failure` green (including the new `tst_responsive_ui`).
- **Run `WITS.exe`** and verify by eye at: small (~800×600), large windowed,
  maximized, and fullscreen; confirm the sidebar holds its flexible width, the content
  + recent-logins columns stretch and stay aligned, and images scale. Open the admin
  window and confirm it resizes/maximizes with the dashboard reflowing.
- Run the `/claude-review` gate; fix Critical/Important findings; re-submit until APPROVE.

## Risks / Mitigations
- **Object-name drift breaks styling/wiring.** Mitigation: conversion only re-parents
  widgets into layouts; no renames. The `tst_responsive_ui` row-layout assertion and a
  visual run catch regressions; stylesheet selectors are name-based.
- **Column misalignment** between header and rows. Mitigation: identical
  `24:23:9:19:27` stretch factors on `frame_3` and every `widget_N` row.
- **Admin `QGridLayout` mismatches original arrangement.** Mitigation: replicate the
  2-up + two full-width rows layout; verify by running the admin window.
- **Heavy/fragile widget tests.** Mitigation: test only the declarative `.ui` contract
  via `QUiLoader`; verify admin manually.

## Task decomposition (for writing-plans)
1. Add `tst_responsive_ui` target + red `QUiLoader` assertions for the responsive contract.
2. Convert `mainwindow.ui`: top-level layout + sidebar VBox + content/table layouts
   (drop `line_6`); make tests green. Confirm no `mainwindow.cpp` changes needed.
3. AdminWindow: remove fixed lock in `.ui`, relax blocking child caps, replace
   `setGeometry` (cpp 665-668) with a `QGridLayout`.
4. Tidy `guestwindow.ui`.
5. Build + ctest + manual multi-size run; then `/claude-review`.
