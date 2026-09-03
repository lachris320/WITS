# Report Orientation Picker (Portrait/Landscape) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. **Line numbers in this plan are current-ish snapshots and WILL drift** — locate every edit by the surrounding code/context quoted here, not by the line number alone.

**Goal:** Add a Portrait / Landscape combo to the Reporting screen's export bar so the librarian can choose the page orientation of the **PDF export** and the **Print** output. Excel is unaffected; the choice is per-session (default Portrait, not persisted); the renderer needs **no code change** because it already derives its whole layout from the paint device's `pageLayout()`. Closes the design gap identified in the orientation spec: today `exportPdf` and `printReport` never set an orientation, so both outputs are always Portrait with no escape hatch from the Quick app.

**Architecture:** A `Q_PROPERTY QString orientation` (default `"Portrait"`) plus a `Q_PROPERTY QStringList orientations` (`{"Portrait","Landscape"}`) on `ReportingViewModel`, written through a validating `Q_INVOKABLE setOrientation` that rejects anything outside the two legal values (stronger than the `palette`/`chartType` same-value-only guard). A pure static mapping helper `pageOrientation(const QString&) -> QPageLayout::Orientation` converts the stored string to the Qt enum, defensively degrading any non-`"Landscape"` input to `QPageLayout::Portrait`. Two call sites apply it: `exportPdf` (one line after `setPageSize`) and `printReport` (one line between the `QPrinter` constructor and the `QPrintDialog` constructor — orientation MUST be set before the dialog is built so the dialog opens already showing it). A third `LComboBox` (`orientationCombo`) is added to `ReportingScreen.qml`'s `exportRow`, mirroring `chartTypeCombo` exactly. `ReportRenderer::paintReport` is untouched — it already reads `device->pageLayout().paintRectPixels(resolution)` and derives every dimension from that, and the charts fit via `drawFullscreenChart`'s `Qt::KeepAspectRatio` scale-into-`targetArea`, not a `usableWidth`-keyed raster (`chartImageSize` `Q_UNUSED`s its `usableWidth` parameter — confirmed by direct read, §Grounding below).

```
LComboBox "Orientation" (ReportingScreen.qml exportRow)
     │  onSelected: screen.vm.setOrientation(v)
     ▼
ReportingViewModel::setOrientation(v)
     │  reject if v == m_orientation OR !orientations().contains(v)  (decision 3)
     ▼
m_orientation : QString ("Portrait" default, per-session, NOT persisted)
     │
     ├──► exportPdf(): writer.setPageOrientation(pageOrientation(m_orientation))
     │        — inserted right after writer.setPageSize(...), BEFORE renderToDevice
     │
     └──► printReport(): printer.setPageOrientation(pageOrientation(m_orientation))
              — inserted BETWEEN "QPrinter printer(...)" and "QPrintDialog dlg(&printer)"
                so the dialog opens already showing the chosen orientation (decision 5)

pageOrientation(QString) -> QPageLayout::Orientation   [pure static helper]
  "Landscape" -> QPageLayout::Landscape;  everything else (incl. "Portrait","",garbage) -> QPageLayout::Portrait

ReportRenderer::paintReport(device, ...)
     reads device->pageLayout().paintRectPixels(resolution)  ← UNCHANGED, zero orientation awareness
```

**Tech Stack:** Qt 6.11.1 / C++17 / CMake + Ninja; QtCharts, QXlsx (vendored), Qt Test + qmltest; MVVM (ViewModel is the only QML-facing C++).

