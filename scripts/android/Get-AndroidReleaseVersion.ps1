[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Properties
)

$ErrorActionPreference = 'Stop'

$propertiesPath = (Resolve-Path -LiteralPath $Properties -ErrorAction Stop).Path
$values = @{}
foreach ($line in Get-Content -LiteralPath $propertiesPath) {
    $trimmed = $line.Trim()
    if (-not $trimmed -or $trimmed.StartsWith('#')) {
        continue
    }
    $separator = $trimmed.IndexOf('=')
    if ($separator -le 0) {
        throw "Invalid version property: $line"
    }
    $values[$trimmed.Substring(0, $separator).Trim()] = $trimmed.Substring($separator + 1).Trim()
}

$required = @('upstreamVersion', 'portRevision', 'versionName', 'versionCode', 'releaseTag')
foreach ($name in $required) {
    if (-not $values.ContainsKey($name) -or -not $values[$name]) {
        throw "Missing Android version property: $name"
    }
}

if ($values.upstreamVersion -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
    throw "upstreamVersion must contain exactly three numeric components: $($values.upstreamVersion)"
}
$major = [int64] $Matches[1]
$minor = [int64] $Matches[2]
$patch = [int64] $Matches[3]
$revision = 0L
if (-not [int64]::TryParse($values.portRevision, [ref] $revision) -or $revision -lt 1 -or $revision -gt 999) {
    throw "portRevision must be between 1 and 999: $($values.portRevision)"
}
if ($major -gt 20 -or $minor -gt 99 -or $patch -gt 999) {
    throw "Version components exceed the Android versionCode encoding limits: $($values.upstreamVersion)"
}

$expectedVersionName = "$($values.upstreamVersion).$revision"
$expectedVersionCode = ($major * 100000000L) + ($minor * 1000000L) + ($patch * 1000L) + $revision
$expectedReleaseTag = $expectedVersionName

if ($values.versionName -ne $expectedVersionName) {
    throw "versionName mismatch. Expected $expectedVersionName but found $($values.versionName)."
}
if ([int64] $values.versionCode -ne $expectedVersionCode) {
    throw "versionCode mismatch. Expected $expectedVersionCode but found $($values.versionCode)."
}
if ($values.releaseTag -ne $expectedReleaseTag) {
    throw "releaseTag mismatch. Expected $expectedReleaseTag but found $($values.releaseTag)."
}

[pscustomobject] @{
    UpstreamVersion = $values.upstreamVersion
    PortRevision = [int] $revision
    VersionName = $values.versionName
    VersionCode = [int64] $expectedVersionCode
    ReleaseTag = $values.releaseTag
}
