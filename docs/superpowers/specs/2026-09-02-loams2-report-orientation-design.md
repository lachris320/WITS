# LOAMS 2.0 — Report Orientation Picker for PDF Export & Print (Design Spec)

**Date:** 2026-09-02
**Status:** Design approved (owner, via brainstorming). Ready for `superpowers:writing-plans`.
**Depends on:** none — this slice is independent of the in-flight "When?" library-hours windowing work (`2026-08-31-loams2-library-hours-when-analytics-design.md`); it touches the same `ReportingViewModel` file but a disjoint set of members/methods.

**One-line summary:** add a Portrait / Landscape combo to the Reporting screen's export bar so the librarian can choose the page orientation of the **PDF export** and the **Print** output; Excel is unaffected; the choice is per-session (default Portrait, not persisted); the renderer needs **no code change** because it already derives its whole layout from the paint device's `pageLayout()`.

---

## 1. Problem & goal

Today `ReportingViewModel::exportPdf` (`ReportingViewModel.cpp:664-680`) constructs a `QPdfWriter` and calls only `writer.setPageSize(QPageSize(QPageSize::A4))` (`:672`) — it never calls `setPageOrientation`, so the PDF is always Portrait (`QPageLayout`'s default orientation). `printReport` (`:682-702`) likewise never sets an orientation on the `QPrinter` before running `QPrintDialog`, so the dialog always opens Portrait too. A librarian who wants a wide report — many course columns, a wide multi-bar chart — has no way to request Landscape from the Quick app; the only escape is to change orientation inside the OS print dialog itself, which does nothing for the PDF path.

**Goal:** let the librarian pick Portrait or Landscape from the export bar, and have that choice apply to both the PDF file that gets written and the initial state of the Print dialog.

**Non-goal / scope guard:** this is a **pure page-setup toggle**. No backend change, no new endpoint, no new source files, no new CMake targets, no renderer change (§5.4 explains why), no change to Excel export, no persistence across app launches, no custom page sizes, no per-chart or per-section orientation.

## 2. Context — how the export device is set up today

### 2.1 PDF export (`ReportingViewModel::exportPdf`, `ReportingViewModel.cpp:664-680`)

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
        ...
    }, Qt::QueuedConnection);
}
```

The `QPdfWriter` is constructed and configured entirely inside the deferred lambda (`Qt::QueuedConnection`, so the busy overlay paints before the blocking render runs). The lambda already captures `[this, path]` — `this` is enough to reach any new VM member, no capture-list change needed.

### 2.2 Print (`ReportingViewModel::printReport`, `ReportingViewModel.cpp:682-702`)

```cpp
void ReportingViewModel::printReport()
{
    if (m_exporting) return;
    if (m_exportRows.isEmpty()) { ... return; }
    // Opening the dialog is NOT "exporting" — the normal UI stays live.
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer);
    if (dlg.exec() != QDialog::Accepted)
        return;   // cancelled -> no-op, no error, no busy state
    ...
}
```

`printer` is constructed, then `dlg` is constructed around it and immediately `exec()`'d — there is exactly one statement's worth of room between "printer exists" and "dialog opens" to set the orientation the dialog should start with.

### 2.3 The renderer already adapts to whatever orientation the device carries — the key finding

`ReportRenderer::paintReport` (`qt-app/core/reportrenderer.cpp`) derives its **entire** layout from the paint device's page geometry, not from any hardcoded Portrait assumption:

```cpp
QRectF pageRect = device->pageLayout().paintRectPixels(resolution);   // reportrenderer.cpp:506
int pageWidth  = pageRect.width();                                     // :507
int pageHeight = pageRect.height();                                    // :508
int margin     = pageWidth * 0.03;                                     // :509
int usableWidth  = pageWidth - 2*margin;                                // :510
int usableHeight = pageHeight - 2*margin;                               // :511
```

Every subsequent drawing call — the header/logo band, the summary tiles, the ranking tables, the roster table columns (`:786-792`), the footer/signature block (`:868-878`) — is positioned as a fraction of `pageWidth`/`usableWidth`/`pageHeight`/`usableHeight`. The **charts** reflow by a slightly different (and even more robust) mechanism: `chartImageSize` does **not** key off `usableWidth` — it `Q_UNUSED`s that argument (`reportrenderer.cpp:122-135`) and derives the raster from the screen (the screen-safe-clamp size), so the raster dimensions are orientation-independent. What makes charts fit a landscape page is `drawFullscreenChart` (`:665-695`), which scales each chart raster into its `targetArea` (`usableWidth` × the remaining page height) with `Qt::KeepAspectRatio` and centers it — so on a shorter, wider landscape page the chart fits both width and height with no clipping. (The in-file comment near `:699-702` that reads as if `chartImageSize` takes `usableWidth` is itself misleading — the argument is unused; do not chase a width-keyed sizing path, it does not exist.) `device->pageLayout()` reflects whatever orientation was set on the device **before** `paintReport` runs (`QPdfWriter::setPageOrientation` / `QPrinter::setPageOrientation`, both called before `renderToDevice` — see §5.2/§5.3), so a Landscape device yields a wider `pageRect`, every fraction-of-width/height call reflows automatically, and tables/roster paginate dynamically on `y` vs `pageHeight` (landscape simply yields more, shorter pages — never a clip). **The renderer requires zero changes.** This is precisely how the app already handles multi-page pagination and the screen-safe chart-image clamp — geometry read from the device, not assumed — so the orientation toggle fits the renderer's existing design rather than fighting it.

The one place orientation is *already* set today is the **legacy** `adminwindow.cpp` export path (`onExportReportBtnClicked`/`printReport`), which unconditionally hardcodes Landscape for both outputs: `pdf.setPageOrientation(QPageLayout::Landscape)` (`adminwindow.cpp:1705`) and `printer.setPageOrientation(QPageLayout::Landscape)` (`:1741`). That is a separate code path (see §5.5) and is untouched by this feature — it is cited here only to confirm `setPageOrientation(QPageLayout::Orientation)` is the correct, already-proven Qt 6 API for both `QPdfWriter` and `QPrinter` in this codebase.

### 2.4 The existing per-session picker pattern to mirror (`palette`/`chartType`)

`palette`/`chartType` are the precedent for "plain ViewModel state, not `AppSettings`, reset each launch":

- `Q_PROPERTY(QStringList palettes READ palettes CONSTANT)` / `Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY paletteChanged)` (`ReportingViewModel.h:63-64`, mirrored for `chartTypes`/`chartType` at `:65-66`).
- `QStringList palettes() const { return { "Default", "Blue", "Green", "Red" }; }` — an inline list literal, no `AppSettings` read (`ReportingViewModel.h:135`; `chartTypes()` likewise at `:138`).
- Members `QString m_palette = QStringLiteral("Default");` / `QString m_chartType = QStringLiteral("Bar");` (`ReportingViewModel.h:272-273`).
- Setters (`ReportingViewModel.cpp:584-593`):
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

**Grounding surprise vs. the brainstormed assumption:** neither setter validates its argument against the list `palettes()`/`chartTypes()` returns — each only guards against the *unchanged* value. `setPalette("Nonsense")` today happily sets `m_palette` to `"Nonsense"` and emits `paletteChanged()`; nothing downstream currently punishes that because `ReportController::getPalette(m_palette)` (consumed in `renderToDevice`, `ReportingViewModel.cpp:631`) presumably degrades gracefully for an unknown name. **`setOrientation` is deliberately stricter than this precedent** (decision 3, §4) — it rejects any value not in `orientations()` outright, not merely a same-value no-op. This is a genuine behavioral deviation from the pattern it otherwise mirrors, and it exists because an invalid orientation string reaching `pageOrientation()` would silently degrade to Portrait (§4, decision 4) rather than erroring, which is a worse failure mode than a rejected setter call for a picker with only two legal values.

### 2.5 The export-bar QML row to extend (`ReportingScreen.qml:533-588`)

The `exportRow` `RowLayout` (`ReportingScreen.qml:534`) currently holds, in order: `paletteCombo` (`:538-547`), `chartTypeCombo` (`:548-557`), a filling spacer `Item` (`:559`), then the three action buttons — `exportPdfButton` (`:562-569`), `exportExcelButton` (`:570-578`), `printButton` (`:579-587`). Both combos share the same `LComboBox` shape: `objectName`, `accessibleName`, `Layout.preferredWidth: 150`, `model` bound to a VM list property, `placeholder`, `currentValue` bound to the VM's current selection with a literal fallback, and `onSelected: function(v) { if (screen.vm) screen.vm.setXxx(v); }`.

## 3. Goal & non-goals

**In scope**
- A `Q_PROPERTY QString orientation` on `ReportingViewModel`, default `"Portrait"`, settable via a validating `Q_INVOKABLE setOrientation`.
- A `Q_PROPERTY QStringList orientations` exposing `{"Portrait", "Landscape"}` for the combo's model.
- `exportPdf` sets the `QPdfWriter`'s page orientation from the VM's current selection before rendering.
- `printReport` sets the `QPrinter`'s page orientation from the VM's current selection **before** the `QPrintDialog` is constructed, so the dialog opens already showing that orientation.
- A third `LComboBox` ("Orientation") added to `exportRow`, mirroring `paletteCombo`/`chartTypeCombo`.

**Non-goals**
- **Excel orientation** — a spreadsheet has no page-orientation concept in the sense a PDF/print page does; `exportExcel`/`writeReportToXlsx` are untouched.
- **Persistence** — the selection is plain `ReportingViewModel` state, reset to `"Portrait"` on every app launch, exactly like `palette`/`chartType`. No `AppSettings` key, no `QSettings` write.
- **Custom page sizes** — `QPageSize(QPageSize::A4)` stays hardcoded in `exportPdf` (`:672`); this feature only toggles orientation, not size.
- **Per-element / per-page orientation** — the whole exported document (PDF) or the whole print job uses one orientation; there is no per-chart or per-page override.
- **Renderer changes** — `ReportRenderer::paintReport` needs no edit (§2.3, §5.4).
- **Legacy `adminwindow.cpp`** — its own export path already hardcodes Landscape unconditionally (§2.3, §5.5) and is not touched.

## 4. The five locked decisions

### Decision 1 — Outputs affected: PDF + Print only
Orientation applies to `exportPdf` and `printReport`. `exportExcel` is untouched.
**Rationale:** a spreadsheet's "page" is a print-preview concept layered on top of cell data; it has no meaningful analog to a PDF/print page's fixed physical dimensions that the renderer's `pageWidth`/`pageHeight` math depends on. Extending orientation to Excel would require inventing a print-area convention QXlsx doesn't need for this app's usage.

### Decision 2 — Persistence: per-session, default Portrait
`m_orientation` is a plain `ReportingViewModel` member, not read from or written to `AppSettings`. It starts `"Portrait"` on every launch — identical to today's unconditional Portrait behavior — and reverts to `"Portrait"` the next time the app starts, regardless of what the librarian picked in a previous session.
**Rationale:** matches the existing `palette`/`chartType`/`includeRosterInExport` pickers exactly (§2.4) — none of the export-bar controls persist, all are "set it fresh each session." Introducing an `AppSettings` key here would make orientation the only export-bar control with a different persistence model, which is an inconsistency the owner explicitly rejected during brainstorming.

### Decision 3 — `setOrientation()` rejects invalid/blank input
Unlike `setPalette`/`setChartType` (§2.4), which accept and store *any* string as long as it differs from the current value, `setOrientation(const QString &v)` accepts **only** `"Portrait"` and `"Landscape"`. Any other value — an empty string, a typo, an unrelated word — leaves `m_orientation` **unchanged** and emits **no** `orientationChanged` signal. This is stronger than a same-value no-op: it is an outright rejection of anything outside the two legal values, even a *novel* invalid value.
**Rationale:** the combo's `model` is `orientations()` itself, so in normal UI use only `"Portrait"`/`"Landscape"` can ever reach the setter — the guard exists for the `Q_INVOKABLE` surface's own robustness (a stray QML binding, a future caller, a test) rather than for anything the shipped combo can trigger. Given only two legal values exist, "quietly accept garbage and let it degrade at paint time" (the `palette`/`chartType` precedent) is a worse trade here than "reject outright," because an unrejected garbage value would silently fall back to Portrait inside `pageOrientation()` (decision 4) with no signal that the request was ignored — a silent divergence between what `orientation` *reports* (the garbage string) and what the export actually *produces* (Portrait). Rejecting outright keeps `m_orientation` always equal to what was actually applied.

### Decision 4 — `pageOrientation()` stays defensive independently
The pure mapping helper `static QPageLayout::Orientation pageOrientation(const QString &s)` returns `QPageLayout::Landscape` **iff** `s == "Landscape"` exactly; every other input — including `"Portrait"`, blank, or garbage — returns `QPageLayout::Portrait`. It does not assume its argument was pre-validated, even though the only production caller (`m_orientation`, guarded by decision 3) can never actually hand it anything but `"Portrait"`/`"Landscape"`.
**Rationale:** the helper is `static` and independently unit-testable (§6); a pure mapping function should be correct on its own terms, not correct-by-caller-convention. Two independent defenses (the setter rejects garbage before it's stored; the mapping helper degrades garbage to a safe default if it ever sees any) is the same "second net" posture the sibling library-hours-window spec used for its own defensive clamp (`2026-08-31-...-design.md` decision 4) — cheap insurance against a future caller that bypasses the setter (e.g. a test constructing a value directly, or a later refactor).

### Decision 5 — Print orientation must be set before the `QPrintDialog` is constructed
`printer.setPageOrientation(pageOrientation(m_orientation))` must execute **before** `QPrintDialog dlg(&printer)` is constructed and `exec()`'d, not after the dialog returns.
**Rationale:** `QPrintDialog` reads the `QPrinter` it wraps to populate its own initial orientation control — setting orientation on `printer` *after* the dialog has already opened and been accepted would either have no visible effect on what the librarian saw in the dialog, or (worse) risk the dialog's own state overwriting the programmatic setting depending on Qt's internal sync order. Setting it first guarantees: the dialog opens already showing the librarian's export-bar choice, and the librarian can still override it inside the native dialog before accepting (native print dialogs always allow that) — the export-bar picker is a *starting point*, not a lock.

## 5. Detailed design

### 5.1 `ReportingViewModel.h` — new property, member, signal, and helper declarations

Add two `Q_PROPERTY` declarations to the block at `ReportingViewModel.h:33-79`, placed near the existing `palette`/`chartType` pair (`:63-66`) since they share the export-bar-picker shape:

```cpp
Q_PROPERTY(QStringList orientations READ orientations CONSTANT)
Q_PROPERTY(QString orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
```

Add the reader inline next to `palettes()`/`chartTypes()` (`ReportingViewModel.h:135,138`):

```cpp
QStringList orientations() const { return { QStringLiteral("Portrait"), QStringLiteral("Landscape") }; }
QString orientation() const { return m_orientation; }
```

Add the invokable declaration next to `setPalette`/`setChartType` (`ReportingViewModel.h:153-154`):

```cpp
Q_INVOKABLE void setOrientation(const QString &v);
```

Add the signal next to `paletteChanged`/`chartTypeChanged` (`ReportingViewModel.h:207-208`):

```cpp
void orientationChanged();
```

Add the pure static mapping helper next to the other presentation-formatting statics (`ReportingViewModel.h:227-232`, the `buildHourlyBars`/`hourTick`/`formatHourRange` group — `pageOrientation` belongs in that same "static + pure, directly unit-testable" private section even though its job is page setup rather than hour formatting):

```cpp
static QPageLayout::Orientation pageOrientation(const QString &s);   // "Landscape" only -> Landscape; else Portrait
```

This requires `#include <QPageLayout>` in the header (the header does not currently include it — `ReportingViewModel.h:1-17` has no `QPageLayout`/`QPageSize`/`QPrinter` includes today; those live only in the `.cpp`). Since `pageOrientation`'s return type `QPageLayout::Orientation` appears in the header's own declaration, the include must move to the header, not stay `.cpp`-only.

Add the member next to `m_palette`/`m_chartType` (`ReportingViewModel.h:272-273`):

```cpp
QString m_orientation = QStringLiteral("Portrait");
```

### 5.2 `ReportingViewModel.cpp` — setter, helper, and the two call sites

**`setOrientation` (placed next to `setPalette`/`setChartType`, `ReportingViewModel.cpp:584-593`):**

```cpp
void ReportingViewModel::setOrientation(const QString &v)
{
    if (v == m_orientation || !orientations().contains(v))
        return;
    m_orientation = v;
    emit orientationChanged();
}
```

The guard is a single early-return covering **both** rejection reasons from decision 3: the ordinary same-value no-op (`v == m_orientation`, the same shape `setPalette`/`setChartType` already use) **OR** the value not being one of the two legal strings (`!orientations().contains(v)`, the new stricter check those setters lack). Either condition alone is sufficient to skip the assignment and the signal — there is no case where an invalid-but-different value reaches `m_orientation = v`.

**`pageOrientation` (placed next to the other static formatting helpers, e.g. near `hourTick`/`formatHourRange`, `ReportingViewModel.cpp:149-167`):**

```cpp
QPageLayout::Orientation ReportingViewModel::pageOrientation(const QString &s)
{
    return s == QStringLiteral("Landscape") ? QPageLayout::Landscape : QPageLayout::Portrait;
}
```

**`exportPdf` — insertion point (`ReportingViewModel.cpp:664-680`):**

Current (relevant excerpt):
```cpp
QMetaObject::invokeMethod(this, [this, path]() {
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
```

New — one line added immediately after `setPageSize`, still inside the lambda, still before `renderToDevice`:
```cpp
QMetaObject::invokeMethod(this, [this, path]() {
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(pageOrientation(m_orientation));
    const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
```

The lambda's existing `[this, path]` capture is unchanged — `m_orientation` and `pageOrientation` are both reached via `this`, so no capture-list edit is needed. Ordering matters here for the same reason as §2.3: `renderToDevice` → `ReportRenderer::paintReport` reads `device->pageLayout()` (`reportrenderer.cpp:506`), so the orientation must be set on `writer` before that call, which it already is (both new and old lines precede `renderToDevice` unconditionally).

**`printReport` — insertion point (`ReportingViewModel.cpp:682-702`):**

Current (relevant excerpt):
```cpp
// Opening the dialog is NOT "exporting" — the normal UI stays live.
QPrinter printer(QPrinter::HighResolution);
QPrintDialog dlg(&printer);
if (dlg.exec() != QDialog::Accepted)
    return;   // cancelled -> no-op, no error, no busy state
```

New — one line inserted **between** the `QPrinter` construction and the `QPrintDialog` construction (decision 5):
```cpp
// Opening the dialog is NOT "exporting" — the normal UI stays live.
QPrinter printer(QPrinter::HighResolution);
printer.setPageOrientation(pageOrientation(m_orientation));
QPrintDialog dlg(&printer);
if (dlg.exec() != QDialog::Accepted)
    return;   // cancelled -> no-op, no error, no busy state
```

`QPrinter::setPageOrientation(QPageLayout::Orientation)` is the Qt 6 API already proven in this codebase at `adminwindow.cpp:1741` (§2.3), so no API-availability risk. The rest of `printReport` — the empty-rows guard, the cancel branch, `renderToDevice(&printer, printer.resolution())`, the status/error strings — is unchanged.

### 5.3 Renderer — no change, and why

`ReportRenderer::paintReport` (§2.3) reads `device->pageLayout().paintRectPixels(resolution)` and derives every subsequent measurement from it. Because `writer`/`printer` already carry the librarian's chosen orientation by the time `renderToDevice` calls into `paintReport` (§5.2 establishes that ordering for both call sites), the renderer sees a wider-than-tall (Landscape) or taller-than-wide (Portrait) `pageRect` and lays out the header, tiles, tables, and charts accordingly (the charts via `drawFullscreenChart`'s `Qt::KeepAspectRatio` fit-to-page, not a `usableWidth`-keyed raster size — see the §2.3 correction), with **zero** renderer-side awareness that orientation is even a concept. No function in `reportrenderer.{h,cpp}` changes.

