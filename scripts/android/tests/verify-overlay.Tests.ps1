$ErrorActionPreference = 'Stop'

$scriptUnderTest = Join-Path (Split-Path -Parent $PSScriptRoot) 'verify-overlay.ps1'
$fixtureRoot = 'D:\Temp\shipwright-overlay-test'

Describe 'verify-overlay.ps1' {
    BeforeEach {
        if (Test-Path -LiteralPath $fixtureRoot) {
            Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'docs\android') -Force | Out-Null
        git -C $fixtureRoot init --quiet
        git -C $fixtureRoot config user.name 'Overlay Test'
        git -C $fixtureRoot config user.email 'overlay-test@example.invalid'
        Set-Content -LiteralPath (Join-Path $fixtureRoot 'stable.txt') -Value 'stable'
        git -C $fixtureRoot add stable.txt
        git -C $fixtureRoot commit --quiet -m 'stable'
        git -C $fixtureRoot tag stable-base
        @"
# Provenance

| Path | Change | Classification |
|---|---:|---|
| ``stable.txt`` | ``M`` | ``excluded-divergence`` |
"@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'docs\android\PROVENANCE.md')
    }

    AfterEach {
        if (Test-Path -LiteralPath $fixtureRoot) {
            Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
        }
    }

    It 'passes when excluded paths match the official stable base' {
        $output = & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $scriptUnderTest `
            -Repository $fixtureRoot -StableRef stable-base

        $LASTEXITCODE | Should Be 0
        $output | Should Be 'PASS excluded-divergence stable.txt'
    }

    It 'rejects an excluded path that differs from the official stable base' {
        Set-Content -LiteralPath (Join-Path $fixtureRoot 'stable.txt') -Value 'drift'

        $previousErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $scriptUnderTest `
                -Repository $fixtureRoot -StableRef stable-base 2>&1 | Out-Null
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        $exitCode | Should Not Be 0
    }
}
