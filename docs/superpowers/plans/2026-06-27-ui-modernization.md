# WITS UI Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restyle the attendance kiosk (`MainWindow`) and admin dashboard (`adminWindow`) to a "clean solid modern" look matching the two provided mockups, with zero behavioral change.

**Architecture:** Introduce a Qt resource system (`.qrc`) and one central QSS stylesheet loaded app-wide in `main.cpp`, layered over the existing `WitsTheme::lightPalette()`. Existing inline `setStyleSheet` blocks are replaced by new-theme rules keyed off existing `objectName`s. Structural additions (kiosk spotlight card, admin header bar) are added in code, reusing existing widgets/signals. Drop shadows and state-dependent styling stay in code.

**Tech Stack:** C++17, Qt 6 (Widgets, Network, Charts, PrintSupport), CMake, QtTest, vendored QXlsx.

**Source spec:** `docs/superpowers/specs/2026-06-27-ui-modernization-design.md`

## Global Constraints

- **No behavioral change.** Networking, RFID, report generation, search, settings persistence, and all signals/slots keep current behavior. This is a skin.
- **Preserve every existing `objectName`.** `tst_responsive_ui` (`qt-app/tests/tst_responsive_ui.cpp`) pins an exhaustive name set + page layouts + size policies + the `searchOverlay` carve-out. It must stay green after every `.ui` edit. New widgets may be **added**; checked names must never be **removed**.
- **Visual style:** clean solid modern — no background image, no glassmorphism.
- **Recent-logins stays the existing fixed 9 label-rows** (restyled, not converted to a dynamic table); `refreshRightPanel`'s label arrays are untouched.
- **Color palette (exact values):** sidebar slate `#1E293B`; card `#FFFFFF` (16px radius, 1px `#E2E8F0` border); app bg `#F1F5F9`; kiosk login emerald `#10B981` (hover `#059669`); admin primary blue `#2563EB` (hover `#1D4ED8`); secondary blue `#3B82F6` (hover `#2563EB`); text `#1E293B`; muted `#64748B`; borders `#E2E8F0`; success `#10B981`; error `#EF4444`.
- **Shadows are code, not QSS** — reuse the `addShadow` lambda pattern (`adminwindow.cpp:675`), one `QGraphicsDropShadowEffect` instance per widget (never shared).
- **Build:** configure `cmake -S qt-app -B qt-app/build`, build `cmake --build qt-app/build`, test `ctest --test-dir qt-app/build --output-on-failure`. (Use the toolchain from project memory: Ninja + Qt 6.11.1 MinGW kit, `CMAKE_PREFIX_PATH` set; tools are not on PATH.)
- **Commit via the `commit` skill**; never raw `git add`/`commit`. Never commit `qt-app/build/`.
- **No secrets, no real student PII, no machine-local paths** in any committed file (icons, QSS, code).

---

## Phase 1 — Theming infrastructure (keystone)

Stand up the resource system, theme helper, and central stylesheet, and move global styling off the inline blocks. End state: app builds, loads `wits.qss`, both windows render with the new base theme, all existing tests green.

### Task 1.1: Create the resource file and central stylesheet, wire into CMake

**Files:**
- Create: `qt-app/resources.qrc`
- Create: `qt-app/resources/wits.qss`
- Modify: `qt-app/CMakeLists.txt:15-27` (add `resources.qrc` to `PROJECT_SOURCES`)

**Interfaces:**
- Produces: a compiled Qt resource at `:/resources/wits.qss` (AUTORCC compiles any `.qrc` in target sources).

- [ ] **Step 1: Create the resource file**

Create `qt-app/resources.qrc`:

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/resources">
        <file>resources/wits.qss</file>
    </qresource>
</RCC>
```

Note: the `<file>` path is relative to the `.qrc` location (`qt-app/`), and the resource alias becomes `:/resources/resources/wits.qss` unless aliased. To get the clean path `:/resources/wits.qss`, use an alias:

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/resources">
        <file alias="wits.qss">resources/wits.qss</file>
    </qresource>
</RCC>
```

- [ ] **Step 2: Create the stylesheet with the global base theme**

Create `qt-app/resources/wits.qss`:

```css
/* WITS central stylesheet — clean solid modern theme.
   Window-specific rules are appended in later phases. */

/* ---- Global widget defaults ---- */
QWidget {
    font-family: "Segoe UI", "Roboto", sans-serif;
    color: #1E293B;
}

QMainWindow, QDialog {
    background-color: #F1F5F9;
}

/* ---- Inputs ---- */
QLineEdit, QComboBox, QSpinBox, QDateEdit, QFontComboBox, QPlainTextEdit, QTextEdit {
    background: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 8px;
    padding: 6px 10px;
    color: #1E293B;
    selection-background-color: #4A90E2;
    selection-color: #FFFFFF;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus,
QFontComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 2px solid #3B82F6;
}

/* ---- Generic buttons (secondary blue) ---- */
QPushButton {
    background-color: #3B82F6;
    color: #FFFFFF;
    border: none;
    border-radius: 8px;
    padding: 8px 14px;
    font-weight: 600;
}
QPushButton:hover   { background-color: #2563EB; }
QPushButton:pressed { background-color: #1D4ED8; }
QPushButton:disabled {
    background-color: #CBD5E1;
    color: #94A3B8;
}

/* ---- Tables ---- */
QTableWidget, QTableView {
    background: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 12px;
    gridline-color: #E2E8F0;
}
QHeaderView::section {
    background-color: #1E293B;
    color: #FFFFFF;
    padding: 8px;
    border: none;
    font-weight: 600;
}

/* ---- Scrollbars ---- */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #CBD5E1; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #94A3B8; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #CBD5E1; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #94A3B8; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* ---- Tooltips ---- */
QToolTip {
    background-color: #1E293B;
    color: #FFFFFF;
    border: none;
    padding: 6px 8px;
    border-radius: 6px;
}
```

- [ ] **Step 3: Add the resource to the CMake target**

In `qt-app/CMakeLists.txt`, add `resources.qrc` to `PROJECT_SOURCES` (lines 15-27):

```cmake
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    adminwindow.cpp
    adminwindow.h
    adminwindow.ui
    rfidscandetector.cpp
    rfidscandetector.h
    rfidkeyboardfilter.cpp
    rfidkeyboardfilter.h
    resources.qrc
)
```

- [ ] **Step 4: Configure and build to verify the resource compiles**

