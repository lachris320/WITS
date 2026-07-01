# SettingsController Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract all settings persistence and file-import logic from `adminWindow` into a dedicated `SettingsController : QObject`, leaving `adminWindow` as a pure View for the Settings tab.

**Architecture:** A plain `SettingsData` struct carries all settings values in memory. `SettingsController` owns `QSettings` and the file-copy logic. `adminWindow` calls `load()` on open and `save()` on Apply; browse slots pass the file path to the controller and let signals update the UI.

**Tech Stack:** Qt 6, C++17, `QSettings`, `QFile`, `QImage`, `QStandardPaths`, `QFileDialog` (stays in View).

## Global Constraints

- Qt 6.11.1 MinGW; build with `cmake -S qt-app -B qt-app/build -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64` then `cmake --build qt-app/build`.
- C++17 — use `#pragma once`, brace-initialised struct defaults, `QLatin1String`.
- QSettings org/app strings are exactly `"MyCompany"` and `"MyApp"` (existing codebase convention — do not change).
- No new Qt modules. `QSettings`, `QFile`, `QImage`, `QStandardPaths`, `QDir` are all in `Qt::Core`; `QFileDialog` is in `Qt::Widgets` — both already linked.
- Do **not** touch QSettings calls inside `makeLineChartImage()` (line 247), `exportReportToPDF()` (line 2254), or `exportReportToExcel()` (line 2549) — those are the Reports section and are out of scope for this step.
- No unit tests in this plan — they are a separate follow-up step.
- Commit via the `commit` skill after each task, never raw `git add/commit`.
- **Three intentional behavior improvements** (not regressions — document as such in manual verification):
  1. School name, address, and font now populate correctly on open. The old constructor read these from QSettings into local variables but never wrote them to the form widgets; the new `populateSettingsForm` fixes this silently-broken load path.
  2. `guestLoginEnabled` is now persisted under `features/guestLogin` (was emitted but never saved to QSettings).
  3. Logo/poster are now copied to `QStandardPaths::AppDataLocation` before the path is stored (old code saved the raw source path). As a consequence, browse-then-close-without-Apply no longer persists paths — paths are only stored on Apply.
- **`changesMade` / dirty-window on open:** `populateSettingsForm` calls `setText` on `ui->schoolName` and `ui->address`, which fire their `textChanged` → `changesMade = true` connections (lines 495–500). This marks the window dirty immediately on open. Wrap the entire `populateSettingsForm` body in a `QSignalBlocker` for each affected widget, or set `changesMade = false` after calling `populateSettingsForm` in the constructor body. The recommended approach: add `changesMade = false;` as the last line of the constructor body (after `populateSettingsForm`), since the constructor runs before the window is shown.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `qt-app/settingsdata.h` | **Create** | Plain struct — in-memory snapshot of all settings values |
| `qt-app/settingscontroller.h` | **Create** | `SettingsController` class declaration |
| `qt-app/settingscontroller.cpp` | **Create** | `load()`, `save()`, `importLogo()`, `importPoster()` implementations |
| `qt-app/adminwindow.h` | **Modify** | Add `m_settingsController`, `m_currentSettings`, new method/slot declarations; remove dead `loadSettings()`/`saveSettings()` declarations |
| `qt-app/adminwindow.cpp` | **Modify** | Wire controller in constructor, implement `populateSettingsForm`, `collectSettingsForm`, `bindSettingsSignals`, new slot bodies; replace old settings slots |
| `qt-app/CMakeLists.txt` | **Modify** | Add three new files to the `qt_add_executable` block |

---

## Task 1: New files — SettingsData, SettingsController, CMakeLists

**Files:**
- Create: `qt-app/settingsdata.h`
- Create: `qt-app/settingscontroller.h`
- Create: `qt-app/settingscontroller.cpp`
- Modify: `qt-app/CMakeLists.txt`

