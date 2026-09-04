# LOAMS 2.0 Photo Display (Phase 4d) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show the real student photo — with an initials fallback — on the kiosk signed-in hero card and in admin search results, unifying two divergent server photo shapes behind one presentational `LAvatar` component.

**Architecture:** C++ does all URL work (Approach A): `SearchResultsModel` joins the relative `photo` to `ApiConfig::baseUrl()` in a new `PhotoRole`; `KioskViewModel` exposes the already-absolute `photo_url` plus precomputed `currentInitials`. Circular image cropping reuses `LLogoCircle`'s core-QtQuick `Canvas` technique, extracted into a shared `LCircleImage` primitive (this repo links no shader-effects module). `LAvatar` is purely presentational: it takes an absolute-or-empty `source` + a precomputed `initials` string and decides image-vs-initials (empty / path-ends-`default.jpg` / `Image.Error`).

**Tech Stack:** Qt 6.11.1, C++17, QML (Qt Quick), CMake+Ninja (MinGW). Qt Test + Qt Quick Test via `wits_add_qttest()`, run under ctest.

**Spec:** `docs/superpowers/specs/2026-09-04-loams2-phase4d-photo-display-design.md` (claude-review APPROVED).

## Global Constraints

- **MVVM:** ViewModels are the ONLY QML-facing C++; QML never calls a `witscore` controller or `ApiConfig` directly. `LAvatar`/`LCircleImage` are purely presentational (primitive props, no `vm`).
- **Theming:** `Theme.qml` (pragma Singleton) is the single source of every visual token. ZERO raw hex outside `Theme.qml`; opacity variants use `Qt.alpha(Theme.<token>, a)`.
- **Naming:** QML types + C++ ViewModel/model classes are `PascalCase`; C++ members `m_camelCase`.
- **Tests:** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add `OFFSCREEN` for any GUI/Quick/painting test.
- **No shader-effects module:** do NOT `import QtQuick.Effects` / `Qt5Compat.GraphicalEffects`; circular crop is `Canvas`-based only (offscreen-safe).
- **Build/test commands** (from `qt-app/build/`): configure `cmake -S qt-app -B qt-app/build`, build `cmake --build qt-app/build`, test `ctest --test-dir qt-app/build --output-on-failure`. Run a single test with `ctest --test-dir qt-app/build -R <name> --output-on-failure`.
- **Commit** via the `commit` skill (Conventional Commits). Each commit ends with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` (this session's attribution rule).

---

### Task 1: `StudentRecord.photo` + `parseSearchResponse`

Add the additive `photo` field to the shared record and parse it. Guard that it does NOT leak into the CSV export or the bulk-update payload (both serialize `StudentRecord` back).

**Files:**
- Modify: `qt-app/core/studentdata.h` (add `photo` to `struct StudentRecord`)
- Modify: `qt-app/core/studentcontroller.cpp:62-74` (parse loop in `parseSearchResponse`)
- Test: `qt-app/tests/tst_studentcontroller.cpp`

**Interfaces:**
- Produces: `StudentRecord::photo` (`QString`, relative path, empty on NULL); `parseSearchResponse` populates it from JSON key `"photo"`.

- [ ] **Step 1: Write the failing test**

Add to `qt-app/tests/tst_studentcontroller.cpp` (new private-slot test; append alongside the existing `parseSearchResponse` tests):

```cpp
void parseSearchResponsePopulatesPhoto()
{
    const QByteArray raw = R"({
        "status":"success",
        "students":[
            {"school_id":"2023-1","name":"Maria Santos","photo":"uploads/students/2023-1.jpg"},
            {"school_id":"2023-2","name":"Jose Ramirez","photo":""},
            {"school_id":"2023-3","name":"Ana Cruz"}
        ],
        "searchTerm":"a"
    })";
    QList<StudentRecord> recs;
    QString msg, term;
    const SearchOutcome out = StudentController::parseSearchResponse(raw, recs, msg, term);
    QCOMPARE(out, SearchOutcome::Results);
    QCOMPARE(recs.size(), 3);
    QCOMPARE(recs[0].photo, QStringLiteral("uploads/students/2023-1.jpg"));
    QCOMPARE(recs[1].photo, QString());   // empty string -> empty
    QCOMPARE(recs[2].photo, QString());   // absent key   -> empty
}

void csvAndBulkUpdateDoNotEmitPhoto()
{
    StudentRecord r;
    r.code = "C1"; r.schoolId = "2023-1"; r.name = "Maria Santos";
    r.course = "BSCE"; r.department = "CE"; r.yearLevel = "3";
    r.gender = "F"; r.status = "Regular"; r.photo = "uploads/students/2023-1.jpg";
    // CSV must not gain a photo column.
    const QByteArray csv = StudentController::toCsv({ r });
    QVERIFY(!csv.contains("uploads/students/2023-1.jpg"));
    QVERIFY(!csv.toLower().contains("photo"));
}
```

Register both names in the test's slots list if the file uses an explicit `private slots:` block (it does — add the two method declarations there).

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: FAIL — `recs[0].photo` compile error (`StudentRecord` has no `photo`) or empty mismatch.

- [ ] **Step 3: Add the field**

In `qt-app/core/studentdata.h`, inside `struct StudentRecord`, after `int visits = 0;`:

```cpp
    QString photo;   // relative path from search_students.php; empty on NULL.
                     // Search-read-only: NOT serialized back (toCsv/bulkUpdate).
