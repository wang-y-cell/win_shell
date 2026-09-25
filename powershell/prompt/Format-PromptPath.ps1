# 文件夹数 >= 3 时保留盘符与最近两级，中间用 /.../ 省略
function global:Format-PromptPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $normalized = ($Path -replace '\\', '/').TrimEnd('/')

    if ($normalized -match '^([A-Za-z]:)(/.*)?$') {
        $drive = $Matches[1]
        $rest = if ($Matches[2]) { $Matches[2].TrimStart('/') } else { '' }

        if ([string]::IsNullOrEmpty($rest)) {
            return "${drive}/"
        }

        $folders = @($rest -split '/' | Where-Object { $_ -ne '' })

        if ($folders.Count -ge 3) {
            $tail = $folders[($folders.Count - 2)..($folders.Count - 1)]
            return "${drive}/.../$($tail -join '/')"
        }

        return "${drive}/$($folders -join '/')"
    }

    $parts = @($normalized -split '/' | Where-Object { $_ -ne '' })
    if ($parts.Count -ge 3) {
        $head = $parts[0]
        $tail = $parts[($parts.Count - 2)..($parts.Count - 1)]
        return "$head/.../$($tail -join '/')"
    }

    return ($parts -join '/')
}
