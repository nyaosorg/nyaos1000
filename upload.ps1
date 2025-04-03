Set-PSDebug -Strict

if ( $args.Length -lt 2 ){
    Write-Host "Usage: upload.ps1 TAG ASSETS..."
    exit 1
}

$version = $args[0]
if ( (git tag | Where-Object{ $_ -eq $version }).Length -le 0 ){
    Write-Host "tag: `"$version`" not found"
    exit 1
}

$assets = @()
foreach ($i in $args[1 .. ($args.Length-1)] ){
    $assets += (Get-ChildItem $i)
}

gh release create --notes "" -t $version $version $assets