**Interfaces produced (used by Task 2):**
- `SettingsData` struct with fields: `schoolName`, `schoolAddress`, `fontFamily`, `fontSize` (int, default 12), `logoPath`, `posterPath`, `adminName`, `adminPosition`, `libraryOpenHour` (int, default 8 — 24 h), `libraryCloseHour` (int, default 17 — 24 h), `guestLoginEnabled` (bool, default false).
- `SettingsController::load() → SettingsData`
- `SettingsController::save(const SettingsData &) → bool`
- `SettingsController::importLogo(const QString &sourcePath) → QString`
- `SettingsController::importPoster(const QString &sourcePath) → QString`
- Signals: `settingsSaved(const SettingsData &)`, `logoChanged(const QString &)`, `posterChanged(const QString &)`, `importError(const QString &)`

- [ ] **Step 1: Create `qt-app/settingsdata.h`**

```cpp
#pragma once
#include <QString>

struct SettingsData
{
    // School
    QString schoolName;
    QString schoolAddress;
    QString fontFamily;
    int     fontSize          = 12;
    QString logoPath;
    QString posterPath;

    // Administrator
    QString adminName;
    QString adminPosition;

    // Library hours stored in 24-hour format (0–23)
    int     libraryOpenHour  = 8;
    int     libraryCloseHour = 17;

    // Features
    bool    guestLoginEnabled = false;
};
```

- [ ] **Step 2: Create `qt-app/settingscontroller.h`**

```cpp
#pragma once
#include <QObject>
#include "settingsdata.h"

class SettingsController : public QObject
{
    Q_OBJECT
public:
    explicit SettingsController(QObject *parent = nullptr);

    SettingsData load();
    bool         save(const SettingsData &data);

    QString importLogo(const QString &sourcePath);
    QString importPoster(const QString &sourcePath);

signals:
    void settingsSaved(const SettingsData &data);
    void logoChanged(const QString &destinationPath);
    void posterChanged(const QString &destinationPath);
    void importError(const QString &message);

private:
    QString importImageFile(const QString &sourcePath, const QString &type);
};
```

- [ ] **Step 3: Create `qt-app/settingscontroller.cpp`**

```cpp
#include "settingscontroller.h"
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
{}

SettingsData SettingsController::load()
{
    QSettings s(QLatin1String("MyCompany"), QLatin1String("MyApp"));
    SettingsData d;
    d.schoolName        = s.value(QLatin1String("school/name"),       QLatin1String("")).toString();
    d.schoolAddress     = s.value(QLatin1String("school/address"),    QLatin1String("")).toString();
    d.fontFamily        = s.value(QLatin1String("school/fontFamily"), QLatin1String("")).toString();
    d.fontSize          = s.value(QLatin1String("school/fontSize"),   12).toInt();
    d.logoPath          = s.value(QLatin1String("school/logoPath"),   QLatin1String("")).toString();
    d.posterPath        = s.value(QLatin1String("school/posterPath"), QLatin1String("")).toString();
    d.adminName         = s.value(QLatin1String("admin/name"),        QLatin1String("")).toString();
    d.adminPosition     = s.value(QLatin1String("admin/position"),    QLatin1String("")).toString();
    d.libraryOpenHour   = s.value(QLatin1String("library/openHour"),  8).toInt();
    d.libraryCloseHour  = s.value(QLatin1String("library/closeHour"),17).toInt();
    d.guestLoginEnabled = s.value(QLatin1String("features/guestLogin"), false).toBool();
    return d;
}

bool SettingsController::save(const SettingsData &data)
{
    QSettings s(QLatin1String("MyCompany"), QLatin1String("MyApp"));
    s.setValue(QLatin1String("school/name"),        data.schoolName);
    s.setValue(QLatin1String("school/address"),     data.schoolAddress);
    s.setValue(QLatin1String("school/fontFamily"),  data.fontFamily);
    s.setValue(QLatin1String("school/fontSize"),    data.fontSize);
    s.setValue(QLatin1String("school/logoPath"),    data.logoPath);
    s.setValue(QLatin1String("school/posterPath"),  data.posterPath);
    s.setValue(QLatin1String("admin/name"),         data.adminName);
    s.setValue(QLatin1String("admin/position"),     data.adminPosition);
    s.setValue(QLatin1String("library/openHour"),   data.libraryOpenHour);
    s.setValue(QLatin1String("library/closeHour"),  data.libraryCloseHour);
    s.setValue(QLatin1String("features/guestLogin"),data.guestLoginEnabled);
    s.sync();
    if (s.status() != QSettings::NoError)
        return false;
    emit settingsSaved(data);
    return true;
}

QString SettingsController::importLogo(const QString &sourcePath)
{
    return importImageFile(sourcePath, QLatin1String("logo"));
}

QString SettingsController::importPoster(const QString &sourcePath)
{
    return importImageFile(sourcePath, QLatin1String("poster"));
}

QString SettingsController::importImageFile(const QString &sourcePath, const QString &type)
{
    if (!QFile::exists(sourcePath)) {
        emit importError(tr("File not found: %1").arg(sourcePath));
        return {};
    }
    QImage img(sourcePath);
    if (img.isNull()) {
        emit importError(tr("Not a valid image: %1").arg(sourcePath));
        return {};
    }
    const QString destDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(destDir);
    const QString destPath = destDir + QLatin1Char('/') + type
                             + QLatin1Char('_') + QFileInfo(sourcePath).fileName();
    if (QFile::exists(destPath))
        QFile::remove(destPath);
    if (!QFile::copy(sourcePath, destPath)) {
        emit importError(tr("Could not copy to: %1").arg(destPath));
        return {};
    }
    if (type == QLatin1String("logo"))
        emit logoChanged(destPath);
    else
        emit posterChanged(destPath);
    return destPath;
}
```

