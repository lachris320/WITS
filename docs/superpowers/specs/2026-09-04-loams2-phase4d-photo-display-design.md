# LOAMS 2.0 — Phase 4d: Photo Display Design

**Date:** 2026-09-04
**Status:** Approved (brainstorm) — ready for `/writing-plans`
**Track:** Phase 4 (Admin Part 2), track 4d — the last remaining Phase 4 slice.
**Parent contract:** `docs/superpowers/specs/2026-07-19-loams2-phase4-admin-part2-design.md` §3.4, §4.4, §5.2, §6.2.

> Naming note: an earlier "4d" was the role-based brand-tokens work
> (`2026-07-21-loams2-phase4d-brand-tokens-design.md`, merged). This document is
> the **photo-display** track, which the parent contract also labels "4d". They
> are distinct; this one closes Phase 4.

## 1. Goal

Show the real student photo — with an **initials fallback** — on the kiosk
signed-in hero card and in admin search results. The two surfaces receive photo
data in **different shapes**; a single new `LAvatar` component makes them look
uniform.

Today neither surface shows a photo:

- The kiosk **signed-in hero card** (`quick/qml/kiosk/KioskMain.qml`, the
  "NOW SIGNED IN" card) shows name/course text and **no avatar at all**.
- The admin **search result rows** (`quick/qml/admin/SearchScreen.qml`) show an
  **initials-only** chip, carrying a comment that literally says "No student
  photo data exists yet".

This track reverses the Phase-3 "initials-only" decision going forward, for the
~171 existing students whose `photo` column is NULL falling back to initials.

## 2. Scope decisions (frozen this brainstorm)

1. **Kiosk: hero card only.** The signed-in hero card gets a photo. The live
   attendance **feed rows stay initials-only** and are untouched. Rationale:
   the parent contract's wording is "the Kiosk login result" (= the hero), and
   feed rows loaded from history on startup carry no photo — showing photos only
   on fresh logins would make the feed mix photos and initials. Hero-only keeps
   the feed uniform and needs no feed-history endpoint change.
2. **Fallback = initials** (not a generic silhouette), matching the existing
   chip pattern everywhere else in the app.
3. **Normalization approach A — C++ does all URL work; `LAvatar` is purely
   presentational.** The relative search path is joined to `ApiConfig::baseUrl()`
   in the C++ model layer, not in QML. This honors the project MVVM rule (QML
   never touches `witscore`/`ApiConfig`; `ApiConfig` stays the single C++ source
   of truth for the base URL, which matters for the Phase-6 base-URL change).
   `LAvatar` receives an absolute-or-empty `source` and only decides
   image-vs-initials. The sentinel + load-error logic — the part that genuinely
   must live in QML — still lives in `LAvatar`.

## 3. The two server shapes and how `LAvatar` unifies them

| Surface | Endpoint | Photo field | Shape | Empty case |
|---|---|---|---|---|
| Kiosk hero | `student_login.php` (already deployed, **no change**) | `photo_url` | **Absolute** URL (`$protocol.$host.$scriptDir/…`) | Substitutes the `uploads/default.jpg` **sentinel** (never empty) |
| Search rows | `search_students.php` (**additive** `photo` this track) | `photo` | **Relative** path | Empty string on NULL |

- `student_login.php` already does a `file_exists()` check
  (`student_login.php:78`) before emitting a real URL, so on the kiosk a missing
  file resolves to the `default.jpg` sentinel server-side.
- `search_students.php` does **no** `file_exists()` check, so a student whose
  `photo` column is set but whose file is missing emits a live-but-broken URL.
  Only the client's **load-failure** fallback catches that case.

**`LAvatar` normalization rule (client-side):** show initials when

1. `source` is empty, **or**
2. `source`'s **path ends with** `default.jpg` (a **suffix/path check**, not an
   exact-filename equality — `https://host/uploads/students/default.jpg` must
   still fall back), **or**
3. the image fails to load (`image.status === Image.Error` — the broken-but-set
   case).

Otherwise load `source` as the photo.

## 4. Design

### 4.1 Circular-crop mechanism — reuse `LLogoCircle`'s Canvas, don't reinvent