```

- [ ] **Step 4: Parse it**

In `qt-app/core/studentcontroller.cpp`, in the `parseSearchResponse` per-student loop (after `rec.visits = s["visits"].toInt();`):

```cpp
        rec.photo      = s["photo"].toString();   // additive; absent/empty -> ""
```

Confirm `toCsv()` (around line 130) and `bulkUpdateStudents()` (around line 252) build their column/field lists explicitly and were NOT touched — they must not reference `rec.photo`.

- [ ] **Step 5: Run tests to verify they pass**

Run: `ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: PASS (all existing + the two new tests).

- [ ] **Step 6: Commit** (via the `commit` skill)

Groups: `qt-app/core/studentdata.h`, `qt-app/core/studentcontroller.cpp`, `qt-app/tests/tst_studentcontroller.cpp`. Subject e.g. `feat(core): parse additive photo field in search response`.

---

### Task 2: `SearchResultsModel` `PhotoRole` (C++ joins the base URL)

Expose `model.photo` as an **absolute** URL (relative path joined to `ApiConfig::baseUrl()`), empty when the record has no photo. This is the single place search photos become absolute.

**Files:**
- Modify: `qt-app/quick/models/SearchResultsModel.h` (add `PhotoRole` to the enum)
- Modify: `qt-app/quick/models/SearchResultsModel.cpp` (include `apiconfig.h`; `data()` + `roleNames()`)
- Create: `qt-app/quick/tests/tst_searchresultsmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (register the new test)

**Interfaces:**
- Consumes: `StudentRecord::photo` (Task 1).
- Produces: `SearchResultsModel::PhotoRole`; QML role name `"photo"` → absolute URL string or `""`.

- [ ] **Step 1: Write the failing test**

Create `qt-app/quick/tests/tst_searchresultsmodel.cpp`:

```cpp
#include <QtTest>
#include "SearchResultsModel.h"
#include "studentdata.h"
#include "apiconfig.h"

class TestSearchResultsModel : public QObject
{
    Q_OBJECT
private slots:
    void photoRoleJoinsBaseUrlWhenRelative()
    {
        SearchResultsModel m;
        StudentRecord r; r.name = "Maria Santos";
        r.photo = "uploads/students/2023-1.jpg";
        m.setRecords({ r });
        const QString got = m.data(m.index(0), SearchResultsModel::PhotoRole).toString();
        QCOMPARE(got, ApiConfig::endpoint("uploads/students/2023-1.jpg").toString());
        QVERIFY(got.startsWith("http"));
        QVERIFY(got.endsWith("uploads/students/2023-1.jpg"));
    }

    void photoRoleEmptyWhenNoPhoto()
    {
        SearchResultsModel m;
        StudentRecord r; r.name = "Jose Ramirez"; r.photo = "";
        m.setRecords({ r });
        QCOMPARE(m.data(m.index(0), SearchResultsModel::PhotoRole).toString(), QString());
    }

    void roleNamesExposePhoto()
    {
        SearchResultsModel m;
        QCOMPARE(m.roleNames().value(SearchResultsModel::PhotoRole), QByteArray("photo"));
    }
};

QTEST_APPLESS_MAIN(TestSearchResultsModel)
#include "tst_searchresultsmodel.moc"
```

- [ ] **Step 2: Register the test target**

In `qt-app/quick/CMakeLists.txt`, after the `tst_studentstablemodel` block (around line 250):

```cmake
# --- SearchResultsModel unit test (C++ QtTest). PhotoRole joins ApiConfig base;
# pure model logic, no NAM -> QTEST_APPLESS_MAIN, no OFFSCREEN. ---
wits_add_qttest(tst_searchresultsmodel
    SOURCES tests/tst_searchresultsmodel.cpp
    LIBS witsquickmodule)
```

- [ ] **Step 3: Reconfigure + run to verify it fails**

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_searchresultsmodel --output-on-failure`
Expected: FAIL to compile — `SearchResultsModel::PhotoRole` undefined.

- [ ] **Step 4: Add the role + join logic**

In `qt-app/quick/models/SearchResultsModel.h`, add `PhotoRole,` to the `Roles` enum after `InitialsRole,`:

```cpp
        InitialsRole,
        PhotoRole,
```

In `qt-app/quick/models/SearchResultsModel.cpp`, add the include near the top:

```cpp
#include "apiconfig.h"
```

In `data()`, after the `InitialsRole` case:

```cpp
    case PhotoRole:
        // Approach A: the ONLY place a search photo becomes absolute. Never join
        // an empty path (ApiConfig::endpoint("") yields the bare base URL, which
        // would force a needless load-failure) — return empty so LAvatar shows
        // initials.
        return r.photo.isEmpty() ? QString()
                                 : ApiConfig::endpoint(r.photo).toString();
```

In `roleNames()`, add the mapping (extend the returned initializer list):

```cpp
        { InitialsRole, "initials" }, { PhotoRole, "photo" },
```

