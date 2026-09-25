# Load win_shell prompt + C++ commands into the current PowerShell session.
# Usage (repo):        . .\powershell\load.ps1
# Usage (installed):   . <PROFILE-DIR>\win_shell\load.ps1

$script:WinShellPs = $PSScriptRoot
$script:WinShellPrompt = Join-Path $script:WinShellPs 'prompt'

$script:WinShellBin = $null
$binPathFile = Join-Path $script:WinShellPs 'bin.path'
if (Test-Path -LiteralPath $binPathFile) { #bin.path这个文件在不在
    # 通常开始运行这个脚本的时候这个文件都是不在的

    # 如果这个文件存在,我们就读取这个文件中的内容
    $recorded = (Get-Content -LiteralPath $binPathFile -TotalCount 1).Trim()
    # 这个path中的内容是一个路径,这个路径事bin的路径,判断这个路径是否正确
    if ($recorded -and (Test-Path -LiteralPath $recorded)) {
        $script:WinShellBin = $recorded
    }
}
# 如果没有这个文件就寻找bin路径,找到之后将路径位置填入candiadate
if (-not $script:WinShellBin) {
    foreach ($candidate in @(
            (Join-Path $script:WinShellPs 'bin'),
            (Join-Path (Split-Path -Parent $script:WinShellPs) 'bin')
        )) {
        if (Test-Path -LiteralPath $candidate) {
            $script:WinShellBin = $candidate
            break
        }
    }
}

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

# bin路径存在,将路径加入环境变量
if ($script:WinShellBin -and (Test-Path -LiteralPath $script:WinShellBin)) {
    $binFull = [IO.Path]::GetFullPath($script:WinShellBin)
    $parts = @($env:PATH -split ';' | Where-Object { $_ -ne '' })
    if ($parts -notcontains $binFull) {
        $env:PATH = "$binFull;$env:PATH"
    }
}

# 移除powershell别名
foreach ($name in @(
        'ls', 'll', 'cat', 'pwd', 'sort', 'diff', 'tee', 'mkdir',
        'rm', 'cp', 'mv', 'clear', 'man', 'echo', 'sleep', 'rmdir'
    )) {
    Remove-Item -Force -ErrorAction SilentlyContinue "alias:$name"
}

foreach ($name in @('mkdir', 'pwd')) {
    Remove-Item -Force -ErrorAction SilentlyContinue "function:$name"
}
