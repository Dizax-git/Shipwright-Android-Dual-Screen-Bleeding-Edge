$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$modulePath = Join-Path $repositoryRoot 'scripts\android\WindowsNativeBuild.psm1'
$entryPointPath = Join-Path $repositoryRoot 'scripts\android\Build-WindowsAndroidRelease.ps1'

if (Test-Path -LiteralPath $modulePath) {
    Import-Module $modulePath -Force
}

function Test-ActionThrows {
    param([Parameter(Mandatory)][scriptblock] $Action)
    try {
        & $Action | Out-Null
        return $false
    }
    catch { return $true }
}

function New-ArchiveFixture {
    param(
        [Parameter(Mandatory)][string] $Path,
        [Parameter(Mandatory)][string[]] $Entries,
        [switch] $Reverse
    )

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $orderedEntries = @($Entries)
    if ($Reverse) { [array]::Reverse($orderedEntries) }
    $archive = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $orderedEntries) {
            $entry = $archive.CreateEntry($name)
            $entry.LastWriteTime = if ($Reverse) {
                [DateTimeOffset]::new(2020, 2, 2, 0, 0, 0, [TimeSpan]::Zero)
            }
            else {
                [DateTimeOffset]::new(2021, 3, 3, 0, 0, 0, [TimeSpan]::Zero)
            }
            $writer = [IO.StreamWriter]::new($entry.Open())
            try { $writer.Write("payload:$name") } finally { $writer.Dispose() }
        }
    }
    finally { $archive.Dispose() }
}

Describe 'Windows-native Shipwright Android build contract' {
    It 'ships a testable module and one release entry point' {
        (Test-Path -LiteralPath $modulePath -PathType Leaf) | Should Be $true
        (Test-Path -LiteralPath $entryPointPath -PathType Leaf) | Should Be $true
    }

    It 'uses a content manifest independent of ZIP order and timestamps' {
        if (-not (Get-Command Get-SohArchiveManifestHash -ErrorAction SilentlyContinue)) {
            throw 'Get-SohArchiveManifestHash is not implemented.'
        }
        $first = Join-Path $TestDrive 'first.o2r'
        $second = Join-Path $TestDrive 'second.o2r'
        $entries = @('assets/custom/a.bin', 'assets/custom/b.bin')
        New-ArchiveFixture -Path $first -Entries $entries
        New-ArchiveFixture -Path $second -Entries $entries -Reverse

        (Get-SohArchiveManifestHash -Path $first) |
            Should Be (Get-SohArchiveManifestHash -Path $second)
        (Get-FileHash -LiteralPath $first -Algorithm SHA256).Hash |
            Should Not Be (Get-FileHash -LiteralPath $second -Algorithm SHA256).Hash
    }

    It 'rejects a ROM or save payload inside soh.o2r' {
        if (-not (Get-Command Assert-SohArchive -ErrorAction SilentlyContinue)) {
            throw 'Assert-SohArchive is not implemented.'
        }
        $archive = Join-Path $TestDrive 'forbidden.o2r'
        New-ArchiveFixture -Path $archive -Entries @('assets/custom/fixture.bin', 'private/game.z64')
        $message = $null
        try { Assert-SohArchive -Path $archive } catch { $message = $_.Exception.Message }
        $message | Should Match 'game\.z64'
    }

    It 'accepts a non-empty project-owned archive' {
        if (-not (Get-Command Assert-SohArchive -ErrorAction SilentlyContinue)) {
            throw 'Assert-SohArchive is not implemented.'
        }
        $archive = Join-Path $TestDrive 'valid.o2r'
        New-ArchiveFixture -Path $archive -Entries @('assets/custom/fixture.bin')
        { Assert-SohArchive -Path $archive } | Should Not Throw
    }

    It 'rejects a dirty source checkout before isolated cloning' {
        if (-not (Get-Command Assert-SohCleanCheckout -ErrorAction SilentlyContinue)) {
            throw 'Assert-SohCleanCheckout is not implemented.'
        }
        $repo = Join-Path $TestDrive 'repo'
        New-Item -ItemType Directory -Path $repo | Out-Null
        git -C $repo init --quiet
        git -C $repo config user.email 'tests@example.invalid'
        git -C $repo config user.name 'Pipeline Tests'
        Set-Content -LiteralPath (Join-Path $repo 'tracked.txt') -Value 'clean'
        git -C $repo add tracked.txt
        git -C $repo commit --quiet -m initial
        Set-Content -LiteralPath (Join-Path $repo 'tracked.txt') -Value 'dirty'
        (Test-ActionThrows { Assert-SohCleanCheckout -RepositoryRoot $repo }) | Should Be $true
    }

    It 'pins dependencies, isolates local work, generates assets and runs Android tests' {
        if (-not (Test-Path -LiteralPath $entryPointPath)) { throw 'Build entry point is not implemented.' }
        $script = Get-Content -LiteralPath $entryPointPath -Raw
        $module = Get-Content -LiteralPath $modulePath -Raw
        $combined = $script + [Environment]::NewLine + $module
        $combined | Should Match 'c76c06644034521fb761a39f8f52d8e87d1103d5'
        $combined | Should Match 'D:\\Temp'
        $combined | Should Match 'UseCurrentCheckout'
        $combined | Should Match 'GenerateSohOtr'
        $combined | Should Match ':app:testReleaseUnitTest'
        $combined | Should Match ':app:assembleRelease'
        $combined | Should Not Match '(?i)docker\s+(run|build|pull)'
    }

    It 'does not retain an active Docker implementation beside the Windows builder' {
        (Test-Path -LiteralPath (Join-Path $repositoryRoot 'Dockerfile')) | Should Be $false
        (Test-Path -LiteralPath (Join-Path $repositoryRoot 'docker')) | Should Be $false
        (Test-Path -LiteralPath (Join-Path $repositoryRoot '.devcontainer')) | Should Be $false

        $parityScript = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'scripts\android\verify-feature-parity.ps1'
        ) -Raw
        $parityScript | Should Not Match '(?i)docker|containerfile|devcontainer'
    }

    It 'returns success after a completed build even when a native tool left a stale exit code' {
        $script = Get-Content -LiteralPath $entryPointPath -Raw
        $script | Should Match '\$result\s*=\s*Invoke-SohWindowsAndroidBuild'
        $script | Should Match '\$global:LASTEXITCODE\s*=\s*0'
        $script | Should Match '(?m)^\s*\$result\s*$'
    }
}
