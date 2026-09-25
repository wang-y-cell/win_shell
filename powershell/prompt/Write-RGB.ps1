function global:Write-RGB {
    param(
        [string]$Text,
        [int]$R,
        [int]$G,
        [int]$B,
        [switch]$NoNewline
    )
    $ansiCode = "$([char]27)[38;2;$R;$G;${B}m"
    $resetCode = "$([char]27)[0m"

    if ($NoNewline) {
        Write-Host "${ansiCode}${Text}${resetCode}" -NoNewline
    } else {
        Write-Host "${ansiCode}${Text}${resetCode}"
    }
}
