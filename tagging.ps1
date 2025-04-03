if ( $args.Length -lt 2 ){
    Write-Host "tagging KEYWORD TAG"
    exit 0
}

$commits = @()
git log --grep $args[0] | ForEach-Object {
    Write-Host $_
    if ( $_ -match "^commit" ){
        $fields = $_ -split " "
        $commits += $fields[1]
    }
}

foreach ($commit in $commits){
    $command = ("git tag {0} $commit" -f $args[1])
    $ans = (Read-host "$command [Y/N] ? ")
    if ( $ans -eq "y" ){
        Invoke-Expression $command
        exit 0
    }
}
