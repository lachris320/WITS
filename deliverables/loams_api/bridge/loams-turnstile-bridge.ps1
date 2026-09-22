<#
    LOAMS Turnstile -> WITS display bridge

    Runs on the SAME PC as the deployed legacy WITS.exe. Polls the local
    turnstile_pull.php endpoint for a gate swipe that turnstile.php already
    recorded, and — only when WITS is ALREADY the foreground window — injects the
    card digits + ENTER as one HID-style keystroke burst. WITS's existing global
    RFID keyboard filter picks it up exactly as if a USB reader had scanned, so
    handleRfidLogin() -> displayStudent() runs. No change to WITS.exe.

    Design guarantees (see the design plan / memory turnstile-loams-integration):
      * NEVER steals focus. If WITS is not already foreground, the event is left
        pending and ages out. Attendance is safe regardless (turnstile.php wrote it).
      * Claims the event (injected_at) BEFORE typing; if foreground is lost or the
        event ages past the freshness window between claim and typing, it RELEASES
        the claim so a later genuine manual scan is not wrongly suppressed.
      * After SendInput fires, the token is never released (WITS may already be
        processing it).
      * Spaces repeat injections of the SAME card past WITS's ~2.5 s handleRfidLogin
        debounce, so a scan WITS would silently discard is never fired.

    Run it via Task Scheduler AT LOGON, "Run only when user is logged on"
    (interactive session) — synthetic keyboard input does NOT work from a
    session-0 / non-interactive task. See install-bridge-task.ps1 in this folder.
#>

[CmdletBinding()]
param(
    # Local poll endpoint. Explicit IPv4 loopback avoids IPv4/IPv6 ambiguity.
    [string] $PullUrl        = 'http://127.0.0.1/loams_api/turnstile_pull.php',
    # Process name of the deployed app (WITS.exe -> 'WITS'). No extension.
    [string] $WitsProcess    = 'WITS',
    [int]    $PollMs         = 500,
    # Client-side freshness ceiling re-checked just before typing. Keep in step
    # with PULL_FRESHNESS_SECONDS in turnstile_pull.php.
    [double] $FreshnessSec   = 5.0,
    # Must exceed WITS's ~2.5 s same-card handleRfidLogin debounce.
    [double] $SameCardGapSec = 2.8,
    [string] $LogPath        = (Join-Path $PSScriptRoot 'loams-turnstile-bridge.log')
)

$ErrorActionPreference = 'Stop'

# -----------------------------------------------------------------------------
# Win32: foreground detection + gap-free keystroke injection (SendInput)
# -----------------------------------------------------------------------------
if (-not ('LoamsBridge.Native' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace LoamsBridge {
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct HARDWAREINPUT { public uint uMsg; public ushort wParamL; public ushort wParamH; }
    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; [FieldOffset(0)] public HARDWAREINPUT hi; }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public INPUTUNION U; }

    public static class Native {
        [DllImport("user32.dll", SetLastError=true)]
        static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
        [DllImport("user32.dll")]
        static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll", SetLastError=true)]
        static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        const uint INPUT_KEYBOARD    = 1;
        const uint KEYEVENTF_KEYUP   = 0x0002;
        const uint KEYEVENTF_UNICODE = 0x0004;
        const ushort VK_RETURN       = 0x0D;

        // PID owning the current foreground window (0 if none).
        public static uint ForegroundPid() {
            IntPtr h = GetForegroundWindow();
            if (h == IntPtr.Zero) return 0;
            uint pid; GetWindowThreadProcessId(h, out pid); return pid;
        }

        static INPUT KeyUnicode(char c, bool up) {
            INPUT i = new INPUT(); i.type = INPUT_KEYBOARD;
            i.U.ki.wVk = 0; i.U.ki.wScan = (ushort)c;
            i.U.ki.dwFlags = KEYEVENTF_UNICODE | (up ? KEYEVENTF_KEYUP : 0);
            return i;
        }
        static INPUT KeyVk(ushort vk, bool up) {
            INPUT i = new INPUT(); i.type = INPUT_KEYBOARD;
            i.U.ki.wVk = vk; i.U.ki.wScan = 0;
            i.U.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
            return i;
        }

        // Types the whole code as ONE SendInput batch: each char as a Unicode
        // down/up pair, then a VK_RETURN down/up terminator. The OS delivers the
        // batch back-to-back with no inter-key delay, so it stays well under the
        // RFID detector's ~60 ms gap threshold.
        public static uint SendCode(string code) {
            var list = new List<INPUT>();
            foreach (char c in code) { list.Add(KeyUnicode(c, false)); list.Add(KeyUnicode(c, true)); }
            list.Add(KeyVk(VK_RETURN, false));
            list.Add(KeyVk(VK_RETURN, true));
            INPUT[] arr = list.ToArray();
            return SendInput((uint)arr.Length, arr, Marshal.SizeOf(typeof(INPUT)));
        }
    }
}
'@
}

