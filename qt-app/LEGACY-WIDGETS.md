# Legacy Widgets source — FROZEN (reference only)

The Qt Widgets `WITS` app in this directory — `main.cpp`, `mainwindow.*`,
`adminwindow.*`, `guestwindow.*`, `attachfilesdialog.*`, `busyindicator.*`,
`rfidkeyboardfilter.*`, the `*.ui` forms and `resources.qrc` — is deprecated
(S1 transport-security spec §3 "Legacy Widgets source").

- Not built by default: `LOAMS_BUILD_LEGACY_WIDGETS=OFF`. Developers may
  configure with `-DLOAMS_BUILD_LEGACY_WIDGETS=ON` (this also builds
  `tst_rfidkeyboardfilter` and `tst_responsive_ui`).
- No new features, security migrations or routine maintenance. Never deployable
  or security-supported; release packaging (S1f) rejects the target.
- Shared `core/` changes are not constrained by keeping it buildable; record any
  resulting incompatibility below.
- The binary deployed on the gate PC is a temporary security exception managed
  by the S1 server-side compatibility paths until LOAMS 2.0 passes field
  validation; then this source is deleted (spec §7 "Retirement").

## Known incompatibilities

- None yet.
