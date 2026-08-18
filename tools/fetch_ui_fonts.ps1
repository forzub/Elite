param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")),
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Test-FontHeader([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $bytes = New-Object byte[] 4
        if ($stream.Read($bytes, 0, 4) -ne 4) { return $false }
        $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
        if ($ascii -eq "OTTO" -or $ascii -eq "ttcf" -or $ascii -eq "wOF2") { return $true }
        return ($bytes[0] -eq 0 -and $bytes[1] -eq 1 -and $bytes[2] -eq 0 -and $bytes[3] -eq 0)
    }
    finally {
        $stream.Dispose()
    }
}

$manifestPath = Join-Path $RepoRoot "src/assets/ui/font_manifest.json"
$manifest = Get-Content -Raw -Encoding UTF8 $manifestPath | ConvertFrom-Json
$destRoot = Join-Path $RepoRoot $manifest.destination
New-Item -ItemType Directory -Force -Path $destRoot | Out-Null

$lock = [ordered]@{
    schema_version = 1
    generated_utc = (Get-Date).ToUniversalTime().ToString("o")
    manifest = "src/assets/ui/font_manifest.json"
    fonts = @()
}

foreach ($font in $manifest.fonts) {
    $dest = Join-Path $destRoot $font.file
    if ($Force -or -not (Test-Path $dest)) {
        $tmp = "$dest.download"
        try {
            Write-Host "[FONT] $($font.id) <- $($font.source)"
            Invoke-WebRequest -UseBasicParsing -Uri $font.source -OutFile $tmp
            if ((Get-Item $tmp).Length -lt 1024) {
                throw "Downloaded font is unexpectedly small: $($font.file)"
            }
            if (-not (Test-FontHeader $tmp)) {
                throw "Downloaded file does not have a recognized OpenType/TrueType/WOFF2 header: $($font.file)"
            }
            Move-Item -Force $tmp $dest
        }
        finally {
            if (Test-Path $tmp) { Remove-Item -Force $tmp }
        }
    }

    if (-not (Test-FontHeader $dest)) {
        throw "Cached file is not a recognized font binary: $($font.file)"
    }

    $hash = (Get-FileHash -Algorithm SHA256 $dest).Hash.ToLowerInvariant()
    $lock.fonts += [ordered]@{
        id = $font.id
        file = $font.file
        sha256 = $hash
        bytes = (Get-Item $dest).Length
        source = $font.source
        license = $font.license
    }
    Write-Host "[FONT] ready $($font.file) sha256=$hash"
}

$lockPath = Join-Path $destRoot "font-lock.json"
$lock | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $lockPath
Write-Host "[FONT] complete: $($manifest.fonts.Count) fonts"
Write-Host "[FONT] lock: $lockPath"
Write-Host "[FONT] review and commit font-lock.json; font binaries remain ignored"
