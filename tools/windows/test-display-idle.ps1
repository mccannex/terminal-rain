<#
Run from an interactive PowerShell session on the Windows desktop.
Temporarily sets this power plan's AC display timeout, starts the real saver,
and restores the timeout when the saver exits or the test reaches its limit.
Use -AutomaticActivation to test the installed saver with a temporary 30-second
saver timeout instead of launching it manually; that mode restores settings at
the test time limit.
Observe whether every display turns off and wakes/dismisses on physical input.
#>
[CmdletBinding()]
param(
    [string]$SaverPath = (Join-Path $PSScriptRoot '../../build-mingw/Terminal Rain.scr'),
    [ValidateRange(30, 300)][int]$DisplayOffSeconds = 60,
    [ValidateRange(90, 600)][int]$MaximumTestSeconds = 180,
    [switch]$AutomaticActivation
)
$ErrorActionPreference = 'Stop'
$saver = (Get-Item -LiteralPath $SaverPath).FullName
if ($MaximumTestSeconds -le $DisplayOffSeconds + 15) {
    throw 'MaximumTestSeconds must allow at least 15 seconds after the display timeout.'
}
if ($AutomaticActivation -and $DisplayOffSeconds -le 30) {
    throw 'DisplayOffSeconds must exceed the 30-second automatic saver timeout.'
}
if (-not ('TerminalRainIdleTest.Power' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace TerminalRainIdleTest {
    public static class Power {
        [DllImport("powrprof.dll")]
        public static extern uint PowerGetActiveScheme(IntPtr root, out IntPtr scheme);
        [DllImport("powrprof.dll")]
        public static extern uint PowerReadACValueIndex(IntPtr root, ref Guid scheme,
            ref Guid subgroup, ref Guid setting, out uint value);
        [DllImport("kernel32.dll")]
        public static extern IntPtr LocalFree(IntPtr memory);
        [DllImport("user32.dll", EntryPoint="SystemParametersInfoW", SetLastError=true)]
        public static extern bool ReadSetting(uint action, uint parameter, ref uint value, uint flags);
        [DllImport("user32.dll", EntryPoint="SystemParametersInfoW", SetLastError=true)]
        public static extern bool WriteSetting(uint action, uint parameter, IntPtr value, uint flags);
    }
}
'@
}
$schemeMemory = [IntPtr]::Zero
$status = [TerminalRainIdleTest.Power]::PowerGetActiveScheme([IntPtr]::Zero, [ref]$schemeMemory)
if ($status -ne 0) { throw "Reading the active power plan failed: $status" }
try { $scheme = [Runtime.InteropServices.Marshal]::PtrToStructure($schemeMemory, [type][Guid]) }
finally { [void][TerminalRainIdleTest.Power]::LocalFree($schemeMemory) }
$subgroup = [Guid]'7516b95f-f776-4464-8c53-06167f40cc99'
$setting = [Guid]'3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e'
[uint32]$originalSeconds = 0
$status = [TerminalRainIdleTest.Power]::PowerReadACValueIndex(
    [IntPtr]::Zero, [ref]$scheme, [ref]$subgroup, [ref]$setting, [ref]$originalSeconds)
if ($status -ne 0) { throw "Reading the original AC display timeout failed: $status" }
[uint32]$originalSaverTimeout = 0
if ($AutomaticActivation) {
    [uint32]$saverActive = 0
    if (-not [TerminalRainIdleTest.Power]::ReadSetting(0x10, 0, [ref]$saverActive, 0) -or
        -not [TerminalRainIdleTest.Power]::ReadSetting(0x0E, 0, [ref]$originalSaverTimeout, 0)) {
        throw 'Reading screensaver settings failed.'
    }
    if (-not $saverActive) { throw 'Enable the screensaver before testing automatic activation.' }
    $installed = (Get-ItemProperty -LiteralPath 'HKCU:\Control Panel\Desktop').'SCRNSAVE.EXE'
    if (-not $installed -or
        (Get-FileHash -LiteralPath $installed).Hash -ne (Get-FileHash -LiteralPath $saver).Hash) {
        throw 'The installed screensaver does not match SaverPath. Install the current build first.'
    }
}

Write-Host "Original AC display timeout: $originalSeconds seconds (0 means Never)."
Write-Host "Temporary timeout: $DisplayOffSeconds seconds. Keep the machine on AC power."
if ($AutomaticActivation) {
    Write-Host "Original saver timeout: $originalSaverTimeout seconds. Temporary timeout: 30 seconds."
    Write-Host 'Leave input alone: Windows should activate the saver, then turn displays off.'
}
else { Write-Host 'The saver starts in 10 seconds. Then leave input alone until displays turn off.' }
Write-Host 'Wake with physical input; check all displays and whether the saver dismisses.'
$process = $null
$saverTimeoutChanged = $false
try {
    & powercfg /setacvalueindex $scheme $subgroup $setting $DisplayOffSeconds
    if ($LASTEXITCODE -ne 0) { throw 'Changing the AC display timeout failed.' }
    & powercfg /setactive $scheme
    if ($LASTEXITCODE -ne 0) { throw 'Applying the power plan failed.' }
    if ($AutomaticActivation) {
        if (-not [TerminalRainIdleTest.Power]::WriteSetting(0x0F, 30, [IntPtr]::Zero, 3)) {
            throw 'Changing the screensaver timeout failed.'
        }
        $saverTimeoutChanged = $true
        Write-Host "Settings will restore automatically in $MaximumTestSeconds seconds."
        $deadline = [DateTime]::UtcNow.AddSeconds($MaximumTestSeconds)
        $activationObserved = $false
        while ([DateTime]::UtcNow -lt $deadline) {
            if (-not $activationObserved) {
                $running = Get-CimInstance Win32_Process -Filter "Name = 'Terminal Rain.scr' OR Name = 'TERMIN~1.SCR'"
                foreach ($candidate in $running) {
                    if ($candidate.CommandLine -match '(?i)[/-]s(?:\s|$)') {
                        Write-Host "Observed automatic fullscreen saver process: $($candidate.ProcessId)."
                        $activationObserved = $true
                        break
                    }
                }
            }
            Start-Sleep -Seconds 1
        }
        if (-not $activationObserved) { Write-Warning 'Automatic saver activation was not observed.' }
    }
    else {
        Start-Sleep -Seconds 10
        $process = Start-Process -FilePath $saver -ArgumentList '/s' -PassThru
        if (-not $process.WaitForExit($MaximumTestSeconds * 1000)) {
            Write-Warning 'Test limit reached. Stopping this test saver and restoring the timeout.'
            Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        }
        else { Write-Host "Saver exited with code $($process.ExitCode)." }
    }
}
finally {
    if ($saverTimeoutChanged) {
        if (-not [TerminalRainIdleTest.Power]::WriteSetting(0x0F, $originalSaverTimeout, [IntPtr]::Zero, 3)) {
            Write-Warning "Restoring the saver timeout failed; original was $originalSaverTimeout seconds."
        }
        else { Write-Host "Restored original saver timeout: $originalSaverTimeout seconds." }
    }
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
    }
    & powercfg /setacvalueindex $scheme $subgroup $setting $originalSeconds
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Restore failed. Run: powercfg /setacvalueindex $scheme $subgroup $setting $originalSeconds"
    }
    else {
        # Preserve a different plan if the user switched plans during the test.
        $active = & powercfg /getactivescheme
        if (($active -join '') -match [regex]::Escape($scheme.ToString())) {
            & powercfg /setactive $scheme
            if ($LASTEXITCODE -ne 0) { Write-Warning 'Restored the stored timeout, but applying it failed.' }
        }
        Write-Host "Restored original AC display timeout: $originalSeconds seconds."
    }
}