Run: `cmake -S qt-app -B qt-app/build` then `cmake --build qt-app/build`
Expected: clean build, no errors. AUTORCC compiles `resources.qrc`.

- [ ] **Step 5: Verify the existing UI tests still pass**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all tests PASS (no `.ui` changed yet; this confirms the resource addition didn't break the build/test harness).

- [ ] **Step 6: Commit** (via the `commit` skill)

Suggested message: `build(ui): add Qt resource system and central stylesheet`

---

### Task 1.2: Extend the theme helper with palette constants and a stylesheet loader

**Files:**
- Modify: `qt-app/theme.h` (add constants + `loadStyleSheet`)
- Modify: `qt-app/tests/tst_theme.cpp` (add a loader test)
- Modify: `qt-app/tests/CMakeLists.txt:40-49` (give `tst_theme` the on-disk QSS path)

**Interfaces:**
- Produces:
  - `WitsTheme::loadStyleSheet(const QString &path = QStringLiteral(":/resources/wits.qss")) -> QString` — returns the file contents, or an empty string if it can't be opened.
  - Color name constants in `namespace WitsTheme` (see code).

- [ ] **Step 1: Write the failing test**

In `qt-app/tests/tst_theme.cpp`, add to the class slots and implement (the test loads from the real file on disk via a compile-time path, since `tst_theme` does not compile the app `.qrc`):

```cpp
// add to the private slots list:
void loadStyleSheetReturnsNonEmpty();
void loadStyleSheetMissingFileIsEmpty();

// add the implementations (before QTEST_MAIN):
void TestTheme::loadStyleSheetReturnsNonEmpty()
{
    // WITS_QSS_PATH is injected by CMake to the on-disk wits.qss.
    const QString qss = WitsTheme::loadStyleSheet(QStringLiteral(WITS_QSS_PATH));
    QVERIFY2(!qss.isEmpty(), "wits.qss should load with content");
    QVERIFY2(qss.contains("QPushButton"), "wits.qss should contain button rules");
}

void TestTheme::loadStyleSheetMissingFileIsEmpty()
{
    QCOMPARE(WitsTheme::loadStyleSheet(QStringLiteral("/no/such/file.qss")), QString());
}
```

Add near the top of the file, after the includes:

```cpp
#ifndef WITS_QSS_PATH
#define WITS_QSS_PATH "resources/wits.qss"
#endif
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_theme --output-on-failure`
Expected: FAIL — `loadStyleSheet` is not a member of `WitsTheme` (compile error).

- [ ] **Step 3: Implement `loadStyleSheet` and color constants**

In `qt-app/theme.h`, add the includes and, inside `namespace WitsTheme`, the constants and function:

```cpp
// add to the existing includes at the top:
#include <QFile>
#include <QString>
#include <QTextStream>
```

```cpp
// inside namespace WitsTheme, after lightPalette():

// Named colors for the clean-solid-modern theme. State-dependent styling in
// code (active sidebar, toast) reads these instead of hardcoding hex.
namespace Color {
inline const QString SidebarBase   = QStringLiteral("#1E293B");
inline const QString Card          = QStringLiteral("#FFFFFF");
inline const QString AppBackground = QStringLiteral("#F1F5F9");
inline const QString Border        = QStringLiteral("#E2E8F0");
inline const QString Text          = QStringLiteral("#1E293B");
inline const QString MutedText     = QStringLiteral("#64748B");
inline const QString KioskPrimary  = QStringLiteral("#10B981");
inline const QString KioskPrimaryHover = QStringLiteral("#059669");
inline const QString AdminPrimary  = QStringLiteral("#2563EB");
inline const QString AdminPrimaryHover = QStringLiteral("#1D4ED8");
inline const QString Secondary     = QStringLiteral("#3B82F6");
inline const QString Success       = QStringLiteral("#10B981");
inline const QString Error         = QStringLiteral("#EF4444");
} // namespace Color

// Read a QSS file (resource path ":/..." or filesystem path). Returns empty
// on failure so a missing stylesheet degrades to the palette, never a crash.
inline QString loadStyleSheet(const QString &path = QStringLiteral(":/resources/wits.qss"))
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&file);
    return in.readAll();
}
```

- [ ] **Step 4: Inject the on-disk QSS path into the test target**

In `qt-app/tests/CMakeLists.txt`, add a compile definition to `tst_theme` (after line 44, alongside the include dir):

```cmake
target_compile_definitions(tst_theme PRIVATE WITS_QSS_PATH="${CMAKE_SOURCE_DIR}/resources/wits.qss")
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_theme --output-on-failure`
Expected: PASS (5 existing + 2 new theme tests).

- [ ] **Step 6: Commit** (via the `commit` skill)

Suggested message: `feat(theme): add palette constants and QSS loader helper`

---

### Task 1.3: Load the central stylesheet app-wide

**Files:**
- Modify: `qt-app/main.cpp:10` (load QSS after setting the palette)

**Interfaces:**
- Consumes: `WitsTheme::loadStyleSheet()` from Task 1.2.

- [ ] **Step 1: Apply the stylesheet in `main`**

In `qt-app/main.cpp`, after `a.setPalette(WitsTheme::lightPalette());`:

```cpp
        QApplication a(argc, argv);
        a.setPalette(WitsTheme::lightPalette());
        a.setStyleSheet(WitsTheme::loadStyleSheet());
        MainWindow w;
        w.show();
```

- [ ] **Step 2: Build**

Run: `cmake --build qt-app/build`
Expected: clean build.

- [ ] **Step 3: Run the app and confirm the base theme loads**

Run the `WITS` executable. Expected: inputs/buttons/scrollbars now show the new base theme (rounded blue buttons, light-card inputs). The kiosk still has its old inline look layered on top (replaced in Task 1.4). No crash, login field focusable.

- [ ] **Step 4: Commit** (via the `commit` skill)

Suggested message: `feat(ui): apply central stylesheet application-wide`

---

### Task 1.4: Replace MainWindow's inline stylesheet with central rules

**Files:**
- Modify: `qt-app/mainwindow.cpp:118-208` (remove the inline `setStyleSheet` block)
- Modify: `qt-app/resources/wits.qss` (append kiosk base rules)

**Interfaces:**
- Consumes: existing `objectName`s `frame`, `frame_2`, `frame_3`, `loginBtn`, `username`, `schLogo_Image`, `schoolNameLabel`, `schAddressLabel`, `time_label`, `date_label`.

- [ ] **Step 1: Append kiosk base rules to `wits.qss`**

Add to `qt-app/resources/wits.qss`:

```css
/* ================= Attendance kiosk (MainWindow) ================= */
QFrame#frame {                 /* left sidebar */
    background-color: #1E293B;
    border: none;
}
QFrame#frame_2 {               /* right content panel */
    background-color: #FFFFFF;
    border-radius: 16px;
}
QFrame#frame_3 {               /* recent-logins header strip */
    background-color: #1E293B;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}
QLabel#schoolNameLabel { color: #FFFFFF; font-size: 18px; font-weight: 700; }
QLabel#schAddressLabel { color: #CBD5E1; font-size: 12px; }
QLabel#schLogo_Image {
    color: #FFFFFF;
    background-color: #FFFFFF;
    border-radius: 12px;
}
QLineEdit#username {
    border: 2px solid #334155;
    border-radius: 10px;
    padding: 12px;
    background: #FFFFFF;
    color: #1E293B;
    font-size: 15px;
}
QLineEdit#username:focus { border: 2px solid #10B981; }
QPushButton#loginBtn {
    background-color: #10B981;
    color: #FFFFFF;
    font-size: 15px;
    font-weight: 700;
    border-radius: 10px;
    padding: 12px;
}
QPushButton#loginBtn:hover   { background-color: #059669; }
QPushButton#loginBtn:pressed { background-color: #047857; }
QLabel#time_label { color: #F1F5F9; font-size: 20px; font-weight: 700; }
QLabel#date_label { color: #CBD5E1; font-size: 13px; }
/* Header-strip column captions (kept by tst_responsive_ui) */
QLabel#label_10, QLabel#label_6, QLabel#label_7, QLabel#label_8, QLabel#label_9 {
    color: #FFFFFF; font-weight: 600;
}
```

- [ ] **Step 2: Remove the inline stylesheet block**

In `qt-app/mainwindow.cpp`, delete the entire `this->setStyleSheet(R"( ... )");` call spanning lines 118-208. Leave the surrounding logic (timer setup, `connect(ui->loginBtn ...)`, `ui->schLogo_Image->setAlignment(...)`) intact.

- [ ] **Step 3: Build**

Run: `cmake --build qt-app/build`
Expected: clean build.

- [ ] **Step 4: Run the app and verify no structural regression**

Run the `WITS` executable. Expected: kiosk renders with the new slate sidebar, white right panel, emerald login button; school name/time/date legible; login field works. Look is the new theme (intentional), structure unchanged.

- [ ] **Step 5: Run UI tests**

Run: `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected: PASS (names/layouts preserved — no `.ui` changed).

- [ ] **Step 6: Commit** (via the `commit` skill)

Suggested message: `refactor(ui): move MainWindow styling into central stylesheet`

---

### Task 1.5: Replace adminWindow + dialog inline styles with central rules

**Files:**
- Modify: `qt-app/adminwindow.cpp` (remove the inline sidebar/page `setStyleSheet` blocks, e.g. ~lines 555-620; keep `setActiveSidebar` body for now — restyled in Phase 3)
- Modify: `qt-app/attachfilesdialog.cpp` (replace its 4 inline `setStyleSheet` calls with reliance on the central sheet or theme colors)
- Modify: `qt-app/resources/wits.qss` (append admin base + dialog rules)

**Interfaces:**
- Consumes: admin `objectName`s `generalBtn`..`visitorBtn`, `stackedWidget`, the page frames.

- [ ] **Step 1: Append admin base rules to `wits.qss`**

Add to `qt-app/resources/wits.qss`:

```css
/* ================= Admin dashboard (adminWindow) ================= */
/* Sidebar container is restyled fully in Phase 3; base nav button look here. */
QPushButton#generalBtn, QPushButton#databaseBtn, QPushButton#reportingBtn,
QPushButton#studentSearchBtn, QPushButton#visitorBtn {
    background-color: transparent;
    color: #CBD5E1;
    text-align: left;
    padding: 12px 16px;
    border: none;
    border-radius: 10px;
    font-weight: 600;
}
QPushButton#generalBtn:hover, QPushButton#databaseBtn:hover,
QPushButton#reportingBtn:hover, QPushButton#studentSearchBtn:hover,
QPushButton#visitorBtn:hover {
    background-color: #334155;
    color: #FFFFFF;
}
/* Card-like group boxes / frames across admin pages */
QGroupBox {
    background-color: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 16px;
    margin-top: 12px;
    padding: 12px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 14px;
    padding: 0 4px;
    color: #64748B;
}
```

- [ ] **Step 2: Remove the inline admin style blocks**

In `qt-app/adminwindow.cpp`, locate the inline `setStyleSheet(R"(...)")` block(s) that style the sidebar buttons and pages (around lines 555-620) and remove them. **Do not** remove `setActiveSidebar` (line 339) or its runtime state styling yet — that is migrated to read theme colors in Phase 3. If any removed inline block contained rules other than sidebar/page chrome, fold the still-needed ones into `wits.qss`.

- [ ] **Step 3: Migrate the attachFilesDialog inline styles**

In `qt-app/attachfilesdialog.cpp`, replace the 4 inline `setStyleSheet` calls. For purely cosmetic ones (buttons, frames), delete them and let the central sheet apply. For any that encode meaningful state, replace the literal hex with `WitsTheme::Color::*` (add `#include "theme.h"`). Append any genuinely dialog-specific rule to `wits.qss` under an `#attachFilesDialog` / object-name selector.