- [ ] **Step 4: Update `qt-app/CMakeLists.txt` — add new files to the `qt_add_executable` block**

In `CMakeLists.txt`, the `qt_add_executable(WITS ...)` block (lines 31–40) already lists files after `${PROJECT_SOURCES}`. Add the three new files there:

```cmake
    qt_add_executable(WITS
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        guestwindow.h guestwindow.cpp guestwindow.ui
        reportpreviewdialog.h reportpreviewdialog.cpp
        busyindicator.h busyindicator.cpp
        attachFilesDialog.ui
        attachfilesdialog.h attachfilesdialog.cpp
        settingsdata.h
        settingscontroller.h settingscontroller.cpp
    )
```

- [ ] **Step 5: Build to verify new files compile cleanly**

```
cmake -S qt-app -B qt-app/build -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build qt-app/build
```

Expected: build succeeds, no warnings or errors related to the new files. The app executable is unchanged in behavior (Task 1 adds files but nothing calls them yet).

- [ ] **Step 6: Commit via the `commit` skill**

Invoke the `commit` skill. Proposed grouping:

- `qt-app/settingsdata.h`, `qt-app/settingscontroller.h`, `qt-app/settingscontroller.cpp`, `qt-app/CMakeLists.txt` → one commit:

```
feat(settings): introduce SettingsController and SettingsData

Extract settings persistence into a dedicated QObject controller.
SettingsData is a plain value struct (no Qt dependency beyond QString).
SettingsController owns QSettings and the logo/poster file-import logic.
adminWindow wiring follows in the next commit.
```

---

## Task 2: Wire SettingsController into adminWindow

**Files:**
- Modify: `qt-app/adminwindow.h`
- Modify: `qt-app/adminwindow.cpp`

**Consumes from Task 1:**
- `SettingsData` struct (all fields as defined above)
- `SettingsController(QObject *parent)` constructor
- `SettingsController::load() → SettingsData`
- `SettingsController::save(const SettingsData &) → bool`
- `SettingsController::importLogo(const QString &) → QString`
- `SettingsController::importPoster(const QString &) → QString`
- Signals: `settingsSaved(const SettingsData &)`, `logoChanged(const QString &)`, `posterChanged(const QString &)`, `importError(const QString &)`

