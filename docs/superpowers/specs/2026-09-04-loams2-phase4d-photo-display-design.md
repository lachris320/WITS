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

### 4.1 New component — `quick/qml/components/LAvatar.qml`

A circular avatar that shows a photo or falls back to initials. Purely
presentational — no knowledge of the backend or `ApiConfig`.

**Public properties:**

- `source` : string — an **absolute** photo URL, or empty. (All relative→absolute
  joining is done by the C++ layer before it reaches here — §4.3.)
- `name` : string — the student's full name, used to derive initials.
- `size` : int — diameter in px (default suited to a list row, e.g. 40).

**Behavior:**

- `readonly property bool showInitials` — true when `source` is empty, its path
  ends with `default.jpg` (suffix check), or the inner `Image.status` is
  `Image.Error`.
- When `showInitials`: render the initials chip — **maroon `Theme.brand.soft`
  background + `Theme.brand.base` text**, the exact styling already used by the
  search row and the kiosk feed chip, so nothing drifts.
- Else: render a circular, cropped `Image` (`fillMode: PreserveAspectCrop`,
  clipped to the circle via an `OpacityMask` or a rounded container).
- The initials themselves come from the shared `Initials` derivation (the same
  helper `RecentLoginsModel`/`SearchResultsModel` already use), so the fallback
  text can never disagree with the chips it replaces.

**Theming:** zero raw hex — brand tokens only, per the `quick/` conventions.

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
  `PhotoRole`. Its `data()` returns the record's `photo` **joined to
  `ApiConfig::baseUrl()`** via `ApiConfig::endpoint()` when non-empty, and an
  **empty string** when the record's photo is empty (never join an empty path —
  that would yield the bare base URL and force a needless load-failure). This is
  the single place search photos become absolute.
- **`KioskViewModel`** (`quick/viewmodels/KioskViewModel.{h,cpp}`):
  `applyStudentLogin` reads `student.value("photo_url").toString()` into a new
  `m_currentPhotoUrl`; expose `QString currentPhotoUrl` on the existing
  `currentChanged` signal. No joining — `photo_url` is already absolute.

### 4.4 Wiring the two surfaces

- **Search rows** (`SearchScreen.qml`, the delegate's avatar `Rectangle` at
  ~line 502): replace the inline initials `Rectangle`/`Text` with
  `LAvatar { source: model.photo; name: model.name; size: 40 }`. Delete the
  stale "No student photo data exists yet" comment. The skeleton-loading
  placeholder circle is unaffected.
- **Kiosk hero** (`KioskMain.qml`, inside the signed-in hero card): add
  `LAvatar { source: vm.currentPhotoUrl; name: vm.currentFullName }` at the
  left of the hero's content column/row, sized for the 120px hero, visible only
  when `vm.hasStudent`. On the maroon gradient the fallback initials chip's
  brand-soft/brand-base pairing must stay legible against the gradient; verify
  by live render (a chip built for the light main area may need a token check
  here — resolve during build, not a contract change).
  **Feed rows are NOT changed.**

## 5. Backend — additive, client-safe first

- **`search_students.php`**: add `"photo" => $row['photo']` to each emitted row
  (the query is `SELECT *`, so `$row['photo']` is already available). Emit the
  **relative** path (empty on NULL); the client joins the base. Purely additive:
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
  absent and when it is an empty string. Existing parser assertions stay green.
- **`SearchResultsModel`** (in the existing model test): `PhotoRole` returns the
  base-joined absolute URL for a relative `photo`, and an **empty string** for
  an empty `photo` (no bare-base-URL leak).
- **`KioskViewModel`** (`tst_kioskviewmodel`): `applyStudentLogin` sets
  `currentPhotoUrl` from `photo_url`; `currentChanged` fires.

### 6.2 QuickTest — `LAvatar` (the key coverage, parent §6.2)

Against **both** server shapes plus the broken case:

1. Absolute `photo_url` (kiosk shape) → **image** shown, not initials.
2. `default.jpg` **sentinel** as a full path (`…/uploads/default.jpg`) →
   **initials**. Include a nested-path variant
   (`…/uploads/students/default.jpg`) to pin that the check is a **suffix/path
   check, not filename equality**.
3. Relative search path already joined to an absolute URL → **image**.
4. **Empty** source → **initials**.
5. **Broken-but-set** source → **initials via `Image.Error`.** This test must
   drive the inner `Image` to a genuine `Image.Error` status (a well-formed but
   non-resolving URL that is actually attempted and fails), **not** merely an
   empty source — the empty-source path (case 4) and the load-error path are
   distinct, and the parent spec explicitly requires broken-but-set sources to
   fall back too. Assert on `image.status === Image.Error` having been reached.

## 7. Out of scope

- **Live camera / Qt Multimedia capture** — file-picker photo capture already
  shipped in 4a; nothing camera-related here.
- **Feed-row photos** and any feed-history endpoint change.
- **Any change to `student_login.php`.**
- **Phase-6 backend hardening** — the hardcoded `http://localhost` base in
  `apiconfig.h`, HTTPS, sessions/tokens. Approach A is chosen partly so that
  base-URL change stays a single-file edit.

## 8. Definition of done

- `LAvatar` exists, is theme-token-only, and passes the §6.2 QuickTest against
  both shapes + the broken-URL case.
- Search rows and the kiosk hero show the photo with initials fallback; feed
  rows unchanged.
- `search_students.php` emits the additive relative `photo`; deployed to the web
  root after the client lands.
- Build clean (no new warnings), `ctest` green, `/claude-review` APPROVE, the
  three-agent `create-pr` gate clean, and a GUI smoke of both surfaces
  (a student with a real photo → image; a NULL-photo student → initials).
- Phase 4 (Admin Part 2) is then complete → Phase 5 (Light/Dark) next.