- [ ] **Step 4: Build**

Run: `cmake --build qt-app/build`
Expected: clean build.

- [ ] **Step 5: Run the app — open admin, click through all 5 tabs**

Log in with the admin key, open each tab (General, Database, Reporting, Search, Visit Logs), open the attach-files dialog. Expected: cards/inputs/buttons show the new theme; every tab switches; no crash; tables populate as before.

- [ ] **Step 6: Run the full test suite**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS.

- [ ] **Step 7: Commit** (via the `commit` skill)

Suggested message: `refactor(ui): move admin and dialog styling into central stylesheet`

---

### Phase 1 gate

- [ ] Build clean, `ctest` all green, app runs, both windows show the new base theme with no structural/behavioral regression.
- [ ] Run `/claude-review` (PHASE mode) on the Phase 1 commits; fix Critical/Important findings before Phase 2.

---

## Phase 2 — Attendance kiosk (MainWindow)

Add the active-profile spotlight card, restyle the recent-logins table, restyle the toast, and fix the dangling student-photo placeholder.

### Task 2.1: Fix the dangling default student photo asset

**Files:**
- Modify: `qt-app/CMakeLists.txt:12-13,51-57` (add `Svg` so SVG actually rasterizes)
- Create: `qt-app/resources/default_student.svg`
- Modify: `qt-app/resources.qrc` (register it)
- Modify: `qt-app/mainwindow.cpp:45-52` (load via `QIcon` so SVG renders)

