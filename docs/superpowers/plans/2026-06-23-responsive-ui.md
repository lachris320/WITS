# Responsive WITS UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the WITS desktop UI fill any screen size by removing fixed-size caps and converting absolute-geometry layouts to Qt layout managers, without breaking the existing stylesheet or signal/slot wiring.

**Architecture:** `mainwindow.ui` is converted from 85 absolutely-positioned widgets (0 layouts) to nested `QHBoxLayout`/`QVBoxLayout`s with a flexible-width sidebar and an expanding content area whose recent-logins "table" uses shared column stretch factors. `adminwindow.ui` is already layout-based but hard-locked to a fixed 760×600 and its pages are absolutely positioned on `centralwidget`; we add a `QHBoxLayout` to `centralwidget`, remove the size lock, and delete the conflicting `setGeometry` calls in `adminwindow.cpp`. A new `QUiLoader`-based Qt Test target locks in the responsive contract for the `.ui` files.

**Tech Stack:** Qt 6.11.1 (Widgets, Test, UiTools), C++17, CMake + Ninja, MinGW kit.

## Global Constraints

- **Build/test commands** (tools NOT on PATH — prepend in the SAME PowerShell call every time):
  ```powershell
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
- Harmless warnings to ignore: `LF will be replaced by CRLF`; the pre-existing QXlsx `GuiPrivate target` CMake warning.
- QtTest exes MUST set `WIN32_EXECUTABLE FALSE` (console subsystem) or ctest captures zero output. Widget tests run under `QT_QPA_PLATFORM=offscreen`.
- **Never rename an existing object name.** The `setStyleSheet(R"(...)")` block in `mainwindow.cpp` selects by name (`#frame`, `#frame_2`, `#frame_3`, `#stdList`, `#schLogo_Image`, `#username`, `#loginBtn`, `#time_label`, `#date_label`, `#label`); `refreshRightPanel()` references label names (`nameLabel`, `_2`,`_3`,`_5`,`_7`,`_8`,`_9`,`_11`,`_13`, plus parallel course/year/dep/time lists); shadows attach to `ui->widget` and `ui->frame_3`. Conversion only **re-parents** widgets into layouts.
- Never commit `qt-app/build/` (gitignored). Commit via the project `commit` skill in the real run; the `git` commands shown here are the intent.
- Column stretch factors for the logins table, used identically on the header and every row: **name:course:year:dep:time = 24:23:9:19:27**.

---

## File Structure

- `qt-app/CMakeLists.txt` — add `UiTools` to the Qt `find_package` COMPONENTS (Task 1).
- `qt-app/tests/tst_responsive_ui.cpp` — **new**, `QUiLoader`-based contract tests (Task 1).
- `qt-app/tests/CMakeLists.txt` — **modify**, register the `tst_responsive_ui` target (Task 1).
- `qt-app/mainwindow.ui` — **modify**, the main conversion (Task 2).
- `qt-app/adminwindow.ui` — **modify**, central layout + remove size lock + relax caps (Task 3).
- `qt-app/adminwindow.cpp:664-668` — **modify**, delete the conflicting `setGeometry` block (Task 3).
- `qt-app/guestwindow.ui` — **modify**, ensure dialog content is layout-owned (Task 4).

---

## Task 1: Responsive-UI test target (red)

**Files:**
- Create: `qt-app/tests/tst_responsive_ui.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (append a new target block)
- Modify: `qt-app/CMakeLists.txt:12-13` (add `UiTools` component)

**Interfaces:**
- Consumes: nothing.
- Produces: a `ctest` test named `tst_responsive_ui` that loads `mainwindow.ui` and `guestwindow.ui` via `QUiLoader` and asserts the responsive contract. Later tasks make it pass.

- [ ] **Step 1: Add `UiTools` to the Qt component lists**

In `qt-app/CMakeLists.txt`, change both lines 12–13 from:
```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Charts Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Charts Test)
```
to:
```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Charts Test UiTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Charts Test UiTools)
```

- [ ] **Step 2: Write the failing test file**

Create `qt-app/tests/tst_responsive_ui.cpp`:
```cpp
#include <QtTest>
#include <QtUiTools/QUiLoader>
#include <QFile>
#include <QWidget>
#include <QLabel>
#include <QLayout>
#include <QSizePolicy>
#include <QString>