### 5.4 QML — a third combo in the export row (`ReportingScreen.qml:533-588`)

Add an `orientationCombo` `LComboBox` to `exportRow`, positioned after `chartTypeCombo` and before the filling spacer (i.e. between the current `:547` and `:559`), mirroring `chartTypeCombo`'s exact shape (`:548-557`):

```qml
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
```

Every property here — `objectName`, `accessibleName`, `Layout.preferredWidth`, the `model`/`placeholder`/`currentValue`/`onSelected` shape — is a direct copy of the `chartTypeCombo` pattern with the identifiers swapped, consistent with how `chartTypeCombo` itself mirrors `paletteCombo`. No other QML in `ReportingScreen.qml` needs to change: the three export buttons (`exportPdfButton`, `exportExcelButton`, `printButton`, `:562-587`) call `screen.vm.exportPdf(...)`, `screen.vm.exportExcel(...)`, `screen.vm.printReport()` with no arguments today and continue to — the orientation travels through VM state (`m_orientation`), not through the button call.

### 5.5 Excel export — unaffected

`exportExcel` (`ReportingViewModel.cpp:704-721`) and `ReportRenderer::writeReportToXlsx` are untouched by this feature (decision 1). Neither reads `m_orientation` nor `pageOrientation()`.

### 5.6 Legacy `WITS.exe` (`adminwindow.cpp`) — unaffected, verified