**Interfaces:**
- Consumes: `ui->studentPhoto` (existing `QLabel`).

> **Prerequisite (from review):** `QIcon(":/…svg")` only rasterizes when Qt's SVG plugin is
> available — used here and in Task 4.1 for every icon. `CMakeLists.txt` does not currently
> pull in `Qt::Svg`, so without this step the placeholder and all nav/login icons render
> blank. This step adds it once for the whole feature.

- [ ] **Step 1: Add Qt Svg to the build**

In `qt-app/CMakeLists.txt`, add `Svg` to both `find_package` lines (12-13):

```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Charts Test UiTools PrintSupport Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Charts Test UiTools PrintSupport Svg)
```

and link it in `target_link_libraries(WITS PRIVATE ...)` (51-57):

```cmake
    Qt${QT_VERSION_MAJOR}::Svg
```

Linking `Qt::Svg` makes the SVG image/icon plugin available; `windeployqt` then bundles it for release. Build once to confirm the package resolves: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build`.

- [ ] **Step 2: Create the placeholder SVG**

Create `qt-app/resources/default_student.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" width="200" height="200">
  <rect width="200" height="200" rx="16" fill="#E2E8F0"/>
  <circle cx="100" cy="78" r="38" fill="#94A3B8"/>
  <path d="M40 170c0-33 27-52 60-52s60 19 60 52z" fill="#94A3B8"/>
</svg>
```

- [ ] **Step 3: Register it in the resource file**

In `qt-app/resources.qrc`, add inside `<qresource prefix="/resources">`:

```xml
        <file alias="default_student.svg">resources/default_student.svg</file>
```

- [ ] **Step 4: Load it via QIcon**

In `qt-app/mainwindow.cpp`, replace the placeholder load (lines 45-52, currently `QPixmap placeholder(":/resources/default_student.png"); ...`) with:

```cpp
    QIcon placeholderIcon(":/resources/default_student.svg");
    ui->studentPhoto->setPixmap(placeholderIcon.pixmap(
        ui->studentPhoto->size()));
    ui->studentPhoto->setScaledContents(false);
    ui->studentPhoto->setAlignment(Qt::AlignCenter);
```

Add `#include <QIcon>` to the includes if not present.

- [ ] **Step 5: Build and run**

Run: `cmake --build qt-app/build` then run `WITS`.
Expected: the student-photo area shows the grey silhouette placeholder (previously blank). Logging in still swaps in the fetched photo.

- [ ] **Step 6: Commit** (via the `commit` skill)

Suggested message: `fix(ui): add student-photo placeholder and resolve dangling resource`

---

### Task 2.2: Restyle the existing active-profile block as the spotlight card

> **Design correction (from review):** `frame_2` already contains the active-profile
> display. Its structure is `frame_2 > frame2Layout (QVBox) > stdList (QScrollArea) >
> scrollContentsLayout (QVBox)` whose **first item is `widget`** (`widgetDetailLayout`,
> QHBox) = `studentPhoto` + `detailInfoLayout` (`nameLabel` [20pt bold] + `detailGridLayout`
> with captions `label_5/label_2/label_4/label_3` and values `courseLabel`, `yrlevel_label`,
> `depLabel`, `timeDate_Label`). These value labels are **row 0** of `refreshRightPanel`'s
> arrays (`nameLabels[0]`…), and `displayStudent` already loads the fetched photo into
> `studentPhoto`. So we **restyle this existing block in place** — do NOT insert a new card
> (a separate card would duplicate the active student and drop the photo). This preserves
> `refreshRightPanel`, the photo feature, and the fade-in, with no new widgets.

**Files:**
- Modify: `qt-app/resources/wits.qss` (card styling on the existing block)
- Modify: `qt-app/mainwindow.cpp` (upgrade the existing `ui->widget` shadow already set at line ~217; optional reveal fade)
- Modify: `qt-app/tests/tst_responsive_ui.cpp` (pin the spotlight host widgets)

**Interfaces:**
- Consumes (all existing): `ui->widget`, `ui->studentPhoto`, `ui->nameLabel`,
  `ui->courseLabel`, `ui->yrlevel_label`, `ui->depLabel`, `ui->timeDate_Label`, and the
  caption labels `label_5/label_2/label_4/label_3`.
- Produces: no new symbols. Styling is keyed off the existing `objectName`s.

- [ ] **Step 1: Add the failing structural test (pin the spotlight host)**

In `qt-app/tests/tst_responsive_ui.cpp`, add `void mainWindowHasSpotlightHost();` to the
`private slots:` list, then implement:

```cpp
void TestResponsiveUi::mainWindowHasSpotlightHost()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY2(w, "failed to load mainwindow.ui");
    // The spotlight is the existing row-0 block; pin its host + key children so a
    // future .ui edit can't silently remove the active-profile display.
    QWidget *block = w->findChild<QWidget *>("widget");
    QVERIFY2(block && block->layout(), "active-profile block 'widget' missing/laid-out");
    for (const char *n : {"studentPhoto", "nameLabel", "courseLabel",
                          "yrlevel_label", "depLabel", "timeDate_Label"}) {
        QVERIFY2(w->findChild<QWidget *>(n),
                 qPrintable(QString("spotlight child %1 missing").arg(n)));
    }
}
```

- [ ] **Step 2: Run it to verify current state**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected: PASS immediately (these widgets all exist today). The test now guards them.

- [ ] **Step 3: Append spotlight QSS targeting the existing block**

Add to `qt-app/resources/wits.qss`:

```css
/* Active-profile spotlight = the existing row-0 block (#widget) */
QWidget#widget {
    background-color: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 16px;
}
QLabel#studentPhoto {
    background-color: #F1F5F9;
    border: 1px solid #E2E8F0;
    border-radius: 12px;
}
QLabel#nameLabel { font-size: 28px; font-weight: 800; color: #1E293B; }
/* Field captions */
QLabel#label_5, QLabel#label_2, QLabel#label_4, QLabel#label_3 {
    font-size: 11px; font-weight: 700; color: #94A3B8;
}
/* Field values */
QLabel#courseLabel, QLabel#yrlevel_label, QLabel#depLabel {
    font-size: 15px; color: #1E293B;
}
QLabel#timeDate_Label { font-size: 15px; font-weight: 700; color: #10B981; }
```

> Note on font conflicts: `nameLabel` etc. carry inline `<font>` point sizes in
> `mainwindow.ui`. A stylesheet `font-size` overrides them, so the QSS wins — no `.ui` edit
> needed. Do not change the `.ui` font properties (keeps `tst_responsive_ui` untouched).

- [ ] **Step 4: Keep/strengthen the existing shadow (already on `ui->widget`)**

`mainwindow.cpp:217` already applies a `QGraphicsDropShadowEffect` to `ui->widget`
(`shadow1`). Leave it; optionally bump `setBlurRadius(20)` / `setColor(QColor(0,0,0,40))` for
a softer card shadow. Do **not** add a second effect to the same widget (a widget holds one
`QGraphicsEffect`).

- [ ] **Step 5: (Optional) subtle reveal fade on login**

`displayStudent` already fades `studentPhoto` in via `QGraphicsOpacityEffect` +
`QPropertyAnimation`. The active-profile labels update through the existing
`refreshRightPanel()` call inside `displayStudent` (row 0). No new populate code is needed.
If a whole-block fade is desired, animate opacity on `ui->widget` — but note this would
replace `shadow1` on that widget for the duration; prefer leaving the photo-only fade as-is.

- [ ] **Step 6: Build, run, verify**

Run: `cmake --build qt-app/build` then run `WITS`. Log in / scan a test student.
Expected: the active-profile block reads as a white rounded card — photo at left, large name,
and the Course/Year/Department/Date-and-Time grid with an emerald time value. On login the
photo fades in and the fields populate (row 0). Idle state shows the empty card (labels
blank — empty-state hint added in Task 4.2).

- [ ] **Step 7: Run tests**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS including `mainWindowHasSpotlightHost`.

- [ ] **Step 8: Commit** (via the `commit` skill)

Suggested message: `feat(kiosk): restyle active-profile block as spotlight card`

---

### Task 2.3: Restyle the recent-logins table and toast

**Files:**
- Modify: `qt-app/resources/wits.qss` (row + toast rules)
- Modify: `qt-app/mainwindow.cpp:337-355` (`showKioskStatus` — source colors from theme, pill shape + status)

**Interfaces:**
- Consumes: existing recent-login row widgets and the `frame_3` header strip; `showKioskStatus(message, error)`.

- [ ] **Step 1: Append recent-logins row QSS**

Add to `qt-app/resources/wits.qss` (the row container objectNames are `widget_2` and siblings; style the data labels generically within the right panel):

The recent-login rows are `widget_2` through `widget_9` (confirmed in `mainwindow.ui`; `tst_responsive_ui` pins `widget_2`, so none may be renamed):

```css
/* Recent-logins rows — restyle the existing fixed label-rows as table rows */
QWidget#widget_2, QWidget#widget_3, QWidget#widget_4, QWidget#widget_5,
QWidget#widget_6, QWidget#widget_7, QWidget#widget_8, QWidget#widget_9 {
    background-color: #FFFFFF;
    border: 1px solid #F1F5F9;
    border-radius: 10px;
}
QWidget#widget_2:hover, QWidget#widget_3:hover, QWidget#widget_4:hover,
QWidget#widget_5:hover, QWidget#widget_6:hover, QWidget#widget_7:hover,
QWidget#widget_8:hover, QWidget#widget_9:hover { background-color: #F8FAFC; }
```

- [ ] **Step 2: Restyle the toast in `showKioskStatus`**

In `qt-app/mainwindow.cpp`, replace the stylesheet lines inside `showKioskStatus` (the `m_statusLabel->setStyleSheet(error ? ... : ...)` call) with theme-sourced colors and a pill shape:

```cpp
    m_statusLabel->setStyleSheet(QString(
        "background:%1; color:white; padding:14px 22px; border-radius:22px;"
        "font-size:18px; font-weight:600;")
        .arg(error ? WitsTheme::Color::Error : WitsTheme::Color::Success));
```

Add `#include "theme.h"` to `mainwindow.cpp` if not already included.

- [ ] **Step 3: Build and run**

Run: `cmake --build qt-app/build` then run `WITS`. Trigger a successful scan and a failed scan (unknown card).
Expected: rows look like clean table rows with hover; the status toast is a rounded pill (green success / red error).

- [ ] **Step 4: Run tests**

Run: `ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit** (via the `commit` skill)

Suggested message: `feat(kiosk): restyle recent-logins rows and status toast`

---

### Phase 2 gate

- [ ] Build clean, tests green, app runs; spotlight card, table rows, toast, and placeholder all verified by running the kiosk.
- [ ] `/claude-review` (PHASE mode) on Phase 2; fix Critical/Important.

---

## Phase 3 — Admin dashboard (adminWindow)

Restyle the sidebar with active/inactive nav states, add the top header bar, and card-wrap the pages.

### Task 3.1: Restyle the sidebar nav with active/inactive state

**Files:**
- Modify: `qt-app/adminwindow.cpp:339-369` (`setActiveSidebar` — source colors from theme)
- Modify: `qt-app/resources/wits.qss` (sidebar container + active state)

**Interfaces:**
- Consumes: `setActiveSidebar(QPushButton*)`, sidebar buttons `generalBtn`..`visitorBtn`, the sidebar container frame (confirm its objectName in `adminwindow.ui`).

- [ ] **Step 1: Append sidebar container QSS**

Add to `qt-app/resources/wits.qss` (the admin sidebar container is `sidebarFrame`, confirmed at `adminwindow.ui:142`):

```css
QFrame#sidebarFrame { background-color: #1E293B; border: none; }
QFrame#sidebarFrame QPushButton[active="true"] {
    background-color: #334155;
    color: #FFFFFF;
}
```

- [ ] **Step 2: Confirm the active-state mechanism (already in code — no rewrite)**

> **Correction (from review):** `setActiveSidebar` (`adminwindow.cpp:339-353`) **already**
> sets the `active` dynamic property on each nav button and calls `unpolish`/`polish`/`update`.
> Do **not** rewrite it — a rewrite would be a no-op and risks dropping the existing
> `btn->update()`. The only genuinely new thing is the QSS `[active="true"]` selector added in
> Step 1, which now has an effect because the property is already being set.

Verify the existing body matches (the dynamic property + repolish loop). Make **no code
change** here unless the QSS doesn't apply, in which case confirm `#include <QStyle>` is
present (it is, via `btn->style()`).

