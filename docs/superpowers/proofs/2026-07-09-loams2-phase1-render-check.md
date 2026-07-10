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
| Windows version (`winver` or `systeminfo`) | Windows 11 Pro |
| GPU model | Intel(R) Iris(R) Xe Graphics (integrated, active adapter) + NVIDIA GeForce MX330 (discrete, present, not used for this swapchain) — hybrid-graphics laptop |
| GPU driver version | 32.0.101.7080 |
| Date of check | 2026-07-11 |

Run the app once with `QSG_INFO=1` set in the environment before launch, and
record the scene-graph backend line Qt prints to the console (it identifies
which RHI backend — e.g. Direct3D 11/12, OpenGL, or a software fallback —
actually initialized):

```
# cmd.exe:
set QSG_INFO=1
WITSQuick.exe

# PowerShell:
$env:QSG_INFO = "1"
.\WITSQuick.exe
```

- [x] Chosen backend line recorded verbatim below:

  ```
  qt.scenegraph.general: Creating QRhi with backend D3D11 for window 0x22e37b1b900 (wflags 0x1)
  qt.rhi.general: Adapter 0: 'Intel(R) Iris(R) Xe Graphics' (vendor 0x8086 device 0x9A49 flags 0x0)
  qt.rhi.general:   using this adapter
  qt.rhi.general: Adapter 1: 'NVIDIA GeForce MX330' (vendor 0x10DE device 0x1D16 flags 0x0)
  qt.rhi.general: Adapter 2: 'Microsoft Basic Render Driver' (vendor 0x1414 device 0x8C flags 0x2)
  ```

  Qt selected D3D11 on the integrated Intel Iris Xe adapter; the swapchain,
  pipeline cache, and texture atlas all initialized without error, and the
  process exited cleanly (`finished successfully`).

### 2. Default-backend launch result

Launch `WITSQuick.exe` with no arguments (the default/hardware-RHI path).

- [x] **Result:** PASS
- Window renders (the "LOAMS 2.0" window appears and paints), no freeze, no
  crash, no black/garbled window.
- Notes:

  ```
  Window opens cleanly on the D3D11/Intel-Iris-Xe path with a plain, uniform
  light-gray background (the expected appearance of Phase 1's empty AppShell
  — no screens ship this phase, so a blank content area is correct, not a
  rendering defect). No freeze, no crash, no black or garbled output.
  ```

### 3. `--software` launch result

Launch `WITSQuick.exe --software` (the forced software-rasterizer path).

- [x] **Result:** PASS
- Window renders (the "LOAMS 2.0" window appears and paints), no freeze, no
  crash.
- Notes:

  ```
  Same plain light-gray blank shell as the default path (§2) — opens cleanly,
  no freeze, no crash.
  ```

### 4. Decision

- [x] Does the library PC need `--software` as the **default** launch mode
  (i.e. did step 2 fail or render incorrectly while step 3 succeeded)?
  - **NO** — the default hardware (D3D11) path passed cleanly.
- If **YES**: the deployment shortcut/launcher must invoke
  `WITSQuick.exe --software`, or set `QT_QUICK_BACKEND=software` in the
  shortcut's environment, so the software path is used every time the app
  starts on that machine — not just for this one-off check.
- If **NO**: the default hardware/RHI path is confirmed working on the
  deployment hardware; no launcher change is needed, and `--software` is kept
  only as a documented emergency fallback.

### 5. Sign-off

- **Date run:** 2026-07-11
- **Run by:** Laurence Christopher Tormis — Developer
- **Machine used:** equivalent test machine (a hybrid-graphics laptop — Intel
  Iris Xe + NVIDIA MX330 — used as a stand-in for the deployment PC's GPU/
  driver stack; described generically above in the environment table, no
  machine-local file paths or personal identifiers recorded).

This sign-off is the **R4 retirement evidence** referenced by Task 7's
consolidated Phase 1 proof. Until this section is filled in and signed off by
someone who ran it on real hardware, Risk 4 remains open.
