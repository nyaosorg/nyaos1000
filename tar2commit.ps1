Set-PSDebug -Strict

function onetgz2commit($tgzpath){
    $tgzname = (split-path -leaf $tgzpath)
    if ( $tgzpath -like "*bz2" ){
        $z = "j"
    } else {
        $z = "z"
    }
    $files = @(tar.exe "-${z}tf" $tgzpath)
    Write-Host "> tar.exe -${z}xvf $tgzpath"
    tar.exe "-${z}xvf" $tgzpath
    $date = $null
    $newfiles = @()
    foreach ( $i in $files ){
        $stamp = (Get-Item $i).LastWriteTime
        if ( $date -eq $null -or $stamp -gt $date ){
            $date = $stamp
        }
        if ( $trim -ne $null -and $i.StartsWith($trim) ){
            $new = $i.SubString($trim.Length)
            Write-Host "Rename $i to $new"
            Move-Item -Path $i -Destination $new
            $newfiles += $new
        } else {
            $newfiles += $i
        }
    }
    Write-Host "> git add $newfiles"
    git.exe add -f $newfiles

    $date = $date.ToString("yyyy-MM-dd HH:mm:ss")
    $env:GIT_COMMITTER_DATE=$date
    Write-Host "> git commit --date `"$date`" -m `"Updates in $tgzname`" -a"
    git.exe commit --date "$date" -m "Updates in $tgzname" -a
    $env:GIT_COMMITTER_DATE=$null
    foreach ( $i in $newfiles ){
        Remove-Item $i
    }
    if ( $trim -ne $null -and (Test-Path $trim) ){
        Remove-Item $trim
    }
}

$trim = $null
if ( $args -ge 2 -and $args[0] -eq "-trim" ){
    $trim = $args[1]
    $args = $args[2..($args.Length -1)]
}

foreach ($arg1 in $args ){
    Write-Host "Expand ${arg1}"
    $files = (Get-ChildItem $arg1)
    foreach ($tgzpath in $files){
        Write-Host "Try ${tgzpath}"
        onetgz2commit $tgzpath
    }
}