- [ ] **Step 3: Build, run, click through tabs**

Run: `cmake --build qt-app/build` then run `WITS` → admin. Click each nav item.
Expected: the active nav item is highlighted (`#334155`, white text); others muted; hover works.

- [ ] **Step 4: Run tests**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS.

- [ ] **Step 5: Commit** (via the `commit` skill)

Suggested message: `feat(admin): restyle sidebar nav with active state`

---

### Task 3.2: Add the top header bar

**Files:**
- Modify: `qt-app/adminwindow.h` (header members)
- Modify: `qt-app/adminwindow.cpp` (build the bar; update title in `setActiveSidebar`)
- Modify: `qt-app/resources/wits.qss` (header styling)
- Modify: `qt-app/tests/tst_responsive_ui.cpp` (assert header host preserved)

**Interfaces:**
- Consumes: the admin central widget / the container that holds `stackedWidget`.
- Produces: `adminWindow::m_headerTitle` (`QLabel*`, objectName `adminHeaderTitle`), `m_headerBar` (`QFrame*`, objectName `adminHeaderBar`); a title map.

- [ ] **Step 1: Declare header members**

In `qt-app/adminwindow.h` private section:

```cpp
    QFrame *m_headerBar = nullptr;
    QLabel *m_headerTitle = nullptr;
    void buildHeaderBar();
```

- [ ] **Step 2: Build the header bar (vertical wrapper)**

> **Layout correction (from review):** `stackedWidget` sits directly in `centralwidget`'s
> `horizontalLayout_main` (a **QHBoxLayout**: `sidebarFrame | stackedWidget`, confirmed at
> `adminwindow.ui:124-145`). A bare `insertWidget(indexOf(stackedWidget))` would put the
> header *beside* the content as a vertical strip, not above it (and `qobject_cast<QBoxLayout>`
> would still succeed, hiding the bug). So we move `stackedWidget` into a **new vertical
> container** (header on top, stack below) and drop that container into the HBox at the slot
> the stack used to occupy, preserving its stretch.

In `qt-app/adminwindow.cpp`, call `buildHeaderBar();` in the constructor after `setupUi`, and implement:

```cpp
void adminWindow::buildHeaderBar()
{
    QWidget *central = ui->stackedWidget->parentWidget();           // centralwidget
    auto *mainRow = qobject_cast<QHBoxLayout *>(central->layout()); // horizontalLayout_main
    if (!mainRow) return;                                           // unexpected: bail safely
    const int stackIdx = mainRow->indexOf(ui->stackedWidget);
    if (stackIdx < 0) return;
    const int stackStretch = mainRow->stretch(stackIdx);

    // --- the header bar itself ---
    m_headerBar = new QFrame;
    m_headerBar->setObjectName("adminHeaderBar");
    m_headerBar->setFixedHeight(56);
    auto *row = new QHBoxLayout(m_headerBar);
    row->setContentsMargins(20, 0, 20, 0);

    m_headerTitle = new QLabel("General", m_headerBar);
    m_headerTitle->setObjectName("adminHeaderTitle");
    row->addWidget(m_headerTitle);
    row->addStretch(1);

    auto *avatar = new QLabel("A", m_headerBar);
    avatar->setObjectName("adminHeaderAvatar");
    avatar->setFixedSize(32, 32);
    avatar->setAlignment(Qt::AlignCenter);
    auto *who = new QLabel("Administrator", m_headerBar);
    who->setObjectName("adminHeaderWho");
    row->addWidget(avatar);
    row->addSpacing(8);
    row->addWidget(who);

    // --- vertical wrapper: header on top, the existing stack below ---
    auto *contentCol = new QWidget(central);
    contentCol->setObjectName("adminContentColumn");
    auto *col = new QVBoxLayout(contentCol);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    mainRow->removeWidget(ui->stackedWidget);   // detach from the HBox
    col->addWidget(m_headerBar);
    col->addWidget(ui->stackedWidget, 1);       // addWidget reparents the stack into contentCol
    mainRow->insertWidget(stackIdx, contentCol, stackStretch);
}
```

Add includes `#include <QHBoxLayout>`, `#include <QVBoxLayout>`, `#include <QFrame>` if needed.

> Note: reparenting `stackedWidget` at runtime does not change `adminwindow.ui`, so
> `tst_responsive_ui`'s page/overlay assertions (which load the static `.ui`) are unaffected.
> The `searchOverlay` carve-out still holds — it stays parented to `studentSearchPage` inside
> the stack.

- [ ] **Step 3: Update the title from `setActiveSidebar`**

At the end of `setActiveSidebar`, set the title from the active button text:

```cpp
    if (m_headerTitle && activeBtn)
        m_headerTitle->setText(activeBtn->text().trimmed());
```

- [ ] **Step 4: Append header QSS**

Add to `qt-app/resources/wits.qss`:

```css
QFrame#adminHeaderBar {
    background-color: #FFFFFF;
    border-bottom: 1px solid #E2E8F0;
}
QLabel#adminHeaderTitle { font-size: 18px; font-weight: 700; color: #1E293B; }
QLabel#adminHeaderWho { color: #64748B; font-weight: 600; }
QLabel#adminHeaderAvatar {
    background-color: #DBEAFE; color: #1D4ED8;
    border-radius: 16px; font-weight: 700;
}
```

- [ ] **Step 5: Guard the header precondition in tests**

The runtime wrap depends on a specific `.ui` shape: the stack's parent layout must be a
`QHBoxLayout` that directly contains `stackedWidget` (so we can pull it out and re-insert the
wrapper at the same slot). Guard exactly that. In `qt-app/tests/tst_responsive_ui.cpp`, add
`void adminCentralRowHostsStack();` to the `private slots:` list, add `#include <QHBoxLayout>`
at the top, then implement:

