# Admin-Page Inner Reflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every admin-window stacked page reflow fully — frames fill the window and their inner controls stay aligned/top-grouped — by replacing leftover absolute `geometry` with nested Qt layouts, verified by an extended `tst_responsive_ui` contract test.

**Architecture:** Pure `.ui` (Qt Designer XML) restructure plus test additions. No `.cpp` changes. The agent edits the `.ui` XML directly (Designer is not available to the worker); a structural `QUiLoader` test, an in-test critical-name check, and a git-based zero-deletions name-set guard are the safety net that proves the XML edits did what was intended without dropping any widget.

**Tech Stack:** Qt 6.11.1 Widgets, C++17, CMake + Ninja, QtTest + QtUiTools (`QUiLoader`).

## Global Constraints

- **`.ui`-only + the one test file.** ZERO edits to `qt-app/adminwindow.cpp`, `qt-app/mainwindow.cpp`, `qt-app/guestwindow.cpp`. If a `.cpp` edit seems necessary, STOP and reassess — the only sanctioned exception is overlay handling, and even that is out of scope here (forward-note in the spec). Never touch the uncommitted RFID changes.
- **Preserve every `objectName`** except the six Qt Designer artifact containers that are intentionally dissolved: `horizontalLayoutWidget`, `horizontalLayoutWidget_2`, `horizontalLayoutWidget_3`, `formLayoutWidget_4`, `formLayoutWidget_5`, `formLayoutWidget_6`. (The spec also names `gridLayoutWidget`; it does **not** exist in the file — ignore it.) These six are referenced by no `.cpp` and no stylesheet.
- **`searchOverlay` carve-out.** On `studentSearchPage`, `searchOverlay` (a `native="true"` full-page busy overlay the `.cpp` `raise()`s and whose `width()/height()` it reads) must remain a direct child of the page but must **NOT** be added to the page layout.
- **Reflow, not zoom.** Native Qt layouts only. Inner form content keeps natural size, top-aligned, with a trailing spacer absorbing slack. Tables / charts / preview panels are the Expanding members.
- **Preserve per-page `styleSheet` blocks** and the object names they target (e.g. `QWidget#reportingPage`).
- **Commit scope:** only `qt-app/adminwindow.ui` and `qt-app/tests/tst_responsive_ui.cpp`, via the `commit` skill. The `mainwindow.ui`/`guestwindow.ui` sweep (Task 7) may add those files only if it actually changes them.

### Build & test commands (Windows; tools are NOT on PATH)

Qt tools must be prepended to PATH each shell. Use the **Bash** tool (Git Bash) for the git name-set guard; use **PowerShell** for build/ctest. PowerShell prelude:

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
```

- Configure (once): `cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`
- Build: `cmake --build qt-app/build`
- Run just this suite (verbose): `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
- Full suite: `ctest --test-dir qt-app/build --output-on-failure`

The suite runs with `QT_QPA_PLATFORM=offscreen` (already set in `qt-app/tests/CMakeLists.txt`), so no display is needed.

### The `.ui` conversion mechanic (shared technique — read once)

Qt lays out only widgets that are `<item>`s of a `<layout>`. A child with absolute `<geometry>` is *not* layout-managed. The conversion, applied by hand to the XML, is:

1. **Add a `<layout>`** as the first child of the container widget (page/frame/group box). Give it a unique `name` (e.g. `verticalLayout_admin`).
2. **Move each child `<widget>`** that should be managed into an `<item>` of that layout (in visual order). For grids/forms use `<item row="R" column="C">`.
3. **Delete the child's `<property name="geometry">…</property>` block.** A layout-managed widget must not carry geometry.
4. **Lift an artifact container** (`…LayoutWidget`): the artifact is a bare `QWidget` wrapping a real `<layout>`. Replace the artifact-widget node with its inner `<layout>` node moved up one level (into the page's new layout as an `<item>`), then delete the now-empty artifact `<widget>` wrapper. Its inner layout name (e.g. `horizontalLayout`) is preserved.
5. **Trailing spacer for top-align:** append `<item><spacer><property name="orientation"><enum>Qt::Vertical</enum></property></spacer></item>` as the last item of a vertical layout so content groups to the top.
6. **Expanding member:** add a `<property name="sizePolicy"><sizepolicy hsizetype="Expanding" vsizetype="Expanding">…</sizepolicy></property>` to the widget that should absorb space.

Concrete before/after (one child becoming a single-item vbox):

```xml
<!-- BEFORE: floating child -->
<widget class="QFrame" name="adminFrame">
  <widget class="QLabel" name="label_5">
    <property name="geometry"><rect><x>20</x><y>0</y><width>151</width><height>31</height></rect></property>
    <property name="text"><string>Admin Information</string></property>
  </widget>
</widget>

<!-- AFTER: layout-managed -->
<widget class="QFrame" name="adminFrame">
  <layout class="QVBoxLayout" name="verticalLayout_admin">
    <item>
      <widget class="QLabel" name="label_5">
        <property name="text"><string>Admin Information</string></property>
      </widget>
    </item>
  </layout>
</widget>
```

**Per-task safety ritual (run for every conversion task before commit):**
- Build clean, target suite green for that task's slots.
- **Name-set guard (Bash tool):** no real widget dropped —
  ```bash
  cd "C:/Users/USER/OneDrive - usep.edu.ph/Documents/WITS/WITS-main"
  ART='name="(horizontalLayoutWidget|horizontalLayoutWidget_2|horizontalLayoutWidget_3|formLayoutWidget_4|formLayoutWidget_5|formLayoutWidget_6)"'
  comm -23 \
    <(git show HEAD:qt-app/adminwindow.ui | grep -oE 'name="[^"]+"' | sort -u | grep -vE "$ART") \
    <(grep -oE 'name="[^"]+"' qt-app/adminwindow.ui | sort -u)
  ```
  Expected output: **empty** (every committed real name still present). Any line printed = a dropped/renamed widget → fix before commit.
- **Semantic-diff sanity (Bash tool):** `git diff --ignore-all-space qt-app/adminwindow.ui` shows only the intended structural changes for that page.

---

### Task 1: Extend `tst_responsive_ui` with the red contract (RED baseline)

**Files:**
- Modify: `qt-app/tests/tst_responsive_ui.cpp` (add slots + implementations; the existing `loadUi` helper already supports any `.ui` filename)

**Interfaces:**
- Consumes: existing `loadUi(const QString&)` helper and `WITS_UI_DIR`.
- Produces: nine new test slots that Tasks 2–6 turn green: `databasePageHasLayout`, `reportingPageHasLayout`, `studentSearchPageHasLayout`, `visitorPageHasLayout`, `generalPageFramesHaveLayouts`, `chartsPreviewExpands`, `visitorTableExpands`, `searchOverlayPreserved`, `adminCriticalNamesResolve`.

- [ ] **Step 1: Add the new slot declarations.** In the `private slots:` block of `class TestResponsiveUi` (after `guestDialogHasLayout();`, line ~26), insert:

```cpp
    // --- admin-page inner reflow ---
    void databasePageHasLayout();
    void reportingPageHasLayout();
    void studentSearchPageHasLayout();
    void visitorPageHasLayout();
    void generalPageFramesHaveLayouts();
    void chartsPreviewExpands();
    void visitorTableExpands();
    void searchOverlayPreserved();
    void adminCriticalNamesResolve();
```

- [ ] **Step 2: Add the implementations.** Insert before the final `QTEST_MAIN(TestResponsiveUi)` line:

```cpp
static QWidget *findPage(QWidget *root, const char *name)
{
    return root->findChild<QWidget *>(QString::fromLatin1(name));
}

void TestResponsiveUi::databasePageHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *page = findPage(w.data(), "databasePage");
    QVERIFY2(page, "databasePage not found");
    QVERIFY2(page->layout() != nullptr, "databasePage has no top-level layout");
}

void TestResponsiveUi::reportingPageHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *page = findPage(w.data(), "reportingPage");
    QVERIFY2(page, "reportingPage not found");
    QVERIFY2(page->layout() != nullptr, "reportingPage has no top-level layout");
}

void TestResponsiveUi::studentSearchPageHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *page = findPage(w.data(), "studentSearchPage");
    QVERIFY2(page, "studentSearchPage not found");
    QVERIFY2(page->layout() != nullptr, "studentSearchPage has no top-level layout");
}

void TestResponsiveUi::visitorPageHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *page = findPage(w.data(), "visitorPage");
    QVERIFY2(page, "visitorPage not found");
    QVERIFY2(page->layout() != nullptr, "visitorPage has no top-level layout");
}

void TestResponsiveUi::generalPageFramesHaveLayouts()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    for (const char *name : {"adminFrame", "securityFrame", "libraryFrame", "settingsFrame"}) {
        QWidget *f = w->findChild<QWidget *>(name);
        QVERIFY2(f, qPrintable(QString("frame %1 not found").arg(name)));
        QVERIFY2(f->layout() != nullptr,
                 qPrintable(QString("frame %1 has no layout manager").arg(name)));
    }
}

void TestResponsiveUi::chartsPreviewExpands()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *g = w->findChild<QWidget *>("chartsPreview");
    QVERIFY2(g, "chartsPreview group box not found");
    QCOMPARE(g->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
}

void TestResponsiveUi::visitorTableExpands()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *t = w->findChild<QWidget *>("visitorTable");
    QVERIFY2(t, "visitorTable not found");
    QCOMPARE(t->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(t->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
}

void TestResponsiveUi::searchOverlayPreserved()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *overlay = w->findChild<QWidget *>("searchOverlay");
    QVERIFY2(overlay, "searchOverlay must still exist after conversion");
    QWidget *page = findPage(w.data(), "studentSearchPage");
    QVERIFY2(page, "studentSearchPage not found");
    // Carve-out: overlay stays a free-floating sibling parented to the page,
    // never managed by the page layout (else it stops covering the page).
    QCOMPARE(overlay->parentWidget(), page);
    if (page->layout())
        QVERIFY2(page->layout()->indexOf(overlay) == -1,
                 "searchOverlay must NOT be added to the studentSearchPage layout");
}

void TestResponsiveUi::adminCriticalNamesResolve()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    // Names bound by adminwindow.cpp slots / per-page stylesheets. Dropping any
    // silently breaks the app. The exhaustive zero-deletions check is the git
    // name-set guard run during each conversion task.
    const char *names[] = {
        "generalPage", "databasePage", "reportingPage", "studentSearchPage", "visitorPage",
        "adminFrame", "securityFrame", "libraryFrame", "settingsFrame",
        "adminNameLineEdit", "adminPositionLineEdit", "schoolName", "address",
        "fontComboBox", "spinBox", "openHourSpinBox", "closeHourSpinBox",
        "lineEdit", "lineEdit_2", "updateBtn", "showOldBtn", "showNewBtn",
        "guestLoginCheckBox", "clearAttendanceCheckBox", "applyChangesBtn",
        "schoolLogoBrowseBtn", "posterBrowseBtn", "bookDropSystemCheckBox",
        "individualRegistrationBox", "bulkRegistrationBox", "bulkTable", "bulkProgressBar",
        "departmentComboBox", "deleteRecordsBtn", "resetCountBtn", "deactivateBtn",
        "filterDepartmentBox", "filterCourseBox", "durationTypeBox", "durationTypeWidget",
        "paletteComboBox", "chartTypeBox", "chartsPreview",
        "generatePDFBtn", "generateExcelBtn", "statusLabel",
        "searchLineEdit", "searchBtn", "searchDepartmentFilter", "searchCourseFilter",
        "studentSearchTable", "selectAllBtn", "editStudentBtn", "deleteStudentBtn",
        "refreshSearchBtn", "cancelEditBtn", "searchOverlay",
        "visitorTable", "extractVisitorBtn", "visitorTotalLabel",
        "visitorSearchLineEdit", "visitorSearchBtn", "visitorDateFilterBox"
    };
    for (const char *n : names) {
        QVERIFY2(w->findChild<QWidget *>(n),
                 qPrintable(QString("critical objectName '%1' missing").arg(n)));
    }
}
```

- [ ] **Step 3: Build.**

Run (PowerShell, after the PATH prelude): `cmake --build qt-app/build`
Expected: compiles clean, `tst_responsive_ui.exe` links.

- [ ] **Step 4: Run the suite and confirm the RED set.**

Run: `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected — these FAIL (the contract not yet met):
- `databasePageHasLayout`, `reportingPageHasLayout`, `studentSearchPageHasLayout`, `visitorPageHasLayout` — no page layout yet.
- `generalPageFramesHaveLayouts` — frames have no inner layout.
- `chartsPreviewExpands` — `chartsPreview` (QGroupBox) defaults to `Preferred`.

Expected — these PASS already (regression guards, must STAY green every task):
- `searchOverlayPreserved` — overlay exists, page has no layout, parent is the page.
- `adminCriticalNamesResolve` — every name still present. **If this fails now, a name in the list is mistyped — correct the list to match the committed `.ui`, not the other way around.**
- `visitorTableExpands` — MAY already pass (QAbstractScrollArea tends to default to Expanding). Either way Task 6 must set it explicitly. Record which it is.

- [ ] **Step 5: Commit (RED).**

Use the **commit** skill, staging only `qt-app/tests/tst_responsive_ui.cpp`. Suggested message:
`test(ui): add red admin-page inner-reflow contract`
Body: lists the failing slots and notes the two regression guards already green.

---

### Task 2: Convert `generalPage` frames (frame-internal layouts)

**Files:**
- Modify: `qt-app/adminwindow.ui` — `generalPage` keeps its existing `gridLayout`; add an inner layout to each of the four frames and remove their children's `<geometry>`.

**Interfaces:**
- Consumes: existing `gridLayout` on `generalPage` (frames already tile via grid cells: `securityFrame` (0,0), `adminFrame` (0,1 colspan 2), `libraryFrame` (1,0 colspan 3), `settingsFrame` (5,0 colspan 3)).
- Produces: non-null `layout()` on `adminFrame`, `securityFrame`, `libraryFrame`, `settingsFrame` → turns `generalPageFramesHaveLayouts` green.

Apply the conversion mechanic to each frame. Keep each frame's existing `minimumSize` (`adminFrame`/`securityFrame`/`settingsFrame` are `250×150`); add `minimumSize 250×150` to `libraryFrame` (it has none today).

- [ ] **Step 1: `adminFrame` → `QVBoxLayout name="verticalLayout_admin"`**, items in order, each with `<geometry>` removed:
  1. `label_5` (heading "Admin Information")
  2. `adminNameLineEdit`
  3. `adminPositionLineEdit`
  4. trailing vertical spacer (top-align)

- [ ] **Step 2: `securityFrame` → `QVBoxLayout name="verticalLayout_security"`**:
  1. `label_2` (heading "Change Admin Password")
  2. a nested `QGridLayout name="gridLayout_security"` with two rows pairing field + reveal button:
     - row 0: `lineEdit` (col 0), `showOldBtn` (col 1)
     - row 1: `lineEdit_2` (col 0), `showNewBtn` (col 1)
  3. `updateBtn`
  4. trailing vertical spacer

- [ ] **Step 3: `libraryFrame` → `QGridLayout name="gridLayout_library"`** (two logical columns). Remove every child `<geometry>`. Suggested cells:
  - row 0 col 0: `label_4` (heading "School Information")
  - row 1 col 0: `schoolName`; row 2 col 0: `address`
  - right group (col 1+): `label_3` (Edit Font) over `fontComboBox`; `label_9` (Font Size) over `spinBox`; `label_34` (Library Hours) over a horizontal sub-layout `[openHourSpinBox] [label_35 "am -"] [closeHourSpinBox] [label_36 "pm"]`; `label_10` (Preview:) over `schoolText_previewLabel`.
  - Add `minimumSize 250×150` to `libraryFrame`.
  - Keep all label/field object names; this is the densest frame — prioritize "every widget is in the grid and no `<geometry>` remains" over pixel-matching the old position.

- [ ] **Step 4: `settingsFrame` → `QGridLayout name="gridLayout_settings"`**, `<geometry>` removed from all children:
  - row 0: `label_6` (heading "Features and Uploads")
  - upload rows: `uploadSchoolLogo` + `schoolLogoBrowseBtn` + `schLogo_previewLabel`; `welcomePoster_img` + `posterBrowseBtn` + `posterPreviewLabel`
  - checkboxes: `guestLoginCheckBox`, `clearAttendanceCheckBox`, `bookDropSystemCheckBox`
  - `applyChangesBtn` (span the row)
  - trailing vertical spacer

- [ ] **Step 5: Build.** Run: `cmake --build qt-app/build` — expected clean.

- [ ] **Step 6: Run the suite.** Run: `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected now GREEN: `generalPageFramesHaveLayouts`, plus the regression guards stay green. (The four page-layout slots and `chartsPreviewExpands` remain RED — later tasks.)

- [ ] **Step 7: Safety ritual.** Run the name-set guard (Bash) → expect empty. Run `git diff --ignore-all-space qt-app/adminwindow.ui` → only `generalPage` frame changes.

- [ ] **Step 8: Commit.** Use the **commit** skill, staging only `qt-app/adminwindow.ui`. Message: `refactor(ui): lay out General Page frame contents`.

---

### Task 3: Convert `databasePage` (page-level layout)

**Files:**
- Modify: `qt-app/adminwindow.ui` — add a top-level layout to `databasePage`.

**Interfaces:**
- Consumes: the three group boxes that already own inner layouts — `individualRegistrationBox` (`horizontalLayout_7`), `bulkRegistrationBox` (`verticalLayout_4`), `groupBox` (`verticalLayout_5`). No frame-internal conversion needed here.
- Produces: non-null `databasePage->layout()` → turns `databasePageHasLayout` green.

- [ ] **Step 1: Add `QHBoxLayout name="horizontalLayout_databasePage"`** as the first child of `databasePage`, with the three group boxes as items in visual order: `groupBox` (delete management, left), `individualRegistrationBox` (register, middle), `bulkRegistrationBox` (bulk, right). Remove the `<geometry>` block from each of the three group boxes. Preserve the page `styleSheet`.
  - Give `bulkRegistrationBox` (the widest, table-bearing) a horizontal `sizePolicy` of `Expanding` so it absorbs slack; leave the others `Preferred`.
  - Add a modest `minimumSize` (e.g. `width 280`) to each group box so the row reflows rather than clipping.

- [ ] **Step 2: Build.** `cmake --build qt-app/build` — clean.

- [ ] **Step 3: Run the suite.** `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected now GREEN: `databasePageHasLayout`; guards stay green.

- [ ] **Step 4: Safety ritual.** Name-set guard empty; `git diff --ignore-all-space` shows only `databasePage` changes.

- [ ] **Step 5: Commit.** commit skill, only `qt-app/adminwindow.ui`. Message: `refactor(ui): give Database page a top-level layout`.

---

### Task 4: Convert `reportingPage` (page-level layout + Expanding charts)

**Files:**
- Modify: `qt-app/adminwindow.ui` — add a top-level layout to `reportingPage`; set `chartsPreview` Expanding; dissolve the `formLayoutWidget_4/5/6` artifacts inside `durationTypeWidget`.

**Interfaces:**
- Consumes: group boxes `groupBox_2` (filters), `groupBox_3` (visualization), `groupBox_4` (export), `chartsPreview` (chart canvas) — each already owns an inner layout. Artifact containers `formLayoutWidget_4` (in `specificDay`), `formLayoutWidget_5` (in `specificMonth`), `formLayoutWidget_6` live inside the `durationTypeWidget` QStackedWidget sub-pages.
- Produces: non-null `reportingPage->layout()` → `reportingPageHasLayout` green; `chartsPreview` Expanding → `chartsPreviewExpands` green.

- [ ] **Step 1: Add `QGridLayout name="gridLayout_reportingPage"`** as the first child of `reportingPage`. Place the four group boxes in a sensible 2×2-ish arrangement (e.g. `groupBox_2` filters top-left, `groupBox_3` visualization top-right, `groupBox_4` export bottom-left, `chartsPreview` spanning the remaining space). Remove each group box's `<geometry>`. Preserve the `QWidget#reportingPage` background `styleSheet`.

- [ ] **Step 2: Make `chartsPreview` the Expanding member** — add `<property name="sizePolicy"><sizepolicy hsizetype="Expanding" vsizetype="Expanding"><horstretch>1</horstretch><verstretch>1</verstretch></sizepolicy></property>` to the `chartsPreview` group box (NOT to `chartsPreviewBox`, which is its inner `QGridLayout`).

- [ ] **Step 3: Dissolve `formLayoutWidget_4`, `formLayoutWidget_5`, `formLayoutWidget_6`.** Locate each by name (they live inside the `durationTypeWidget` QStackedWidget sub-pages — `specificDay`, `specificMonth`, `semester`). Each is a bare `QWidget` with `<geometry>` wrapping a real layout. For each: lift its inner `<layout>` up to become the layout of its immediate parent container, then delete the empty `…LayoutWidget` wrapper (mechanic step 4). Inner layout names are preserved; only the three wrapper names disappear (exempted by the guard).

- [ ] **Step 4: Build.** `cmake --build qt-app/build` — clean.

- [ ] **Step 5: Run the suite.** `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected now GREEN: `reportingPageHasLayout`, `chartsPreviewExpands`; guards stay green.

- [ ] **Step 6: Safety ritual.** Name-set guard empty (the only removed names are `formLayoutWidget_4/5/6`, which are exempt); `git diff --ignore-all-space` shows only `reportingPage` changes.

- [ ] **Step 7: Commit.** commit skill, only `qt-app/adminwindow.ui`. Message: `refactor(ui): lay out Reporting page and expand chart panel`.

---

### Task 5: Convert `studentSearchPage` (page layout + overlay carve-out)

**Files:**
- Modify: `qt-app/adminwindow.ui` — add a top-level layout to `studentSearchPage`; dissolve `horizontalLayoutWidget`/`_2`/`_3`; keep `searchOverlay` OUT of the layout.

**Interfaces:**
- Consumes: artifact rows `horizontalLayoutWidget` (search row → `horizontalLayout`), `horizontalLayoutWidget_2` (filter row → `horizontalLayout_2`), `horizontalLayoutWidget_3` (button row → `horizontalLayout_3`); `studentSearchTable` (results); `searchOverlay` (busy overlay — CARVE-OUT).
- Produces: non-null `studentSearchPage->layout()` with `searchOverlay` NOT in it → turns `studentSearchPageHasLayout` green and keeps `searchOverlayPreserved` green.

- [ ] **Step 1: Add `QVBoxLayout name="verticalLayout_searchPage"`** as the first child of `studentSearchPage`. Items in visual order, dissolving each artifact wrapper (mechanic step 4) so its inner layout becomes a page item:
  1. `horizontalLayout` (search: `label_37`, `searchLineEdit`, `searchBtn`)
  2. `horizontalLayout_2` (filters: `label_38`, `searchDepartmentFilter`, `label_39`, `searchCourseFilter`)
  3. `studentSearchTable` — set its `sizePolicy` to Expanding/Expanding so it absorbs vertical space; remove its `<geometry>`
  4. `horizontalLayout_3` (buttons: `cancelEditBtn`, `selectAllBtn`, `editStudentBtn`, `deleteStudentBtn`, `refreshSearchBtn`)

- [ ] **Step 2: Leave `searchOverlay` untouched.** It stays a direct child `<widget>` of `studentSearchPage` AFTER the `</layout>` close tag, keeping its `<geometry>`, `native="true"`, and its children `overlayTextLabel` / `searchLoadingBar`. Do NOT add it as a layout `<item>`. (This is what `searchOverlayPreserved` enforces.)

- [ ] **Step 3: Build.** `cmake --build qt-app/build` — clean.

- [ ] **Step 4: Run the suite.** `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected now GREEN: `studentSearchPageHasLayout`; `searchOverlayPreserved` STILL green (now with the indexOf branch active — proves the overlay is not in the layout). Guards stay green.

- [ ] **Step 5: Safety ritual.** Name-set guard empty (only `horizontalLayoutWidget`/`_2`/`_3` removed — exempt); `git diff --ignore-all-space` shows only `studentSearchPage` changes; confirm `searchOverlay`, `overlayTextLabel`, `searchLoadingBar` still present in the file.

- [ ] **Step 6: Commit.** commit skill, only `qt-app/adminwindow.ui`. Message: `refactor(ui): lay out Student Search page, preserve busy overlay`.

---

### Task 6: Convert `visitorPage` (page layout + Expanding table)

**Files:**
- Modify: `qt-app/adminwindow.ui` — add a top-level layout to `visitorPage`; set `visitorTable` Expanding.

**Interfaces:**
- Consumes: `visitorTable` (results), `extractVisitorBtn`, `visitorTotalLabel`, `widget_3` (search/filter bar owning `horizontalLayout_5`).
- Produces: non-null `visitorPage->layout()` → `visitorPageHasLayout` green; `visitorTable` Expanding/Expanding → `visitorTableExpands` green (explicitly, regardless of default).

- [ ] **Step 1: Add `QVBoxLayout name="verticalLayout_visitorPage"`** as the first child of `visitorPage`. Items in order, `<geometry>` removed from each:
  1. a top `QHBoxLayout name="horizontalLayout_visitorTop"` with three items, in order: `widget_3` (the search/filter bar — leave it as-is, it already owns `horizontalLayout_5`), `extractVisitorBtn`, `visitorTotalLabel`.
  2. `visitorTable` (results, expands)

- [ ] **Step 2: Make `visitorTable` the Expanding member** — add `<property name="sizePolicy"><sizepolicy hsizetype="Expanding" vsizetype="Expanding"><horstretch>0</horstretch><verstretch>1</verstretch></sizepolicy></property>`.

- [ ] **Step 3: Build.** `cmake --build qt-app/build` — clean.

- [ ] **Step 4: Run the suite.** `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected now GREEN: `visitorPageHasLayout`, `visitorTableExpands`. **All nine new slots and all seven original slots should now be green.**

- [ ] **Step 5: Safety ritual.** Name-set guard empty; `git diff --ignore-all-space` shows only `visitorPage` changes.

- [ ] **Step 6: Commit.** commit skill, only `qt-app/adminwindow.ui`. Message: `refactor(ui): lay out Visitor Logs page and expand table`.

---

### Task 7: Re-check `mainwindow.ui` and `guestwindow.ui` sweep

**Files:**
- Inspect (modify only if leftover absolute-geometry children are found): `qt-app/mainwindow.ui`, `qt-app/guestwindow.ui`.

**Interfaces:**
- Consumes: nothing new. The existing `mainWindow*` and `guestDialog*` slots already pass and must stay green.
- Produces: confirmation (success-criterion #4) that no frame on these two forms has absolute-geometry children that fail to reflow.

- [ ] **Step 1: Find geometry-bearing widgets nested inside frames** (Bash):
```bash
cd "C:/Users/USER/OneDrive - usep.edu.ph/Documents/WITS/WITS-main"
for f in qt-app/mainwindow.ui qt-app/guestwindow.ui; do
  echo "=== $f ==="
  grep -nE '<widget class="QFrame"|name="geometry"' "$f" | head -60
done
```
A frame whose direct child `<widget>` blocks contain `<geometry>` while the frame has no `<layout>` is a leftover. (Top-level window/centralwidget geometry is fine and expected.)

- [ ] **Step 2a (if leftovers found): convert them** using the same mechanic, one form at a time; build and run the full suite (`ctest --test-dir qt-app/build --output-on-failure`) to confirm the `mainWindow*`/`guestDialog*` slots stay green; commit each touched file via the commit skill (`refactor(ui): reflow leftover <form> frame contents`).

- [ ] **Step 2b (if clean — the likely case): record the no-op.** No commit. Note in the task report that both forms were swept and are free of stray absolute-geometry frame children, satisfying success-criterion #4.

---

### Task 8: Full verification gate

**Files:** none (verification only).

- [ ] **Step 1: Clean configure + build.** PowerShell (after PATH prelude):
`cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"` then `cmake --build qt-app/build`
Expected: no new warnings/errors; `WITS.exe` and all test exes link.

- [ ] **Step 2: Full ctest.** `ctest --test-dir qt-app/build --output-on-failure`
Expected: all 5 suites pass; `tst_responsive_ui` = 16/16 slots green (7 original + 9 new).

- [ ] **Step 3: Final name-set guard.** Run the Bash name-set guard once more against the fully-converted `adminwindow.ui` — expect empty (only the six artifact names removed across the whole branch).

- [ ] **Step 4: Confirm `.cpp` untouched.** Run `git status --porcelain` — `adminwindow.cpp`, `mainwindow.cpp`, `guestwindow.cpp` show only the pre-existing RFID modifications (unstaged, never committed); no responsive commit touched them.

- [ ] **Step 5: Manual GUI run (handoff to user).** Launch `qt-app/build/WITS.exe`; open the admin window; for each of the five pages resize the window large → small → minimal and confirm: frames fill the window, inner controls stay aligned/top-grouped (no top-left glue, no floating fixed-size frames), and at the mini size the arrangement reflows rather than clips. **Exercise a student search** to confirm the `searchOverlay` still covers the page and its spinner centers (success-criterion #5). This step needs the desktop and is the user's to run.

- [ ] **Step 6: Review gate.** After the manual run looks right, run `/claude-review` (PHASE mode on the new commits) per the project workflow, fix any Critical/Important findings, then proceed to `create-pr`.

---

## Notes for the executor

- **Out of scope:** zoom/proportional scaling; any widget rename; the RFID working-tree changes; making `searchOverlay` track page size (the optional `resizeEvent` follow-up); the three Low responsive-UI review follow-ups.
- **If a `.cpp` edit seems required:** stop and reassess — it almost certainly means a widget was about to be renamed/dropped or an overlay was about to be laid out. The guards exist to catch exactly this.
- **Per-frame visual fidelity** (exact spacing/columns) is confirmed only by the manual run in Task 8; the automated test guarantees structure (layouts exist, names preserved, Expanding members expand), not pixels.
- **minimumSize (spec guardrail) applies to every converted container**, not just the two pages where it is called out inline (Tasks 2–3). On Reporting, Student Search, and Visitor, give each converted page/group box a modest `minimumSize` (e.g. width 280–320) so the mini window reflows rather than clipping — success-criterion #2.
