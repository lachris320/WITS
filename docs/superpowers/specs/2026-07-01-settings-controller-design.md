# Design Spec: SettingsController Extraction

**Date:** 2026-07-01
**Status:** Approved
**Scope:** Step 1 of the WITS admin window structural refactor

---

## Context

`adminWindow` is currently a ~4,000-line God Object that handles school settings,
student management, reporting, visitor logs, and bulk imports all in one class.
This spec covers extracting the Settings section into a dedicated `SettingsController`,
establishing the architecture pattern that will be repeated for `DatabaseController`,
`ReportsController`, `StudentController`, and `VisitorController`.

**Goal:** behavior-identical extraction — no new features, no regressions. Unit tests
are a separate follow-up step after the extraction is verified working.

---

## Architecture

```
                adminWindow  (View only)
                     │
         ┌───────────┴───────────┐
         │                       │
populateSettingsForm()   collectSettingsForm()
         │                       │
         └───────────┬───────────┘
                     │
              SettingsData
                     │
                     ▼
            SettingsController
              ├── load()
              ├── save()
              ├── importLogo(path)
              ├── importPoster(path)
              ├── validation
              └── QSettings
```

---

## New Files

| File | Purpose |
|------|---------|
| `qt-app/settingsdata.h` | Plain struct — in-memory snapshot of all settings values |
| `qt-app/settingscontroller.h` | QObject controller — declaration |
| `qt-app/settingscontroller.cpp` | QObject controller — implementation |

`adminwindow.h` and `adminwindow.cpp` are modified (not replaced).

---

## `SettingsData` Struct

Defined in `settingsdata.h`. A plain value container with no logic, no persistence,
and no Qt dependency beyond `QString` and `bool`.

```cpp
struct SettingsData
{
    // School
    QString schoolName;
    QString schoolAddress;
    QString fontFamily;
    int     fontSize           = 12;
    QString logoPath;
    QString posterPath;

    // Administrator
    QString adminName;
    QString adminPosition;

    // Library
    int     libraryOpenHour   = 8;
    int     libraryCloseHour  = 17;

    // Features
    bool    guestLoginEnabled = false;
};
```

---

## `SettingsController` — Public API

```cpp
class SettingsController : public QObject
{
    Q_OBJECT
public:
    explicit SettingsController(QObject *parent = nullptr);

    // Persistence
    SettingsData load();
    bool         save(const SettingsData &data);

    // File import — View opens QFileDialog and passes the chosen path here.
    // Controller copies the file to the app data directory and returns the
    // destination path, or an empty string on failure.
    QString importLogo(const QString &sourcePath);
    QString importPoster(const QString &sourcePath);

signals:
    void settingsSaved(const SettingsData &data);
    void logoChanged(const QString &destinationPath);
    void posterChanged(const QString &destinationPath);
    void importError(const QString &message);   // emitted when importLogo/importPoster fails
};
```

---

## `SettingsController` — Responsibilities

### `load()`
- Reads all keys from `QSettings` into a `SettingsData` struct.
- Returns the struct with defaults for any missing keys (matching `SettingsData`
  field defaults).
- Never touches the UI.

### `save(const SettingsData &)`
- Writes all fields of the struct to `QSettings`.
- Emits `settingsSaved(data)` on success.
- Returns `true` on success, `false` if `QSettings` is not writable.

### `importLogo(const QString &sourcePath)` / `importPoster(const QString &sourcePath)`
- Validates that `sourcePath` exists (`QFile::exists`).
- Validates that the file is a loadable image (`QImage::isNull`).
- Copies the file to the app data directory
  (`QStandardPaths::AppDataLocation`).
- Returns the destination path on success, empty string on failure.
- Emits `logoChanged` / `posterChanged` on success.
- Emits `importError(message)` on any failure (file missing, invalid image,
  copy failed).
- Does **not** open any dialog or touch any widget.

---

## `adminWindow` Changes

### New members

```cpp
// In adminwindow.h (private section)
SettingsController* const m_settingsController;   // owned, never replaced
SettingsData              m_currentSettings;       // working copy
```