**Important context — existing adminwindow state before this task:**
- `adminWindow` inherits `QMainWindow` (not QDialog — the spec diagram was illustrative).
- Constructor body runs from line 406 to line 1134. Lines 1103–1133 load settings inline from `QSettings` directly; there is no separate `void adminWindow::loadSettings()` definition in the cpp (the declaration in the header is dead — remove it).
- `saveSettings()` is also declared in the header but never defined — remove it.
- `onApplyChangesBtnClicked()` (line 1632) writes all settings to `QSettings` inline and emits signals.
- `onSchoolLogoBrowseBtnClicked()` (line 1723) opens `QFileDialog`, saves the raw path to `QSettings`, shows `QMessageBox::information`.
- `onPosterBrowseBtnClicked()` (line 1752) opens `QFileDialog`, saves the raw path to `QSettings`, shows `QMessageBox::information`.
- `QSettings` calls in `makeLineChartImage()`, `exportReportToPDF()`, `exportReportToExcel()` — **do not touch** these.
- The existing self-connection in the constructor (`connect(this, &adminWindow::logoChanged, ...)`) updates `ui->schoolLabelLogo` — **keep it**.

- [ ] **Step 1: Update `qt-app/adminwindow.h`**

Add the two includes near the top (after existing includes):
```cpp
#include "settingsdata.h"
#include "settingscontroller.h"
```

In the `private:` section, remove the two dead declarations:
```cpp
void loadSettings(); // Helper to load saved settings into UI  ← DELETE THIS LINE
void saveSettings();                                            ← DELETE THIS LINE
```

In the `private:` section, add new members immediately after `bool reportEdited = false;`:
```cpp
    SettingsController* const m_settingsController;
    SettingsData              m_currentSettings;

    void bindSettingsSignals();
    void populateSettingsForm(const SettingsData &data);
    SettingsData collectSettingsForm();
```

In the `private slots:` section, add four new slots (place them near `onSchoolLogoBrowseBtnClicked`):
```cpp
    void onSettingsSaved(const SettingsData &data);
    void onLogoChanged(const QString &path);
    void onPosterChanged(const QString &path);
    void onSettingsImportError(const QString &message);
```

- [ ] **Step 2: Update the constructor initializer list in `qt-app/adminwindow.cpp`**

Locate line 406–408:
```cpp
adminWindow::adminWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::adminWindow)
```

Change to:
```cpp
adminWindow::adminWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::adminWindow)
    , m_settingsController(new SettingsController(this))
```

- [ ] **Step 3: Replace the inline settings-load block in the constructor body**

Locate lines 1103–1133 (the block that creates a local `QSettings`, reads school/admin/library values, and sets spinbox values):

```cpp
    QSettings settings("MyCompany", "MyApp");
    QString savedName = settings.value("admin/name", "").toString();
    // ... (all lines through the end of the constructor at line 1133)
    ui->closeHourSpinBox->setValue(closeHour12);

    if (!savedName.isEmpty())
        ui->adminNameLineEdit->setText(savedName);
    if (!savedPosition.isEmpty())
        ui->adminPositionLineEdit->setText(savedPosition);
}    // ← this closing brace ends the constructor at line 1134
```

Replace that entire block (keeping the final `}`) with:

```cpp
    bindSettingsSignals();
    m_currentSettings = m_settingsController->load();
    populateSettingsForm(m_currentSettings);
    changesMade = false;   // populateSettingsForm fires textChanged; reset the dirty flag
}
```

- [ ] **Step 4: Add `bindSettingsSignals()` implementation**

Add this function anywhere before `onApplyChangesBtnClicked()`:

```cpp
void adminWindow::bindSettingsSignals()
{
    connect(m_settingsController, &SettingsController::settingsSaved,
            this,                 &adminWindow::onSettingsSaved);
    connect(m_settingsController, &SettingsController::logoChanged,
            this,                 &adminWindow::onLogoChanged);
    connect(m_settingsController, &SettingsController::posterChanged,
            this,                 &adminWindow::onPosterChanged);
    connect(m_settingsController, &SettingsController::importError,
            this,                 &adminWindow::onSettingsImportError);
}
```

- [ ] **Step 5: Add `populateSettingsForm()` implementation**

Add immediately after `bindSettingsSignals()`:

```cpp
void adminWindow::populateSettingsForm(const SettingsData &d)
{
    ui->schoolName->setText(d.schoolName);
    ui->address->setText(d.schoolAddress);

    if (!d.fontFamily.isEmpty())
        ui->fontComboBox->setCurrentFont(QFont(d.fontFamily, d.fontSize));
    ui->spinBox->setValue(d.fontSize);

    if (!d.adminName.isEmpty())
        ui->adminNameLineEdit->setText(d.adminName);
    if (!d.adminPosition.isEmpty())
        ui->adminPositionLineEdit->setText(d.adminPosition);

    // QSettings stores 24 h; spinboxes display 12 h (1–12 AM / PM)
    const int openH12  = (d.libraryOpenHour  == 0)  ? 12 : d.libraryOpenHour;
    const int closeH12 = (d.libraryCloseHour == 12) ? 12 : d.libraryCloseHour - 12;
    ui->openHourSpinBox->setValue(openH12);
    ui->closeHourSpinBox->setValue(closeH12);

    ui->guestLoginCheckBox->setChecked(d.guestLoginEnabled);

    if (!d.logoPath.isEmpty() && QFile::exists(d.logoPath)) {
        QPixmap pix(d.logoPath);
        if (!pix.isNull())
            ui->schoolLabelLogo->setPixmap(
                pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}
```

- [ ] **Step 6: Add `collectSettingsForm()` implementation**

Add immediately after `populateSettingsForm()`:

```cpp
SettingsData adminWindow::collectSettingsForm()
{
    SettingsData d;
    d.schoolName    = ui->schoolName->text();
    d.schoolAddress = ui->address->text();
    d.fontFamily    = ui->fontComboBox->currentFont().family();
    d.fontSize      = ui->spinBox->value();
    d.adminName     = ui->adminNameLineEdit->text().trimmed();
    d.adminPosition = ui->adminPositionLineEdit->text().trimmed();

    // Spinboxes are 12 h (1–12 AM / PM) → convert to 24 h for storage
    const int openH12  = ui->openHourSpinBox->value();
    const int closeH12 = ui->closeHourSpinBox->value();
    d.libraryOpenHour  = (openH12  == 12) ? 0  : openH12;
    d.libraryCloseHour = (closeH12 == 12) ? 12 : closeH12 + 12;

    d.guestLoginEnabled = ui->guestLoginCheckBox->isChecked();

    // logoPath and posterPath are maintained by onLogoChanged/onPosterChanged
    d.logoPath   = m_currentSettings.logoPath;
    d.posterPath = m_currentSettings.posterPath;
    return d;
}
```

- [ ] **Step 7: Replace `onApplyChangesBtnClicked()`**

Find the existing body (lines 1632–1693) and replace it entirely:

```cpp
void adminWindow::onApplyChangesBtnClicked()
{
    m_currentSettings = collectSettingsForm();
    if (!m_settingsController->save(m_currentSettings))
        return;

    updatePreviewLabel();
    changesMade = false;

    QString msg = tr("Changes applied successfully.");
    if (ui->clearAttendanceCheckBox->isChecked()) {
        emit clearAttendanceRequested();
        msg += tr("\n\nAttendance panel has been cleared.");
        ui->clearAttendanceCheckBox->setChecked(false);
    }

    QMessageBox::information(this, tr("Saved"), msg);
    this->close();
}
```

- [ ] **Step 8: Add `onSettingsSaved()` slot**

Add after `onApplyChangesBtnClicked()`:

```cpp
void adminWindow::onSettingsSaved(const SettingsData &data)
{
    QFont selectedFont(data.fontFamily, data.fontSize);
    emit schoolInfoUpdated(data.schoolName, data.schoolAddress, selectedFont);
    if (!data.logoPath.isEmpty() && QFile::exists(data.logoPath))
        emit logoChanged(data.logoPath);
    if (!data.posterPath.isEmpty() && QFile::exists(data.posterPath))
        emit posterChanged(data.posterPath);
    emit guestLoginToggled(data.guestLoginEnabled);
}
```

- [ ] **Step 9: Replace `onSchoolLogoBrowseBtnClicked()`**

