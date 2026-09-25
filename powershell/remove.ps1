<#
.SYNOPSIS
    Remove the win_shell install from $PROFILE and its sibling directory.

.DESCRIPTION
    Deletes the marked block written by build.ps1 (# >>> win_shell BEGIN ... END)
    and the copied <PROFILE-DIR>\win_shell folder. Other $PROFILE lines stay.
    The current session is not undone.

.EXAMPLE
    .\powershell\remove.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$markerBegin = '# >>> win_shell BEGIN'
$markerEnd   = '# <<< win_shell END'

if (-not $PROFILE) {
    throw '$PROFILE is not defined, cannot remove.'
}

Write-Host "profile: $PROFILE"

if (-not (Test-Path -LiteralPath $PROFILE)) {
    Write-Host '$PROFILE does not exist; nothing to remove.'
    return
}

$out = [System.Collections.Generic.List[string]]::new()
$skip = $false
$removed = $false
foreach ($line in @(Get-Content -LiteralPath $PROFILE -ErrorAction SilentlyContinue)) {
    if ($line -eq $markerBegin) {
        $skip = $true
        $removed = $true
        continue
    }
    if ($skip) {
        if ($line -eq $markerEnd) {
            $skip = $false
        }
        continue
    }
    $out.Add($line)
}

if ($skip) {
    throw "win_shell block in `$PROFILE is missing '$markerEnd'"
}

if (-not $removed) {
    Write-Host 'no win_shell block found in $PROFILE'
    return
}

while ($out.Count -gt 0 -and $out[$out.Count - 1] -eq '') {
    $out.RemoveAt($out.Count - 1)
}

Set-Content -LiteralPath $PROFILE -Value ($out -join [Environment]::NewLine) -Encoding UTF8
Write-Host 'removed win_shell block from $PROFILE'

$installDir = Join-Path (Split-Path -Parent $PROFILE) 'win_shell'
if (Test-Path -LiteralPath $installDir) {
    Remove-Item -LiteralPath $installDir -Recurse -Force
    Write-Host "removed $installDir"
}

Write-Host 'current session is unchanged; new terminals will not load win_shell.'