**Spec (source of truth):** `docs/superpowers/specs/2026-09-02-loams2-report-orientation-design.md` (owner-approved via brainstorming, claude-review APPROVED). Every §-decision is binding; the five locked decisions in §4 are the tie-breaker:
1. Outputs affected: PDF + Print only, Excel untouched.
2. Persistence: per-session, default `"Portrait"`, no `AppSettings` key.
3. `setOrientation()` rejects invalid/blank input outright (stronger than `setPalette`/`setChartType`'s same-value-only guard): `if (v == m_orientation || !orientations().contains(v)) return;`
4. `pageOrientation()` stays defensively correct independent of caller pre-validation.
5. `printer.setPageOrientation(...)` must execute BEFORE `QPrintDialog dlg(&printer)` is constructed.

---

## Global Constraints

**Build (PowerShell; Qt tools are NOT on PATH; use a SHORT external build dir to avoid the Windows MAX_PATH overflow on the QML-module autogen path):**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S "<worktree>\qt-app" -B C:\b\loams-orient -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:\b\loams-orient
ctest --test-dir C:\b\loams-orient --output-on-failure
```

- **Baseline once at branch start:** configure + build + `ctest`, and record the green test count. Do **NOT** hard-code the number — capture whatever the baseline reports and require it to stay green (plus the new cases) at every task boundary.
- **`tst_settingsviewmodel` is a known flake** under full-suite parallel load. If it fails, re-run it alone (`ctest --test-dir C:\b\loams-orient -R tst_settingsviewmodel --output-on-failure`) before treating it as real.
- **No CMakeLists changes are expected** — this slice adds no files and no targets, only extends existing signatures/bodies in `ReportingViewModel.{h,cpp}`, `ReportingScreen.qml`, `tst_reportingviewmodel.cpp`, and `tst_qml_admin.qml`, all of which the current test targets already compile/link. **Verify, don't assume:** confirm `tst_reportingviewmodel` still links `witsquickmodule` → `witscore` before Task 1's first build. If a build fails for an undefined-reference reason, STOP and re-check — do not "fix" it by editing CMake unless a genuine missing-source is proven.
- **Close any running `WITSQuick.exe` before rebuilding** — a live process locks the binary and breaks relink.
- **Both `WITS.exe` and `WITSQuick.exe` must link at every task boundary.** `adminwindow.cpp` already hardcodes `QPageLayout::Landscape` for both its PDF (`:1705`) and Print (`:1741`) paths, references no `ReportingViewModel` symbol, and needs no edit — but confirm it still links after every task.
- **Ignore** the "LF will be replaced by CRLF" warnings and the pre-existing QXlsx "GuiPrivate" deprecation warnings — they are not introduced by this slice.
- **This plan's branch was cut from `master` BEFORE PR #49 (the library-hours-window "When?" analytics slice) merged**, so it does **not** contain that slice's changes to `ReportingViewModel.{h,cpp}` (the `m_openHour`/`m_closeHour` cache, the windowed `buildHourlyBars`/`buildTimeExport`, the `windowedHourCaption()` helper). **Build this plan off the post-#49-merge `master`** — the grounding snippets below were read from a worktree that already has #49 merged, so they show the post-#49 file shape (e.g. `private:` now also declares `windowedHourCaption() const` next to the other static helpers, and `m_openHour`/`m_closeHour` sit next to `m_timeAnalytics`). Line numbers WILL differ between the two states — locate every edit by the quoted surrounding code, never by a bare line number. This feature touches a **disjoint** region of `ReportingViewModel` (the `palette`/`chartType`/export-bar area, `exportPdf`, `printReport`) from what #49 touches (the "When?" section, `onTimeAnalyticsReady`, `buildTimeExport`'s hour loop), so the two compose cleanly regardless of merge order.
- **Commit each task** via direct `git add`/`git commit` (per this repo's convention on this branch — do **NOT** `git add -A`, stage only the task's files). Conventional Commit scope `reporting`. **NO Claude/Anthropic co-author trailer** — the branch is trailer-free, matching the sibling library-hours-window branch's commits (`afdf93c`, `364759d`, `da5ba57`, `ac7422d`, `1b11cad` all lack one).
- **Both `WITS.exe` and `WITSQuick.exe` must link at every task boundary** (repeated for emphasis — this is the release-blocking check the manual-smoke task exists to catch what ctest cannot).
- **No real student PII** in any test/fixture — synthetic data only.

---

## Grounding — the REAL current code (read directly; every anchor below is a verbatim quote)

### `qt-app/quick/viewmodels/ReportingViewModel.h`

The `Q_PROPERTY` block holding the `palette`/`chartType` pair this feature's properties are inserted next to:

```cpp
    Q_PROPERTY(QStringList palettes READ palettes CONSTANT)
    Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(QStringList chartTypes READ chartTypes CONSTANT)
    Q_PROPERTY(QString chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
```

The inline readers:

```cpp
    QStringList palettes() const { return { QStringLiteral("Default"), QStringLiteral("Blue"),
                                            QStringLiteral("Green"), QStringLiteral("Red") }; }
    QString palette() const { return m_palette; }
    QStringList chartTypes() const { return { QStringLiteral("Bar"), QStringLiteral("Pie") }; }
    QString chartType() const { return m_chartType; }
```

The `Q_INVOKABLE` setter declarations:

```cpp
    Q_INVOKABLE void setPalette(const QString &p);
    Q_INVOKABLE void setChartType(const QString &c);
```

The signals block (the two neighbors to insert `orientationChanged()` next to):

```cpp
    void paletteChanged();
    void chartTypeChanged();
```

The private static "presentation shaping" section — **post-#49-merge this already includes `windowedHourCaption()`**. (`pageOrientation` is NOT added here — it is a directly-tested pure static and goes in the **public** seams group instead; see Task 1 Step 3. This block is quoted only to show the surrounding shape and the #49 drift.)

```cpp
    static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly,
                                                 int openHour, int closeHour);     // [open,close], every label
    static QList<BarsModel::Bar> buildWeekdayBars(const QList<int> &weekdayMonFirst); // 7, Mon-first
    static QString hourTick(int hour);          // 0..23 -> "12A","3A",...,"9P"
    static QString formatHourRange(int hour);   // 14 -> "2–3 PM"
    static QString weekdayName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Monday".."Sunday"
    static QString weekdayShortName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Mon".."Sun"
    QString windowedHourCaption() const;   // decision-5 gate: peak label, empty when no in-window peak
```

The `m_palette`/`m_chartType` members:

```cpp
    QString m_palette = QStringLiteral("Default");
    QString m_chartType = QStringLiteral("Bar");
```

The header's own includes (`ReportingViewModel.h:1-18`) — confirmed **no** `QPageLayout`/`QPageSize`/`QPrinter` include today:

```cpp
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqml.h>                 // QML_ELEMENT — AdminScreen instantiates this type
#include "BarsModel.h"
#include "RankingModel.h"
#include "ReportRowsModel.h"
#include "reportanalytics.h"   // ReportAnalytics — cached from applyResult, reused by the export path
#include "reportdata.h"            // DateRange — return type of semesterWindow()
#include "timeanalytics.h"        // TimeAnalytics — computed in applyResult's sibling path
```

`pageOrientation`'s return type (`QPageLayout::Orientation`) appears in this header's own declaration, so `#include <QPageLayout>` must live HERE, not stay `.cpp`-only.

### `qt-app/quick/viewmodels/ReportingViewModel.cpp`

The setter bodies to mirror (and exceed — decision 3 adds the `orientations().contains(v)` check these lack):

```cpp
void ReportingViewModel::setPalette(const QString &p)
{
    if (m_palette == p) return;
    m_palette = p; emit paletteChanged();
}
void ReportingViewModel::setChartType(const QString &c)
{
    if (m_chartType == c) return;
    m_chartType = c; emit chartTypeChanged();
}
```

`exportPdf` — the exact insertion point (unchanged shape from the spec; #49 did not touch this method):

```cpp
void ReportingViewModel::exportPdf(const QUrl &fileUrl)
{
    QString path;
    if (!beginFileExport(fileUrl, &path))
        return;
    // Defer one turn so the busy overlay paints before the blocking render.
    QMetaObject::invokeMethod(this, [this, path]() {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
        if (ok)
            setExportStatus(tr("Saved %1").arg(QFileInfo(path).fileName()));
        else
            setExportError(tr("Couldn't write %1 — choose a different location.").arg(QFileInfo(path).fileName()));
        setExporting(false);
    }, Qt::QueuedConnection);
}
```

`printReport` — the exact insertion point (unchanged shape from the spec; #49 did not touch this method):

```cpp
void ReportingViewModel::printReport()
{
    if (m_exporting) return;
    if (m_exportRows.isEmpty()) {
        setExportError(tr("No data to export. Adjust the filters and generate a report with results."));
        return;
    }
    // Opening the dialog is NOT "exporting" — the normal UI stays live.
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer);
    if (dlg.exec() != QDialog::Accepted)
        return;   // cancelled -> no-op, no error, no busy state
    setExportError(QString());
    setExporting(true);   // now rendering begins
    const bool ok = renderToDevice(&printer, printer.resolution());
    if (ok)
        setExportStatus(tr("Sent to printer"));
    else
        setExportError(tr("Couldn't print the report."));
    setExporting(false);
}
```

A good anchor for placing the `pageOrientation` static definition — the `weekdayShortName`/`hourTick`/`formatHourRange`/`weekdayName` group of static formatting helpers (any one of these serves as the "near the other statics" anchor the header comment calls for).

The `.cpp`'s existing includes (confirms `QPageSize`/`QPdfWriter`/`QPrintDialog`/`QPrinter` are already `.cpp`-side; `QPageLayout` is pulled in transitively via `QPrinter`/`QPdfWriter` today but the header needs its own explicit include per the point above):

```cpp
#include "ReportingViewModel.h"

#include <QFileInfo>
#include <QMap>
#include <QNetworkAccessManager>
#include <QPageSize>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QUrl>
#include <algorithm>
```

### `qt-app/quick/qml/admin/ReportingScreen.qml`

The `exportRow` to extend — `paletteCombo` and `chartTypeCombo`, the filling spacer, then the three buttons:

```qml
                RowLayout {
                    id: exportRow
                    Layout.fillWidth: true
                    spacing: Theme.spacing.md

                    LComboBox {
                        id: paletteCombo
                        objectName: "paletteCombo"
                        accessibleName: qsTr("Report palette")
                        Layout.preferredWidth: 150
                        model: screen.vm ? screen.vm.palettes : []
                        placeholder: qsTr("Palette")
                        currentValue: screen.vm ? screen.vm.palette : "Default"
                        onSelected: function(v) { if (screen.vm) screen.vm.setPalette(v); }
                    }
                    LComboBox {
                        id: chartTypeCombo
                        objectName: "chartTypeCombo"
                        accessibleName: qsTr("Chart type")
                        Layout.preferredWidth: 150
                        model: screen.vm ? screen.vm.chartTypes : []
                        placeholder: qsTr("Chart")
                        currentValue: screen.vm ? screen.vm.chartType : "Bar"
                        onSelected: function(v) { if (screen.vm) screen.vm.setChartType(v); }
                    }

                    Item { Layout.fillWidth: true }   // spacer

                    // Design OS §4.5: disabled controls expose WHY via Accessible.description.
                    LButton {
                        objectName: "exportPdfButton"
                        ...
```

The new `orientationCombo` goes between `chartTypeCombo`'s closing `}` and the `Item { Layout.fillWidth: true }` spacer.

### `qt-app/quick/tests/tst_reportingviewmodel.cpp`

The `private slots:` declaration block starts at the top of the `TestReportingViewModel` class:

```cpp
class TestReportingViewModel : public QObject
{
    Q_OBJECT
private slots:
    void buildFiltersDaySendsStringTypeAndRange();
    ...
    void canExportTruthTable();
    void paletteAndChartTypeSettersEmit();
    void applyResultStoresNormalizedExportRows();
    ...
```

New slot declarations are added after `void paletteAndChartTypeSettersEmit();`. **Reminder (spec §6.1): a QtTest slot without its `private slots:` declaration is silently never run — every new C++ test method below needs BOTH the declaration here AND the definition near `QTEST_MAIN`.**

The template test to mirror:

```cpp
void TestReportingViewModel::paletteAndChartTypeSettersEmit()
{
    ReportingViewModel vm;
    QCOMPARE(vm.palette(), QStringLiteral("Default"));
    QCOMPARE(vm.chartType(), QStringLiteral("Bar"));
    QSignalSpy pSpy(&vm, &ReportingViewModel::paletteChanged);
    QSignalSpy cSpy(&vm, &ReportingViewModel::chartTypeChanged);
    vm.setPalette("Blue");
    vm.setChartType("Pie");
    QCOMPARE(vm.palette(), QStringLiteral("Blue"));
    QCOMPARE(vm.chartType(), QStringLiteral("Pie"));
    QCOMPARE(pSpy.count(), 1);
    QCOMPARE(cSpy.count(), 1);
    QCOMPARE(vm.palettes().size(), 4);
    QCOMPARE(vm.chartTypes().size(), 2);
}
```

New test bodies go right before `QTEST_MAIN(TestReportingViewModel)` at the bottom of the file. `#include <QPageLayout>` must be added to this file's includes (`#include <QtTest>` / `#include <QDate>` / ... block near the top) so the `pageOrientation()` mapping assertions can name `QPageLayout::Portrait`/`QPageLayout::Landscape`.

### `qt-app/quick/tests/tst_qml_admin.qml`

The `reportingStub` `QtObject` carries `palette`/`chartType` analogues to extend the same way:

```qml
        property var palettes: ["Default", "Blue", "Green", "Red"]
        property string palette: "Default"
        property var chartTypes: ["Bar", "Pie"]
        property string chartType: "Bar"
        ...
        function setPalette(p) { palette = p }
        function setChartType(c) { chartType = c }
```

The template test:

```qml
        function test_paletteComboHasAccessibleNameAndWrites() {
            var combo = findChild(reporting, "paletteCombo");
            verify(combo);
            compare(combo.accessibleName, "Report palette");
            combo.selectValue("Blue");
            compare(reportingStub.palette, "Blue");
            reportingStub.palette = "Default";
        }
```

`selectValue(v)` is confirmed present on `LComboBox` (`qt-app/quick/qml/components/LComboBox.qml:31`) and used by many other combo tests in this same file — no risk it's missing.

### `qt-app/core/reportrenderer.cpp` — confirmed NO change needed

```cpp
QRectF pageRect = device->pageLayout().paintRectPixels(resolution);
int pageWidth  = pageRect.width();
int pageHeight = pageRect.height();
int margin     = pageWidth * 0.03;
int usableWidth  = pageWidth - 2*margin;
int usableHeight = pageHeight - 2*margin;
```

`chartImageSize` — confirmed by direct read that it `Q_UNUSED`s its `usableWidth` parameter (the neighboring in-file comment at the call sites, "Chart raster sizes key off usableWidth via chartImageSize," is itself misleading — do not trust it):

```cpp
QSize ReportRenderer::chartImageSize(int usableWidth, bool square) {
    Q_UNUSED(usableWidth);
    QSize base = square ? QSize(1000, 1000) : QSize(1600, 1000);
    if (const QScreen *scr = QGuiApplication::primaryScreen()) {
        const QSize avail = scr->availableSize() * 0.85;
        if (avail.width() >= 320 && avail.height() >= 240
            && (base.width() > avail.width() || base.height() > avail.height())) {
            base.scale(avail, Qt::KeepAspectRatio);
        }
    }
    return base;
}
```

`drawFullscreenChart` is what actually fits a chart raster to the page — `Qt::KeepAspectRatio` scale into `targetArea` (`margin, y, usableWidth, pageHeight - y - margin - bottomReserve`), then centered:

```cpp
QRect targetArea(margin, y, usableWidth, pageHeight - y - margin - bottomReserve);
QSize scaledSize = img.size().scaled(targetArea.size(), Qt::KeepAspectRatio);
```

A wider-than-tall Landscape `pageRect` therefore yields a wider `usableWidth`/`targetArea`, and every chart/table/tile fraction-of-width call reflows automatically. **No function in `reportrenderer.{h,cpp}` changes for this feature.**

### `qt-app/adminwindow.cpp` — confirmed UNCHANGED

```cpp
    pdf.setPageOrientation(QPageLayout::Landscape);       // :1705
    ...
    printer.setPageOrientation(QPageLayout::Landscape);   // :1741
```

Grepped for `ReportingViewModel` in this file: **zero matches**. `adminwindow.cpp`'s own export/print methods already hardcode Landscape unconditionally and never reference `ReportingViewModel`, `m_orientation`, `orientations()`, or `pageOrientation()` — this feature's edits are fully confined to `qt-app/quick/` and change zero bytes of `adminwindow.cpp`'s behavior.

---

## Task decomposition

Two code tasks (ViewModel; QML + QuickTest) plus one mandatory manual-smoke release gate. The feature is a small, self-contained slice — no need to split the ViewModel task further.

---

## Task 1 — ViewModel: `orientation` property, validating setter, mapping helper, two call sites, C++ tests

### Files

- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h` — `#include <QPageLayout>`; `orientation`/`orientations` `Q_PROPERTY`s; readers; `Q_INVOKABLE setOrientation` decl; `orientationChanged()` signal; private static `pageOrientation` decl; `m_orientation` member.
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` — `setOrientation` body; `pageOrientation` static body; one-line insert in `exportPdf`; one-line insert in `printReport`.
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp` — `#include <QPageLayout>`; new slot declarations + bodies (default, valid-emit, two invalid-rejected, `orientations()` contents, four `pageOrientation()` mapping cases).

### Interfaces

**Produces:**
- `Q_PROPERTY(QStringList orientations READ orientations CONSTANT)` — `{"Portrait","Landscape"}`.
- `Q_PROPERTY(QString orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)` — default `"Portrait"`.
- `Q_INVOKABLE void ReportingViewModel::setOrientation(const QString &v);` — rejects (no assignment, no signal) when `v == m_orientation` OR `v` is not in `orientations()`.
- `static QPageLayout::Orientation ReportingViewModel::pageOrientation(const QString &s);` — **PUBLIC** static (test calls it directly); `"Landscape"` (exact match) → `QPageLayout::Landscape`; everything else → `QPageLayout::Portrait`.

**Consumes:** nothing new — `exportPdf`/`printReport` already own `QPdfWriter`/`QPrinter` instances; this task only adds one `setPageOrientation(...)` call to each.

### Steps

- [ ] **Step 1: Add the failing C++ test cases (RED).**

  In `qt-app/quick/tests/tst_reportingviewmodel.cpp`, add `#include <QPageLayout>` to the include block at the top:

  ```cpp
  #include <QtTest>
  #include <QDate>
  #include <QJsonObject>
  #include <QJsonDocument>
  #include <QPageLayout>
  #include <QSignalSpy>
  #include <QUrl>
  #include <QTemporaryDir>
  #include <QFileInfo>
  #include "xlsxdocument.h"
  #include "ReportingViewModel.h"
  ```

  Add these slot declarations to the `private slots:` block, directly after `void paletteAndChartTypeSettersEmit();`:

  ```cpp
      void orientation_defaultsToPortrait();
      void setOrientation_validValueEmitsAndUpdates();
      void setOrientation_blankRejectedNoSignal();
      void setOrientation_garbageRejectedNoSignal();
      void orientations_containsPortraitAndLandscape();
      void pageOrientation_mapsPortraitAndLandscape();
      void pageOrientation_mapsBlankAndGarbageToPortrait();
  ```

  Add the seven bodies just before `QTEST_MAIN(TestReportingViewModel)`:

  ```cpp
  void TestReportingViewModel::orientation_defaultsToPortrait()
  {
      ReportingViewModel vm;
      QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));
  }

  void TestReportingViewModel::setOrientation_validValueEmitsAndUpdates()
  {
      ReportingViewModel vm;
      QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
      vm.setOrientation("Landscape");
      QCOMPARE(vm.orientation(), QStringLiteral("Landscape"));
      QCOMPARE(spy.count(), 1);
  }

  void TestReportingViewModel::setOrientation_blankRejectedNoSignal()
  {
      ReportingViewModel vm;
      QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
      vm.setOrientation("");
      QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));   // unchanged
      QCOMPARE(spy.count(), 0);                                  // no signal at all (decision 3)
  }

  void TestReportingViewModel::setOrientation_garbageRejectedNoSignal()
  {
      ReportingViewModel vm;
      QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
      vm.setOrientation("Diagonal");
      QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));   // unchanged
      QCOMPARE(spy.count(), 0);                                  // no signal at all (decision 3)
  }

  void TestReportingViewModel::orientations_containsPortraitAndLandscape()
  {
      ReportingViewModel vm;
      QCOMPARE(vm.orientations(), QStringList({ QStringLiteral("Portrait"), QStringLiteral("Landscape") }));
  }

  void TestReportingViewModel::pageOrientation_mapsPortraitAndLandscape()
  {
      QCOMPARE(ReportingViewModel::pageOrientation("Portrait"), QPageLayout::Portrait);
      QCOMPARE(ReportingViewModel::pageOrientation("Landscape"), QPageLayout::Landscape);
  }

  void TestReportingViewModel::pageOrientation_mapsBlankAndGarbageToPortrait()
  {
      QCOMPARE(ReportingViewModel::pageOrientation(""), QPageLayout::Portrait);
      QCOMPARE(ReportingViewModel::pageOrientation("garbage"), QPageLayout::Portrait);
  }
  ```

- [ ] **Step 2: Build the test target and watch it FAIL to compile (RED).**

  ```powershell
  cmake --build C:\b\loams-orient --target tst_reportingviewmodel
  ```

  Expected failure: `'orientation'/'setOrientation'/'orientations'/'pageOrientation' is not a member of 'ReportingViewModel'` (the seven new bodies reference symbols that don't exist yet).

- [ ] **Step 3: Add the header declarations.**

  In `qt-app/quick/viewmodels/ReportingViewModel.h`, add the include. Find:

  ```cpp
  #include <QDate>
  #include <QJsonArray>
  #include <QJsonObject>
  #include <QObject>
  #include <QString>
  #include <QStringList>
  #include <QUrl>
  ```

  Replace with (alphabetical slot for `QPageLayout` between `QObject` and `QString`):

  ```cpp
  #include <QDate>
  #include <QJsonArray>
  #include <QJsonObject>
  #include <QObject>
  #include <QPageLayout>
  #include <QString>
  #include <QStringList>
  #include <QUrl>
  ```

  Add the two `Q_PROPERTY` declarations. Find:

  ```cpp
      Q_PROPERTY(QStringList palettes READ palettes CONSTANT)
      Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY paletteChanged)
      Q_PROPERTY(QStringList chartTypes READ chartTypes CONSTANT)
      Q_PROPERTY(QString chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
  ```

  Replace with:

  ```cpp
      Q_PROPERTY(QStringList palettes READ palettes CONSTANT)
      Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY paletteChanged)
      Q_PROPERTY(QStringList chartTypes READ chartTypes CONSTANT)
      Q_PROPERTY(QString chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
      Q_PROPERTY(QStringList orientations READ orientations CONSTANT)
      Q_PROPERTY(QString orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
  ```

  Add the inline readers. Find:

  ```cpp
      QStringList palettes() const { return { QStringLiteral("Default"), QStringLiteral("Blue"),
                                              QStringLiteral("Green"), QStringLiteral("Red") }; }
      QString palette() const { return m_palette; }
      QStringList chartTypes() const { return { QStringLiteral("Bar"), QStringLiteral("Pie") }; }
      QString chartType() const { return m_chartType; }
  ```

  Replace with:

  ```cpp
      QStringList palettes() const { return { QStringLiteral("Default"), QStringLiteral("Blue"),
                                              QStringLiteral("Green"), QStringLiteral("Red") }; }
      QString palette() const { return m_palette; }
      QStringList chartTypes() const { return { QStringLiteral("Bar"), QStringLiteral("Pie") }; }
      QString chartType() const { return m_chartType; }
      QStringList orientations() const { return { QStringLiteral("Portrait"), QStringLiteral("Landscape") }; }
      QString orientation() const { return m_orientation; }
  ```

  Add the `Q_INVOKABLE` declaration. Find:

  ```cpp
      Q_INVOKABLE void setPalette(const QString &p);
      Q_INVOKABLE void setChartType(const QString &c);
  ```

  Replace with:

  ```cpp
      Q_INVOKABLE void setPalette(const QString &p);
      Q_INVOKABLE void setChartType(const QString &c);
      Q_INVOKABLE void setOrientation(const QString &v);
  ```

  Add the signal. Find:

  ```cpp
      void paletteChanged();
      void chartTypeChanged();
  ```

  Replace with:

  ```cpp
      void paletteChanged();
      void chartTypeChanged();
      void orientationChanged();
  ```

  Add the pure static mapping helper to the **PUBLIC** "Pure statics (network-free test seams)" group — NOT the private presentation-helper group. `pageOrientation` **must be public** because `tst_reportingviewmodel` calls `ReportingViewModel::pageOrientation(...)` directly (a private static would fail to compile in the non-friend test — this mirrors why 4b-i's `buildTimeExport` and the other directly-asserted pure statics live in the public seams group). This anchor is also #49-independent (`buildFilters`/`normalizeExportRows`/`semesterWindow` predate #49), so it sidesteps the post-#49 line-drift entirely. Find the public seams group (near the top of the `public:` section):

  ```cpp
      static QJsonArray normalizeExportRows(const QJsonArray &data);   // visits string -> number
      // Display-only Period for a semester, matching get_report_data.php's server windows.
      static DateRange semesterWindow(const QString &semester, int year);
  ```

  Replace with (insert `pageOrientation` right after `normalizeExportRows`):

  ```cpp
      static QJsonArray normalizeExportRows(const QJsonArray &data);   // visits string -> number
      // "Landscape" (exact) -> QPageLayout::Landscape; everything else (incl. "Portrait", "",
      // garbage) -> QPageLayout::Portrait. PUBLIC: a pure network-free test seam asserted
      // directly by tst_reportingviewmodel (like buildFilters/normalizeExportRows).
      static QPageLayout::Orientation pageOrientation(const QString &s);
      // Display-only Period for a semester, matching get_report_data.php's server windows.
      static DateRange semesterWindow(const QString &semester, int year);
  ```

  (If the exact neighbors differ at build time, keep the rule: `pageOrientation` goes in the **public** static-seams group, never the private presentation-helper group — the test calls it directly.)

  Add the member. Find:

  ```cpp
      QString m_palette = QStringLiteral("Default");
      QString m_chartType = QStringLiteral("Bar");
  ```

  Replace with:

  ```cpp
      QString m_palette = QStringLiteral("Default");
      QString m_chartType = QStringLiteral("Bar");
      QString m_orientation = QStringLiteral("Portrait");
  ```

- [ ] **Step 4: Add the setter + helper bodies, and the two call-site insertions.**

  In `qt-app/quick/viewmodels/ReportingViewModel.cpp`, add `setOrientation` next to `setPalette`/`setChartType`. Find:

  ```cpp
  void ReportingViewModel::setPalette(const QString &p)
  {
      if (m_palette == p) return;
      m_palette = p; emit paletteChanged();
  }
  void ReportingViewModel::setChartType(const QString &c)
  {
      if (m_chartType == c) return;
      m_chartType = c; emit chartTypeChanged();
  }
  ```

  Replace with:

  ```cpp
  void ReportingViewModel::setPalette(const QString &p)
  {
      if (m_palette == p) return;
      m_palette = p; emit paletteChanged();
  }
  void ReportingViewModel::setChartType(const QString &c)
  {
      if (m_chartType == c) return;
      m_chartType = c; emit chartTypeChanged();
  }
  void ReportingViewModel::setOrientation(const QString &v)
  {
      if (v == m_orientation || !orientations().contains(v))
          return;
      m_orientation = v;
      emit orientationChanged();
  }
  ```

  Add the `pageOrientation` static definition next to the other static formatting helpers — e.g. directly after `weekdayShortName`'s definition:

  ```cpp
  QString ReportingViewModel::weekdayShortName(int monFirstIndex)
  {
      static const char *const kShort[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
      if (monFirstIndex < 0 || monFirstIndex >= 7)
          return QString();
      return QString::fromLatin1(kShort[monFirstIndex]);
  }

  QPageLayout::Orientation ReportingViewModel::pageOrientation(const QString &s)
  {
      return s == QStringLiteral("Landscape") ? QPageLayout::Landscape : QPageLayout::Portrait;
  }
  ```

  Insert the `exportPdf` call site. Find:

  ```cpp
      QMetaObject::invokeMethod(this, [this, path]() {
          QPdfWriter writer(path);
          writer.setPageSize(QPageSize(QPageSize::A4));
          const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
  ```

  Replace with:

  ```cpp
      QMetaObject::invokeMethod(this, [this, path]() {
          QPdfWriter writer(path);
          writer.setPageSize(QPageSize(QPageSize::A4));
          writer.setPageOrientation(pageOrientation(m_orientation));
          const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
  ```

  Insert the `printReport` call site (BEFORE the `QPrintDialog` constructor — decision 5). Find:

  ```cpp
      // Opening the dialog is NOT "exporting" — the normal UI stays live.
      QPrinter printer(QPrinter::HighResolution);
      QPrintDialog dlg(&printer);
      if (dlg.exec() != QDialog::Accepted)
          return;   // cancelled -> no-op, no error, no busy state
  ```

  Replace with:

  ```cpp
      // Opening the dialog is NOT "exporting" — the normal UI stays live.
      QPrinter printer(QPrinter::HighResolution);
      printer.setPageOrientation(pageOrientation(m_orientation));
      QPrintDialog dlg(&printer);
      if (dlg.exec() != QDialog::Accepted)
          return;   // cancelled -> no-op, no error, no busy state
  ```

- [ ] **Step 5: Build + run `tst_reportingviewmodel` to GREEN.**

  ```powershell
  cmake --build C:\b\loams-orient --target tst_reportingviewmodel
  ctest --test-dir C:\b\loams-orient -R tst_reportingviewmodel --output-on-failure
  ```

  Expected: all seven new cases pass, and every pre-existing case in the file stays green (in particular `paletteAndChartTypeSettersEmit` — untouched).

- [ ] **Step 6: Full build + full `ctest` (no regressions), both exes link.**

  ```powershell
  cmake --build C:\b\loams-orient
  ctest --test-dir C:\b\loams-orient --output-on-failure
  ```

  Expected: baseline green + the 7 new cases. `WITS` and `WITSQuick` both link (the QML combo doesn't exist yet — that's fine, QML is loaded at runtime, not compiled; `ReportingScreen.qml` is unmodified so far). If `tst_settingsviewmodel` flakes, re-run it alone.

- [ ] **Step 7: Commit.** Stage only `qt-app/quick/viewmodels/ReportingViewModel.h`, `qt-app/quick/viewmodels/ReportingViewModel.cpp`, `qt-app/quick/tests/tst_reportingviewmodel.cpp`. Subject e.g.:

  `feat(reporting): add Portrait/Landscape orientation to ReportingViewModel`

### Deliverable

`ReportingViewModel` exposes `orientation`/`orientations`, a validating `setOrientation` (reject-invalid, zero-signal on rejection), and a defensive `pageOrientation()` mapping helper; `exportPdf`/`printReport` both apply it to their paint device (the latter before the print dialog opens). VM tests green; full suite green; both exes link.

---

## Task 2 — QML combo + QuickTest

### Files

- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml` — new `orientationCombo` `LComboBox` in `exportRow`.
- Test: `qt-app/quick/tests/tst_qml_admin.qml` — `reportingStub` gains `orientation`/`orientations`/`setOrientation`; new `test_orientationComboHasAccessibleNameAndWrites`.

### Interfaces

**Produces:** the `orientationCombo` `LComboBox`, `objectName: "orientationCombo"`, `accessibleName: qsTr("Report orientation")`.

**Consumes:** `screen.vm.orientations` (model), `screen.vm.orientation` (currentValue), `screen.vm.setOrientation(v)` (onSelected) — all added by Task 1.

### Steps

- [ ] **Step 1: Extend the `reportingStub` + add the failing QuickTest case (RED).**

  In `qt-app/quick/tests/tst_qml_admin.qml`, find the stub's palette/chartType properties + setters:

  ```qml
          property var palettes: ["Default", "Blue", "Green", "Red"]
          property string palette: "Default"
          property var chartTypes: ["Bar", "Pie"]
          property string chartType: "Bar"
  ```

  and

  ```qml
          function setPalette(p) { palette = p }
          function setChartType(c) { chartType = c }
  ```

  Replace the first block with (add the orientation analogues right after `chartType`):

  ```qml
          property var palettes: ["Default", "Blue", "Green", "Red"]
          property string palette: "Default"
          property var chartTypes: ["Bar", "Pie"]
          property string chartType: "Bar"
          property var orientations: ["Portrait", "Landscape"]
          property string orientation: "Portrait"
  ```

  Replace the second block with:

  ```qml
          function setPalette(p) { palette = p }
          function setChartType(c) { chartType = c }
          function setOrientation(v) { orientation = v }
  ```

  Add the test case directly after `test_paletteComboHasAccessibleNameAndWrites`:

  ```qml
          function test_paletteComboHasAccessibleNameAndWrites() {
              var combo = findChild(reporting, "paletteCombo");
              verify(combo);
              compare(combo.accessibleName, "Report palette");
              combo.selectValue("Blue");
              compare(reportingStub.palette, "Blue");
              reportingStub.palette = "Default";
          }

          function test_orientationComboHasAccessibleNameAndWrites() {
              var combo = findChild(reporting, "orientationCombo");
              verify(combo);
              compare(combo.accessibleName, "Report orientation");
              combo.selectValue("Landscape");
              compare(reportingStub.orientation, "Landscape");
              reportingStub.orientation = "Portrait";
          }
  ```

- [ ] **Step 2: Build + run and watch the new case FAIL (RED).**

  ```powershell
  cmake --build C:\b\loams-orient --target tst_qml_admin
  ctest --test-dir C:\b\loams-orient -R tst_qml_admin --output-on-failure
  ```

  Expected: `test_orientationComboHasAccessibleNameAndWrites` fails with `findChild(...)` returning `null` (`combo` undefined) — `orientationCombo` doesn't exist in `ReportingScreen.qml` yet. `test_paletteComboHasAccessibleNameAndWrites` and every other case stay green (the stub additions are additive).

- [ ] **Step 3: Add the `orientationCombo` to `ReportingScreen.qml`.**

  In `qt-app/quick/qml/admin/ReportingScreen.qml`, find:

  ```qml
                      LComboBox {
                          id: chartTypeCombo
                          objectName: "chartTypeCombo"
                          accessibleName: qsTr("Chart type")
                          Layout.preferredWidth: 150
                          model: screen.vm ? screen.vm.chartTypes : []
                          placeholder: qsTr("Chart")
                          currentValue: screen.vm ? screen.vm.chartType : "Bar"
                          onSelected: function(v) { if (screen.vm) screen.vm.setChartType(v); }
                      }

                      Item { Layout.fillWidth: true }   // spacer
  ```

  Replace with (new combo inserted between `chartTypeCombo` and the spacer):

  ```qml
                      LComboBox {
                          id: chartTypeCombo
                          objectName: "chartTypeCombo"
                          accessibleName: qsTr("Chart type")
                          Layout.preferredWidth: 150
                          model: screen.vm ? screen.vm.chartTypes : []
                          placeholder: qsTr("Chart")
                          currentValue: screen.vm ? screen.vm.chartType : "Bar"
                          onSelected: function(v) { if (screen.vm) screen.vm.setChartType(v); }
                      }
                      LComboBox {
                          id: orientationCombo
                          objectName: "orientationCombo"
                          accessibleName: qsTr("Report orientation")
                          Layout.preferredWidth: 150
                          model: screen.vm ? screen.vm.orientations : []
                          placeholder: qsTr("Orientation")
                          currentValue: screen.vm ? screen.vm.orientation : "Portrait"
                          onSelected: function(v) { if (screen.vm) screen.vm.setOrientation(v); }
                      }

                      Item { Layout.fillWidth: true }   // spacer
  ```

- [ ] **Step 4: Build + run `tst_qml_admin` to GREEN.**

  ```powershell
  cmake --build C:\b\loams-orient --target tst_qml_admin
  ctest --test-dir C:\b\loams-orient -R tst_qml_admin --output-on-failure
  ```

  Expected: `test_orientationComboHasAccessibleNameAndWrites` passes; every pre-existing case (including `test_paletteComboHasAccessibleNameAndWrites` and the export-row-adjacent cases) stays green.

- [ ] **Step 5: Full build + full `ctest`, both exes link.**

  ```powershell
  cmake --build C:\b\loams-orient
  ctest --test-dir C:\b\loams-orient --output-on-failure
  ```

  Expected: all green (baseline + Task 1's 7 + Task 2's 1). `WITS.exe`/`WITSQuick.exe` both link. Close any running `WITSQuick.exe` first.

- [ ] **Step 6: Commit.** Stage only `qt-app/quick/qml/admin/ReportingScreen.qml`, `qt-app/quick/tests/tst_qml_admin.qml`. Subject e.g.:

  `feat(reporting): add orientation combo to the export bar`

### Deliverable

The export bar shows a third combo — Portrait/Landscape — that writes through to the VM exactly like `paletteCombo`/`chartTypeCombo`. QuickTest green; full suite green; both exes link.

---

## Task 3 — Manual `WITSQuick.exe` smoke (MANDATORY release gate, owner-run, no code)

Neither `QtTest` nor `qmltest` can observe the actual rendered PDF page geometry or the `QPrintDialog`'s real on-screen initial orientation — offscreen Qt Test runs have no physical screen or interactive dialog to inspect (same rationale as the project's other export-path manual gates, e.g. the QtChart export screen-clamp memory and the sibling library-hours-window spec's §6.3). This task has no files to modify — it is a checklist to run against the built `WITSQuick.exe`.

### Steps

- [ ] **Step 1: Close any running `WITSQuick.exe`, rebuild, then launch `C:\b\loams-orient\quick\WITSQuick.exe`** (adjust the path if the build dir differs) → Reporting screen. Synthetic data only — no real student PII.

- [ ] **Step 2: Run the six-point smoke (spec §6.3):**

  1. **Landscape PDF:** pick "Landscape" in the orientation combo, export a PDF over a range with data. Confirm the page is landscape-oriented and every element (header, tiles, tables, charts) reflowed to the wider page with no clipping or blank charts.
  2. **Print dialog:** pick "Landscape", click Print. Confirm the native print dialog opens **already showing Landscape** as the selected orientation — not just that a landscape page eventually prints. This is the one behavior offscreen tests structurally cannot see (decision 5's ordering requirement — the printer's orientation must be set before `QPrintDialog` is constructed).
  3. **Portrait (default):** with orientation left at (or reselected to) "Portrait", export a PDF and print. Confirm output is unchanged from today's Portrait-only behavior — no regression for the default path.
  4. **Excel unaffected:** export Excel with "Landscape" selected in the combo. Confirm the `.xlsx` output is identical to before this feature — orientation has no effect on it.
  5. **Legacy `WITS.exe`:** confirm its own PDF/Print export still produces Landscape output unconditionally, with no orientation combo present anywhere in its UI — unchanged.
  6. **Per-session reset:** close and relaunch `WITSQuick.exe`. Confirm the orientation combo shows "Portrait" again (decision 2 — no persistence across launches).

  On any failure, route through `superpowers:systematic-debugging` (reproduce with a failing test → isolate → fix under TDD) before re-exporting.

### Deliverable

Owner-confirmed smoke across Landscape PDF, the Print-dialog initial-state proof, Portrait-default parity, Excel non-effect, legacy-`WITS.exe` parity, and per-session reset. This is the release gate the automated suite structurally cannot replace.

---

## Post-build gate

After Task 2 (all automated green) and before finishing the branch: run `/claude-review` on the slice per the project workflow (`.claude/rules/workflow.md` §3), fix Critical/Important findings, and re-submit until APPROVE or the 3-round cap. Task 3's manual smoke is the release gate the automated suite structurally cannot replace — run it after review approves, before `create-pr`. Then `superpowers:finishing-a-development-branch` → the project-scoped `create-pr` (three-agent gate: `dry-checker`, `security-reviewer`, `general-code-reviewer`).