- [ ] **Step 5: Build + run to verify it passes**

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_searchresultsmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit** (via the `commit` skill)

Subject e.g. `feat(quick): add absolute PhotoRole to SearchResultsModel`.

---

### Task 3: `KioskViewModel.currentPhotoUrl` + `currentInitials`

Read `photo_url` from the login response and precompute initials in C++ (the kiosk has no `initials` role for `LAvatar` to reuse).

**Files:**
- Modify: `qt-app/quick/viewmodels/KioskViewModel.h` (2 properties + getters + members)
- Modify: `qt-app/quick/viewmodels/KioskViewModel.cpp` (`#include "Initials.h"`; `applyStudentLogin`)
- Test: `qt-app/quick/tests/tst_kioskviewmodel.cpp`

**Interfaces:**
- Produces: `KioskViewModel::currentPhotoUrl` (`QString`, absolute or ""), `KioskViewModel::currentInitials` (`QString`), both on the existing `currentChanged` signal.

- [ ] **Step 1: Write the failing test**

Add to `qt-app/quick/tests/tst_kioskviewmodel.cpp` (new private-slot test; `<QJsonObject>` is already available via the `student()` helper):

```cpp
void test_applyStudentLoginSetsPhotoUrlAndInitials()
{
    KioskViewModel vm;
    QSignalSpy cur(&vm, &KioskViewModel::currentChanged);
    QJsonObject s;
    s["name"] = "Maria Santos";
    s["course"] = "BSCE";
    s["year_level"] = "3rd Year";
    s["department"] = "CE";
    s["photo_url"] = "http://localhost/loams_api/uploads/students/2023-1.jpg";
    vm.applyStudentLogin(s);
    QVERIFY(cur.count() >= 1);
    QCOMPARE(vm.currentPhotoUrl(),
             QStringLiteral("http://localhost/loams_api/uploads/students/2023-1.jpg"));
    QCOMPARE(vm.currentInitials(), QStringLiteral("MS"));
}

void test_applyStudentLoginEmptyPhotoUrlStaysEmpty()
{
    KioskViewModel vm;
    QJsonObject s; s["name"] = "Ana Cruz";   // no photo_url
    vm.applyStudentLogin(s);
    QCOMPARE(vm.currentPhotoUrl(), QString());
    QCOMPARE(vm.currentInitials(), QStringLiteral("AC"));
}
```

Add the two method names to the file's `private slots:` block.

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_kioskviewmodel --output-on-failure`
Expected: FAIL — `currentPhotoUrl`/`currentInitials` not members.

- [ ] **Step 3: Declare the properties**

In `qt-app/quick/viewmodels/KioskViewModel.h`, after the `currentTime` property/getter, add the `Q_PROPERTY` lines (in the property block):

```cpp
    Q_PROPERTY(QString currentPhotoUrl READ currentPhotoUrl NOTIFY currentChanged)
    Q_PROPERTY(QString currentInitials READ currentInitials NOTIFY currentChanged)
```

Add getters (next to `currentTime()`):

```cpp
    QString currentPhotoUrl() const { return m_currentPhotoUrl; }
    QString currentInitials() const { return m_currentInitials; }
```

Add members (next to `m_currentTime`):

```cpp
    QString m_currentPhotoUrl, m_currentInitials;
```

- [ ] **Step 4: Populate them**

In `qt-app/quick/viewmodels/KioskViewModel.cpp`, add the include near the other quick includes:

```cpp
#include "Initials.h"
```

In `applyStudentLogin`, after `m_currentName = m_currentFullName.section(...)`:

```cpp
    m_currentPhotoUrl = student.value(QStringLiteral("photo_url")).toString();
    m_currentInitials = Initials::of(m_currentFullName);
```

(The existing `emit currentChanged()` at the end of `applyStudentLogin` already covers both.)

- [ ] **Step 5: Run tests to verify they pass**

Run: `ctest --test-dir qt-app/build -R tst_kioskviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit** (via the `commit` skill)

Subject e.g. `feat(quick): expose currentPhotoUrl + currentInitials on KioskViewModel`.

---

### Task 4: Extract `LCircleImage` from `LLogoCircle` (verbatim Canvas move)

Move `LLogoCircle`'s core-QtQuick `Canvas` circular-crop into a reusable `LCircleImage` primitive; refactor `LLogoCircle` to compose it. Behavior-preserving by construction — the existing `LLogoCircle` QuickTests are the guard. **Preserve the `objectName`s `logoImage`/`logoCanvas`/`logoPlaceholder`** (the tests find them via `findChild`).

**Files:**
- Create: `qt-app/quick/qml/components/LCircleImage.qml`
- Modify: `qt-app/quick/qml/components/LLogoCircle.qml` (compose `LCircleImage`)
- Modify: `qt-app/quick/CMakeLists.txt` (add `LCircleImage.qml` to `QML_FILES`)
- Guard (no new test): `qt-app/quick/tests/tst_qml_components.qml` LLogoCircle cases (`test_noLogoShowsPlaceholder`, `test_logoLoadsAndHidesPlaceholder`, `test_paintIsCenterCroppedNotStretched`, `test_paintStaysInsideTheCircle`).

