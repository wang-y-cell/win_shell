function global:prompt {
    $path = $ExecutionContext.SessionState.Path.CurrentLocation.Path
    $displayPath = Format-PromptPath -Path $path

    Write-RGB -Text 'windows@PS:' -R $GREEN[0] -G $GREEN[1] -B $GREEN[2] -NoNewline

    $segments = $displayPath -split '/'
    for ($i = 0; $i -lt $segments.Count; $i++) {
        if ($i -gt 0) {
            Write-RGB -Text '/' -R $WHITE[0] -G $WHITE[1] -B $WHITE[2] -NoNewline
        }
        if ($segments[$i] -ne '') {
            Write-RGB -Text $segments[$i] -R $BLUE[0] -G $BLUE[1] -B $BLUE[2] -NoNewline
        }
    }

    Write-RGB -Text '$' -R $WHITE[0] -G $WHITE[1] -B $WHITE[2] -NoNewline

    return ' '
}
