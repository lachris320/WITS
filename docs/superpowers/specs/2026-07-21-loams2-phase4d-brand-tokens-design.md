# LOAMS 2.0 Phase 4d — Brand Tokens Design Spec (role-based palette: both logo colours on both surfaces)

Date: 2026-07-21
Status: Approved by owner 2026-07-21 (all design decisions locked below); ready for implementation-plan (writing-plans skill next)
Supersedes/Extends: builds on the branding engine landed in Phase 1 (`qt-app/core/brandtheme.*`, `brandthemedata.h`, `brandcolormath.h`) and the `Theme.qml` token surface shipped since Phase 1.
Summary: Replace the current **surface-scoped** palette — logo primary → *all* Admin, logo secondary → *all* Kiosk — with a **role-based** palette where BOTH surfaces use BOTH logo colours, split by element role: **primary = surface / structure / default action; accent = highlight / CTA-on-dark / attention.** This matches the owner-supplied reference mockups, in which both the Admin Dashboard and the Library Kiosk draw structure from `--brand` (#7E1A15) and highlights from `--gold` (#E8B10E). The change is contained to the C++ derivation engine, the `BrandPalette` value type, the QML `Theme` token surface, and the QML call sites that consume those tokens; the backend and the persisted-JSON contract are handled with read-compat aliases so no stored config is broken.

## 1. Goal & framing

Today the engine maps the logo's dominant colour onto every Admin surface and its secondary colour onto every Kiosk surface (`brandthemedata.h:16-18`: *"the logo's primary maps onto Admin, its secondary/accent onto Kiosk"*). The reference mockups reject that split. In both `Library System UI Modernization/Admin Dashboard.dc.html` and `Library Kiosk v2.dc.html`, the `:root` declares **both** `--brand:#7E1A15` (logo primary) and `--gold:#E8B10E` (logo secondary), and **both** surfaces use **both** — brand for the sidebar gradient, nav structure, primary buttons, IDs and stat values; gold for the active-nav highlight, the Kiosk "now signed in" accents, the Admin search CTA, the peak chart bar, and the freshest-row highlight.

**Phase 4d re-expresses the palette by ROLE, not by surface.** The two logo colours become two role families —

- **brand** — surface, structure, and the default action. The maroon.
- **accent** — highlight, call-to-action on a dark field, and attention. The gold.

— and every surface (Admin *and* Kiosk) draws from both, choosing the family by the element's *role*.

This is a design spec: the what and why plus the token/derivation interfaces and the migration contract. The step-by-step task list (TDD cycles, commit points) is produced separately by the writing-plans skill.

### 1.1 Scope & the "4d" name clash (read this — there are two different 4d's)

This is **Phase 4d "brand-tokens"** — its **own phase**, distinct from the separately-planned **Phase 4d "Photo display"** described in `2026-07-19-loams2-phase4-admin-part2-design.md` §2/§4.4 (`LAvatar` on Kiosk + Search, additive `photo` on `search_students.php`). The two share the "4d" label by historical accident and are **unrelated work** — different files, different backend, different risk profile. Wherever this document says "4d" unqualified it means **brand-tokens**. The Photo-display 4d is referenced only to flag the clash; when both are scheduled, the plans must disambiguate them by their suffixes ("4d brand-tokens" vs "4d photo").

Track 4c (Settings) is already merged to master; this phase reuses the Settings status-line component built there (§5). This phase depends on **nothing** from the un-built 4a (Database) / 4b (Reporting) tracks.

## 2. Three design principles (the intent this phase encodes)

These three principles are the owner's stated design intent. They are captured verbatim because every mechanical decision below descends from one of them.

1. **Brand colours express identity; neutral colours express structure.** Only the brand/accent roles follow the logo. The warm neutrals — `card`, the paper/app background, the table-header fill, the row hairline, the muted caption — are FIXED and never logo-derived. This is **not a behaviour change**: the neutrals are *already* fixed today. `fallbackPalette()` seeds every neutral from `WitsTheme::Color::*` (`brandtheme.cpp:41-48`), and `extractPalette()` copies those same neutrals verbatim into a *derived* palette (`brandtheme.cpp:349-357`). Phase 4d freezes that existing behaviour as an explicit contract rather than introducing it.

2. **Minor logo changes should not materially change the semantic palette (token-stability goal).** This is a stated GOAL but an explicit **NON-GOAL for 4d's implementation** — see §7 for the deferred follow-up mechanism and why it is deferred, not delivered here.

3. **Deprecated aliases are temporary and must be removed immediately after the QML migration lands** — **but** this principle applies to only *one* of the two alias kinds this phase introduces. See §6.4: **API aliases** are removed in PR 2; **JSON read-compat aliases** must OUTLIVE PR 2 indefinitely. Principle 3's "remove immediately" governs the API aliases alone.

## 3. The token model — named by role, not by surface

Two role families replace the four surface-scoped brand fields. Each family carries the sub-roles a family needs: a base fill, a deep (hover/pressed) variant, a soft tint, an on-colour for text/icons sitting *on* the fill, and — new in 4d — a *text* variant for the colour used *as* text on a light background.

**Contrast policy (owner-decided): 4.5:1 for any token pair that carries text; 3:1 for a purely graphical pair.** The "text-bearing?" column below is the WCAG floor each token must satisfy against its stated companion.

| New token | Role | Floor | Maps losslessly from |
|---|---|---|---|
| `brand.base` | Structure / default fill (sidebar, primary button, ID text on light) | text 4.5 vs white | `adminPrimary` |
| `brand.deep` | Brand hover / pressed / gradient bottom | (graphical, follows base) | `adminPrimaryHover` — already `shade(base, -0.28)` |
| `brand.soft` | Brand tinted background (avatar chip, soft panel) | (graphical) | `adminPrimarySoft` |
| `brand.on` | Text/icon **on** a `brand.base` fill | text 4.5 vs `brand.base` | `adminOnPrimary` |
| `brand.onMuted` | Muted text/icon on the brand sidebar (inactive nav label) | text 4.5 vs `brand.base` | frozen literal `onBrandMuted` (#EFC9A8) → **derived** |
| `brand.text` | Brand colour used **as text** on a light (`card`) background | text 4.5 vs `card` | **NEW** |
| `accent.base` | Highlight / CTA-on-dark / attention fill (the gold) | **graphical 3:1 vs `brand.base`** | `kioskPrimary` |
| `accent.deep` | Accent hover / pressed | (graphical) | `kioskPrimaryHover` |
| `accent.soft` | Accent tinted background (report-type icon tile) | (graphical) | `kioskPrimarySoft` + frozen literal `secondarySoft` (#FDF3E0) → **derived** |
| `accent.on` | Text/icon **on** an `accent.base` fill (e.g. the Admin search CTA label) | text 4.5 vs `accent.base` | `kioskOnPrimary` |
| `accent.text` | Accent colour used **as text** on a light background (the #8a6a08 role) | text 4.5 vs `card` **and** vs `accent.soft` | **NEW** |

The old `secondary` field (today `= kioskPrimary`, `brandtheme.cpp:347`) collapses into `accent.base`; it has no independent role and is dropped as a distinct token (kept only as an API alias through PR 1 — §6).

### 3.1 The load-bearing invariant

**`accent.base` is the on-DARK accent; `accent.text` is the on-LIGHT accent. Gold-on-white is unrepresentable by construction.** The mockups make this concrete: `--gold` (#E8B10E) is used only as a *fill* or as text on a *dark* field; where a gold hue is needed as text on a light card the designers darken it to `#8a6a08` (`Admin Dashboard.dc.html:184,423` — the report-type icon glyphs). 4d bakes that split into the type system: any element that wants "the gold, as readable text on paper" reaches for `accent.text`, never `accent.base`. This removes the single most common way a designer would produce a contrast failure with this palette.

## 4. Derivation & the split-contrast engine

Derivation still runs **only on an explicit logo upload**, never at runtime: `regenerateFromLogo()` (`brandtheme.cpp:412`) fires from `ThemeViewModel::regenerateFromImportedLogo()` (`ThemeViewModel.h:65`) on the Settings logo-import path; the result is cached as JSON via `saveCachedConfig`/`loadCachedConfig` (`brandtheme.cpp:368-404`) and reloaded thereafter. 4d changes **what** `extractPalette()` builds from the two seeds, not **when**.

Seed extraction is **unchanged**: quantise the logo into 4-bit-per-channel buckets (4096 max), `primarySeed` = `bucketCenter(highestCountKey(...))` (highest-count bucket, snapped to bucket centre), `secondarySeed` = highest-count bucket whose hue differs from the primary's by ≥ `kMinHueSeparationDeg` (60°), else the primary rotated +45° (`brandtheme.cpp:296-319`). Admission thresholds `kMinSaturation = 0.15`, `kMinValue = 0.15` stay.

The **build** step (`brandtheme.cpp:321-360`) is rewritten to produce the role-based palette below. Colour math reuses the existing header-only `shade` / `mix` / `contrastRatio` / `relativeLuminance` (`brandcolormath.h`) unchanged.

| Token | Derives from | Transform | Contrast floor |
|---|---|---|---|
| `brand.base` | `primarySeed` | darken until ≥ **4.5:1** vs white | 4.5 vs white |
| `brand.deep` | `brand.base` | `shade(base, -0.28)` (`kHoverShade`) | — |
| `brand.soft` | `brand.base` | `mix(base, white, 0.90)` (`kSoftMixToWhite`) | — |
| `brand.on` | — | `white` (guaranteed ≥ 4.5 once `brand.base` is clamped to 4.5 vs white) | 4.5 vs `brand.base` |
| `brand.onMuted` | `brand.on`, `brand.base` | `mix(brand.on, brand.base, 0.25)` | 4.5 vs `brand.base` — **verify; clamp if it fails** |
| `brand.text` | `brand.base` | darken until ≥ 4.5:1 vs `card` | 4.5 vs `card` |
| `accent.base` | `secondarySeed` | clamp (darken) until ≥ **3:1 vs `brand.base`** (graphical; **NOT** vs white) | 3 vs `brand.base` |
| `accent.deep` | `accent.base` | `shade(base, -0.28)` | — |
| `accent.soft` | `accent.base` | `mix(base, white, 0.88)` | — |
| `accent.on` | `brand.deep` / `accent.base` | `brand.deep` if ≥ 4.5 vs `accent.base`; else `shade(accent.base, -0.60)` (`kOnColorDeepShade`); else best of black/white | 4.5 vs `accent.base` |
| `accent.text` | `accent.base` | `shade(base, -0.40)`, then darken until ≥ 4.5:1 vs **both** `card` **and** `accent.soft` | 4.5 vs `card` and vs `accent.soft` |

Notes that pin the transforms to the mockups:

- **`brand.base` raises the contrast target from 3.0 to 4.5.** Today `enforceOnWhite()` (`brandtheme.cpp:243-253`) hardcodes `MinContrast` (= 3.0, `brandtheme.h:22`). `brand.base` is a *text-bearing* fill (IDs, stat values, the brand-as-fill button label is white on it), so it needs the 4.5 floor. The engine change: parameterise the enforce target (a `target` argument, or a sibling `enforceContrast(c, against, target)`), so the same routine serves the 3.0 graphical floor and the 4.5 text floor. Darkening converges toward black (max contrast 21), so 4.5 is always reachable; confirm `kEnforceMaxIterations` (24 steps of `kEnforceStep = -0.08`) is sufficient headroom for the 4.5 target and widen it if a pathological seed needs more.
- **`accent.soft` uses `mix(..., 0.88)`, not `0.90`.** The mockup's `--gold-soft = mix(g, #FFFFFF, 0.88)` (`Admin Dashboard.dc.html:317`), whereas `--brand-soft = mix(a, #FFFFFF, 0.90)` (`:315`). 4d preserves that asymmetry: brand-soft stays 0.90, accent-soft is 0.88.
- **`accent.on` first branch = `brand.deep`.** The Admin search CTA is `background:var(--gold); color:var(--brand-deep)` (`Admin Dashboard.dc.html:201`) — maroon-deep text on gold. The derivation tries `brand.deep` first precisely to reproduce that.
- **`accent.text` recovers #8a6a08.** `shade(#E8B10E, -0.40)` then darkening to clear 4.5 against both the light card and the gold-soft tile lands in the #8a6a08 neighbourhood the designers hand-picked.

**General engine rule (state it in code and in the header doc):** the palette builder must **distinguish text-bearing token roles from purely graphical roles and apply the correct WCAG floor to each** (4.5 vs 3.0), rather than clamping every colour against a single uniform `MinContrast`. This is the core behavioural change of the derivation rewrite.

### 4.1 Neutrals stay frozen

Per Principle 1, the derived palette continues to copy every neutral from `fallbackPalette()` (`brandtheme.cpp:349-357`) — no neutral is logo-derived. The two *formerly frozen QML literals* that DO change are `onBrandMuted` and `secondarySoft`: 4d turns them from `Theme.qml` hex literals (`theme.qml:34,41`) into the derived `brand.onMuted` / part of `accent.soft`. Those two are brand-adjacent, not neutral, so deriving them is consistent with Principle 1. Every genuinely-neutral token (`card`, `appBackground`, `tableHeaderBg`, `rowHairline`, `mutedTextCaption`, `border`, `errorSoft`, `errorBorder`, `scrim`, `success`, `error`) stays exactly as-is.

## 5. Bad-logo fallback + non-blocking notification (owner-decided: Option 2)

A logo can be chromatically unusable — greyscale, near-monochrome, or with no colour distinguishable enough to serve as an accent. Today `extractPalette()` already returns `fallbackPalette()` for the no-chromatic-pixels case (`brandtheme.cpp:291-294`) but does so **silently** and does not catch the "two colours too similar to split into brand + accent" case. 4d adds an explicit, reported quality gate.

**Quality gate (runs after derivation, before the palette is accepted).** Reject the derived palette and return `fallbackPalette()` when any of these hold:

- **Near-monochrome / no distinguishable accent:** the hue distance between the brand seed and the accent seed is below a floor (i.e. the secondary fell back to the +45° rotation *and* even that does not clear a minimum separation), so brand and accent would read as the same colour.
- **Insufficient chroma:** the primary seed's saturation is below a floor (a washed-out logo that yields a muddy fill).
- **The split-contrast clamps cannot be satisfied** for a text-bearing pair within the iteration budget.

Concretely: define a `paletteIsUsable(...)` predicate over the derived palette + seeds checking (a) `hueDistanceDeg(brandSeedHue, accentSeedHue) ≥` a floor, (b) `primarySeed.hsvSaturationF() ≥` a floor, (c) the required 4.5/3.0 floors were actually reached. On failure, return `fallbackPalette()` **and** set a **`didFallBack`** flag.

**Surfacing the fallback — carry a `didFallBack` signal, not a silent swap.** The flag rides on the derivation result and into `BrandingConfig`. The Settings screen surfaces it through the **`SectionStatus`** status-line component already built in 4c (`qt-app/quick/qml/admin/SettingsScreen.qml`) — **non-blocking, no modal**:

> "This logo couldn't produce a usable colour palette. LOAMS is using the default theme instead."

**Fire-once semantics.** Show the message on the upload that *triggered* the fallback (or when the fallback state *changes*), **not** on every Settings open. Track this via the **logo hash already in `BrandingConfig`** (`brandthemedata.h:71`, `c.logoHash`): a fallback is "new" when the current logo hash differs from the last hash for which the message was shown, or when `didFallBack` flips relative to the cached config. A re-open of Settings with the same fallen-back logo shows nothing.

## 6. Migration — 3 implementation landings, 2 reviewable PRs

The rename touches a value type, a JSON contract, a QML singleton, and ~60 call sites. Splitting it into two reviewable PRs keeps each diff auditable while never leaving master in a state that drops a stored config to fallback.

### 6.1 PR 1 — Infrastructure (no rendered change against the existing cached config)

**Honest framing:** PR 1 produces **no rendered change against the *existing* cached config** — the old JSON keys still deserialize, the API aliases resolve to the same colours, and the running UI looks identical. It is **not** "nothing changes": **derivation output changes on the next logo upload**, because the role-based build + split-contrast clamps + quality gate are live. State it that way in the PR body — "no visible change until the next logo upload," never "nothing visible changes."

Structure PR 1 as **two commits**:

- **Commit (a) — mechanical rename + API aliases.** Rename the `BrandPalette` fields and `kPaletteFields` map (`brandthemedata.h:19-63`, `brandtheme.cpp:56-74`) to the role names; rename the `ThemeViewModel` `Q_PROPERTY`/accessor surface (`ThemeViewModel.h:19-56`); add the new role tokens to `Theme.qml`. Keep every old name as an **API alias**.
- **Commit (b) — new derivation.** Rewrite `extractPalette()`'s build step to the §4 role derivation + split-contrast clamps; add the §5 quality gate + `didFallBack`; and extract a **`buildPalette(primarySeed, secondarySeed)` seam** so tests can drive derivation from raw seed pairs without rendering a logo (§8).

**API aliases are pure bindings / accessor-forwards — one source of truth, never duplicated values.** `Theme.brand.admin` becomes a binding to the new `brand.base`; `ThemeViewModel::adminPrimary()` forwards to the new accessor; `secondary` forwards to `accent.base`. Because every alias *reads through* to the single new token, the old and new names cannot drift apart — there is structurally no second copy of any colour to fall out of sync.

**JSON contract handled two ways so no stored config breaks:**

- **Read-compat in `paletteFromJson`.** `paletteFromJson` starts from `fallbackPalette()` and overwrites only keys it finds valid (`brandtheme.cpp:88-96`). Add the **old snake_case keys** (`admin_primary`, `kiosk_primary`, `secondary`, …) as accepted aliases that populate the corresponding new fields. Without this, a cached config written by any prior build — which holds only old keys — would silently keep fallback values for every renamed field. This is the **crux migration hazard** and the reason read-compat is mandatory.
- **Dual-write in `paletteToJson`.** Emit **both** the new role keys **and** the old keys, so an *older* build that reads a config written by a new build still finds its expected keys and does not fall back. (Belt-and-suspenders against a mixed-version rollback window.)

**Backend is safe.** `deliverables/loams_api/save_branding.php` stores the palette JSON **opaquely** — it `json_decode`s the `palette` field and only checks the result is an array/object (`save_branding.php:18-23`), never validating specific keys. Renaming keys is therefore backend-safe with no PHP edit. (Context only, not scope: `save_branding.php`/`get_branding.php` are a pre-existing not-yet-deployed gap, unrelated to and unblocked by this phase.)

**Nothing in `qt-app/quick/qml/` is *renamed* in PR 1.** The only `quick/qml/` edit is **additive**: `Theme.qml` gains the new role-token bindings and keeps the old `brand.*` / `secondary*` tokens as alias bindings. No screen or component *call site* is migrated in PR 1 — that is entirely PR 2's job.

### 6.2 PR 2 — QML sweep (the visible UI change)

PR 2 migrates the consumers and deletes the API aliases:

- Rename the **~47 `Theme.brand.*` occurrences across 16 QML files** and the **~14 `Theme.secondary*` occurrences across 7 QML files** to the new role tokens, applying the correct **role per the mockups** — e.g. Admin search button → `accent`; active nav item → `accent` (inactive stays `brand.onMuted`, mockup `#EFC9A8`); chart peak bar → `accent`; freshest / newest row highlight → `accent.soft`. This is the phase's visible change: elements that were uniformly one surface colour now split by role.
- Turn `secondarySoft` and `onBrandMuted` from frozen `Theme.qml` literals into the derived `accent.soft` / `brand.onMuted` tokens — the one neutral-adjacent visible change.
- **Remove the API aliases** (the old `BrandPalette` accessor forwards, the old `ThemeViewModel` properties, the old `Theme.brand.*`/`secondary*` bindings).
- Add the **grep-guard test** (§8) asserting **zero** old-token occurrences remain in `qt-app/quick/qml/`.
- Update all affected tests.

### 6.3 Third landing: keep the JSON read-compat aliases — indefinitely

The three *landings* are: PR 1 commit (a), PR 1 commit (b), PR 2. The **JSON read-compat aliases from §6.1 are not a landing that gets reverted** — they stay in `paletteFromJson` **forever**. This is the critical asymmetry in §6.4.

### 6.4 Two alias kinds, two lifetimes (Principle 3, precisely)

| Alias kind | Where | Lifetime | Enforcement |
|---|---|---|---|
| **API aliases** | QML `Theme.brand.*`/`secondary*` bindings; C++ `BrandPalette`/`ThemeViewModel` accessor forwards | **Removed in PR 2**, immediately after the QML sweep | Grep-guard test: zero old-token occurrences in `qt-app/quick/qml/` |
| **JSON read-compat aliases** | old snake_case keys accepted by `paletteFromJson` | **OUTLIVE PR 2 — kept indefinitely** | JSON round-trip test that keeps covering old-key input forever (§8) |

**Why the JSON aliases must never be deleted:** cached local configs (`QSettings`) and backend-stored JSON hold the *old* keys forever, and **nothing migrates them** — the backend stores the blob opaquely, so no server-side rewrite exists. Delete the read-compat aliases and every config that has not been re-saved through a new build silently drops to the maroon/gold **fallback** on next load, because `paletteFromJson` would find none of its expected keys valid and keep the `fallbackPalette()` seed for all of them. Principle 3's "remove aliases immediately" therefore applies to the **API aliases only** — the JSON read-compat path is permanent infrastructure, not a deprecation.

## 7. Token-stability (Principle 2) — deferred follow-up, not delivered in 4d

Principle 2's goal — a near-identical re-upload should not materially change the palette — is **explicitly out of 4d's scope**. Document why it is not guaranteed today and specify the follow-up mechanism so a later phase can pick it up cleanly.

**Why it is not guaranteed.** The seed is the **highest-count bucket** (`highestCountKey`, `brandtheme.cpp:226-240`). Two failure modes make a near-identical re-upload flip discontinuously:

- **Near-tied buckets:** if two buckets are within a few pixels of each other in count, a one-pixel re-encode can swap which one wins, moving the seed to a different bucket centre.
- **Threshold crossings:** pixels that sit right at the `kMinSaturation` / `kMinValue` admission edge can cross in or out between two near-identical encodings, shifting the histogram.

**Mitigating facts (why the gap is tolerable for now):** within-bucket edits are *already* stable because `bucketCenter` **snaps** the seed to the bucket centre — sub-bucket colour drift produces the identical seed. And derivation only fires on a **deliberate** logo upload with the admin **watching the result live** in Settings, so a surprising flip is immediately visible and correctable, not a silent background event.

**The deferred mechanism — a seed-Δ check.** After derive + quality-gate, if the new primary/accent seeds are within a threshold Δ (an HSV distance or a ΔE-lite metric) of the **cached** palette's seeds, keep the **cached palette wholesale** rather than adopting the freshly-derived one. Ordering is strict: **derive → quality-gate → Δ-compare** — never Δ-compare a palette that failed the quality gate (a gate-failed derivation must go to fallback, not be Δ-matched against a good cached palette). This reuses the **logo-hash plumbing already in `BrandingConfig`** (and would store the accepted seeds alongside it). It is deferred partly because it needs a **"why didn't my new logo change anything?" escape hatch** — an admin who deliberately tweaks the logo and sees no change needs a way to force adoption — and designing that UX is its own small piece of work. 4d ships the honest, immediate-and-visible derivation; stability is a later refinement.

## 8. Testing

Follows the project TDD rule: QtTest + QuickTest under ctest, registered via `wits_add_qttest()`, `OFFSCREEN` for any GUI/Quick/painting test. The brand-theme suite lives in `qt-app/tests/tst_brandtheme.cpp`.

### 8.1 Remove the obsolete surface-distinctness assertions

The current suite asserts `adminPrimary != kioskPrimary` in **more than one place** — `fallbackKeepsAdminAndKioskDistinct` (`tst_brandtheme.cpp:178`, plus the hover variant `:179`) and `twoToneLogoMapsPrimaryAndSecondary` (`:326`). Under the role model these fields are renamed (`brand.base` / `accent.base`) and the "two *surfaces* differ" framing is obsolete. Replace the surface-distinctness idea with a single **role-distinctness** invariant: `brand.base` ≠ `accent.base` by a **hue-distance floor** (they must be visibly different colours). The two-tone hue-mapping test survives with renamed fields (brand seed ≈ maroon hue, accent seed ≈ gold hue).

### 8.2 Per-palette WCAG invariants (using `contrastRatio()`)

For every derived palette assert, via the existing `BrandColorMath::contrastRatio()`:

- `CR(brand.on, brand.base) ≥ 4.5`
- `CR(accent.on, accent.base) ≥ 4.5`
- `CR(accent.base, brand.base) ≥ 3.0`
- `CR(accent.text, card) ≥ 4.5`
- `CR(accent.text, accent.soft) ≥ 4.5`
- `CR(brand.text, card) ≥ 4.5`
- `brand.base` vs `accent.base` hue distance ≥ the §8.1 floor

The old `generatedPalettesMeetMinContrast` test (`tst_brandtheme.cpp:329-349`, currently checking the 3.0 floor for admin/kiosk on-colours) is superseded by this fuller set with the correct split floors.

### 8.3 Adversarial-seed test via the `buildPalette(seeds)` seam

Add a **data-driven** test that feeds pathological seed pairs directly through the `buildPalette(primarySeed, secondarySeed)` seam (§6.1) — near-white/near-white, black/navy, neon/neon, greyscale — and asserts that for each case **either** the §8.2 invariants hold **or** the fallback fired (`didFallBack == true` and the palette equals `fallbackPalette()`). This is the coverage that proves the quality gate catches what the split-contrast clamps cannot satisfy.

### 8.4 JSON round-trip, including permanent old-key coverage

- New-key round-trip: `paletteToJson` → `paletteFromJson` is identity (as `paletteJsonRoundTrip` is today, `:182-187`, with renamed keys).
- **Old-key input → correct new fields** (read-compat): a JSON object carrying only the old snake_case keys deserializes into the right new role fields. This test is **kept indefinitely** (§6.4) — it is the regression guard for the permanent read-compat aliases.
- **Mixed old+new keys → new wins:** when a config carries both, the new key's value is authoritative.
- The dual-write is covered by asserting `paletteToJson` output contains **both** key sets.

### 8.5 Grep-guard (PR 2)

A test asserting **zero** old token names (`adminPrimary`, `kioskPrimary`, `Theme.brand.admin`, `Theme.secondary`, …) appear anywhere under `qt-app/quick/qml/`. This is what makes Principle 3's "remove the API aliases immediately" enforceable rather than aspirational.

### 8.6 Build & GUI verification

Debug + Release build clean with **no new warnings**; full ctest green (the brand-theme suite plus the wider regression floor). Because this is a visible theming change, a **human GUI walkthrough** of the running `WITS` executable is required: upload a two-tone logo in Settings and confirm both Admin and Kiosk pick up brand *and* accent by role; upload a greyscale/monochrome logo and confirm the non-blocking `SectionStatus` fallback message fires once and the theme reverts to default. `/claude-review` is the design/code review gate (the Codex CLI is not installed on this machine).

## 9. Out of scope / deferred

- **Token-stability seed-Δ check (Principle 2)** — deferred with a specified mechanism (§7); 4d ships the immediate-and-visible derivation only.
- **Phase 4d "Photo display"** — the *other* 4d (`LAvatar`, `search_students.php` photo field); unrelated work sharing the label (§1.1).
- **Backend key validation / deploy of `save_branding.php`/`get_branding.php`** — a pre-existing gap; 4d relies only on the backend's existing opaque storage (§6.1) and adds no PHP.
- **Full light/dark-mode derivation** — `Theme.qml` still hardcodes `mode: "Light"` (`theme.qml:129`); dark-mode brand/accent derivation remains a later phase.
- **Manual-mode theme UI** — `ThemeMode::Manual` stays a code hook with no UI (`brandthemedata.h:11-13`), unchanged by 4d.

## 10. Risks

| Risk | Mitigation |
|---|---|
| Deleting the JSON read-compat aliases would silently drop every un-resaved config to fallback. | The aliases are permanent (§6.3/§6.4); a JSON round-trip test covering old-key input is kept indefinitely (§8.4). |
| PR 1 framed as "nothing changes" would hide that the next logo upload derives differently. | PR 1 body must say "no visible change until the next logo upload," not "nothing changes" (§6.1). |
| Raising `brand.base` to a 4.5 floor could exceed the enforce iteration budget for a pathological seed. | Parameterise the enforce target and confirm/​widen `kEnforceMaxIterations`; the adversarial-seed test exercises it (§4, §8.3). |
| `brand.onMuted = mix(brand.on, brand.base, 0.25)` may fail 4.5 for the small text it carries. | The derivation verifies it against 4.5 vs `brand.base` and clamps if it fails (§4 table); a WCAG invariant asserts it (§8.2 covers the on-colour pairs). |
| A near-tied-bucket flip makes a re-uploaded logo re-theme surprisingly. | Accepted for 4d (Principle 2 is a non-goal); derivation only fires on a deliberate, live-previewed upload, and the seed-Δ follow-up is specified for later (§7). |
| The QML sweep (~61 call sites) mis-assigns a role and an element uses the wrong family. | Roles are pinned to the mockups token-by-token (§6.2); the GUI walkthrough verifies both surfaces against the reference (§8.6). |