**Interfaces:**
- Produces: `LCircleImage` with props `source: url`, `size: int`, `ringWidth: int` (0 = none), `ringColor: color` (default `Theme.accent.base`), a `default` placeholder slot, and `readonly property int imageStatus` + `readonly property bool showingImage`.

- [ ] **Step 1: Establish the green baseline**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS (this is a refactor — the guard must be green BEFORE the move, so a later red proves the move changed behavior).

- [ ] **Step 2: Create `LCircleImage.qml`** (Canvas body moved verbatim from `LLogoCircle.qml:49-146`, generalized `effectiveUrl`→`source`, `hasLogo` veto dropped since the consumer blanks `source`)

Create `qt-app/quick/qml/components/LCircleImage.qml`:

```qml
import QtQuick
import LOAMS

// Circular image with a "cover" crop, drawn on a Canvas (core QtQuick only — this
// repo links no shader-effects module, and a Rectangle clip clips to the bounding
// box, not the circle). Extracted from LLogoCircle so LAvatar and LLogoCircle
// share one tested crop. Presentational: primitive props, no vm.
//
// The Canvas image cache is imperative; the placeholder slot shows whenever no
// image is drawn (empty source / not yet Ready / failed).
Item {
    id: frame

    property url source: ""
    property int size: 52
    property int ringWidth: 0
    property color ringColor: Theme.accent.base
    // Shown when the canvas isn't drawing an image. Consumers fill this slot.
    default property alias placeholderContent: placeholderHost.data
    // Exposed so consumers can gate on load state (LAvatar reads imageStatus for
    // the Image.Error fallback; showingImage says the photo is actually painted).
    readonly property int imageStatus: circleImage.status
    readonly property bool showingImage: circleCanvas.visible

    implicitWidth: size
    implicitHeight: size

    onSourceChanged: circleCanvas.reloadSource()

    // Loads off-scene. Nothing draws this element — the Canvas draws from the
    // url — but it reports load *status* (gates the placeholder swap) and the
    // *natural* pixel dimensions (sourceSize) the cover-crop maths needs.
    Image {
        id: circleImage
        objectName: "logoImage"        // PRESERVED: LLogoCircle regression tests
        anchors.fill: parent
        source: frame.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        visible: false
        onStatusChanged: circleCanvas.requestPaint()
    }

    Canvas {
        id: circleCanvas
        objectName: "logoCanvas"       // PRESERVED
        anchors.fill: parent
        renderTarget: Canvas.Image

        property url loadedUrl: ""
        property bool canvasImageReady: false

        function reloadSource() {
            if (loadedUrl != "")
                unloadImage(loadedUrl);
            canvasImageReady = false;
            loadedUrl = frame.source;
            if (loadedUrl != "") {
                loadImage(loadedUrl);
                if (isImageLoaded(loadedUrl))
                    canvasImageReady = true;
            }
            requestPaint();
        }

        Component.onCompleted: reloadSource()
        onImageLoaded: { canvasImageReady = true; requestPaint(); }

        visible: frame.source != ""
                 && circleImage.status === Image.Ready
                 && canvasImageReady
        onVisibleChanged: if (visible) requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            if (loadedUrl == "" || !isImageLoaded(loadedUrl))
                return;
            var natW = circleImage.sourceSize.width;
            var natH = circleImage.sourceSize.height;
            if (natW <= 0 || natH <= 0)
                return;
            var side = Math.min(natW, natH);
            var sx = (natW - side) / 2;
            var sy = (natH - side) / 2;
            ctx.save();
            ctx.beginPath();
            ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2, true);
            ctx.closePath();
            ctx.clip();
            ctx.drawImage(loadedUrl, sx, sy, side, side, 0, 0, width, height);
            ctx.restore();
        }
    }

    // Placeholder slot host: visible whenever the canvas isn't. Consumers put
    // their fallback (LOGO circle, initials chip) here.
    Item {
        id: placeholderHost
        objectName: "logoPlaceholder"  // PRESERVED
        anchors.fill: parent
        visible: !circleCanvas.visible
    }

    // Optional ring over the drawn photo (LLogoCircle's gold ring).
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: frame.ringWidth
        border.color: frame.ringColor
        visible: circleCanvas.visible && frame.ringWidth > 0
    }
}
```

- [ ] **Step 3: Refactor `LLogoCircle.qml` to compose it**

Replace the body of `qt-app/quick/qml/components/LLogoCircle.qml` (keep its public API `logoUrl`/`hasLogo`/`size`/`ringWidth` and the top doc comment) with:

```qml
import QtQuick
import LOAMS

// Circular school-logo badge (see LCircleImage for the crop mechanism). Real
// logo cropped to a circle when configured+loaded, else a "LOGO" placeholder.
// Shared by LSidebarBrand (52px/2px ring) and BrandPanel (96px/3px ring).
// Presentational: primitive props, not a vm.
Item {
    id: logoFrame

    property url logoUrl: ""
    property bool hasLogo: false
    // hasLogo:false vetoes a set logoUrl (VM reports hasLogo:false for a
    // configured-but-missing file); LCircleImage keys off this blanked url.
    readonly property url effectiveUrl: hasLogo ? logoUrl : ""
    property int size: 52
    property int ringWidth: 2

    implicitWidth: size
    implicitHeight: size

    LCircleImage {
        anchors.fill: parent
        source: logoFrame.effectiveUrl
        size: logoFrame.size
        ringWidth: logoFrame.ringWidth   // gold ring over the photo (ringColor default = accent.base)

        // Placeholder: the "LOGO" circle with its own gold ring.
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Theme.card
            border.width: logoFrame.ringWidth
            border.color: Theme.accent.base
            Text {
                anchors.centerIn: parent
                text: qsTr("LOGO")
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.eyebrow
            }
        }
    }
}
```