#ifndef WITS_UI_DIR
#define WITS_UI_DIR "."
#endif

class TestResponsiveUi : public QObject
{
    Q_OBJECT
private:
    QWidget *loadUi(const QString &fileName);
private slots:
    void mainWindowHasNoMaximumSizeCap();
    void mainWindowCentralWidgetHasLayout();
    void mainWindowSidebarWidthIsBounded();
    void mainWindowContentExpands();
    void mainWindowLoginRowHasLayout();
    void mainWindowHeaderHasColumns();
    void guestDialogHasLayout();
};

QWidget *TestResponsiveUi::loadUi(const QString &fileName)
{
    QUiLoader loader;
    QFile file(QString(WITS_UI_DIR) + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly))
        return nullptr;
    QWidget *w = loader.load(&file);
    file.close();
    return w;
}

void TestResponsiveUi::mainWindowHasNoMaximumSizeCap()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY2(w, "failed to load mainwindow.ui");
    QCOMPARE(w->maximumWidth(), QWIDGETSIZE_MAX);
    QCOMPARE(w->maximumHeight(), QWIDGETSIZE_MAX);
}

void TestResponsiveUi::mainWindowCentralWidgetHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY2(w, "failed to load mainwindow.ui");
    QWidget *central = w->findChild<QWidget *>("centralwidget");
    QVERIFY2(central, "centralwidget not found");
    QVERIFY2(central->layout() != nullptr, "centralwidget has no layout manager");
}

void TestResponsiveUi::mainWindowSidebarWidthIsBounded()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *frame = w->findChild<QWidget *>("frame");
    QVERIFY2(frame, "sidebar frame not found");
    QVERIFY2(frame->maximumWidth() <= 320, "sidebar should be width-bounded (<=320)");
    QVERIFY2(frame->minimumWidth() >= 240, "sidebar should have a sensible minimum width");
}

void TestResponsiveUi::mainWindowContentExpands()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *content = w->findChild<QWidget *>("frame_2");
    QVERIFY2(content, "frame_2 not found");
    QCOMPARE(content->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
}

void TestResponsiveUi::mainWindowLoginRowHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *row = w->findChild<QWidget *>("widget_2");
    QVERIFY2(row, "recent-login row widget_2 not found");
    QVERIFY2(row->layout() != nullptr, "login row has no layout manager");
}

void TestResponsiveUi::mainWindowHeaderHasColumns()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *header = w->findChild<QWidget *>("frame_3");
    QVERIFY2(header, "header frame_3 not found");
    QVERIFY2(header->layout() != nullptr, "header frame_3 has no layout manager");
    // The five column captions must live inside the header strip so they align
    // with the data rows and the strip does not collapse to zero height.
    for (const char *name : {"label_10", "label_6", "label_7", "label_8", "label_9"}) {
        QLabel *cap = header->findChild<QLabel *>(name);
        QVERIFY2(cap, qPrintable(QString("caption %1 not under frame_3").arg(name)));
    }
}

void TestResponsiveUi::guestDialogHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("guestwindow.ui"));
    QVERIFY2(w, "failed to load guestwindow.ui");
    QVERIFY2(w->layout() != nullptr, "guest dialog has no top-level layout manager");
    QCOMPARE(w->maximumWidth(), QWIDGETSIZE_MAX);
}