**Do not use `OpacityMask`/`MultiEffect` or a `Rectangle{radius;clip:true}`.** This
repo links **no** shader-effects module (`QtQuick.Effects` /
`Qt5Compat.GraphicalEffects`), and `Rectangle clip` clips to the bounding box,
not the circle — both facts are documented verbatim at `LLogoCircle.qml:15-22`.
`LLogoCircle` already solves circular image cropping with a **`Canvas`** circular
clip path drawn from the image URL (`Canvas.loadImage`/`Context2D.drawImage`) —
**core QtQuick only, offscreen-safe** under the OFFSCREEN QuickTest platform, and
it carries hard-won handling for the async-load, sync-pixmap-cache-hit, and
empty-disc-gating cases (`LLogoCircle.qml:49-146`).

**Design decision — extract a shared primitive `quick/qml/components/LCircleImage.qml`.**
`LAvatar` and `LLogoCircle` would otherwise carry two near-identical copies of
that ~30-line Canvas cover-crop (the `create-pr` DRY gate would flag it). To
avoid that, **move `LLogoCircle`'s Canvas mechanism verbatim** into a new
`LCircleImage` primitive:

- `LCircleImage` props: `source` (url), `size` (int, drives implicit size),
  `ringWidth` (int, 0 = no ring), and a `default property` **placeholder slot**
  shown when `source` is empty or not `Image.Ready`/loaded. It exposes
  `readonly property int imageStatus` (the inner `Image.status`) and
  `readonly property bool showingImage` so consumers can gate on load state.
- `LLogoCircle` is refactored to **compose** `LCircleImage`, supplying the
  existing "LOGO" placeholder and its `hasLogo` veto (`effectiveUrl`) + gold ring.
  The move is **behavior-preserving by construction** (code moved, not rewritten);
  `LLogoCircle`'s existing QuickTests are the regression guard and must stay green.

This is the one deliberate scope addition beyond the plumbing: it turns the
Critical (infeasible crop) and the DRY risk into one shared, tested primitive.

### 4.1a New component — `quick/qml/components/LAvatar.qml`

A circular avatar that shows a photo or falls back to an initials chip. Purely
presentational — **no knowledge of the backend or `ApiConfig`, and no name→initials
derivation** (that is C++'s job — see the Important finding below and §4.3).

**Public properties:**

- `source` : string — an **absolute** photo URL, or empty. (All relative→absolute
  joining is done by the C++ layer before it reaches here — §4.3.)
- `initials` : string — the **already-computed** initials to show in the fallback.
  LAvatar does NOT derive these from a name: `Initials::of()` is a plain C++
  namespace (`quick/Initials.h`), never registered as a QML type/singleton, so
  QML cannot call it. The app's established pattern is that C++ computes an
  `initials` value and QML consumes it (`SearchResultsModel`'s `initials` role →
  `model.initials`; `KioskFeedRow`'s `rowInitials`). LAvatar follows that pattern
  — taking a precomputed string — so the fallback text can never drift from the
  chips it replaces. Search passes `model.initials`; the kiosk passes a new
  `KioskViewModel.currentInitials` (§4.3).
- `size` : int — diameter in px (default suited to a list row, e.g. 40).
- `fallbackBackground` : color — chip background, **default `Theme.brand.soft`**.
- `fallbackForeground` : color — chip initials text, **default `Theme.brand.base`**.
  These two are overridable **because the default light-maroon-on-light pairing is
  a contrast hazard on the kiosk's maroon hero gradient** (the emphasized feed chip
  already inverts to `brand.base` bg + `brand.on` text for exactly this reason —
  `KioskFeedRow.qml:41-45`). Making them props settles the API now rather than
  discovering the clash at build time; the kiosk hero passes an on-dark pairing
  (§4.4), search/other callers use the defaults.

**Behavior:**

- `readonly property bool showInitials` — true when `source` is empty, its path
  **ends with** `default.jpg` (a **suffix/path check**, not filename equality —
  `…/uploads/students/default.jpg` must also fall back), or the inner
  `LCircleImage.imageStatus === Image.Error`.
- When `showInitials`: render the initials chip (circle filled `fallbackBackground`,
  `initials` text in `fallbackForeground`).
- Else: render the photo via `LCircleImage { source: <source> }` (its Canvas
  cover-crop), with the initials chip as its placeholder slot so a
  still-loading/failed image shows initials, never an empty disc.

**Theming:** zero raw hex — brand tokens only (the two fallback props default to
tokens and callers pass tokens), per the `quick/` conventions.

### 4.1b Module registration

Both new QML files must be added to the `witsquickmodule` `QML_FILES` list in
`quick/CMakeLists.txt` (the `qt_add_qml_module` call) so the `LOAMS` module
exposes them: `qml/components/LCircleImage.qml` and `qml/components/LAvatar.qml`.

### 4.2 Data model — `core/studentdata.h`

Add one field to `StudentRecord`:

```cpp
QString photo;   // relative path from search_students.php; empty on NULL
```

Purely additive; existing serialize-back paths (bulk update, CSV) are unchanged
(they do not include `photo`).

### 4.3 Parsing + role exposure (C++ prefixing)

- **`StudentController::parseSearchResponse`** (`core/studentcontroller.cpp`):
  read `rec.photo = s["photo"].toString();` (absent/empty tolerated → empty).
- **`SearchResultsModel`** (`quick/models/SearchResultsModel.{h,cpp}`): add a
  `PhotoRole` to the `Roles` enum. Its `data()` returns the record's `photo`
  **joined to `ApiConfig::baseUrl()`** via `ApiConfig::endpoint()` when non-empty
  (include `core/apiconfig.h` — it is header-only and already links into both app
  and tests), and an **empty string** when the record's photo is empty (never
  join an empty path — `ApiConfig::endpoint("")` yields the bare base URL and
  would force a needless load-failure). **Also add the explicit
  `roleNames()` entry `{ PhotoRole, "photo" }`** next to `{ InitialsRole,
  "initials" }` — without it `model.photo` will not resolve in QML. This is the
  single place search photos become absolute.