- [ ] **Step 4: Register `LCircleImage.qml`**

In `qt-app/quick/CMakeLists.txt`, add to the `QML_FILES` list (near the other `qml/components/` entries):

```cmake
        qml/components/LCircleImage.qml
```

- [ ] **Step 5: Build + run the guard**

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS — including `test_noLogoShowsPlaceholder`, `test_logoLoadsAndHidesPlaceholder`, `test_paintIsCenterCroppedNotStretched`, `test_paintStaysInsideTheCircle`. A failure here means the move changed behavior — fix the extraction, do not weaken the tests.

- [ ] **Step 6: Commit** (via the `commit` skill)

Subject e.g. `refactor(quick): extract LCircleImage from LLogoCircle`. Body: behavior-preserving Canvas move, objectNames preserved, existing tests are the guard.

---

### Task 5: `LAvatar.qml` + QuickTest

Build the presentational avatar on `LCircleImage`. Initials when `source` is empty, its path ends with `default.jpg` (suffix check), or the image load errors. Keep `LCircleImage` mounted (toggle via its own canvas/placeholder), never unmount — avoids oscillating `imageStatus`.

**Files:**
- Create: `qt-app/quick/qml/components/LAvatar.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (add `LAvatar.qml` to `QML_FILES`)
- Test: `qt-app/quick/tests/tst_qml_components.qml` (new `LAvatarFallback` TestCase + fixtures)

**Interfaces:**
- Consumes: `LCircleImage` (Task 4).
- Produces: `LAvatar` with props `source: string`, `initials: string`, `size: int` (default 40), `fallbackBackground: color` (default `Theme.brand.soft`), `fallbackForeground: color` (default `Theme.brand.base`); introspection `readonly property bool showInitials`, `readonly property bool showingImage`, `readonly property int imageStatus`. The initials `Text` carries `objectName: "avatarInitials"`.

- [ ] **Step 1: Write the failing test**

Add fixtures near the top of `qt-app/quick/tests/tst_qml_components.qml` (with the other fixtures, before the `TestCase`s). Reuse the file's synthetic 1×1 data-URI PNG idiom (a valid, synchronously-loadable source):

```qml
    // --- LAvatar fixtures (photo display) ---
    // A valid, tiny, synchronously-decodable image (1x1 PNG data URI).
    readonly property string tinyPng:
        "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="

    LAvatar { id: avImage;   initials: "MS"; source: root.tinyPng }
    LAvatar { id: avEmpty;   initials: "MS"; source: "" }
    LAvatar { id: avSentinel;   initials: "MS"; source: "http://h/loams_api/uploads/default.jpg" }
    LAvatar { id: avSentinelNested; initials: "MS"; source: "http://h/loams_api/uploads/students/default.jpg" }
    LAvatar { id: avBroken;  initials: "MS"; source: "file:///no/such/avatar_zzz.jpg" }
    LAvatar { id: avTokens;  initials: "MS"; source: "";
              fallbackBackground: "#123456"; fallbackForeground: "#654321" }
```

Add a new `TestCase` (anywhere among the others):

```qml
    TestCase {
        name: "LAvatarFallback"
        when: windowShown

        function test_emptySourceShowsInitials() {
            compare(avEmpty.showInitials, true);
            var t = findChild(avEmpty, "avatarInitials");
            verify(t !== null); compare(t.text, "MS");
        }
        function test_sentinelShowsInitials() {
            compare(avSentinel.showInitials, true);        // path ends default.jpg
        }
        function test_sentinelNestedShowsInitials() {
            compare(avSentinelNested.showInitials, true);  // suffix check, not filename equality
        }
        function test_validImageShowsPhotoNotInitials() {
            tryCompare(avImage, "showingImage", true, 5000);
            compare(avImage.showInitials, false);
        }
        function test_brokenSourceFallsBackViaImageError() {
            // Must reach a genuine Image.Error (distinct from empty source).
            tryCompare(avBroken, "imageStatus", Image.Error, 5000);
            compare(avBroken.showInitials, true);
            compare(avBroken.showingImage, false);
        }
        function test_fallbackTokensOverrideDefaults() {
            var t = findChild(avTokens, "avatarInitials");
            verify(t !== null);
            compare(String(t.color), String(Qt.color("#654321")));
        }
        function test_fallbackTokensDefaultToBrand() {
            var t = findChild(avEmpty, "avatarInitials");
            compare(String(t.color), String(Theme.brand.base));
        }
    }
```

- [ ] **Step 2: Register the QML file + run to verify the tests fail**

In `qt-app/quick/CMakeLists.txt`, add to `QML_FILES`:

```cmake
        qml/components/LAvatar.qml
