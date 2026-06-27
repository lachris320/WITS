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
- Create: `qt-app/resources/default_student.svg`
- Modify: `qt-app/resources.qrc` (register it)
- Modify: `qt-app/mainwindow.cpp:45-52` (load via `QIcon` so SVG renders)

**Interfaces:**
- Consumes: `ui->studentPhoto` (existing `QLabel`).

- [ ] **Step 1: Create the placeholder SVG**

Create `qt-app/resources/default_student.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" width="200" height="200">
  <rect width="200" height="200" rx="16" fill="#E2E8F0"/>
  <circle cx="100" cy="78" r="38" fill="#94A3B8"/>
  <path d="M40 170c0-33 27-52 60-52s60 19 60 52z" fill="#94A3B8"/>
</svg>
```

- [ ] **Step 2: Register it in the resource file**

In `qt-app/resources.qrc`, add inside `<qresource prefix="/resources">`:

```xml
        <file alias="default_student.svg">resources/default_student.svg</file>
```

- [ ] **Step 3: Load it via QIcon**

In `qt-app/mainwindow.cpp`, replace the placeholder load (lines 45-52, currently `QPixmap placeholder(":/resources/default_student.png"); ...`) with:

```cpp
    QIcon placeholderIcon(":/resources/default_student.svg");
    ui->studentPhoto->setPixmap(placeholderIcon.pixmap(
        ui->studentPhoto->size()));
    ui->studentPhoto->setScaledContents(false);
    ui->studentPhoto->setAlignment(Qt::AlignCenter);
```

Add `#include <QIcon>` to the includes if not present.

- [ ] **Step 4: Build and run**

Run: `cmake --build qt-app/build` then run `WITS`.
Expected: the student-photo area shows the grey silhouette placeholder (previously blank). Logging in still swaps in the fetched photo.

- [ ] **Step 5: Commit** (via the `commit` skill)

Suggested message: `fix(ui): add student-photo placeholder and resolve dangling resource`

---

### Task 2.2: Add the active-profile spotlight card

**Files:**
- Modify: `qt-app/mainwindow.h` (add member labels + a build helper)
- Modify: `qt-app/mainwindow.cpp` (build the card in the constructor; populate in `displayStudent`)
- Modify: `qt-app/resources/wits.qss` (card styling)
- Modify: `qt-app/tests/tst_responsive_ui.cpp` (assert the card exists)

**Interfaces:**
- Consumes: `ui->frame_2` (right panel), the `student` JSON in `displayStudent` (keys `name`, `course`, `year_level`, `department`, `time_date`), `ui->studentPhoto`.
- Produces: `MainWindow::m_spotlightCard` (`QFrame*`, objectName `spotlightCard`) and labels `m_spotName`, `m_spotCourse`, `m_spotYear`, `m_spotDept`, `m_spotTime`, `m_spotAvatar`; method `void buildSpotlightCard()` and `void updateSpotlight(const QJsonObject &student)`.

- [ ] **Step 1: Add the failing structural test**

In `qt-app/tests/tst_responsive_ui.cpp`, add a slot and implementation:

```cpp
// in the private slots list:
void mainWindowHasSpotlightCard();

// implementation:
void TestResponsiveUi::mainWindowHasSpotlightCard()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY2(w, "failed to load mainwindow.ui");
    // The spotlight card is built in code, so the .ui must expose a host
    // container the code can attach to: the right panel frame_2 with a layout.
    QWidget *panel = w->findChild<QWidget *>("frame_2");
    QVERIFY2(panel, "frame_2 not found");
    QVERIFY2(panel->layout() != nullptr,
             "frame_2 needs a layout to host the spotlight card");
}
```

> Note: the card itself is created at runtime (not in the `.ui`), so this test guards the *host* (`frame_2` has a layout). A widget-level test that the card populates is added in Step 6.

