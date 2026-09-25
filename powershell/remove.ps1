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

function Remove-WinShellDir {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $true
    }

    $last = $null
    foreach ($delay in @(0, 200, 400, 800, 1600)) {
        if ($delay) {
            Start-Sleep -Milliseconds $delay
        }
        try {
            Get-ChildItem -LiteralPath $Path -Force -Recurse -ErrorAction SilentlyContinue |
                Remove-Item -Force -Recurse -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
            Write-Host "removed $Path"
            return $true
        } catch {
            $last = $_
        }
    }

    Write-Warning "could not delete $Path : $($last.Exception.Message)"
    Write-Warning 'folder is in use (another terminal, or OneDrive sync). close them and run remove.ps1 again, or delete the folder by hand.'
    return $false
}

if (-not $PROFILE) {
    throw '$PROFILE is not defined, cannot remove.'
}

Write-Host "profile: $PROFILE"

$profileDir = Split-Path -Parent $PROFILE
$installDir = Join-Path $profileDir 'win_shell'

if (Test-Path -LiteralPath $PROFILE) {
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

    if ($removed) {
        while ($out.Count -gt 0 -and $out[$out.Count - 1] -eq '') {
            $out.RemoveAt($out.Count - 1)
        }
        Set-Content -LiteralPath $PROFILE -Value ($out -join [Environment]::NewLine) -Encoding UTF8
        Write-Host 'removed win_shell block from $PROFILE'
    } else {
        Write-Host 'no win_shell block found in $PROFILE'
    }
} else {
    Write-Host '$PROFILE does not exist'
}

$dirOk = Remove-WinShellDir $installDir
if ($dirOk) {
    Write-Host 'current session is unchanged; new terminals will not load win_shell.'
} else {
    Write-Host 'profile hook is gone, but the copied folder is still there.'
    exit 1
}