QTEST_MAIN(TestResponsiveUi)
#include "tst_responsive_ui.moc"
```

- [ ] **Step 3: Register the test target**

Append to `qt-app/tests/CMakeLists.txt`:
```cmake
qt_add_executable(tst_responsive_ui
    tst_responsive_ui.cpp
)
set_target_properties(tst_responsive_ui PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_responsive_ui PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(tst_responsive_ui PRIVATE WITS_UI_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(tst_responsive_ui PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::UiTools
)
add_test(NAME tst_responsive_ui COMMAND tst_responsive_ui)
set_tests_properties(tst_responsive_ui PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 4: Configure + build + run the new test — verify it FAILS**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_responsive_ui
```
Expected: `tst_responsive_ui` FAILS. Specifically `mainWindowHasNoMaximumSizeCap` fails (maximumWidth is 1400, not QWIDGETSIZE_MAX), `mainWindowCentralWidgetHasLayout` fails (no layout), and `mainWindowSidebarWidthIsBounded`/`mainWindowContentExpands`/`mainWindowLoginRowHasLayout`/`mainWindowHeaderHasColumns` fail. **`guestDialogHasLayout` also fails** — the `guestwindow` dialog currently has no top-level layout (only a `QFormLayout` nested inside `formLayoutWidget`); it goes green in Task 4.

**If the build fails at the compile-define** (the source path on this machine contains a space and `.`: `…/OneDrive - usep.edu.ph/…`): CMake normally escapes `target_compile_definitions` correctly (the existing test targets already reference `${CMAKE_SOURCE_DIR}` via `target_include_directories`), but if `WITS_UI_DIR` does not resolve, fall back to copying the `.ui` files next to the test executable at build time and loading by relative name:
```cmake
add_custom_command(TARGET tst_responsive_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mainwindow.ui ${CMAKE_SOURCE_DIR}/guestwindow.ui
        $<TARGET_FILE_DIR:tst_responsive_ui>)
```
and set `WITS_UI_DIR="."` (ctest runs with the test's working dir at the build tree).

- [ ] **Step 5: Commit**

```bash
git add qt-app/CMakeLists.txt qt-app/tests/CMakeLists.txt qt-app/tests/tst_responsive_ui.cpp
git commit -m "test(ui): add QUiLoader responsive-contract tests (red)"
```

---

## Task 2: Convert `mainwindow.ui` to layouts (green)

**Files:**
- Modify: `qt-app/mainwindow.ui`
- (Verify only, expect no change: `qt-app/mainwindow.cpp`)

**Interfaces:**
- Consumes: the `tst_responsive_ui` contract from Task 1.
- Produces: a fully layout-driven `mainwindow.ui` that satisfies every `tst_responsive_ui` assertion. No new object names and no renames; the only removals are the separator lines `line_2`, `line_3`, `line_4`, `line_5`, `line_6`.

This task is a structural XML transform. Apply the rules below; the Task-1 tests are the executable spec for "done."

- [ ] **Step 1: Remove the window size cap**

In `qt-app/mainwindow.ui`, delete the entire `maximumSize` property block on `MainWindow` (the `<property name="maximumSize">…1400×846…</property>`, lines ~19–24). Keep `minimumSize` (800×600) and the top-level `geometry`.

- [ ] **Step 2: Wrap `centralwidget` in a `QHBoxLayout`**

Give `centralwidget` a `QHBoxLayout` (set contents margins to `0,0,0,0` and spacing `0`) containing two items in order:
1. `frame` (the existing sidebar) — stretch `0`.
2. `frame_2` (the existing content frame) — stretch `1`.

Remove the absolute `<property name="geometry">` rects from `frame` and `frame_2` (layouts own geometry now).

- [ ] **Step 3: Bound the sidebar width and make content expand**

On `frame`, add:
```xml
<property name="minimumSize"><size><width>240</width><height>0</height></size></property>
<property name="maximumSize"><size><width>320</width><height>16777215</height></size></property>
<property name="sizePolicy"><sizepolicy hsizetype="Preferred" vsizetype="Expanding"><horstretch>0</horstretch><verstretch>0</verstretch></sizepolicy></property>
```
On `frame_2`, add:
```xml
<property name="sizePolicy"><sizepolicy hsizetype="Expanding" vsizetype="Expanding"><horstretch>0</horstretch><verstretch>0</verstretch></sizepolicy></property>
```

- [ ] **Step 4: Lay out the sidebar internals (`frame`)**

Give `frame` a `QVBoxLayout`. Re-parent its existing children into it, top→bottom, removing each child's `<geometry>` rect:
`schLogo_Image`, `schoolNameLabel`, `schAddressLabel`, a vertical spacer (Fixed ~20px), `username`, `loginBtn`, `guestLoginBtn`, `line`, `time_label`, `date_label`, an expanding vertical spacer, `poster_Image`.
- Wrap `loginBtn` and `guestLoginBtn` each in a `QHBoxLayout` with an expanding spacer on each side (or set `maximumWidth` ~200) so they keep their current centered, non-full-width look.
- Give `schLogo_Image` and `poster_Image` a `sizePolicy` of `Preferred`/`Preferred` so they scale but don't dominate; their pixmaps are re-scaled by `updateLogo()`/`updatePoster()` on `resizeEvent`.

- [ ] **Step 5: Lay out the content area (`frame_2`) and scroll contents**

- Give `frame_2` a `QVBoxLayout` (margins ~6) holding the existing `stdList` `QScrollArea` (keep `widgetResizable=true`), stretch `1`. Remove `stdList`'s absolute `<geometry>`.
- `scrollAreaWidgetContents` currently has **16 direct children**, all absolutely placed — account for every one:
  - `widget` (detail card), `frame_3` (styled header strip), `widget_2`…`widget_9` (8 data rows) → **kept**, re-parented into the new layout.
  - `label_10` (Name), `label_6` (Course), `label_7` (Year Level), `label_8` (Department), `label_9` (Date and Time) → the real column captions at y≈290 → **moved into `frame_3`** in Step 7 (NOT left as siblings, or they float over the reflowed content).
  - `line_2` (horizontal, y=310) and `line_3`/`line_4`/`line_5` (vertical column dividers) and `line_6` (vertical divider at x=810) → **deleted** entirely.
- Give `scrollAreaWidgetContents` a `QVBoxLayout` whose items, in order, are: `widget` (detail card), `frame_3` (header row), then `widget_2`, `widget_3`, `widget_4`, `widget_5`, `widget_6`, `widget_7`, `widget_8`, `widget_9` (the data rows), then a trailing expanding vertical spacer. Remove each kept widget's absolute `<geometry>` rect.
- Confirm none of `label_6`/`label_7`/`label_8`/`label_9`/`label_10` or `line_2`…`line_6` are referenced in `mainwindow.cpp`/`mainwindow.h` (verified: they are not — only the `_N`-suffixed data-row labels are used by `refreshRightPanel()`), so deleting the lines and re-parenting the captions is compile-safe.

- [ ] **Step 6: Lay out the detail card (`widget`)**

Give `widget` a `QHBoxLayout`: `studentPhoto` (left, `sizePolicy` Fixed/Fixed, keep ~201×171 via `minimumSize`), then a `QVBoxLayout` containing `nameLabel` (top) and a `QGridLayout` of caption/value pairs:
- row 0: `label_5` (col 0), `courseLabel` (col 1), `label_2` (col 2), `yrlevel_label` (col 3)
- row 1: `label_4` (col 0), `depLabel` (col 1), `label_3` (col 2), `timeDate_Label` (col 3)

Remove all these children's `<geometry>` rects. Keep all object names.

- [ ] **Step 7: Lay out the header and each data row with one shared column mechanism**

Give `frame_3` and each `widget_N` (N = 2,3,4,5,6,7,8,9) a `QHBoxLayout` with left margin ~10, each holding exactly five widgets left→right in column order **name, course, year, dep, time**:
- `frame_3` (header): `label_10`, `label_6`, `label_7`, `label_8`, `label_9`.
- `widget_N` (row): `nameLabel_*`, `courseLabel_*`, `yrlevel_label_*`, `depLabel_*`, `timeDate_Label_*` (the row's own suffixed names).

**Use a single alignment mechanism — the layout's `stretch` property — applied identically to the header and every row**, so the columns line up regardless of text. On each of these nine `QHBoxLayout`s set:
```xml
<property name="stretch"><string>24,23,9,19,27</string></property>
```
Do **not** also set per-widget `sizePolicy` horstretch (mixing the two fights itself). To let columns actually divide the full width (rather than snapping to each label's text width), give every one of these labels a shrinkable/growable horizontal size policy and no hard min width:
```xml
<property name="sizePolicy"><sizepolicy hsizetype="Preferred" vsizetype="Preferred"><horstretch>0</horstretch><verstretch>0</verstretch></sizepolicy></property>
<property name="minimumSize"><size><width>0</width><height>0</height></size></property>
```
This matters once `refreshRightPanel()` populates real text — long names must not push columns out of alignment. `frame_3` keeps its `#frame_3` styling and drop shadow and is now non-empty (the captions), so it will not collapse to zero height. Remove every `<geometry>` rect on `frame_3` and on all the labels listed here. Keep all object names exactly.

- [ ] **Step 8: Build + run the responsive tests — verify they PASS**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_responsive_ui
```
Expected: `tst_responsive_ui` PASSES all six slots.

- [ ] **Step 9: Confirm no `mainwindow.cpp` change is required**

Confirm the build links `WITS.exe` with no new errors and `mainwindow.cpp` is untouched (stylesheet selectors, `refreshRightPanel()` label lists, and the `ui->widget`/`ui->frame_3` shadows all still resolve because no names changed). If the compiler reports an unknown member (e.g. `line_6`), check `mainwindow.cpp`/`mainwindow.h` — `line_6` has no code references (it is UI-only), so removing it must not break the build. Fix only if a real reference surfaces.

- [ ] **Step 10: Commit**

```bash
git add qt-app/mainwindow.ui
git commit -m "refactor(ui): convert MainWindow to responsive Qt layouts"
```

---

## Task 3: Make AdminWindow resizable (manual verification)

> **TDD note:** Per the design spec §6, AdminWindow is verified manually, not auto-tested — its fix is partly cpp-side and instantiating `adminWindow` in a unit test pulls in Charts/Network/QXlsx (too heavy/fragile). This task therefore uses build + manual run as its gate instead of red→green.

**Files:**
- Modify: `qt-app/adminwindow.ui`
- Modify: `qt-app/adminwindow.cpp:660-668`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: an admin window that resizes and maximizes, with the dashboard reflowing.

- [ ] **Step 1: Remove the fixed-size lock on `adminWindow` (`.ui`)**

In `qt-app/adminwindow.ui`, on the top-level `adminWindow`:
- Keep `minimumSize` at `760×600` (sensible floor).
- **Delete** the `maximumSize` property block (currently `760×600`) so the max defaults to unlimited.

- [ ] **Step 2: Give `centralwidget` a `QHBoxLayout` so the pages fill the window**

`centralwidget` currently holds two absolutely-positioned children: `sidebarFrame` (nav, 121×601 at x=0) and `stackedWidget` (pages, 641×601 at x=123). Add a `QHBoxLayout` (margins `0,0,0,0`, spacing `0`) to `centralwidget` with:
1. `sidebarFrame` — `sizePolicy` Fixed/Expanding, keep width ~121 via `minimumSize`/`maximumSize` width 121; stretch `0`.
2. `stackedWidget` — `sizePolicy` Expanding/Expanding; stretch `1`.
Remove the absolute `<geometry>` rects from `sidebarFrame` and `stackedWidget`.

- [ ] **Step 3: Delete the conflicting `setGeometry` block in `adminwindow.cpp`**

The four dashboard frames already live in `generalPage`'s `QGridLayout` (`gridLayout`). The manual `setGeometry` calls fight that layout. In `qt-app/adminwindow.cpp`, delete lines 664–668:
```cpp
    // --- Frame positions (manual) ---
    ui->securityFrame->setGeometry(30,30,250,120);
    ui->adminFrame->setGeometry(310,30,250,120);
    ui->settingsFrame->setGeometry(30,180,530,120);
    ui->libraryFrame->setGeometry(30,330,530,220);
```
Leave the surrounding `verticalLayout` spacing setup (661–662) and the shadow lambda (670+) intact.

- [ ] **Step 4: Relax blocking child size caps**

Scan `adminwindow.ui` for `maximumSize` properties on **container** widgets that would stop the dashboard/pages from growing (e.g. a page or content frame capped near 600px). Remove/raise only those; **leave** caps on genuinely fixed small controls (icons, the 48×48 spinner, fixed-width nav buttons). When unsure, prefer leaving a control's cap and only relaxing containers. Verify the four dashboard frames keep their `minimumSize` (so the grid still looks right) but gain room to breathe.

- [ ] **Step 5: Build — verify clean compile/link**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build qt-app/build
```
Expected: links `WITS.exe` with no new warnings/errors.

- [ ] **Step 6: Manual run — verify admin resizes**

Launch `qt-app/build/WITS.exe`, open the admin window, and confirm: the window can be resized and maximized; the left nav stays ~121px while the page area grows; the General page's four dashboard frames reflow via the grid (no overlap, no clipping); switch through the stacked pages (database, reporting, search, visitor) and confirm none are clipped at large or minimum size.

- [ ] **Step 7: Commit**

```bash
git add qt-app/adminwindow.ui qt-app/adminwindow.cpp
git commit -m "fix(admin): make AdminWindow resizable and reflow dashboard"
```

---

## Task 4: Tidy `guestwindow.ui`

**Files:**
- Modify: `qt-app/guestwindow.ui`

**Interfaces:**
- Consumes: the `guestDialogHasLayout` assertion from Task 1.
- Produces: a guest dialog whose content is fully owned by a new top-level layout and has no hard size cap.

- [ ] **Step 1: Build the dialog's top-level layout**

The `guestwindow` `QDialog` currently has **no top-level layout** — `label_5` (title "VISITORS LOGIN"), `formLayoutWidget` (which wraps the existing `QFormLayout`), `submitBtn`, and `cancelBtn` are all absolutely positioned siblings. In `qt-app/guestwindow.ui`:
- Add a top-level **`QVBoxLayout`** on `guestwindow` (margins ~12, spacing ~10) with items in order:
  1. `label_5` (title).
  2. `formLayoutWidget` (the form) — stretch `1` so it takes the slack.
  3. a **`QHBoxLayout`** button row: a leading expanding horizontal spacer, then `submitBtn`, then `cancelBtn` (so the buttons stay bottom-right as they look today).
- Remove the absolute `<geometry>` rects from `label_5`, `formLayoutWidget`, `submitBtn`, and `cancelBtn`. Keep all object names (`onSubmit`/`cancel` slots in `guestwindow.cpp` connect by name).
- Confirm there is **no** `maximumSize` cap (there is none today — leave it that way).

- [ ] **Step 2: Build + run the responsive test — verify guest assertion PASSES**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_responsive_ui
```
Expected: `tst_responsive_ui` PASSES (including `guestDialogHasLayout`).

- [ ] **Step 3: Commit**

```bash
git add qt-app/guestwindow.ui
git commit -m "refactor(ui): make guest dialog content layout-owned"
```

---

## Task 5: Full verification + review gate

**Files:** none (verification only).

- [ ] **Step 1: Clean configure + full build + full ctest**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: all tests pass, including `tst_responsive_ui`; no new warnings/errors.

- [ ] **Step 2: Manual multi-size run of `WITS.exe`**

Run `qt-app/build/WITS.exe` and verify by eye at: ~800×600, large windowed, maximized, and fullscreen. Confirm: the sidebar holds its flexible width (240–320), the content + recent-logins columns stretch and stay aligned (header lines up with rows), logo/poster images scale, and no widget is clipped or floating. Then open the admin window and re-confirm Task 3 Step 6.

- [ ] **Step 3: Run the `/claude-review` gate**

Invoke the `claude-review` skill on the branch. Fix Critical/Important findings (re-running Tasks above as needed), re-submit until APPROVE or 3 rounds.

- [ ] **Step 4: Final confirmation**

Confirm build green, ctest green, manual run good, review APPROVED. The branch is ready for `finishing-a-development-branch` / `create-pr`.

---

## Self-Review

- **Spec coverage:** §1 MainWindow top-level → Task 2 Steps 1–3. §2 sidebar → Task 2 Step 4. §3 content/table (header captions `label_10`/`label_6`/`label_7`/`label_8`/`label_9` re-homed into `frame_3`; `line_2`…`line_6` deleted; single `stretch`=24,23,9,19,27 mechanism) → Task 2 Steps 5–7. §4 admin → Task 3. §5 guest (build top-level `QVBoxLayout`) → Task 4. §6 tests → Task 1 + Task 3 TDD note. §7 verification → Task 5. All covered.
- **Refinement vs. spec:** spec §4 said "replace setGeometry with a QGridLayout"; the `generalPage` grid already exists, so the plan instead **deletes** the setGeometry block and adds the missing `centralwidget` `QHBoxLayout` (sidebar + stacked). Same outcome, less invasive — noted in Task 3.
- **Claude-review round 1 fixes folded in:** the recent-logins header is the real caption labels (`label_6`…`label_10`), not an empty `frame_3` — they are enumerated and re-homed (Task 2 Steps 5/7), guarded by the new `mainWindowHeaderHasColumns` test; the empty-`frame_3`-collapse risk is resolved by the captions living in it; the guest dialog genuinely lacks a top-level layout, so Task 4 now builds one and Task 1 expects its assertion RED; the column mechanism is pinned to a single `stretch` property; a copy-`.ui`-next-to-exe fallback covers the spaced source path.
- **Placeholder scan:** no TBD/TODO; every code step shows exact XML/cpp/CMake.
- **Name consistency:** no object renames; `tst_responsive_ui` slot names (incl. `mainWindowHeaderHasColumns`), `WITS_UI_DIR` define, and target name are consistent across Tasks 1/2/4.