- [ ] **Step 2: Run it to verify current state**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_responsive_ui --output-on-failure`
Expected: if `frame_2` already has a layout, this PASSES immediately (acceptable — it documents the dependency). If it FAILS, add a layout to `frame_2` in `mainwindow.ui` (a `QVBoxLayout`) without removing existing children, then re-run to green.

- [ ] **Step 3: Declare the card members and helpers in the header**

In `qt-app/mainwindow.h`, add to the private section:

```cpp
    QFrame *m_spotlightCard = nullptr;
    QLabel *m_spotAvatar = nullptr;
    QLabel *m_spotName = nullptr;
    QLabel *m_spotCourse = nullptr;
    QLabel *m_spotYear = nullptr;
    QLabel *m_spotDept = nullptr;
    QLabel *m_spotTime = nullptr;
    void buildSpotlightCard();
    void updateSpotlight(const QJsonObject &student);
```

Add `#include <QFrame>` and `#include <QGridLayout>` to the header includes.

- [ ] **Step 4: Build the card in the constructor**

In `qt-app/mainwindow.cpp`, add a call `buildSpotlightCard();` near the end of the constructor (after the existing UI setup), and implement:

```cpp
void MainWindow::buildSpotlightCard()
{
    m_spotlightCard = new QFrame(ui->frame_2);
    m_spotlightCard->setObjectName("spotlightCard");

    auto *outer = new QVBoxLayout(m_spotlightCard);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(14);

    auto *headerRow = new QHBoxLayout();
    m_spotAvatar = new QLabel("?", m_spotlightCard);
    m_spotAvatar->setObjectName("spotAvatar");
    m_spotAvatar->setFixedSize(64, 64);
    m_spotAvatar->setAlignment(Qt::AlignCenter);
    m_spotName = new QLabel("Waiting for log in…", m_spotlightCard);
    m_spotName->setObjectName("spotName");
    headerRow->addWidget(m_spotAvatar);
    headerRow->addWidget(m_spotName, 1);
    outer->addLayout(headerRow);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(40);
    grid->setVerticalSpacing(10);
    auto makeCaption = [&](const QString &t) {
        auto *l = new QLabel(t, m_spotlightCard);
        l->setObjectName("spotCaption");
        return l;
    };
    m_spotCourse = new QLabel("—", m_spotlightCard); m_spotCourse->setObjectName("spotValue");
    m_spotYear   = new QLabel("—", m_spotlightCard); m_spotYear->setObjectName("spotValue");
    m_spotDept   = new QLabel("—", m_spotlightCard); m_spotDept->setObjectName("spotValue");
    m_spotTime   = new QLabel("—", m_spotlightCard); m_spotTime->setObjectName("spotValueAccent");
    grid->addWidget(makeCaption("COURSE"),     0, 0); grid->addWidget(m_spotCourse, 1, 0);
    grid->addWidget(makeCaption("YEAR LEVEL"), 0, 1); grid->addWidget(m_spotYear,   1, 1);
    grid->addWidget(makeCaption("DEPARTMENT"), 2, 0); grid->addWidget(m_spotDept,   3, 0);
    grid->addWidget(makeCaption("TIME LOGGED"),2, 1); grid->addWidget(m_spotTime,   3, 1);
    outer->addLayout(grid);

    // Insert the card at the top of the right panel's layout.
    if (auto *panelLayout = qobject_cast<QBoxLayout *>(ui->frame_2->layout()))
        panelLayout->insertWidget(0, m_spotlightCard);

    // Soft shadow (code, not QSS — one effect instance for this widget).
    auto *shadow = new QGraphicsDropShadowEffect(m_spotlightCard);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 40));
    m_spotlightCard->setGraphicsEffect(shadow);
}
```

Add includes as needed: `#include <QVBoxLayout>`, `#include <QHBoxLayout>`, `#include <QGridLayout>`. (`QGraphicsDropShadowEffect`, `QLabel` already included.)

- [ ] **Step 5: Populate the card on login**

In `qt-app/mainwindow.cpp`, implement `updateSpotlight` and call it from `displayStudent` (before/after the existing `recentLogins.prepend(student)`):

```cpp
void MainWindow::updateSpotlight(const QJsonObject &student)
{
    const QString name = student["name"].toString();
    m_spotName->setText(name.isEmpty() ? "Waiting for log in…" : name);
    m_spotAvatar->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
    m_spotCourse->setText(student["course"].toString());
    m_spotYear->setText(student["year_level"].toString());
    m_spotDept->setText(student["department"].toString());
    m_spotTime->setText(student["time_date"].toString());

    // Reuse the existing fade pattern for a subtle reveal.
    auto *fade = new QGraphicsOpacityEffect(m_spotlightCard);
    m_spotlightCard->setGraphicsEffect(fade);
    auto *anim = new QPropertyAnimation(fade, "opacity");
    anim->setDuration(400);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
```