function Write-Log([string] $msg) {
    $line = ('{0} {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'), $msg)
    try { Add-Content -Path $LogPath -Value $line -Encoding utf8 } catch { }
}

# WITS is foreground iff the foreground window's owning process is $WitsProcess.
function Test-WitsForeground {
    try {
        $fgPid = [LoamsBridge.Native]::ForegroundPid()
        if ($fgPid -eq 0) { return $false }
        $p = Get-Process -Id $fgPid -ErrorAction SilentlyContinue
        return ($p -and $p.ProcessName -ieq $WitsProcess)
    } catch { return $false }
}

# True age (seconds) of the event from its server-local created_at. Same PC as
# the DB, so clocks match; on any parse trouble we return 0 (don't block — the
# pull endpoint already filtered to fresh events).
function Get-EventAgeSec([string] $createdAt) {
    try { return ((Get-Date) - [datetime]::Parse($createdAt)).TotalSeconds }
    catch { return 0.0 }
}

Write-Log "bridge start; pull=$PullUrl proc=$WitsProcess poll=${PollMs}ms fresh=${FreshnessSec}s"

$lastCard = ''
$lastInject = [datetime]::MinValue

while ($true) {
    try {
        $evt = Invoke-RestMethod -Uri $PullUrl -Method Get -TimeoutSec 3
        $id  = 0; if ($evt -and $evt.PSObject.Properties['id']) { $id = [int]$evt.id }

        if ($id -gt 0) {
            $card = [string]$evt.card

            # Same-card spacing: don't fire a scan WITS would discard under its
            # ~2.5 s debounce. Leave the event pending; it retries or ages out.
            $sinceLast = ((Get-Date) - $lastInject).TotalSeconds
            if ($card -eq $lastCard -and $sinceLast -lt $SameCardGapSec) {
                Start-Sleep -Milliseconds $PollMs; continue
            }

            # Never steal focus: only proceed if WITS is ALREADY foreground.
            if (Test-WitsForeground) {
                $claim = Invoke-RestMethod -Uri $PullUrl -Method Post -TimeoutSec 3 `
                            -Body @{ action = 'claim'; id = $id }

                if ($claim -and $claim.claimed) {
                    # Re-verify foreground AND freshness AFTER claiming.
                    $age = Get-EventAgeSec ([string]$evt.created_at)
                    if ((Test-WitsForeground) -and ($age -lt $FreshnessSec)) {
                        [void][LoamsBridge.Native]::SendCode($card)  # token now NEVER released
                        $lastCard = $card
                        $lastInject = Get-Date
                        Write-Log "injected id=$id card=$card age=$([math]::Round($age,2))s"
                    } else {
                        # Aborted before typing -> release so a later manual scan
                        # of this card is not wrongly suppressed.
                        try {
                            Invoke-RestMethod -Uri $PullUrl -Method Post -TimeoutSec 3 `
                                -Body @{ action = 'release'; id = $id } | Out-Null
                        } catch { }
                        Write-Log "released id=$id (fg=$(Test-WitsForeground) age=$([math]::Round($age,2))s)"
                    }
                }
            }
        }
    } catch {
        Write-Log ("poll error: {0}" -f $_.Exception.Message)
    }

    Start-Sleep -Milliseconds $PollMs
}