```cpp
void TestResponsiveUi::adminCentralRowHostsStack()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *stack = w->findChild<QWidget *>("stackedWidget");
    QVERIFY2(stack && stack->parentWidget(), "stackedWidget/parent not found");
    auto *row = qobject_cast<QHBoxLayout *>(stack->parentWidget()->layout());
    QVERIFY2(row, "stack parent must use a QHBoxLayout (header-wrap relies on it)");
    QVERIFY2(row->indexOf(stack) >= 0,
             "stackedWidget must be a direct child of that QHBoxLayout");
}
```

This catches a future `.ui` change that would silently break the header placement (the exact
failure mode the original plan missed). If it ever fails, adjust `buildHeaderBar` to the new
shape rather than forcing the `.ui`.

- [ ] **Step 6: Build, run, verify**

Run: `cmake --build qt-app/build` then run `WITS` → admin. Switch tabs.
Expected: a white header bar shows the current tab title (updates on tab switch) + the "A / Administrator" chip; content sits below it; tables retain space.

- [ ] **Step 7: Run tests**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS.

- [ ] **Step 8: Commit** (via the `commit` skill)

Suggested message: `feat(admin): add dashboard header bar with tab title`

---

### Task 3.3: Card-wrap the page content and restyle tables

**Files:**
- Modify: `qt-app/resources/wits.qss` (page background, card frames, table polish)
- Modify: `qt-app/adminwindow.cpp` (apply `addShadow` to the page card frames; reuse the existing lambda at line 675)

**Interfaces:**
- Consumes: page frames `adminFrame`, `securityFrame`, `libraryFrame`, `settingsFrame`, `individualRegistrationBox`, `bulkRegistrationBox`, `chartsPreview`, `visualizatioWidget`, tables `bulkTable`, `studentSearchTable`, `visitorTable` (all pinned by `tst_responsive_ui`).

- [ ] **Step 1: Append page/card/table QSS**

Add to `qt-app/resources/wits.qss`:

```css
QStackedWidget > QWidget { background-color: #F1F5F9; }
QFrame#adminFrame, QFrame#securityFrame, QFrame#libraryFrame, QFrame#settingsFrame {
    background-color: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 16px;
}
QTableWidget#bulkTable, QTableWidget#studentSearchTable, QTableWidget#visitorTable {
    background: #FFFFFF;
    alternate-background-color: #F8FAFC;
    border: 1px solid #E2E8F0;
    border-radius: 12px;
}
```

- [ ] **Step 2: Apply shadows to the card frames**

In `qt-app/adminwindow.cpp`, the `addShadow` lambda (line 675) is already applied to `securityFrame`/`adminFrame`/`libraryFrame`/`settingsFrame` (lines 1033-1036). Extend its use to the other primary card containers as appropriate (e.g. `individualRegistrationBox`, `bulkRegistrationBox`, `chartsPreview`) — **one new effect per widget**, never reuse an instance:

```cpp
    addShadow(ui->individualRegistrationBox);
    addShadow(ui->bulkRegistrationBox);
    addShadow(ui->chartsPreview);
```

Place these alongside the existing `addShadow(...)` calls (after line 1036).

- [ ] **Step 3: Build, run, click through all tabs**

Run: `cmake --build qt-app/build` then run `WITS` → admin → every tab.
Expected: page backgrounds are slate-100, content sits in rounded white cards with soft shadows, tables have clean alternating rows and a slate header. Verify at fullscreen that card corners show no shadow clipping (see Risks; fall back to flat border if any card clips).

- [ ] **Step 4: Run the full suite**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS.

- [ ] **Step 5: Commit** (via the `commit` skill)

Suggested message: `feat(admin): card-wrap pages and restyle data tables`

---

### Phase 3 gate

- [ ] Build clean, tests green, app runs; sidebar states, header bar, and card layout verified across all 5 tabs at fullscreen.
- [ ] `/claude-review` (PHASE mode) on Phase 3; fix Critical/Important.

---

## Phase 4 — Polish extras

Icons, micro-interactions, empty-state polish, themed loading.

### Task 4.1: Add the icon set and wire icons into nav + key buttons

**Files:**
- Create: `qt-app/resources/icons/*.svg` (one per icon below)
- Modify: `qt-app/resources.qrc` (register icons)
- Modify: `qt-app/adminwindow.cpp` (set icons on the 5 nav buttons)
- Modify: `qt-app/mainwindow.cpp` (set icon on `loginBtn`)

**Interfaces:**
- Consumes: nav buttons, `loginBtn`.

- [ ] **Step 1: Create the SVG icons**

Create these files (Lucide-style, `currentColor` stroke so they inherit nicely). Example — `qt-app/resources/icons/settings.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#CBD5E1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
```

Create the remaining icons with the same header/stroke and these viewBox paths:
- `database.svg` — `<ellipse cx="12" cy="5" rx="9" ry="3"/><path d="M3 5v14a9 3 0 0 0 18 0V5"/><path d="M3 12a9 3 0 0 0 18 0"/>`
- `bar-chart.svg` — `<line x1="12" y1="20" x2="12" y2="10"/><line x1="18" y1="20" x2="18" y2="4"/><line x1="6" y1="20" x2="6" y2="16"/>`
- `search.svg` — `<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>`
- `map-pin.svg` — `<path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"/><circle cx="12" cy="10" r="3"/>`
- `log-in.svg` — (stroke `#FFFFFF` for the emerald login button) `<path d="M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4"/><polyline points="10 17 15 12 10 7"/><line x1="15" y1="12" x2="3" y2="12"/>`

- [ ] **Step 2: Register icons in the resource file**

In `qt-app/resources.qrc`, add inside the qresource:

```xml
        <file alias="icons/settings.svg">resources/icons/settings.svg</file>
        <file alias="icons/database.svg">resources/icons/database.svg</file>
        <file alias="icons/bar-chart.svg">resources/icons/bar-chart.svg</file>
        <file alias="icons/search.svg">resources/icons/search.svg</file>
        <file alias="icons/map-pin.svg">resources/icons/map-pin.svg</file>
        <file alias="icons/log-in.svg">resources/icons/log-in.svg</file>
```

- [ ] **Step 3: Set icons on the nav buttons**

In `qt-app/adminwindow.cpp` constructor (after the sidebar buttons exist), add:

```cpp
    ui->generalBtn->setIcon(QIcon(":/resources/icons/settings.svg"));
    ui->databaseBtn->setIcon(QIcon(":/resources/icons/database.svg"));
    ui->reportingBtn->setIcon(QIcon(":/resources/icons/bar-chart.svg"));
    ui->studentSearchBtn->setIcon(QIcon(":/resources/icons/search.svg"));
    ui->visitorBtn->setIcon(QIcon(":/resources/icons/map-pin.svg"));
    const QSize navIcon(20, 20);
    for (QPushButton *b : {ui->generalBtn, ui->databaseBtn, ui->reportingBtn,
                           ui->studentSearchBtn, ui->visitorBtn})
        b->setIconSize(navIcon);
```

Add `#include <QIcon>` if needed.

- [ ] **Step 4: Set the login icon**

In `qt-app/mainwindow.cpp` constructor:

```cpp
    ui->loginBtn->setIcon(QIcon(":/resources/icons/log-in.svg"));
    ui->loginBtn->setIconSize(QSize(20, 20));
```

- [ ] **Step 5: Build and run**

Run: `cmake --build qt-app/build` then run `WITS`.
Expected: nav items and the login button show their icons. If SVGs render blank, ensure the Qt `qsvgicon` plugin is deployed (standard with the Qt MinGW kit); as a fallback render at fixed size via `QIcon::pixmap`.

- [ ] **Step 6: Commit** (via the `commit` skill)

Suggested message: `feat(ui): add icon set for nav and primary actions`

---

### Task 4.2: Micro-interactions and empty-state polish

**Files:**
- Modify: `qt-app/resources/wits.qss` (focus/hover/pressed already partly present; add empty-state label style)
- Modify: `qt-app/mainwindow.cpp` (idle empty-state text/icon on the spotlight)

**Interfaces:**
- Consumes: spotlight labels from Task 2.2; existing "no records" placeholders.

- [ ] **Step 1: Add empty-state QSS**

Add to `qt-app/resources/wits.qss`:

```css
QLabel#emptyState {
    color: #94A3B8;
    font-size: 15px;
    font-style: italic;
}
```

- [ ] **Step 2: Give the idle spotlight an empty-state look**

In `updateSpotlight` (Task 2.2), when `name` is empty set the avatar/name to the waiting state (already handled) and ensure the caption values show "—". Confirm the idle card reads: avatar "?", "Waiting for log in…", and a hint. Optionally add a hint label `objectName("emptyState")` under the name in `buildSpotlightCard`:

```cpp
    auto *hint = new QLabel("Scan your ID or type your ID number.", m_spotlightCard);
    hint->setObjectName("emptyState");
    outer->addWidget(hint);
```

- [ ] **Step 3: Build, run, verify**

Run: `cmake --build qt-app/build` then run `WITS`. Observe idle state and button hover/pressed/focus feedback (from the global QSS).
Expected: idle spotlight shows the hint; buttons/inputs visibly respond to hover/press/focus.

- [ ] **Step 4: Run tests**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS.

- [ ] **Step 5: Commit** (via the `commit` skill)

Suggested message: `feat(ui): add micro-interactions and empty-state polish`

---

### Task 4.3: Theme the loading states

**Files:**
- Modify: `qt-app/busyindicator.cpp` (source colors from theme)
- Modify: `qt-app/adminwindow.cpp` (`showSearchOverlay`/`hideSearchOverlay` overlay styling)

**Interfaces:**
- Consumes: `BusyIndicator`, `showSearchOverlay()`, `hideSearchOverlay()`, `overlayEffect`.

- [ ] **Step 1: Theme the busy indicator**

In `qt-app/busyindicator.cpp`, replace any hardcoded spinner/overlay colors with `WitsTheme::Color::*` (add `#include "theme.h"`). Use `Secondary` (`#3B82F6`) for the active arc and a translucent slate for the track. Keep the existing animation/logic.

- [ ] **Step 2: Theme the search overlay**

In `qt-app/adminwindow.cpp` `showSearchOverlay`, ensure the overlay backdrop uses a theme-consistent translucent fill and the overlay text uses `WitsTheme::Color::MutedText`. Do not change the overlay's parenting (it must stay a free-floating sibling of `studentSearchPage` — `tst_responsive_ui::searchOverlayPreserved`).

- [ ] **Step 3: Build, run, trigger a search**

Run: `cmake --build qt-app/build` then run `WITS` → admin → Search → run a query that shows the overlay.
Expected: spinner/overlay match the new palette; overlay still covers the page; results display.

- [ ] **Step 4: Run the full suite**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS (especially `searchOverlayPreserved`).

- [ ] **Step 5: Commit** (via the `commit` skill)

Suggested message: `feat(ui): theme busy indicator and search overlay`

---

### Phase 4 gate / Branch finish

- [ ] Full build clean, `ctest` all green, app runs end-to-end (student login, admin-key login, RFID scan, guest toggle, all 5 admin tabs, report PDF/Excel export, student search, settings save/load) — verify each by running the app, not just the build.
- [ ] Run `/claude-review` (BRANCH mode) for the final pass; fix Critical/Important.
- [ ] Use `superpowers:finishing-a-development-branch`, then the `create-pr` skill (base branch `master` per project memory — pass `--base master`).

---

## Self-Review notes (coverage map)

- Spec §Architecture (central QSS, theme helper, main.cpp) → Tasks 1.1–1.3.
- Spec §Resource wiring (keystone) → Task 1.1 (+ asset additions in 2.1, 4.1).
- Spec §Inline-style migration (mainwindow, adminwindow, attachfilesdialog, runtime-state, busyindicator) → Tasks 1.4, 1.5, 2.3, 4.3.
- Spec §Color system → Global Constraints + theme constants (1.2) + per-phase QSS.
- Spec §Kiosk (sidebar, spotlight, table, toast, empty state) → Tasks 1.4, 2.2, 2.3, 4.2.
- Spec §Admin (sidebar states, header bar, card pages, tables) → Tasks 3.1–3.3.
- Spec §Extras (icons, micro-interactions, toast/empty states, themed loading) → Tasks 4.1–4.3.
- Spec §Testing (loadStyleSheet test + resource caveat; run app) → Task 1.2 + per-task run/build/ctest steps.
- Spec §Phasing → the four phases above with PHASE/BRANCH review gates.
- Spec §Risks (inline override order, shadow-as-code one-per-widget, corner artifacts, fullscreen scaling) → Global Constraints + Task 2.2/3.3 notes.
```