`adminwindow.cpp` is a separate export path with its own `onExportReportBtnClicked`/`printReport`/`exportReportToExcel` methods (`:1704-1781`) that already hardcode `QPageLayout::Landscape` unconditionally for both the PDF writer (`:1705`) and the printer (`:1741`) — confirmed by direct read (§2.3). Neither method references `ReportingViewModel`, `m_orientation`, `orientations()`, or `pageOrientation()`; those symbols exist only on the Quick-side `ReportingViewModel` class, which `adminwindow.cpp` neither includes nor instantiates (its own reporting flow is driven by `ReportController`/`ReportRenderer` directly, not through the Quick ViewModel layer). This feature's edits are therefore fully confined to `qt-app/quick/` and change zero bytes of `adminwindow.cpp`'s behavior: the legacy app keeps exporting/printing Landscape-only, with no orientation picker, exactly as it does today.

## 6. Testing strategy

### 6.1 `tst_reportingviewmodel` (`qt-app/quick/tests/tst_reportingviewmodel.cpp`)

The existing `paletteAndChartTypeSettersEmit` test (`:530-543`) is the direct template — it constructs a bare `ReportingViewModel vm;`, asserts a default, wires a `QSignalSpy` on the `*Changed` signal, calls the setter, and asserts both the new value and the spy count. **Each new C++ test method must be declared under `private slots:` (near `:15`/`:53`) AND defined — a QtTest slot without its `private slots:` declaration is silently never run; the QuickTest `function test_*` form (§6.2) needs no such declaration, so watch the asymmetry.** New cases, added alongside it:

- **Default:** `vm.orientation() == "Portrait"` on a freshly constructed VM (no setter called).
- **Valid change emits:** `QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged); vm.setOrientation("Landscape");` → `vm.orientation() == "Landscape"` and `spy.count() == 1`.
- **Invalid values are rejected, not just no-ops (decision 3):** with `orientation()` still `"Portrait"`, call `vm.setOrientation("")` and separately `vm.setOrientation("Diagonal")`; after each, assert `vm.orientation() == "Portrait"` (unchanged) **and** `spy.count() == 0` (no signal at all) — this is the case that distinguishes the stronger guard from a plain unchanged-value no-op, so assert the spy count explicitly rather than only the final value.
- **`orientations()` contents:** `vm.orientations() == QStringList({"Portrait", "Landscape"})`, mirroring the existing `QCOMPARE(vm.palettes().size(), 4)` assertion shape at `:543`.
- **`pageOrientation()` mapping (static, called directly — it needs no `ReportingViewModel` instance):**
  - `ReportingViewModel::pageOrientation("Portrait") == QPageLayout::Portrait`
  - `ReportingViewModel::pageOrientation("Landscape") == QPageLayout::Landscape`
  - `ReportingViewModel::pageOrientation("") == QPageLayout::Portrait`
  - `ReportingViewModel::pageOrientation("garbage") == QPageLayout::Portrait`

  These four cases directly assert decision 4's independence: even though only `"Portrait"`/`"Landscape"` can reach the helper via `m_orientation` in production (decision 3 guarantees that), the helper itself must degrade any other input to Portrait rather than assuming pre-validated input.

