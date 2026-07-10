# LOAMS 2.0 Phase 1 — Deployment-Hardware Render Check

**Purpose.** This is a **human, on-device gate** — it cannot be run from CI and is not
an automated test. It must be executed on the actual library deployment PC (or a
faithful stand-in with the same GPU/driver stack) before LOAMS 2.0 goes live there.
It retires **Risk 4** ("library PC may lack a working OpenGL/RHI path") from the
Phase 0 proposal (§19) / Phase 1 spec (§10), and its result feeds the Task 7
consolidated proof.

**Why this exists.** `WITSQuick` now supports a software-rendering fallback:
pass `--software` on the command line, or set the environment variable
`QT_QUICK_BACKEND=software`, to force the Qt Quick scene graph onto the software
rasterizer instead of the hardware/RHI backend. The dev-machine build check
(done in this task) only proves both code paths *launch* on a machine known to
have working graphics drivers. It does **not** prove the default (hardware) path
works on the real deployment PC, whose GPU/driver situation is unknown. This
checklist is how that gets proved, once, on the real hardware.

---

## Prerequisites

- The library deployment PC (or an equivalent test machine — same GPU/driver/
  Windows build as the eventual deployment target).
- A built copy of `WITSQuick.exe` (and its Qt runtime DLLs / plugins directory)
  copied to the deployment PC. Do not build on the deployment PC unless the
  toolchain is already provisioned there.
- A terminal (Command Prompt or PowerShell) on that PC, so `QSG_INFO=1` can be
  set and the launch can be observed with console output visible.

---

## Checklist

### 1. Environment captured

Record the following about the machine actually used for this check:

| Item | Value |
|---|---|
| Windows version (`winver` or `systeminfo`) | |
| GPU model | |
| GPU driver version | |
| Date of check | |

Run the app once with `QSG_INFO=1` set in the environment before launch, and
record the scene-graph backend line Qt prints to the console (it identifies
which RHI backend — e.g. Direct3D 11/12, OpenGL, or a software fallback —
actually initialized):

```
set QSG_INFO=1
WITSQuick.exe
```

- [ ] Chosen backend line recorded verbatim below:

  ```
  <paste the QSG_INFO backend line here>
  ```

### 2. Default-backend launch result

Launch `WITSQuick.exe` with no arguments (the default/hardware-RHI path).

- [ ] **Result:** PASS / FAIL
- Window renders (the "LOAMS 2.0" window appears and paints), no freeze, no
  crash, no black/garbled window.
- Notes:

  ```
  <free-form notes: render artifacts, delay before first paint, crash dialog, etc.>
  ```

### 3. `--software` launch result

Launch `WITSQuick.exe --software` (the forced software-rasterizer path).

- [ ] **Result:** PASS / FAIL
- Window renders (the "LOAMS 2.0" window appears and paints), no freeze, no
  crash.
- Notes:

  ```
  <free-form notes>
  ```

### 4. Decision

- [ ] Does the library PC need `--software` as the **default** launch mode
  (i.e. did step 2 fail or render incorrectly while step 3 succeeded)?
  - **YES / NO**
- If **YES**: the deployment shortcut/launcher must invoke
  `WITSQuick.exe --software`, or set `QT_QUICK_BACKEND=software` in the
  shortcut's environment, so the software path is used every time the app
  starts on that machine — not just for this one-off check.
- If **NO**: the default hardware/RHI path is confirmed working on the
  deployment hardware; no launcher change is needed, and `--software` is kept
  only as a documented emergency fallback.

### 5. Sign-off

- **Date run:** \_\_\_\_\_\_\_\_\_\_\_\_
- **Run by:** \_\_\_\_\_\_\_\_\_\_\_\_ (name/role of the person who executed this on the
  real deployment hardware)
- **Machine used:** deployment PC / equivalent test machine (circle one) —
  described generically above in the environment table; no machine-local file
  paths or personal identifiers recorded here.

This sign-off is the **R4 retirement evidence** referenced by Task 7's
consolidated Phase 1 proof. Until this section is filled in and signed off by
someone who ran it on real hardware, Risk 4 remains open.