`m_settingsController` is initialized in the constructor's member-initializer list
and is `const` — `adminWindow` cannot swap it out after construction.

### New private methods

```cpp
void bindSettingsSignals();           // all connect() calls for settings
void populateSettingsForm(const SettingsData &data);  // fill widgets from struct
SettingsData collectSettingsForm();   // read widgets into struct
```

### Constructor shape

```cpp
adminWindow::adminWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::adminWindow)
    , m_settingsController(new SettingsController(this))
    // ... other controllers will follow here
{
    ui->setupUi(this);
    bindSettingsSignals();
    m_currentSettings = m_settingsController->load();
    populateSettingsForm(m_currentSettings);
}
```

### `bindSettingsSignals()`

All `connect()` calls for the settings tab live here — nothing else. Example shape:

```cpp
void adminWindow::bindSettingsSignals()
{
    // Controller → View
    connect(m_settingsController, &SettingsController::settingsSaved,
            this, &adminWindow::onSettingsSaved);
    connect(m_settingsController, &SettingsController::logoChanged,
            this, &adminWindow::onLogoChanged);
    connect(m_settingsController, &SettingsController::posterChanged,
            this, &adminWindow::onPosterChanged);
    connect(m_settingsController, &SettingsController::importError,
            this, &adminWindow::onSettingsImportError);
}
```

### `QFileDialog` stays in the View

When the user clicks Browse for logo:

```cpp
void adminWindow::onBrowseLogoClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Logo"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;

    const QString dest = m_settingsController->importLogo(path);
    if (!dest.isEmpty()) {
        m_currentSettings.logoPath = dest;
        // preview update handled by onLogoChanged signal
    }
}
```

The controller never opens a dialog. The View never copies a file.

### What is removed from `adminWindow`

- All `QSettings` read/write calls in the settings tab slots
- All file-copy logic for logo and poster
- All file-existence and image-validity checks
- The `QFileDialog` calls for logo/poster (moved to the browse slot wrappers above)

---

## Data Flow

### On admin window open
```
adminWindow constructor
  → m_settingsController->load()        returns SettingsData
  → m_currentSettings = data
  → populateSettingsForm(m_currentSettings)  fills widgets
```

### On Apply / Save clicked
```
adminWindow::onApplyClicked()
  → m_currentSettings = collectSettingsForm()
  → m_settingsController->save(m_currentSettings)
        → writes QSettings
        → emits settingsSaved(data)
  → adminWindow::onSettingsSaved(data)
        → update kiosk display, etc.
```

### On Browse Logo clicked
```
adminWindow::onBrowseLogoClicked()
  → QFileDialog::getOpenFileName(...)   returns sourcePath
  → m_settingsController->importLogo(sourcePath)
        → validates file
        → copies to AppDataLocation
        → emits logoChanged(destPath)   or importError(msg)
  → adminWindow::onLogoChanged(destPath)
        → update logo preview widget
        → m_currentSettings.logoPath = destPath
```

---

## Files to Modify in `CMakeLists.txt`

Add to the `SOURCES` / `HEADERS` lists:

```cmake
settingsdata.h
settingscontroller.h
settingscontroller.cpp
```

No new Qt modules required — `QSettings`, `QFile`, `QImage`, and
`QStandardPaths` are all already available via `Qt::Widgets` / `Qt::Core`.

---

## Out of Scope (This Step)

- Unit tests (separate follow-up spec)
- Any change to `MainWindow` or the kiosk screen
- Extraction of any other admin tab (Database, Reports, Students, Visitors)
- New settings fields not already present in `adminWindow`
- Backend URL configurability (separate spec)

---

## Verification Criteria

The extraction is complete when:

1. The app builds with no new warnings or errors.
2. The Settings tab in the admin window behaves identically to before:
   - Load: all fields populated correctly on open.
   - Save: changes persist across app restart.
   - Logo/poster: browse → pick → preview updates → save → persists.
   - Library hours and guest login toggle work as before.
3. `adminWindow` contains no direct `QSettings` calls in the settings section.
4. `adminWindow` contains no file-copy or image-validation logic.
5. `SettingsController` contains no `QFileDialog` calls.
