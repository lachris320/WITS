<#
    Registers the LOAMS turnstile bridge as a Scheduled Task that starts at user
    logon in the INTERACTIVE session. This is required: synthetic keyboard input
    (SendInput) does not reach WITS.exe from a session-0 / "run whether user is
    logged on or not" task.

    Run this once, in an ELEVATED PowerShell, on the gate PC:
        powershell -ExecutionPolicy Bypass -File .\install-bridge-task.ps1

    Re-running updates the existing task. Use -Uninstall to remove it.
#>

[CmdletBinding()]
param(
    [string] $TaskName = 'LOAMS Turnstile Bridge',
    [string] $BridgeScript = (Join-Path $PSScriptRoot 'loams-turnstile-bridge.ps1'),
    [switch] $Uninstall
)

$ErrorActionPreference = 'Stop'

if ($Uninstall) {
    if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
        "Removed scheduled task '$TaskName'."
    } else {
        "No scheduled task '$TaskName' found."
    }
    return
}

if (-not (Test-Path $BridgeScript)) {
    throw "Bridge script not found: $BridgeScript"
}

# Run the bridge hidden, interactively, as the current user.
$action = New-ScheduledTaskAction -Execute 'powershell.exe' `
    -Argument ('-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "{0}"' -f $BridgeScript)

# At logon of THIS user (interactive session).
$trigger = New-ScheduledTaskTrigger -AtLogOn -User ([System.Security.Principal.WindowsIdentity]::GetCurrent().Name)

# Interactive token (not S4U / not "run whether logged on or not"), least privilege.
$principal = New-ScheduledTaskPrincipal `
    -UserId ([System.Security.Principal.WindowsIdentity]::GetCurrent().Name) `
    -LogonType Interactive -RunLevel Limited

# Keep it alive: restart on failure, never auto-stop, allow on battery.
$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew

Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
    -Principal $principal -Settings $settings -Force | Out-Null

"Registered scheduled task '$TaskName' (at logon, interactive)."
"Start it now without logging off:  Start-ScheduledTask -TaskName '$TaskName'"
