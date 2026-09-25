# Load win_shell prompt + C++ commands into the current PowerShell session.
# Usage (repo root):  . .\powershell\load.ps1
# Also works as .\powershell\load.ps1 (prompt is installed in the global scope).

$script:WinShellPs = $PSScriptRoot
$script:WinShellRoot = Split-Path -Parent $script:WinShellPs
$script:WinShellPrompt = Join-Path $script:WinShellPs 'prompt'
$script:WinShellBin = Join-Path $script:WinShellRoot 'bin'

try {
    chcp 65001 | Out-Null
    $utf8 = [System.Text.UTF8Encoding]::new($false)
    [Console]::InputEncoding = $utf8
    [Console]::OutputEncoding = $utf8
    $OutputEncoding = $utf8
} catch {
}

foreach ($rel in @(
        'colors.ps1',
        'Write-RGB.ps1',
        'Format-PromptPath.ps1',
        'prompt.ps1'
    )) {
    $full = Join-Path $script:WinShellPrompt $rel
    if (-not (Test-Path -LiteralPath $full)) {
        Write-Error "win_shell: missing file: $full"
        continue
    }
    . $full
}

if (Test-Path -LiteralPath $script:WinShellBin) {
    $binFull = [IO.Path]::GetFullPath($script:WinShellBin)
    $parts = @($env:PATH -split ';' | Where-Object { $_ -ne '' })
    if ($parts -notcontains $binFull) {
        $env:PATH = "$binFull;$env:PATH"
    }
}

foreach ($name in @(
        'ls', 'll', 'cat', 'pwd', 'sort', 'diff', 'tee', 'mkdir',
        'rm', 'cp', 'mv', 'clear', 'man'
    )) {
    Remove-Item -Force -ErrorAction SilentlyContinue "alias:$name"
}

foreach ($name in @('mkdir', 'pwd')) {
    Remove-Item -Force -ErrorAction SilentlyContinue "function:$name"
}
