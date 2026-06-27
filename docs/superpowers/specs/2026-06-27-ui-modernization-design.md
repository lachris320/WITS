# WITS UI Modernization — Design Spec

**Date:** 2026-06-27
**Status:** Approved for planning
**Scope:** Presentation-layer redesign of the attendance kiosk (`MainWindow`) and the
admin dashboard (`adminWindow`) to match two provided React/Tailwind mockups, while
preserving all existing functionality.

## Goal

Restyle the two main screens of the WITS Qt 6 / C++17 desktop app to a modern,
"dynamic"-feeling UI based on:

- `dynamic_attendance_system.tsx` — the attendance kiosk (`MainWindow`)
- `admin_dashboard.tsx` — the admin dashboard (`adminWindow`)

This is a **skin, not a rewrite**. No behavioral change to networking, RFID, report
generation, search, or settings persistence.

## Non-Goals / Hard Boundaries

The following are explicitly **out of scope** and must not change behavior:

- Network calls and endpoints (`handleLogin`, `handleRfidLogin`, all admin `on*Btn`
  network slots, `ApiConfig`).
- RFID scan handling and debounce (`RfidKeyboardFilter`, `handleRfidLogin`).
- Recent-logins data logic — `refreshRightPanel`'s fixed 9-row label arrays
  (`nameLabel`…`timeDate_Label_13`) keep their structure and wiring; they are
  **restyled, not converted** to a dynamic table.
- Report generation, charts, PDF/Excel export, student search, settings persistence
  (`QSettings`), and all existing signals/slots.
- All existing widget `objectName`s are preserved so QSS and `.cpp` keep matching.

The mockups contain features the app does not have (live color pickers, background-image
picker, glassy/solid toggle). These are **not** added — "keep functionalities, only adjust
the UI."

## Key Decisions (from brainstorming)

