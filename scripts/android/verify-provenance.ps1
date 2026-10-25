[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Repository
)

$ErrorActionPreference = 'Stop'

$resolvedRepository = (Resolve-Path -LiteralPath $Repository).Path
$provenancePath = Join-Path $resolvedRepository 'docs\android\PROVENANCE.md'
if (-not (Test-Path -LiteralPath $provenancePath -PathType Leaf)) {
    throw "Provenance document not found: $provenancePath"
}

$rows = @()
foreach ($line in Get-Content -LiteralPath $provenancePath) {
    if ($line -match '^\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*([0-9a-fA-F]+)\s*\|$') {
        $expectedSha = $Matches[3].ToLowerInvariant()
        if ($expectedSha.Length -ne 40) {
            throw "Provenance commit IDs must contain 40 hexadecimal characters: $expectedSha"
        }
        $rows += [pscustomobject]@{
            Repository = $Matches[1].Trim()
            Ref = $Matches[2].Trim().Trim('`')
            ExpectedSha = $expectedSha
        }
    }
}

if ($rows.Count -eq 0) {
    throw "No provenance rows were found in $provenancePath"
}

foreach ($row in $rows) {
    $actual = (& git -C $resolvedRepository rev-parse $row.Ref 2>&1).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to resolve provenance ref $($row.Ref): $actual"
    }
    if ($actual -ne $row.ExpectedSha) {
        throw "Provenance mismatch for $($row.Ref): expected $($row.ExpectedSha), found $actual"
    }
    "PASS $($row.Repository) $($row.Ref) $actual"
}