Find the existing body (lines 1723–1750) and replace it entirely:

```cpp
void adminWindow::onSchoolLogoBrowseBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Logo"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;
    const QString dest = m_settingsController->importLogo(path);
    if (!dest.isEmpty())
        QMessageBox::information(this, tr("Changes Applied"),
                                 tr("School logo has been uploaded successfully!"));
}
```

- [ ] **Step 10: Add `onLogoChanged()` slot**

Add after `onSchoolLogoBrowseBtnClicked()`:

```cpp
void adminWindow::onLogoChanged(const QString &path)
{
    m_currentSettings.logoPath = path;
    ui->schLogo_previewLabel->setPixmap(
        QPixmap(path).scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
```

- [ ] **Step 11: Replace `onPosterBrowseBtnClicked()`**

Find the existing body (lines 1752–1779) and replace it entirely:

```cpp
void adminWindow::onPosterBrowseBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Poster"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;
    const QString dest = m_settingsController->importPoster(path);
    if (!dest.isEmpty())
        QMessageBox::information(this, tr("Changes Applied"),
                                 tr("Poster has been updated successfully!"));
}
```

- [ ] **Step 12: Add `onPosterChanged()` and `onSettingsImportError()` slots**

Add after `onPosterBrowseBtnClicked()`:

```cpp
void adminWindow::onPosterChanged(const QString &path)
{
    m_currentSettings.posterPath = path;
    ui->posterPreviewLabel->setPixmap(
        QPixmap(path).scaled(
            ui->posterPreviewLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

void adminWindow::onSettingsImportError(const QString &message)
{
    QMessageBox::warning(this, tr("Import Error"), message);
}
```

- [ ] **Step 13: Build and verify**

```
cmake --build qt-app/build
```

Expected: build completes with **no new warnings or errors**. If the compiler complains about `loadSettings` or `saveSettings` being declared but not defined, the removals in Step 1 of the header fixed the declarations — confirm they are gone from the header.

- [ ] **Step 14: Run the app and exercise the Settings tab**

Launch `qt-app/build/WITS.exe`. Work through each criterion:

1. Open admin window → Settings tab loads with previously saved values in ALL fields — school name, address, font, size, admin name/position, library hours, guest login. *(Intentional fix: the old constructor never wrote school name/address/font to the form.)*
2. Open admin window with saved values → close without touching anything → NO "Unsaved Changes" dialog. *(Verifies the `changesMade = false` line after `populateSettingsForm`.)*
3. Change school name, address, font, size → click Apply → reopen admin → values persisted.
4. Click Browse Logo → pick an image → preview updates → click Apply → logo persists across restart.
5. Click Browse Poster → pick an image → preview updates → click Apply → poster persists across restart.
6. Browse logo → close window WITHOUT Apply → reopen → logo from previous Apply still shown. *(New behavior: browse-without-Apply no longer persists — intentional.)*
7. Change open/close hour spinboxes → Apply → reopen → same hours shown.
8. Toggle guest login → Apply → expected behaviour in kiosk → reopen → checkbox state persists. *(Intentional new persistence under `features/guestLogin`.)*
9. Click Apply without picking a file → no crash, no spurious error dialog.
10. Confirm `adminWindow` source no longer contains direct `QSettings` construction in the settings-tab methods (grep for `QSettings settings` in the settings-tab methods).

- [ ] **Step 15: Commit via the `commit` skill**

Proposed commit:

```
refactor(settings): wire SettingsController into adminWindow

adminWindow is now a pure View for the Settings tab. QSettings,
file-copy, and image-validation logic live entirely in
SettingsController. The three settings slots (apply, browse logo,
browse poster) are replaced with thin wrappers that delegate to the
controller and respond to its signals.

guestLoginEnabled is now persisted under features/guestLogin in
QSettings (was previously only emitted, never stored).
```

---

## Post-Task: Run /claude-review

After both tasks are committed and the app passes manual verification, run `/claude-review` (via the Skill tool) before opening the PR. Fix any Critical or Important findings, then proceed to `create-pr`.