- **`KioskViewModel`** (`quick/viewmodels/KioskViewModel.{h,cpp}`):
  `applyStudentLogin` reads `student.value("photo_url").toString()` into a new
  `m_currentPhotoUrl`; expose `QString currentPhotoUrl` on the existing
  `currentChanged` signal. No joining — `photo_url` is already absolute. **Also
  expose `QString currentInitials`** = `Initials::of(m_currentFullName)` on the
  same `currentChanged` signal (the kiosk has no `initials` role to reuse, and
  LAvatar needs a precomputed string — §4.1a); computing it in C++ keeps it
  identical to how the feed/search derive initials.

### 4.4 Wiring the two surfaces

- **Search rows** (`SearchScreen.qml`, the delegate's avatar `Rectangle` at
  ~line 502): replace the inline initials `Rectangle`/`Text` with
  `LAvatar { source: model.photo; initials: model.initials; size: 40 }` (default
  `brand.soft`/`brand.base` fallback tokens — the same look as today). Delete the
  stale "No student photo data exists yet" comment. The skeleton-loading
  placeholder circle is unaffected.
- **Kiosk hero** (`KioskMain.qml`, inside the signed-in hero card): add
  `LAvatar { source: vm.currentPhotoUrl; initials: vm.currentInitials }` at the
  left of the hero's content, sized for the 120px hero, visible only when
  `vm.hasStudent`. **Because the hero is the maroon `brand` gradient, pass an
  on-dark fallback pairing** via `fallbackBackground`/`fallbackForeground` (e.g.
  a translucent-cream fill + `brand.on` cream text, mirroring the emphasized feed
  chip's `brand.base`/`brand.on` inversion at `KioskFeedRow.qml:41-45`). The
  exact token values are a live-render detail, but the component API now supports
  the on-dark variant, so it is no longer a latent contract gap. **Feed rows are
  NOT changed.**

## 5. Backend — additive, client-safe first

- **`search_students.php`**: add `"photo" => $row['photo'] ?? ""` to each emitted
  row (the query is `SELECT *`, so the column is available). Use `?? ""` so a NULL
  column serializes as an empty string, not JSON `null` — matching the "empty on
  NULL" contract and the file's existing `isset(...) ? ... : ""` idiom for
  `code`/`visits` (`QJsonValue::toString()` coerces `null`→`""` anyway, so this is
  belt-and-suspenders, not a correctness bug). Emit the **relative** path; the
  client joins the base. Purely additive:
  existing callers ignore unknown fields, and the client tolerates the field's
  absence — so the **client PR can merge and run before** the endpoint is
  deployed (it simply shows initials until the endpoint ships).
- Repo source of truth: `deliverables/loams_api/search_students.php`. Deploy the
  one-line change to `C:/xampp/htdocs/loams_api/` after the client lands
  (there is a `search_students.php.bak-preloams2` already in the web root).
- **`student_login.php`**: **no change** — it already returns `photo_url` with
  the sentinel + `file_exists()` check.

## 6. Testing (Qt Test + QuickTest, offscreen)

Register via `wits_add_qttest()`; add `OFFSCREEN` for the Quick/painting test.

### 6.1 Unit (Qt Test)

- **`tst_studentcontroller`**: `parseSearchResponse` populates `photo` from a
  synthetic response with the field present; leaves it empty when the field is
  absent and when it is an empty string. Existing parser assertions stay green —
  and confirm `toCsv()` / `bulkUpdateStudents()` did **not** gain a `photo`
  column/field (the added struct member must stay search-read-only).
- **`SearchResultsModel`** (in the existing model test): `PhotoRole` returns the
  base-joined absolute URL for a relative `photo`, and an **empty string** for
  an empty `photo` (no bare-base-URL leak); `roleNames()` maps
  `PhotoRole → "photo"`.
- **`KioskViewModel`** (`tst_kioskviewmodel`): `applyStudentLogin` sets
  `currentPhotoUrl` from `photo_url` **and `currentInitials` from the name**;
  `currentChanged` fires.
- **`LLogoCircle` regression guard:** the existing `LLogoCircle` QuickTests must
  stay green across the `LCircleImage` extraction (§4.1) — they are the proof the
  verbatim Canvas move preserved behavior. No new assertions required; a red here
  means the refactor changed behavior.

### 6.2 QuickTest — `LAvatar` (the key coverage, parent §6.2)

Drive `LAvatar` with a fixed `initials` prop and vary `source`. Against **both**
server shapes plus the broken case (assert on `showInitials` / the initials chip
vs. the `LCircleImage` photo path):

1. Absolute `photo_url` (kiosk shape) → **image** shown, not initials.
2. `default.jpg` **sentinel** as a full path (`…/uploads/default.jpg`) →
   **initials**. Include a nested-path variant
   (`…/uploads/students/default.jpg`) to pin that the check is a **suffix/path
   check, not filename equality**.
3. Relative search path already joined to an absolute URL → **image**.
4. **Empty** source → **initials**.
5. **Broken-but-set** source → **initials via `Image.Error`.** This test must
   drive the inner `Image` (inside `LCircleImage`) to a genuine `Image.Error`
   status (a well-formed but non-resolving URL that is actually attempted and
   fails), **not** merely an empty source — the empty-source path (case 4) and
   the load-error path are distinct, and the parent spec explicitly requires
   broken-but-set sources to fall back too. Assert that
   `LCircleImage.imageStatus === Image.Error` was actually reached (poll/wait for
   the status transition), then that `showInitials` is true.

Also assert the fallback-token props: passing `fallbackBackground`/
`fallbackForeground` overrides the chip colors (the kiosk on-dark path), and the
defaults are `Theme.brand.soft`/`Theme.brand.base` (the search path).

## 7. Out of scope

- **Live camera / Qt Multimedia capture** — file-picker photo capture already
  shipped in 4a; nothing camera-related here.
- **Feed-row photos** and any feed-history endpoint change.
- **Any change to `student_login.php`.**
- **Phase-6 backend hardening** — the hardcoded `http://localhost` base in
  `apiconfig.h`, HTTPS, sessions/tokens. Approach A is chosen partly so that
  base-URL change stays a single-file edit.

## 8. Definition of done

- `LCircleImage` extracted (Canvas crop moved verbatim), `LLogoCircle` refactored
  onto it with its existing QuickTests still green; both registered in `QML_FILES`.
- `LAvatar` exists, is theme-token-only, takes a precomputed `initials`, and
  passes the §6.2 QuickTest against both shapes + the broken-URL case.
- Search rows and the kiosk hero show the photo with initials fallback; feed
  rows unchanged.
- `search_students.php` emits the additive relative `photo`; deployed to the web
  root after the client lands.
- Build clean (no new warnings), `ctest` green, `/claude-review` APPROVE, the
  three-agent `create-pr` gate clean, and a GUI smoke of both surfaces
  (a student with a real photo → image; a NULL-photo student → initials).
- Phase 4 (Admin Part 2) is then complete → Phase 5 (Light/Dark) next.