```

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: FAIL — `LAvatar` is not a type (module doesn't expose it yet).

- [ ] **Step 3: Implement `LAvatar.qml`**

Create `qt-app/quick/qml/components/LAvatar.qml`:

```qml
import QtQuick
import LOAMS

// Circular student avatar: a photo (cropped via LCircleImage) or an initials
// chip fallback. Presentational: absolute-or-empty `source` + a PRECOMPUTED
// `initials` string (Initials::of() is C++-only, not QML-accessible — callers
// pass model.initials / vm.currentInitials). Falls back to initials when the
// source is empty, its path ends with default.jpg (suffix check, not filename
// equality), or the image fails to load (Image.Error).
Item {
    id: avatar

    property string source: ""
    property string initials: ""
    property int size: 40
    property color fallbackBackground: Theme.brand.soft
    property color fallbackForeground: Theme.brand.base

    // "No usable photo" — blank LCircleImage's source in these cases so it never
    // tries to load the default.jpg sentinel (a valid URL that would otherwise
    // paint), and shows the initials placeholder instead.
    readonly property bool _emptyOrSentinel:
        !source || source.length === 0 || avatar._endsWithDefault(source)
    // True whenever the initials chip should be the visible content. The
    // Image.Error term is kept SEPARATE from _emptyOrSentinel (and we do NOT
    // blank source on error) so a broken-but-set url actually reaches
    // Image.Error and the binding doesn't oscillate.
    readonly property bool showInitials: _emptyOrSentinel || circle.imageStatus === Image.Error
    readonly property bool showingImage: circle.showingImage
    readonly property int imageStatus: circle.imageStatus

    implicitWidth: size
    implicitHeight: size

    function _endsWithDefault(s) {
        var path = ("" + s).split("?")[0].split("#")[0];  // strip query/hash
        var needle = "default.jpg";
        return path.length >= needle.length
            && path.slice(-needle.length).toLowerCase() === needle;
    }

    LCircleImage {
        id: circle
        anchors.fill: parent
        size: avatar.size
        // Empty/sentinel -> blank (no load, show placeholder). A real url -> load;
        // if it errors, LCircleImage keeps the canvas hidden and shows the
        // placeholder, and imageStatus stays Error (source unchanged).
        source: avatar._emptyOrSentinel ? "" : avatar.source

        // Placeholder slot: the initials chip. Always mounted; LCircleImage
        // shows it whenever the canvas isn't painting a photo.
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: avatar.fallbackBackground
            Text {
                objectName: "avatarInitials"
                anchors.centerIn: parent
                text: avatar.initials
                color: avatar.fallbackForeground
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                font.weight: Font.ExtraBold
            }
        }
    }
}
```

- [ ] **Step 4: Build + run to verify the tests pass**

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS (all new `LAvatarFallback` cases + all pre-existing component tests).

- [ ] **Step 5: Commit** (via the `commit` skill)

Subject e.g. `feat(quick): add LAvatar photo-or-initials component`.

---

### Task 6: Wire `LAvatar` into the admin search rows

Replace the inline initials chip in the search result delegate with `LAvatar`. The existing `test_avatarRendersInitials` (finds `avatarInitials`, expects `"MS"`) must still pass — `LAvatar` carries that `objectName` and shows initials when `model.photo` is empty/undefined.

**Files:**
- Modify: `qt-app/quick/qml/admin/SearchScreen.qml:497-517` (the avatar `Rectangle`)
- Guard: `qt-app/quick/tests/tst_qml_admin.qml` (`test_avatarRendersInitials`)

**Interfaces:**
- Consumes: `SearchResultsModel` role `photo` (Task 2), existing role `initials`; `LAvatar` (Task 5).

- [ ] **Step 1: Replace the inline avatar**

In `qt-app/quick/qml/admin/SearchScreen.qml`, replace the entire avatar `Rectangle { … objectName:"avatarInitials" … }` block (lines ~497-517, including the "No student photo data exists yet" comment) with:

```qml
                                // Photo-or-initials avatar (Phase 4d). Photo
                                // comes from SearchResultsModel's absolute
                                // PhotoRole; empty -> initials fallback.
                                LAvatar {
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    Layout.alignment: Qt.AlignVCenter
                                    size: 40
                                    source: model.photo
                                    initials: model.initials
                                }
```

- [ ] **Step 2: Build + run the admin guard**

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS — `test_avatarRendersInitials` still finds `avatarInitials` = `"MS"` (the stub row has no `photo`, so initials show).

- [ ] **Step 3: Commit** (via the `commit` skill)

Subject e.g. `feat(quick): show student photo in admin search results`.

---

### Task 7: Wire `LAvatar` into the kiosk hero card

Add the avatar to the signed-in hero, visible only when a student is signed in, with an **on-dark** fallback pairing legible on the maroon `brand` gradient.

**Files:**
- Modify: `qt-app/quick/qml/kiosk/KioskMain.qml` (hero card content, ~lines 91-124)
- Test: `qt-app/quick/tests/tst_qml_kiosk.qml` (extend the vm stub + a hero-avatar test)

**Interfaces:**
- Consumes: `KioskViewModel.currentPhotoUrl` + `currentInitials` (Task 3); `LAvatar` (Task 5).

- [ ] **Step 1: Write the failing test**

In `qt-app/quick/tests/tst_qml_kiosk.qml`, add to the kiosk vm stub the two properties (next to the other `current*` stub properties):

```qml
        property string currentPhotoUrl: ""
        property string currentInitials: "MS"
