Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:VcpkgCommit = 'c76c06644034521fb761a39f8f52d8e87d1103d5'
$script:CMakeVersion = '3.31.5'
$script:NdkVersion = '29.0.14206865'

function Invoke-SohProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $FilePath,
        [string[]] $Arguments = @(),
        [Parameter(Mandatory)][string] $WorkingDirectory,
        [switch] $AllowFailure,
        [switch] $Quiet
    )

    $originalDirectory = (Get-Location).Path
    try {
        Set-Location -LiteralPath $WorkingDirectory
        if ([IO.Path]::GetFileName($FilePath) -eq 'gradlew.bat' -and $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
            $gate = Join-Path $env:USERPROFILE '.agents\skills\gradle-build-gate\scripts\invoke_gradle_build_gate.py'
            if (-not (Test-Path -LiteralPath $gate -PathType Leaf)) {
                throw 'Installed Gradle build gate is unavailable; refusing direct Gradle execution.'
            }
            $gateArguments = @($gate, 'run', '--project', $WorkingDirectory,
                '--owner-task', 'Shipwright release tests and arm64 build')
            if ($script:BuildThreadName) { $gateArguments += @('--thread-name', $script:BuildThreadName) }
            $gateArguments += '--'
            $gateArguments += @($Arguments | Where-Object { $_ -ne '--no-daemon' })
            & python @gateArguments | ForEach-Object { Write-Host $_ }
            if ($LASTEXITCODE -ne 0) { throw "Managed Gradle build failed: $LASTEXITCODE" }
            return [pscustomobject]@{ ExitCode = 0; Output = '' }
        }
        $lines = @(& $FilePath @Arguments 2>&1 | ForEach-Object { [string] $_ })
        $exitCode = $LASTEXITCODE
    }
    finally {
        Set-Location -LiteralPath $originalDirectory
    }
    if (-not $Quiet) {
        $lines | ForEach-Object { Write-Host $_ }
    }
    $text = $lines -join [Environment]::NewLine
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw "Command failed with exit code $exitCode`: $FilePath $($Arguments -join ' ')$([Environment]::NewLine)$text"
    }
    [pscustomobject]@{ ExitCode = $exitCode; Output = $text }
}

function Remove-SohDirectory {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.Directory]::Exists($fullPath)) { return }
    Get-ChildItem -LiteralPath $fullPath -File -Force -Recurse -ErrorAction SilentlyContinue |
        ForEach-Object { if ($_.IsReadOnly) { $_.IsReadOnly = $false } }
    [IO.Directory]::Delete($fullPath, $true)
}

function Get-SohArchiveManifestHash {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)

    $archivePath = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $files = @($archive.Entries | Where-Object { -not $_.FullName.EndsWith('/') } | Sort-Object FullName)
        if ($files.Count -eq 0) { throw "Archive has no file entries: $archivePath" }
        $duplicates = @($files | Group-Object FullName | Where-Object Count -gt 1)
        if ($duplicates.Count -gt 0) {
            throw "Archive contains duplicate entries: $($duplicates.Name -join ', ')"
        }
        $manifestLines = foreach ($entry in $files) {
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $entryHash = ([BitConverter]::ToString($sha.ComputeHash($stream)) -replace '-', '').ToLowerInvariant()
            }
            finally {
                $sha.Dispose()
                $stream.Dispose()
            }
            '{0}|{1}|{2}' -f $entry.FullName.Replace('\', '/'), $entry.Length, $entryHash
        }
    }
    finally { $archive.Dispose() }

    $manifest = ($manifestLines -join "`n") + "`n"
    $bytes = [Text.Encoding]::UTF8.GetBytes($manifest)
    $manifestSha = [Security.Cryptography.SHA256]::Create()
    try {
        ([BitConverter]::ToString($manifestSha.ComputeHash($bytes)) -replace '-', '').ToLowerInvariant()
    }
    finally { $manifestSha.Dispose() }
}

function Assert-SohArchive {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Path)

    $archivePath = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $entries = @($archive.Entries | Where-Object { -not $_.FullName.EndsWith('/') })
        if ($entries.Count -eq 0) { throw "SOH archive has no file entries: $archivePath" }
        foreach ($entry in $entries) {
            $name = $entry.FullName.Replace('\', '/').ToLowerInvariant()
            if ($name -match '\.(gb|gbc|gba|sfc|smc|nes|n64|z64|v64|nds|3ds|cia|iso|gcm|rvz|otr|sav|sra|srm|eep|fla|state)$' -or
                $name -match '(^|/)data\.solarus$' -or $name.EndsWith('.solarus')) {
                throw "SOH archive contains prohibited game, save, state, or quest payload: $($entry.FullName)"
            }
        }
    }
    finally { $archive.Dispose() }
    Get-SohArchiveManifestHash -Path $archivePath
}

