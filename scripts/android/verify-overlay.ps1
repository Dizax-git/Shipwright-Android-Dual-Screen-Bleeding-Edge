[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Repository,

    [Parameter(Mandatory = $true)]
    [string]$StableRef
)

$ErrorActionPreference = 'Stop'

$resolvedRepository = (Resolve-Path -LiteralPath $Repository).Path
$provenancePath = Join-Path $resolvedRepository 'docs\android\PROVENANCE.md'
if (-not (Test-Path -LiteralPath $provenancePath -PathType Leaf)) {
    throw "Provenance document not found: $provenancePath"
}

& git -C $resolvedRepository rev-parse --verify "$StableRef^{commit}" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Stable base does not resolve to a commit: $StableRef"
}

$excludedPaths = @()
foreach ($line in Get-Content -LiteralPath $provenancePath) {
    if ($line -match '^\|\s*`([^`]+)`\s*\|\s*`[^`]+`\s*\|\s*`excluded-divergence`\s*\|$') {
        $excludedPaths += $Matches[1]
    }
}

if ($excludedPaths.Count -eq 0) {
    throw "No excluded-divergence paths were found in $provenancePath"
}

foreach ($path in $excludedPaths) {
    & git -C $resolvedRepository diff --quiet $StableRef -- $path
    if ($LASTEXITCODE -eq 1) {
        throw "Excluded path differs from official stable base ${StableRef}: $path"
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to compare excluded path with official stable base ${StableRef}: $path"
    }
    "PASS excluded-divergence $path"
}