In `displayStudent`, add `updateSpotlight(student);` as the first line.

> Note: setting an opacity effect replaces the drop-shadow effect on reveal (a widget holds one effect). This is acceptable — the shadow is cosmetic and the card keeps its border. If both are desired, drop the per-reveal opacity animation and keep only the shadow.

- [ ] **Step 6: Append spotlight QSS**

Add to `qt-app/resources/wits.qss`:

```css
QFrame#spotlightCard {
    background-color: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 16px;
}
QLabel#spotAvatar {
    background-color: #6366F1;
    color: #FFFFFF;
    border-radius: 16px;
    font-size: 28px;
    font-weight: 700;
}
QLabel#spotName { font-size: 28px; font-weight: 800; color: #1E293B; }
QLabel#spotCaption { font-size: 11px; font-weight: 700; color: #94A3B8; }
QLabel#spotValue { font-size: 15px; color: #1E293B; }
QLabel#spotValueAccent { font-size: 15px; font-weight: 700; color: #10B981; }
```

- [ ] **Step 7: Build, run, verify**

Run: `cmake --build qt-app/build` then run `WITS`. Log in / scan a test student.
Expected: spotlight card shows the avatar initial, big name, and the 4 fields; fades in on login; idle state reads "Waiting for log in…".

- [ ] **Step 8: Run tests**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: all PASS including `mainWindowHasSpotlightCard`.

- [ ] **Step 9: Commit** (via the `commit` skill)

Suggested message: `feat(kiosk): add active-profile spotlight card`

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

- [ ] **Step 2: Drive the active state from `setActiveSidebar`**

In `qt-app/adminwindow.cpp`, in `setActiveSidebar` (line 339), set a dynamic property and repolish instead of (or in addition to) any inline style, so the QSS `[active="true"]` selector applies:

```cpp
void adminWindow::setActiveSidebar(QPushButton* activeBtn) {
    const QList<QPushButton*> navButtons = {
        ui->generalBtn, ui->databaseBtn, ui->reportingBtn,
        ui->studentSearchBtn, ui->visitorBtn
    };
    for (QPushButton *btn : navButtons) {
        btn->setProperty("active", btn == activeBtn);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}
```

Add `#include <QStyle>` if not present. Remove any leftover inline per-button style strings in this function.

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

- [ ] **Step 2: Build the header bar**

In `qt-app/adminwindow.cpp`, call `buildHeaderBar();` in the constructor after `setupUi`, and implement (attach above `stackedWidget` in its parent layout — confirm the parent/layout in `adminwindow.ui`):

```cpp
void adminWindow::buildHeaderBar()
{
    QWidget *host = ui->stackedWidget->parentWidget();
    auto *hostLayout = qobject_cast<QBoxLayout *>(host->layout());
    if (!hostLayout) return; // host has no box layout; skip rather than crash

    m_headerBar = new QFrame(host);
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

    const int idx = hostLayout->indexOf(ui->stackedWidget);
    hostLayout->insertWidget(idx < 0 ? 0 : idx, m_headerBar);
}
```

Add includes `#include <QHBoxLayout>`, `#include <QFrame>` if needed.

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

- [ ] **Step 5: Guard the header host in tests**

In `qt-app/tests/tst_responsive_ui.cpp`, add a new slot `adminStackHostHasBoxLayout` to the `private slots:` list and implement it (this guards the layout the header bar attaches to):

```cpp
// add `void adminStackHostHasBoxLayout();` to the private slots list, then:
void TestResponsiveUi::adminStackHostHasBoxLayout()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *stack = w->findChild<QWidget *>("stackedWidget");
    QVERIFY2(stack, "stackedWidget not found");
    QVERIFY2(stack->parentWidget() && stack->parentWidget()->layout(),
             "stackedWidget parent must have a layout to host the header bar");
}
```

If this fails, add a box layout to the stack's parent in `adminwindow.ui` without removing children.

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