1. **Visual style: Clean solid modern.** No background photo, no fake glassmorphism.
   Flat modern cards: rounded corners, soft shadows, slate sidebar, emerald/blue accents,
   generous spacing. (Matches the mockups' "Solid White" mode; fully achievable in Qt.)
2. **Kiosk right panel: full mockup layout.** Add the active-profile spotlight card on top;
   restyle the recent-logins area into the mockup's table look over the existing 9 rows.
3. **Admin: full mockup restyle.** Restyle the dark sidebar nav, add a top header bar
   (tab title + "Administrator" avatar chip), and wrap each of the 5 pages in rounded cards.
4. **Extras included (all four):** iconography, micro-interactions, toast + empty-state
   polish, themed loading states.

## Architecture — Centralized Theming

Separate styling from logic so the restyle is mostly QSS, not C++:

- **One central QSS stylesheet** shipped as a Qt resource (e.g. `:/resources/wits.qss`),
  loaded once in `main.cpp` via `qApp->setStyleSheet(...)`, layered on top of the existing
  `WitsTheme::lightPalette()`.
- Rules are keyed off existing `objectName`s (`#frame`, `#loginBtn`, `#generalBtn`,
  `stackedWidget`, etc.), so the bulk of the restyle requires **no `.cpp` change**.
- MainWindow's large inline `setStyleSheet(...)` block (`mainwindow.cpp`) and any scattered
  admin styles are **migrated into the central stylesheet**. Per-widget runtime styling that
  depends on state (e.g. active-sidebar highlight, toast success/error color) stays in code
  but reads its colors from a small shared theme helper.
- **Theme helper:** extend `theme.h` (or add `theme.cpp`) with the palette constants from
  the color system below, plus a `loadStyleSheet()` helper. Keep it dependency-light so it
  still links into the unit-test target.

*Rationale:* one source of truth, consistent look across both windows, and it satisfies the
project's UI/logic-separation review rule. Chosen over the current inline-per-window approach
(scattered, hard to keep consistent).

### Structural additions

- **Kiosk spotlight card** and **admin top header bar** are new layout, added in the `.ui`
  files where static, or built in code where they bind to dynamic data. They reuse existing
  widgets/signals (`studentPhoto`, `displayStudent` data, `setActiveSidebar`).

## Color System

Unified palette, defined as constants in the theme helper and referenced by the `.qss`:

| Role | Color |
|------|-------|
| Sidebar base | slate `#1E293B` |
| Card / panel | white `#FFFFFF`, 16px radius, 1px `#E2E8F0` border, soft drop shadow |
| App background | slate-100 `#F1F5F9` |
| Primary action — kiosk login | emerald `#10B981` (hover `#059669`) |
| Primary action — admin | blue `#2563EB` (hover `#1D4ED8`) |
| Secondary buttons | blue `#3B82F6` (hover `#2563EB`) |
| Text / muted text | `#1E293B` / `#64748B` |
| Borders / dividers | `#E2E8F0` |
| Success / error | emerald `#10B981` / red `#EF4444` |

Shadows via `QGraphicsDropShadowEffect` (the app already uses this pattern in
`mainwindow.cpp`).

## Component Design

### Attendance MainWindow (kiosk)

**Sidebar** (`#frame`, slate):
- Logo (`schLogo_Image`) in a white rounded square at top (keeps the existing square-area
  responsive logic in `updateLogo`).
- School name/address (`schoolNameLabel`, `schAddressLabel`) below.
- ID input (`username`): rounded, left icon padding, visible focus ring. Existing
  echo-mode-by-first-char behavior preserved.
- Emerald **LOG IN** button (`loginBtn`) with hover/pressed states + a login icon.
- Status-toast area (see Toast below).
- Bottom block: clock/date (`time_label`, `date_label`) with **Admin** and **Fullscreen**
  affordances. `guestLoginBtn` stays (shown only when enabled, as today).

**Right panel:**
- **Active-profile spotlight card** — avatar initial in a gradient rounded square, large
  name, "logged in" status dot, and a 2×2 grid of Course / Year / Department / Time, bound
  to the data already passed to `displayStudent` + `studentPhoto`. Subtle fade-in on login
  (reuse the existing `QPropertyAnimation` + `QGraphicsOpacityEffect` pattern).
- **Empty state** when idle: centered "Waiting for log in…" placeholder with icon and the
  "scan your ID or type your ID number" hint.
- **Recent-logins table** below: a colored header row (Name / Course / Year / Department /
  Date & Time) over the **existing 9 restyled label-rows**. `refreshRightPanel`'s arrays
  are untouched; rows get card/table row styling and an empty-state message.

### Admin dashboard (`adminWindow`)

**Sidebar** (slate):
- Logo circle on top.
- 5 nav buttons (`generalBtn`, `databaseBtn`, `reportingBtn`, `studentSearchBtn`,
  `visitorBtn`) restyled as rounded nav items with active/inactive states, each with an
  icon, driven by the existing `setActiveSidebar(...)`.
- Small version footer.

**Top header bar (new):** slim bar showing the active tab's title + an "Administrator"
avatar chip. The title updates from the same code path that fires `setActiveSidebar`.

**Content (`stackedWidget`, 5 pages):** each page re-grouped into rounded white cards with
consistent spacing/accents — General, Database, Reporting, Search, Visit Logs. All existing
inputs, tables, buttons, and slots preserved. Data tables get the mockup's clean header/row
styling. Optional light fade on tab switch.

## Extras (all included)

1. **Iconography:** bundle a Lucide-style icon set (SVG/PNG) as resources; wire icons into
   sidebar nav, primary buttons, and empty states.
2. **Micro-interactions:** hover / pressed / focus QSS states on all buttons; visible focus
   ring on the ID input and admin form fields.
3. **Toast + empty states:** restyle `showKioskStatus` as a rounded pill with a
   success/error icon; give all empty states (kiosk waiting, "No data loaded", "No records
   found") the polished centered-placeholder look.
4. **Themed loading states:** restyle `BusyIndicator` and the admin search overlays
   (`showSearchOverlay`/`hideSearchOverlay`) to match the new palette.

## Testing & Verification

Presentation-layer work, so verification is primarily visual + behavioral:

- Clean `cmake -S qt-app -B qt-app/build` + `cmake --build qt-app/build` with no new
  warnings/errors.
- **Run the `WITS` executable** and confirm both screens render and every existing action
  still works: student login, admin-key login, RFID scan path, guest login toggle, all 5
  admin tabs, report PDF/Excel export, student search, settings save/load.
- If the QSS loader gets a helper in `theme.h`, add a trivial Qt Test that it returns a
  non-empty stylesheet (keeps the unit-test target meaningful).
- Run `/claude-review` before finishing the branch (per workflow rule), fix Critical/
  Important findings, then PR via `create-pr`.

## Risks

- **Hand-editing `.ui` XML** for the spotlight card / header bar is fiddly; mitigate by
  building dynamic-bound structure in code and keeping `.ui` edits minimal and validated by
  a build.
- **QSS specificity vs. inline styles:** existing inline `setStyleSheet` calls override the
  central sheet; all must be migrated or they'll fight the new theme.
- **Fullscreen kiosk scaling:** keep layouts using stretches/spacers so the new cards scale
  across resolutions (recent commits already moved toward responsive layout).
