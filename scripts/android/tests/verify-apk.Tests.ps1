$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$verifierPath = Join-Path $repositoryRoot 'scripts\android\verify-apk.ps1'
$gradlePath = Join-Path $repositoryRoot 'Android\app\build.gradle'
$expectedSigner = '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'

function Test-Throws {
    param([scriptblock] $Action)
    try {
        & $Action | Out-Null
        return $false
    }
    catch {
        return $true
    }
}

function New-TestApk {
    param(
        [Parameter(Mandatory)][string] $Path,
        [string[]] $Entries = @('lib/arm64-v8a/libmain.so', 'assets/soh.o2r')
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    Add-Type -AssemblyName System.IO.Compression
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force
    }
    $archive = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $Entries) {
            $entry = $archive.CreateEntry($name)
            $entryStream = $entry.Open()
            try {
                if ($name -eq 'assets/soh.o2r') {
                    $nested = [IO.Compression.ZipArchive]::new($entryStream, [IO.Compression.ZipArchiveMode]::Create, $true)
                    try {
                        $nestedEntry = $nested.CreateEntry('assets/custom/fixture.bin')
                        $writer = [IO.StreamWriter]::new($nestedEntry.Open())
                        try { $writer.Write('fixture') } finally { $writer.Dispose() }
                    }
                    finally { $nested.Dispose() }
                }
                else {
                    $writer = [IO.StreamWriter]::new($entryStream)
                    try { $writer.Write('fixture') } finally { $writer.Dispose() }
                }
            }
            finally { $entryStream.Dispose() }
        }
    }
    finally {
        $archive.Dispose()
    }
}

Describe 'Shipwright Android APK release contract' {
    BeforeEach {
        $script:apk = Join-Path $TestDrive 'candidate.apk'
        New-TestApk -Path $apk

        $script:aapt2 = Join-Path $TestDrive 'aapt2.cmd'
        $script:zipalign = Join-Path $TestDrive 'zipalign.cmd'
        $script:apksigner = Join-Path $TestDrive 'apksigner.cmd'
        "@echo package: name='com.dishii.soh' versionCode='902003005' versionName='9.2.3.5'`r`n@echo native-code: 'arm64-v8a'" |
            Set-Content -LiteralPath $aapt2 -Encoding Ascii
        '@exit /b 0' | Set-Content -LiteralPath $zipalign -Encoding Ascii
        "@echo Verifies`r`n@echo Signer #1 certificate SHA-256 digest: $expectedSigner" |
            Set-Content -LiteralPath $apksigner -Encoding Ascii
    }

    It 'accepts the exact package, version, ABI, alignment, signer and payload contract' {
        Test-Throws {
            & $verifierPath -Apk $apk -ExpectedVersion '9.2.3.5' -ExpectedVersionCode 902003005 `
                -ExpectedSignerSha256 $expectedSigner -Aapt2 $aapt2 -Zipalign $zipalign -Apksigner $apksigner
        } | Should Be $false
    }

    It 'rejects a signer mismatch with expected and actual evidence' {
        $message = $null
        try {
            & $verifierPath -Apk $apk -ExpectedVersion '9.2.3.5' -ExpectedVersionCode 902003005 `
                -ExpectedSignerSha256 ('0' * 64) -Aapt2 $aapt2 -Zipalign $zipalign -Apksigner $apksigner
        }
        catch { $message = $_.Exception.Message }
        $message | Should Match ('0' * 64)
        $message | Should Match $expectedSigner
    }

    It 'rejects a raw game payload' {
        Remove-Item -LiteralPath $apk -Force
        New-TestApk -Path $apk -Entries @('lib/arm64-v8a/libmain.so', 'assets/soh.o2r', 'assets/game.z64')
        Test-Throws {
            & $verifierPath -Apk $apk -ExpectedVersion '9.2.3.5' -ExpectedVersionCode 902003005 `
                -ExpectedSignerSha256 $expectedSigner -Aapt2 $aapt2 -Zipalign $zipalign -Apksigner $apksigner
        } | Should Be $true
    }
}