Because `exportPdf`/`printReport` open native file/print dialogs (`QPdfWriter` to a real path, `QPrintDialog::exec()`), the *device configuration* itself (`writer.setPageOrientation(...)` actually taking effect on the object, `QPrintDialog` opening with the printer already carrying the orientation) is not something an offscreen `QtTest` run can assert without driving a real dialog — this is the same category of untestable-offscreen concern the sibling `2026-08-31-...` spec's §8.4 flags for the `QChartView` screen clamp. The unit-testable surface here is fully covered by the property/setter/helper cases above; the device-wiring itself is covered by the manual smoke (§6.3).

### 6.2 QuickTest (`qt-app/quick/tests/tst_qml_admin.qml`)

`test_paletteComboHasAccessibleNameAndWrites` (`:2718-2723`) is the direct template — it locates the combo by `objectName` via `findChild`, asserts `accessibleName`, calls `combo.selectValue(...)`, and asserts the stub VM's property updated. Add a mirrored case:

```qml
function test_orientationComboHasAccessibleNameAndWrites() {
    var combo = findChild(reporting, "orientationCombo");
    verify(combo);
    compare(combo.accessibleName, "Report orientation");
    combo.selectValue("Landscape");
    compare(reportingStub.orientation, "Landscape");
}
```

This exercises the same fixture (`reportingStub`, a plain-QML VM stub per the project's MVVM testing convention — screens receive their VM as `property var vm` precisely so QuickTests can inject such a stub) and the same `reporting` root already used by the palette/chart-type combo tests in this file, so no new test fixture or stub property is needed beyond adding `orientation`/`orientations`/`setOrientation` to whatever stub object the existing `paletteCombo`/`chartTypeCombo` tests already use (the stub already carries `palette`/`chartType` analogues — extend it the same way).

### 6.3 Manual `WITSQuick.exe` smoke — MANDATORY release gate

Neither `QtTest` nor `qmltest` can observe the actual rendered PDF page geometry, the actual `QPrintDialog`'s on-screen initial orientation state, or genuine chart reflow at a different aspect ratio — offscreen Qt Test runs have no physical screen or interactive dialog to inspect, the same rationale the project's existing export-path testing already relies on for anything downstream of `QPagedPaintDevice`/`QChartView` rendering (per the project memory on the QtChart export screen clamp, and per the sibling `2026-08-31-...` spec's manual-smoke rationale). Before this slice is done, run `WITSQuick.exe` and, over a range with data:

1. **Landscape PDF:** pick "Landscape" in the orientation combo, export a PDF. Confirm the page is landscape-oriented, all content (header, tiles, tables, charts) reflowed to the wider page with no clipping or blank charts.
2. **Print dialog:** pick "Landscape", click Print. Confirm the native print dialog opens already showing Landscape as the selected orientation (not just that a landscape page eventually prints) — this is the one behavior offscreen tests structurally cannot see (decision 5's ordering requirement).
3. **Portrait (default):** with orientation left at "Portrait" (or explicitly reselected), export a PDF and print. Confirm output is byte-for-byte the same shape as today's unchanged Portrait behavior — no regression for the default path.
4. **Excel unaffected:** export Excel with "Landscape" selected in the combo; confirm the `.xlsx` output is identical to before this feature (orientation has no effect on it).
5. **Legacy `WITS.exe`:** confirm its own PDF/Print export still produces Landscape output unconditionally, with no orientation combo present — unchanged (§5.6).
6. **Per-session reset:** close and relaunch `WITSQuick.exe`; confirm the orientation combo shows "Portrait" again (decision 2 — no persistence).

Synthetic data only — no real student PII, per the project security-hygiene rule.

## 7. Files-touched map

| File | Change |
|---|---|
| `qt-app/quick/viewmodels/ReportingViewModel.h` | New `Q_PROPERTY orientation`/`orientations`; new `orientation()`/`orientations()` readers; new `Q_INVOKABLE void setOrientation(const QString&)` declaration; new `orientationChanged()` signal; new private static `pageOrientation(const QString&)` declaration; new `#include <QPageLayout>`; new `QString m_orientation = QStringLiteral("Portrait");` member near `m_palette`/`m_chartType`. |
| `qt-app/quick/viewmodels/ReportingViewModel.cpp` | New `setOrientation` (reject-invalid guard, decision 3); new `pageOrientation` static (decision 4); `exportPdf` gains one line (`writer.setPageOrientation(...)`) after `setPageSize`; `printReport` gains one line (`printer.setPageOrientation(...)`) between `QPrinter` construction and `QPrintDialog` construction (decision 5). |
| `qt-app/quick/qml/admin/ReportingScreen.qml` | New `orientationCombo` `LComboBox` in `exportRow`, mirroring `chartTypeCombo`, inserted between the existing combos and the filling spacer. |
| `qt-app/quick/tests/tst_reportingviewmodel.cpp` | New cases (each declared under `private slots:` AND defined): default `"Portrait"`; valid `setOrientation` emits + updates; invalid/blank `setOrientation` rejected with **zero** signal emissions; `orientations()` contents; four `pageOrientation()` mapping cases. |
| `qt-app/quick/tests/tst_qml_admin.qml` | New `test_orientationComboHasAccessibleNameAndWrites`, mirroring `test_paletteComboHasAccessibleNameAndWrites`; stub VM gains `orientation`/`orientations`/`setOrientation`. |
| `qt-app/core/reportrenderer.{h,cpp}` | **Unchanged.** `paintReport` already derives layout from `device->pageLayout().paintRectPixels(resolution)` (§2.3, §5.3) — no orientation-aware code needed. |
| `qt-app/adminwindow.cpp` | **Unchanged.** Separate export path; already hardcodes `QPageLayout::Landscape` unconditionally for PDF and Print (`:1705`, `:1741`); does not reference `ReportingViewModel` (§5.6). |

No new source files, no new CMake targets — the change extends existing `ReportingViewModel` signatures/bodies and one QML file only. This slice runs its own `superpowers:writing-plans` → subagent-driven TDD build → `/claude-review` → `create-pr` cycle, per the project workflow rule.
