param(
    [string]$Destination = "third_party/procedural_upstream"
)

$ErrorActionPreference = "Stop"

$repos = @(
    @{ Name="Tobler"; Url="https://github.com/Ian-Parberry/Tobler.git"; Sha="bee7c4d77992db31a427e406a5f201246227cf04" },
    @{ Name="DesignerWorlds"; Url="https://github.com/Ian-Parberry/DesignerWorlds.git"; Sha="f175201ad9cebd6c1153c9a51feb3c327724b432" },
    @{ Name="Scape"; Url="https://github.com/OGRECave/scape.git"; Sha="9c95ec7b6d8372a7a2a4eb6b96dcb5f2219e2bf2" },
    @{ Name="NoMansTerrain"; Url="https://github.com/gistya/NoMansTerrain.git"; Sha="b7042f1fdbe50184cbe023a484248748b81e2d6b" }
)

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

foreach ($repo in $repos) {
    $path = Join-Path $Destination $repo.Name
    if (-not (Test-Path (Join-Path $path ".git"))) {
        git clone --no-checkout $repo.Url $path
        if ($LASTEXITCODE -ne 0) { throw "git clone failed: $($repo.Name)" }
    }

    git -C $path fetch --all --tags --prune
    if ($LASTEXITCODE -ne 0) { throw "git fetch failed: $($repo.Name)" }

    git -C $path checkout --detach $repo.Sha
    if ($LASTEXITCODE -ne 0) { throw "git checkout failed: $($repo.Name)" }

    Write-Host "$($repo.Name): $($repo.Sha)"
}

Write-Host ""
Write-Host "Reference sources are pinned under $Destination"
Write-Host "Do not add them as runtime dependencies without an explicit architecture decision."
