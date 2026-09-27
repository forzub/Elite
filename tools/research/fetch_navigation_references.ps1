param(
    [string]$Destination = "third_party/navigation_upstream"
)

$ErrorActionPreference = "Stop"

$repos = @(
    @{ Name="Pioneer"; Url="https://github.com/pioneerspacesim/pioneer.git"; Sha="c62b938356e37c35d67406b32ebb69d57cf4eeb9" },
    @{ Name="Pioneer-Autopilot-v2"; Url="https://github.com/Mc-Pain/pioneer.git"; Sha="23e20bf7c3eece93d17547f890ae379dc65a92eb" },
    @{ Name="Naev"; Url="https://github.com/naev/naev.git"; Sha="3163e7b9e50cac28778e2cbb679c3fad84dcd663" },
    @{ Name="SpaceEngineers-Archived"; Url="https://github.com/KeenSoftwareHouse/SpaceEngineers.git"; Sha="54f2f0f3169cda687a25a438097902a43bdfa603" }
)

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

foreach ($repo in $repos) {
    $path = Join-Path $Destination $repo.Name

    if (-not (Test-Path (Join-Path $path ".git"))) {
        git clone --filter=blob:none --no-checkout $repo.Url $path
        if ($LASTEXITCODE -ne 0) {
            throw "git clone failed: $($repo.Name)"
        }
    }

    git -C $path fetch --all --tags --prune
    if ($LASTEXITCODE -ne 0) {
        throw "git fetch failed: $($repo.Name)"
    }

    git -C $path checkout --detach $repo.Sha
    if ($LASTEXITCODE -ne 0) {
        throw "git checkout failed: $($repo.Name)"
    }

    Write-Host "$($repo.Name): $($repo.Sha)"
}

Write-Host ""
Write-Host "Pinned navigation research sources are under $Destination"
Write-Host "These repositories are research references only. Do not create a production dependency without an explicit architecture and license decision."