```

Add a test (in the kiosk `TestCase`) that with `hasStudent: true` and no photo, the hero shows initials:

```qml
        function test_heroShowsInitialsWhenNoPhoto() {
            vmStub.hasStudent = true;
            vmStub.currentFullName = "Maria Santos";
            vmStub.currentInitials = "MS";
            vmStub.currentPhotoUrl = "";
            waitForRendering(kiosk);
            var t = findChild(kiosk, "avatarInitials");
            verify(t !== null);
            compare(t.text, "MS");
        }
```

(Match the existing stub's id/name in this file — use whatever the file already calls the kiosk vm stub; the snippet uses `vmStub`/`kiosk` as placeholders for the file's existing ids.)

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_kiosk --output-on-failure`
Expected: FAIL — no `avatarInitials` in the hero yet.

- [ ] **Step 3: Add `LAvatar` to the hero**

In `qt-app/quick/qml/kiosk/KioskMain.qml`, inside the hero `Rectangle` (id `hero`), wrap the existing text `ColumnLayout` in a `RowLayout` with the avatar first. Concretely, change the hero's inner `ColumnLayout { anchors.fill: parent; anchors.margins: Theme.spacing.xl; … }` so the avatar sits to its left:

```qml
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.xl
                    spacing: Theme.spacing.lg

                    LAvatar {
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 72
                        Layout.alignment: Qt.AlignVCenter
                        size: 72
                        visible: mainArea.vm ? mainArea.vm.hasStudent : false
                        source: mainArea.vm ? mainArea.vm.currentPhotoUrl : ""
                        initials: mainArea.vm ? mainArea.vm.currentInitials : ""
                        // On-dark pairing for the maroon hero gradient (mirrors
                        // the emphasized feed chip's brand.base/brand.on inversion,
                        // KioskFeedRow.qml:41-45). No raw hex — tokens + Qt.alpha.
                        fallbackBackground: Qt.alpha(Theme.brand.on, 0.18)
                        fallbackForeground: Theme.brand.on
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.xs
                        // ... existing hero contents (LEyebrow + the three Text
                        // items) moved here verbatim ...
                    }
                }
```

Move the existing `LEyebrow`/`Text` children into the inner `ColumnLayout` unchanged.

- [ ] **Step 4: Build + run to verify it passes**

Run: `cmake --build qt-app/build` then `ctest --test-dir qt-app/build -R tst_qml_kiosk --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit** (via the `commit` skill)

Subject e.g. `feat(quick): show student photo on the kiosk hero card`.

---

### Task 8: Backend — additive `photo` on `search_students.php`

One additive line; client already tolerates its absence (client-safe first). Deploy after the client lands.

**Files:**
- Modify: `deliverables/loams_api/search_students.php:74-85` (the row-building loop)

**Interfaces:**
- Produces: JSON field `photo` (relative path, empty on NULL) on each search row — consumed by Task 1's parser.

- [ ] **Step 1: Add the field**

In `deliverables/loams_api/search_students.php`, inside the `while ($row = ...)` loop's `$students[] = [ ... ]`, add after the `"visits"` entry:

```php
        "photo" => $row['photo'] ?? ""
