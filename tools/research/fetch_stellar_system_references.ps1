param(
    [string]$Destination = "third_party/stellar_system_upstream"
)

$ErrorActionPreference = "Stop"

$repos = @(
    @{ Name="REBOUND"; Url="https://github.com/hannorein/rebound.git"; Sha="5a93ed17a90cd90a0ba87e095621e6dba3daba0a" },
    @{ Name="VPLanet"; Url="https://github.com/VirtualPlanetaryLaboratory/vplanet.git"; Sha="dd55da7e1ff063f0ea7048f91c9d2d97d6ba9a5d" },
    @{ Name="P-pop"; Url="https://github.com/kammerje/P-pop.git"; Sha="b2179f93952cf6a1e8c5845fdb040e8c98cf918b" },
    @{ Name="ExoplanetsSysSim"; Url="https://github.com/ExoJulia/ExoplanetsSysSim.jl.git"; Sha="9a0793a9ababbc606b192e4063ba567c2f20d36c" },
    @{ Name="SysSimExClusters"; Url="https://github.com/ExoJulia/SysSimExClusters.git"; Sha="c329140458499521ed2cb0900a83d6f7085eaacb" },
    @{ Name="Synthpop"; Url="https://github.com/synthpop-galaxy/synthpop.git"; Sha="bc170e053231e19965f6085e397d029e683b1ed5" }
)

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

foreach ($repo in $repos) {
    $path = Join-Path $Destination $repo.Name

    if (-not (Test-Path (Join-Path $path ".git"))) {
        git clone --no-checkout $repo.Url $path
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
Write-Host "Pinned stellar-system research sources are under $Destination"
Write-Host "REBOUND and Synthpop are GPL-3.0 research references; do not link or copy them into Elite runtime without an explicit license decision."
