[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Apk,
    [Parameter(Mandatory)][string] $ExpectedVersion,
    [Parameter(Mandatory)][int64] $ExpectedVersionCode,
    [Parameter(Mandatory)][string] $ExpectedSignerSha256,
    [string] $Aapt2,
    [string] $Zipalign,
    [string] $Apksigner
)

$ErrorActionPreference = 'Stop'

function Resolve-AndroidBuildTool {
    param(
        [string] $ExplicitPath,
        [Parameter(Mandatory)][string] $Name
    )

    if ($ExplicitPath) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "Android build tool does not exist: $ExplicitPath"
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $sdkRoot = $env:ANDROID_SDK_ROOT
    if (-not $sdkRoot) { $sdkRoot = $env:ANDROID_HOME }
    if (-not $sdkRoot) { $sdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
    $versions = @(Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'build-tools') -Directory -ErrorAction Stop |
        Sort-Object { [version] $_.Name } -Descending)
    foreach ($version in $versions) {
        $candidate = Join-Path $version.FullName $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw "Android build tool '$Name' was not found under $sdkRoot."
}

function Invoke-ApkTool {
    param(
        [Parameter(Mandatory)][string] $FilePath,
        [Parameter(Mandatory)][string[]] $Arguments,
        [Parameter(Mandatory)][string] $Description
    )

    $output = @(& $FilePath @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    $text = ($output | ForEach-Object { [string] $_ }) -join [Environment]::NewLine
    if ($exitCode -ne 0) {
        throw "$Description failed with exit code ${exitCode}: $text"
    }
    return $text
}

function Assert-NativeElfAlignment {
    param([Parameter(Mandatory)] $Entry)

    $reader = [IO.BinaryReader]::new($Entry.Open())
    try {
        $header = $reader.ReadBytes(64)
        if ($header.Length -ne 64 -or [BitConverter]::ToUInt32($header, 0) -ne 0x464c457f -or
            $header[4] -ne 2 -or $header[5] -ne 1 -or [BitConverter]::ToUInt16($header, 18) -ne 183) {
            throw "Invalid arm64 ELF header: $($Entry.FullName)"
        }
        $offset = [BitConverter]::ToUInt64($header, 32)
        $entrySize = [BitConverter]::ToUInt16($header, 54)
        $count = [BitConverter]::ToUInt16($header, 56)
        if ($offset -lt 64 -or $entrySize -lt 56 -or $count -eq 0 -or
            ([decimal]$offset + [decimal]$entrySize * $count) -gt $Entry.Length) {
            throw "Invalid ELF program header table: $($Entry.FullName)"
        }
        # ZIP entry streams are not seekable; skip in bounded chunks, not a whole-library allocation.
        $remaining = $offset - 64
        while ($remaining -gt 0) {
            $chunk = $reader.ReadBytes([int][Math]::Min(16384, $remaining))
            if ($chunk.Length -eq 0) { throw 'Truncated ELF program header table.' }
            $remaining -= $chunk.Length
        }
        $loads = 0
        for ($i = 0; $i -lt $count; $i++) {
            $program = $reader.ReadBytes($entrySize)
            if ($program.Length -ne $entrySize) { throw 'Truncated ELF program header.' }
            if ([BitConverter]::ToUInt32($program, 0) -ne 1) { continue }
            $loads++
            $fileOffset = [BitConverter]::ToUInt64($program, 8)
            $address = [BitConverter]::ToUInt64($program, 16)
            $alignment = [BitConverter]::ToUInt64($program, 48)
            if ($alignment -lt 16384 -or ($alignment -band ($alignment - 1)) -ne 0 -or
                ($fileOffset % 16384) -ne ($address % 16384)) {
                throw "ELF LOAD alignment is not 16 KiB compatible: $($Entry.FullName)"
            }
        }
        if ($loads -eq 0) { throw "ELF has no LOAD segments: $($Entry.FullName)" }
    }
    finally { $reader.Dispose() }
}

function Assert-NoForbiddenPayload {
    param(
        [Parameter(Mandatory)][string[]] $Entries,
        [Parameter(Mandatory)][string] $Container
    )

    foreach ($entry in $Entries) {
        $lower = $entry.Replace('\', '/').ToLowerInvariant()
        if ($lower -match '\.(gb|gbc|gba|sfc|smc|nes|n64|z64|v64|nds|3ds|cia|iso|otr|sav|sra|srm|eep|fla|state)$' -or
            $lower -match '(^|/)data\.solarus$' -or $lower.EndsWith('.solarus')) {
            throw "$Container contains a prohibited ROM, OTR, save, state, or quest payload: $entry"
        }
    }
}

$apkPath = (Resolve-Path -LiteralPath $Apk -ErrorAction Stop).Path
$normalizedExpectedSigner = ($ExpectedSignerSha256 -replace '[^0-9A-Fa-f]', '').ToLowerInvariant()
if ($normalizedExpectedSigner -notmatch '^[0-9a-f]{64}$') {
    throw "Expected signer is not a SHA-256 fingerprint: $ExpectedSignerSha256"
}

$aapt2Path = Resolve-AndroidBuildTool -ExplicitPath $Aapt2 -Name 'aapt2.exe'
$zipalignPath = Resolve-AndroidBuildTool -ExplicitPath $Zipalign -Name 'zipalign.exe'
$apksignerPath = Resolve-AndroidBuildTool -ExplicitPath $Apksigner -Name 'apksigner.bat'

$badging = Invoke-ApkTool -FilePath $aapt2Path -Arguments @('dump', 'badging', $apkPath) -Description 'aapt2 manifest inspection'
$packageMatch = [regex]::Match($badging, "(?m)^package: name='([^']+)' versionCode='([^']*)' versionName='([^']*)'")
if (-not $packageMatch.Success) { throw 'Could not parse package metadata from aapt2 output.' }
$package = $packageMatch.Groups[1].Value
$versionCode = $packageMatch.Groups[2].Value
$versionName = $packageMatch.Groups[3].Value
if ($package -ne 'com.dishii.soh') {
    throw "APK package mismatch. Expected com.dishii.soh but found $package."
}
if ($versionName -ne $ExpectedVersion) {
    throw "APK version mismatch. Expected $ExpectedVersion but found $versionName."
}
if ([int64] $versionCode -ne $ExpectedVersionCode) {
    throw "APK versionCode mismatch. Expected $ExpectedVersionCode but found $versionCode."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($apkPath)
try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName })
    Assert-NoForbiddenPayload -Entries $entries -Container 'APK'

    $abis = @($entries | ForEach-Object {
        if ($_ -match '^lib/([^/]+)/[^/]+\.so$') { $Matches[1] }
    } | Sort-Object -Unique)
    if (($abis -join ',') -ne 'arm64-v8a') {
        throw "APK ABI mismatch. Expected exactly arm64-v8a but found $($abis -join ',')."
    }
    foreach ($requiredLibrary in @('libSDL2.so', 'libsoh.so')) {
        if ($entries -notcontains "lib/arm64-v8a/$requiredLibrary") {
            throw "Missing runtime library: $requiredLibrary"
        }
    }
    foreach ($native in @($archive.Entries | Where-Object { $_.FullName -match '^lib/arm64-v8a/[^/]+\.so$' })) {
        Assert-NativeElfAlignment -Entry $native
    }

    $projectArchives = @($archive.Entries | Where-Object { $_.FullName -match '(?i)\.o2r$' })
    if ($projectArchives.Count -ne 1 -or $projectArchives[0].FullName -ne 'assets/soh.o2r') {
        throw "APK must contain only the expected project archive assets/soh.o2r; found $($projectArchives.FullName -join ',')."
    }
    $projectStream = $projectArchives[0].Open()
    $projectArchive = [IO.Compression.ZipArchive]::new($projectStream, [IO.Compression.ZipArchiveMode]::Read, $false)
    try {
        Assert-NoForbiddenPayload -Entries @($projectArchive.Entries | ForEach-Object { $_.FullName }) -Container 'assets/soh.o2r'
    }
    finally {
        $projectArchive.Dispose()
        $projectStream.Dispose()
    }
}
finally {
    $archive.Dispose()
}

$null = Invoke-ApkTool -FilePath $zipalignPath -Arguments @('-c', '-P', '16', '4', $apkPath) -Description 'zipalign verification'
$signing = Invoke-ApkTool -FilePath $apksignerPath -Arguments @('verify', '--verbose', '--print-certs', $apkPath) -Description 'APK signature verification'
$signerMatches = [regex]::Matches($signing, '(?im)^[^:\r\n]*Signer(?::|\s+#[0-9]+)?\s*certificate SHA-256 digest:\s*([0-9a-f:]{64,95})\s*$')
$signers = @($signerMatches | ForEach-Object {
    ($_.Groups[1].Value -replace '[^0-9A-Fa-f]', '').ToLowerInvariant()
} | Select-Object -Unique)
if ($signers.Count -ne 1) {
    throw "Expected exactly one APK signing certificate; found $($signers.Count)."
}
$actualSigner = $signers[0]
if ($actualSigner -ne $normalizedExpectedSigner) {
    throw "APK signer mismatch. Expected $normalizedExpectedSigner but found $actualSigner."
}

$sha256 = (Get-FileHash -LiteralPath $apkPath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output "PASS package=$package"
Write-Output "PASS versionName=$versionName versionCode=$versionCode"
Write-Output "PASS ABIs=$($abis -join ',')"
Write-Output 'PASS alignment=zipalign-16KiB'
Write-Output 'PASS alignment=all-ELF-LOAD-segments-16KiB'
Write-Output "PASS signerSha256=$actualSigner"
Write-Output 'PASS payload=ROM-OTR-save-quest-free'
Write-Output "PASS sha256=$sha256"