```

(Use `?? ""` so a NULL column serializes as `""`, matching the contract and the file's `isset(...) ? ... : ""` idiom.)

- [ ] **Step 2: Lint**

Run: `php -l "deliverables/loams_api/search_students.php"`
Expected: `No syntax errors detected`. (If `php` is not on PATH, use the XAMPP php: `C:/xampp/php/php.exe -l ...`.)

- [ ] **Step 3: Commit** (via the `commit` skill)

Subject e.g. `feat(api): emit additive relative photo field in search_students`.

- [ ] **Step 4: Deploy (manual, after the client PR merges)**

Copy `deliverables/loams_api/search_students.php` → `C:/xampp/htdocs/loams_api/search_students.php` (a `search_students.php.bak-preloams2` backup already exists in the web root). This is a deploy step, not a code change — do it once the client build is in place; the client shows initials until then. `student_login.php` needs no deploy (unchanged).

---

### Task 9: Harden bulk-import photo→student matching

Independent backend correctness fix (surfaced while designing display: a wrong match becomes a *visible* wrong face once photos are shown). The current bulk matcher `glob("*<school_id>*.*")` + `$candidates[0]` is a **substring, first-wins** match: a shorter ID that is a prefix of another (`2023-1` inside `2023-12`) can grab the wrong file, and multi-matches are non-deterministic. Replace it with a **whole-token** match (the ID must appear delimited by string start/end or a non-alphanumeric char), evaluated over a **sorted** file list for determinism. This preserves the existing "ID anywhere in the filename" flexibility (e.g. `lastname_2023-1234.jpg`) while eliminating the prefix collision.

**Files:**
- Modify: `deliverables/loams_api/upload_students_zip.php:86-96` (the per-row photo match)

**No automated test:** this repo has no PHP test harness (backend is untested by design until Phase 6). Verify with `php -l` and the reasoning table below; do NOT stand up a PHP test framework for this.

**Interfaces:**
- Produces: unchanged JSON/DB contract — still writes `uploads/students/<school_id>.jpg` to the `photo` column; only the *matching* is stricter.

- [ ] **Step 1: Add a token-match helper**

In `deliverables/loams_api/upload_students_zip.php`, add near the top (after the includes, before the request handling) a helper:

```php
// Match a ZIP photo to a student by school_id as a WHOLE TOKEN: the id must be
// bounded by string start/end or a non-alphanumeric char, so "2023-1" does NOT
// match a "2023-12..." filename (the old glob("*id*") substring match did).
// Scans a sorted list so a genuine multi-match is deterministic (first wins).
function matchPhotoForId($photoDir, $schoolId) {
    $files = glob($photoDir . "*.*");
    if (!$files) return null;
    sort($files); // deterministic order
    $pattern = '/(^|[^A-Za-z0-9])' . preg_quote($schoolId, '/') . '([^A-Za-z0-9]|$)/';
    foreach ($files as $f) {
        if (preg_match($pattern, basename($f)) === 1) return $f;
    }
    return null;
}
```

- [ ] **Step 2: Use it in the row loop**

Replace the photo-match block (`deliverables/loams_api/upload_students_zip.php:86-96`, the `$photoPath = null; if ($zipExtracted) { $candidates = glob(...); ... }`) with:

```php
    // Photo comes ONLY from a whole-token ZIP match (never from a file column).
    $photoPath = null;
    if ($zipExtracted) {
        $match = matchPhotoForId($photoDir, $school_id);
        if ($match !== null) {
            $targetPhoto = "uploads/students/" . $school_id . ".jpg";
            if (copy($match, $targetPhoto)) {
                $photoPath = $targetPhoto;
            }
        }
    }
```

- [ ] **Step 3: Lint + reason through the cases**

Run: `php -l "deliverables/loams_api/upload_students_zip.php"` (or `C:/xampp/php/php.exe -l ...`)
Expected: `No syntax errors detected`.

Confirm the intended behavior by inspection (matching `basename`, pattern anchored on non-alnum boundaries):

| Student ID | ZIP filename | Old (substring) | New (token) |
|---|---|---|---|
| `2023-1`   | `2023-12.jpg`            | ✅ (wrong!) | ❌ correct |
| `2023-1`   | `2023-1.jpg`            | ✅ | ✅ |
| `2023-1234`| `2023-1234_maria.png`   | ✅ | ✅ (`_` boundary) |
| `2023-1234`| `lastname_2023-1234.jpg`| ✅ | ✅ (mid-name, `_`/`.` boundaries) |
| `2023-12`  | `2023-1.jpg`            | ❌ | ❌ |

- [ ] **Step 4: Commit** (via the `commit` skill)

Subject e.g. `fix(api): match bulk-import photos to students by whole-token id`. Body: substring+first-wins → deterministic whole-token match; note the prefix-collision (`2023-1` vs `2023-12`) it fixes and that display makes such mismatches user-visible; deploy alongside Task 8.

- [ ] **Step 5: Deploy (manual, with Task 8)**

Copy `deliverables/loams_api/upload_students_zip.php` → `C:/xampp/htdocs/loams_api/` when the client work deploys (a `.pre4a3-*.bak` backup already exists in the web root). Matching-only change; DB/JSON contract unchanged.

---

## Verification (whole track, before `/create-pr`)

- [ ] Full suite green: `ctest --test-dir qt-app/build --output-on-failure` (44 existing + `tst_searchresultsmodel` + the new component/kiosk/controller/kioskvm cases).
- [ ] Clean build, no new warnings: `cmake --build qt-app/build`.
- [ ] GUI smoke (`WITSQuick.exe`): a student **with** a real photo → image on kiosk hero + search row; a NULL-photo student → initials on both; a set-but-missing file in search → initials (Image.Error path).
- [ ] Then run `/claude-review` (branch/phase mode) and the project `/create-pr` three-agent gate.

## Self-Review notes

- **Spec coverage:** §4.1 LCircleImage → Task 4; §4.1a LAvatar → Task 5; §4.1b registration → Tasks 4-5; §4.1c builder notes (objectName preservation, no-unmount toggle) → Task 4 Step 2 + Task 5 Step 3; §4.2 StudentRecord → Task 1; §4.3 PhotoRole/roleNames + currentPhotoUrl/currentInitials → Tasks 2-3; §4.4 wiring → Tasks 6-7; §5 backend → Task 8; §6.1 units → Tasks 1-3 + Task 4 guard; §6.2 LAvatar QuickTest → Task 5.
- **Type consistency:** `PhotoRole` (Task 2) consumed as `model.photo` (Task 6); `currentPhotoUrl`/`currentInitials` (Task 3) consumed in Task 7; `LCircleImage` props `source`/`size`/`ringWidth`/`imageStatus`/`showingImage` (Task 4) consumed by `LAvatar` (Task 5); `LAvatar` props `source`/`initials`/`size`/`fallbackBackground`/`fallbackForeground` (Task 5) consumed by Tasks 6-7.
