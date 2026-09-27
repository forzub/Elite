param(
    [string]$Destination = "third_party/systemic_upstream"
)

$ErrorActionPreference = "Stop"

$repos = @(
    @{ Name="StarRuler2"; Url="https://github.com/BlindMindStudios/StarRuler2-Source.git"; Sha="beec9bff697ffbebafaeb66d0cba1856a02cb6db" },
    @{ Name="Pioneer"; Url="https://github.com/pioneerspacesim/pioneer.git"; Sha="c62b938356e37c35d67406b32ebb69d57cf4eeb9" }
)

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

foreach ($repo in $repos) {
    $path = Join-Path $Destination $repo.Name

    if (-not (Test-Path (Join-Path $path ".git"))) {
        git clone --filter=blob:none --no-checkout $repo.Url $path
        if ($LASTEXITCODE -ne 0) { throw "git clone failed: $($repo.Name)" }
    }

    git -C $path fetch --all --tags --prune
    if ($LASTEXITCODE -ne 0) { throw "git fetch failed: $($repo.Name)" }

    git -C $path checkout --detach $repo.Sha
    if ($LASTEXITCODE -ne 0) { throw "git checkout failed: $($repo.Name)" }

    Write-Host "$($repo.Name): $($repo.Sha)"
}

Write-Host ""
Write-Host "Pinned systemic-simulation research sources are under $Destination"
Write-Host "Star Ruler 2 code is MIT. Pioneer is GPL-3.0 and remains research/reference unless a license decision is made."