function Assert-SohCleanCheckout {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $RepositoryRoot)

    $root = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
    $git = (Get-Command git -ErrorAction Stop).Source
    $null = Invoke-SohProcess -FilePath $git -Arguments @('rev-parse', '--verify', 'HEAD') -WorkingDirectory $root -Quiet
    $status = Invoke-SohProcess -FilePath $git -Arguments @('status', '--porcelain=v1', '--untracked-files=all') -WorkingDirectory $root -Quiet
    if ($status.Output.Trim()) {
        throw "Repository must be clean before an isolated release build:$([Environment]::NewLine)$($status.Output)"
    }
}

function Resolve-SohAndroidSdk {
    param([string] $ExplicitPath)

    $candidates = @($ExplicitPath, $env:ANDROID_SDK_ROOT, $env:ANDROID_HOME)
    if ($env:LOCALAPPDATA) { $candidates += (Join-Path $env:LOCALAPPDATA 'Android\Sdk') }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Android SDK was not found. Supply -AndroidSdkRoot or set ANDROID_SDK_ROOT.'
}

function Resolve-SohCMake {
    param([string] $ExplicitPath, [Parameter(Mandatory)][string] $AndroidSdkRoot)

    $candidates = @(
        $ExplicitPath,
        (Join-Path $AndroidSdkRoot "cmake\$script:CMakeVersion\bin\cmake.exe"),
        'C:\Program Files\CMake\bin\cmake.exe'
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw "CMake $script:CMakeVersion was not found in the Android SDK or Program Files."
}

function Assert-SohVisualStudio {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Visual Studio vswhere.exe was not found.' }
    $result = Invoke-SohProcess -FilePath $vswhere -Arguments @(
        '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'
    ) -WorkingDirectory ([IO.Path]::GetDirectoryName($vswhere)) -Quiet
    $installation = $result.Output.Trim()
    if (-not $installation -or -not (Test-Path -LiteralPath (Join-Path $installation 'VC\Tools\MSVC'))) {
        throw 'Visual Studio 2022 with the v143 C++ tools is required.'
    }
    $installation
}

function Get-SohAndroidVersion {
    param([Parameter(Mandatory)][string] $SourceRoot)

    $propertiesPath = Join-Path $SourceRoot 'Android\version.properties'
    $properties = @{}
    foreach ($line in Get-Content -LiteralPath $propertiesPath) {
        if ($line -match '^\s*([^#=]+?)\s*=\s*(.*?)\s*$') { $properties[$Matches[1]] = $Matches[2] }
    }
    if (-not $properties.versionName -or -not $properties.versionCode -or -not $properties.releaseTag) {
        throw "Incomplete Android version manifest: $propertiesPath"
    }
    [pscustomobject]@{
        VersionName = [string] $properties.versionName
        VersionCode = [int64] $properties.versionCode
        ReleaseTag = [string] $properties.releaseTag
    }
}

function Resolve-SohBuildTool {
    param([Parameter(Mandatory)][string] $AndroidSdkRoot, [Parameter(Mandatory)][string] $Name)

    $buildTools = @(Get-ChildItem -LiteralPath (Join-Path $AndroidSdkRoot 'build-tools') -Directory -ErrorAction Stop |
        Sort-Object { [version] $_.Name } -Descending)
    foreach ($version in $buildTools) {
        $candidate = Join-Path $version.FullName $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "Android build tool not found: $Name"
}

function Assert-SohUnsignedApk {
    param(
        [Parameter(Mandatory)][string] $Path,
        [Parameter(Mandatory)][string] $AndroidSdkRoot,
        [Parameter(Mandatory)] $Version
    )

    $apk = (Resolve-Path -LiteralPath $Path).Path
    $aapt2 = Resolve-SohBuildTool -AndroidSdkRoot $AndroidSdkRoot -Name 'aapt2.exe'
    $zipalign = Resolve-SohBuildTool -AndroidSdkRoot $AndroidSdkRoot -Name 'zipalign.exe'
    $apksigner = Resolve-SohBuildTool -AndroidSdkRoot $AndroidSdkRoot -Name 'apksigner.bat'
    $root = [IO.Path]::GetDirectoryName($apk)
    $badging = Invoke-SohProcess -FilePath $aapt2 -Arguments @('dump', 'badging', $apk) -WorkingDirectory $root -Quiet
    $match = [regex]::Match($badging.Output, "(?m)^package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'")
    if (-not $match.Success) { throw 'Could not parse APK package metadata.' }
    if ($match.Groups[1].Value -ne 'com.dishii.soh') { throw "Unexpected APK package: $($match.Groups[1].Value)" }
    if ($match.Groups[2].Value -ne [string] $Version.VersionCode) { throw "Unexpected APK versionCode: $($match.Groups[2].Value)" }
    if ($match.Groups[3].Value -ne $Version.VersionName) { throw "Unexpected APK versionName: $($match.Groups[3].Value)" }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($apk)
    try {
        $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
        $abis = @($entries | ForEach-Object { if ($_ -match '^lib/([^/]+)/[^/]+\.so$') { $Matches[1] } } | Sort-Object -Unique)
        if (($abis -join ',') -ne 'arm64-v8a') { throw "Expected exactly arm64-v8a; found $($abis -join ',')." }
        $o2r = @($archive.Entries | Where-Object FullName -eq 'assets/soh.o2r')
        if ($o2r.Count -ne 1) { throw 'APK does not contain exactly one assets/soh.o2r.' }
        foreach ($entry in $entries) {
            $lower = $entry.ToLowerInvariant()
            if ($lower -match '\.(gb|gbc|gba|sfc|smc|nes|n64|z64|v64|nds|3ds|cia|iso|gcm|rvz|otr|sav|sra|srm|eep|fla|state)$') {
                throw "APK contains prohibited payload: $entry"
            }
        }
    }
    finally { $archive.Dispose() }

    $null = Invoke-SohProcess -FilePath $zipalign -Arguments @('-c', '-P', '16', '4', $apk) -WorkingDirectory $root -Quiet
    $signature = Invoke-SohProcess -FilePath $apksigner -Arguments @('verify', '--verbose', $apk) -WorkingDirectory $root -Quiet -AllowFailure
    if ($signature.ExitCode -eq 0) { throw 'Gradle release candidate is unexpectedly signed.' }

    [pscustomobject]@{
        Package = 'com.dishii.soh'
        VersionName = $Version.VersionName
        VersionCode = $Version.VersionCode
        Abis = $abis
        Sha256 = (Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Invoke-SohWindowsAndroidBuild {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $RepositoryRoot,
        [Parameter(Mandatory)][string] $OutputDirectory,
        [string] $WorkingRoot,
        [string] $AndroidSdkRoot,
        [string] $CMakePath,
        [string] $ReferenceSohO2r,
        [switch] $UseCurrentCheckout,
        [string] $ThreadName,
        [switch] $DeferWorkingDirectoryCleanup,
        [switch] $KeepWorkingDirectoryOnFailure
    )

    $repository = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    if (-not $WorkingRoot) {
        $base = if (Test-Path -LiteralPath 'D:\Temp' -PathType Container) { 'D:\Temp' }
            elseif ($env:RUNNER_TEMP) { $env:RUNNER_TEMP }
            else { [IO.Path]::GetTempPath() }
        $WorkingRoot = Join-Path $base ('soh-windows-native-' + [guid]::NewGuid().ToString('N'))
    }
    $working = [IO.Path]::GetFullPath($WorkingRoot)
    if ($working.StartsWith($repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Working root must be outside the permanent repository.'
    }
    if (Test-Path -LiteralPath $working) { throw "Working root already exists: $working" }
    [IO.Directory]::CreateDirectory($working) | Out-Null
    [IO.Directory]::CreateDirectory($output) | Out-Null

    $succeeded = $false
    $script:BuildThreadName = $ThreadName
    $oldVcpkgRoot = $env:VCPKG_ROOT
    $oldVcpkgCache = $env:VCPKG_DEFAULT_BINARY_CACHE
    $oldGradleHome = $env:GRADLE_USER_HOME
    $oldCMakeBuildParallelLevel = $env:CMAKE_BUILD_PARALLEL_LEVEL
    $oldVcpkgVisualStudioPath = $env:VCPKG_VISUAL_STUDIO_PATH
    $oldVs170CommonTools = $env:vs170comntools
    $oldVcpkgMaxConcurrency = $env:VCPKG_MAX_CONCURRENCY
    try {
        # CMake's build parallelism is independent of Gradle's worker limit.
        # Keep both direct host builds and AGP's nested Ninja build bounded.
        $env:CMAKE_BUILD_PARALLEL_LEVEL = '2'
        $git = (Get-Command git -ErrorAction Stop).Source
        $python = (Get-Command python -ErrorAction Stop).Source
        $java = (Get-Command java -ErrorAction Stop).Source
        $visualStudio = Assert-SohVisualStudio
        # Use the same component-qualified installation for CMake and vcpkg.
        # vs170comntools also supplies vcpkg's supported developer-environment
        # discovery path when its vswhere scan omits another installed instance.
        $env:VCPKG_VISUAL_STUDIO_PATH = $visualStudio
        $env:vs170comntools = Join-Path $visualStudio 'Common7\Tools'
        $env:VCPKG_MAX_CONCURRENCY = '2'
        $sdk = Resolve-SohAndroidSdk -ExplicitPath $AndroidSdkRoot
        $cmake = Resolve-SohCMake -ExplicitPath $CMakePath -AndroidSdkRoot $sdk
        $ndk = Join-Path $sdk "ndk\$script:NdkVersion"
        if (-not (Test-Path -LiteralPath $ndk -PathType Container)) { throw "Android NDK $script:NdkVersion is missing: $ndk" }
        $null = Invoke-SohProcess -FilePath $java -Arguments @('-version') -WorkingDirectory $working -Quiet

        if ($UseCurrentCheckout) {
            $source = $repository
        }
        else {
            Assert-SohCleanCheckout -RepositoryRoot $repository
            $source = Join-Path $working 'source'
            $null = Invoke-SohProcess -FilePath $git -Arguments @(
                'clone', '--local', '--no-hardlinks', '--recurse-submodules', $repository, $source
            ) -WorkingDirectory $working
            $expectedCommit = (Invoke-SohProcess -FilePath $git -Arguments @('rev-parse', 'HEAD') -WorkingDirectory $repository -Quiet).Output.Trim()
            $actualCommit = (Invoke-SohProcess -FilePath $git -Arguments @('rev-parse', 'HEAD') -WorkingDirectory $source -Quiet).Output.Trim()
            if ($actualCommit -ne $expectedCommit) { throw "Isolated checkout mismatch: expected $expectedCommit, found $actualCommit" }
        }

        $vcpkg = Join-Path $working 'vcpkg'
        $null = Invoke-SohProcess -FilePath $git -Arguments @('init', $vcpkg) -WorkingDirectory $working
        $null = Invoke-SohProcess -FilePath $git -Arguments @('-C', $vcpkg, 'remote', 'add', 'origin', 'https://github.com/microsoft/vcpkg.git') -WorkingDirectory $working
        $null = Invoke-SohProcess -FilePath $git -Arguments @('-C', $vcpkg, 'fetch', '--depth', '1', 'origin', $script:VcpkgCommit) -WorkingDirectory $working
        $null = Invoke-SohProcess -FilePath $git -Arguments @('-C', $vcpkg, 'checkout', '--detach', 'FETCH_HEAD') -WorkingDirectory $working
        $env:VCPKG_ROOT = $vcpkg
        $env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $working 'vcpkg-binary-cache'
        [IO.Directory]::CreateDirectory($env:VCPKG_DEFAULT_BINARY_CACHE) | Out-Null

        $nativeTestBuild = Join-Path $working 'native-tests'
        $null = Invoke-SohProcess -FilePath $python -Arguments @(
            (Join-Path $source 'scripts\android\run-modifier-stick-tests.py'), '--build-dir', $nativeTestBuild
        ) -WorkingDirectory $source

        $hostBuild = Join-Path $working 'host-build'
        $null = Invoke-SohProcess -FilePath $cmake -Arguments @(
            '-S', $source, '-B', $hostBuild, '-G', 'Visual Studio 17 2022', '-T', 'v143', '-A', 'x64',
            "-DVCPKG_ROOT:PATH=$vcpkg", "-DCMAKE_GENERATOR_INSTANCE:PATH=$visualStudio",
            '-DCMAKE_BUILD_TYPE:STRING=Release', '-DSOH_TOOLS_ONLY=ON'
        ) -WorkingDirectory $working
        $null = Invoke-SohProcess -FilePath $cmake -Arguments @(
            '--build', $hostBuild, '--config', 'Release', '--target', 'GenerateSohOtr', '--parallel', '2'
        ) -WorkingDirectory $working

        $generatedO2r = Join-Path $hostBuild 'soh\soh.o2r'
        $manifestHash = Assert-SohArchive -Path $generatedO2r
        if ($ReferenceSohO2r) {
            $referenceHash = Assert-SohArchive -Path $ReferenceSohO2r
            if ($manifestHash -ne $referenceHash) {
                throw "Generated soh.o2r content differs from reference. Generated=$manifestHash Reference=$referenceHash"
            }
        }
        $androidO2r = Join-Path $source 'Android\app\src\main\assets\soh.o2r'
        Copy-Item -LiteralPath $generatedO2r -Destination $androidO2r -Force

        $localProperties = Join-Path $source 'Android\local.properties'
        $sdkProperty = $sdk.Replace('\', '/')
        $ndkProperty = $ndk.Replace('\', '/')
        [IO.File]::WriteAllText($localProperties, "sdk.dir=$sdkProperty`nndk.dir=$ndkProperty`n", [Text.UTF8Encoding]::new($false))
        $env:GRADLE_USER_HOME = Join-Path $working 'gradle-home'
        $gradle = Join-Path $source 'Android\gradlew.bat'
        $null = Invoke-SohProcess -FilePath $gradle -Arguments @(
            '--no-daemon', ':app:testReleaseUnitTest', ':app:assembleRelease'
        ) -WorkingDirectory (Join-Path $source 'Android')

        $candidateDirectory = Join-Path $source 'Android\app\build\outputs\apk\release'
        $candidates = @(Get-ChildItem -LiteralPath $candidateDirectory -Filter '*.apk' -File)
        if ($candidates.Count -ne 1) { throw "Expected exactly one release APK; found $($candidates.Count)." }
        $version = Get-SohAndroidVersion -SourceRoot $source
        $apkEvidence = Assert-SohUnsignedApk -Path $candidates[0].FullName -AndroidSdkRoot $sdk -Version $version
        $destination = Join-Path $output $candidates[0].Name
        $temporaryDestination = "$destination.$([guid]::NewGuid().ToString('N')).tmp"
        Copy-Item -LiteralPath $candidates[0].FullName -Destination $temporaryDestination
        if ((Get-FileHash -LiteralPath $temporaryDestination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $apkEvidence.Sha256) {
            throw 'Exported APK hash mismatch.'
        }
        [IO.File]::Move($temporaryDestination, $destination, $true)

        $succeeded = $true
        [pscustomobject]@{
            Status = 'Built'
            Apk = $destination
            ApkSha256 = $apkEvidence.Sha256
            SohO2rSha256 = (Get-FileHash -LiteralPath $generatedO2r -Algorithm SHA256).Hash.ToLowerInvariant()
            SohO2rManifestSha256 = $manifestHash
            Package = $apkEvidence.Package
            VersionName = $apkEvidence.VersionName
            VersionCode = $apkEvidence.VersionCode
            Abis = $apkEvidence.Abis
            VcpkgCommit = $script:VcpkgCommit
        }
    }
    finally {
        $env:VCPKG_ROOT = $oldVcpkgRoot
        $env:VCPKG_DEFAULT_BINARY_CACHE = $oldVcpkgCache
        $env:GRADLE_USER_HOME = $oldGradleHome
        $env:CMAKE_BUILD_PARALLEL_LEVEL = $oldCMakeBuildParallelLevel
        $env:VCPKG_VISUAL_STUDIO_PATH = $oldVcpkgVisualStudioPath
        $env:vs170comntools = $oldVs170CommonTools
        $env:VCPKG_MAX_CONCURRENCY = $oldVcpkgMaxConcurrency
        if ($DeferWorkingDirectoryCleanup) {
            Write-Host "Working directory retained for the caller's exact transactional cleanup: $working"
        }
        elseif ($succeeded -or -not $KeepWorkingDirectoryOnFailure) {
            Remove-SohDirectory -Path $working
        }
        elseif (Test-Path -LiteralPath $working) {
            Write-Warning "Build failed; diagnostic working directory retained by request: $working"
        }
    }
}

Export-ModuleMember -Function @(
    'Get-SohArchiveManifestHash',
    'Assert-SohArchive',
    'Assert-SohCleanCheckout',
    'Invoke-SohWindowsAndroidBuild'
)
