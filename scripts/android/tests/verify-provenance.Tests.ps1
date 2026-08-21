$ErrorActionPreference = 'Stop'

$scriptUnderTest = Join-Path (Split-Path -Parent $PSScriptRoot) 'verify-provenance.ps1'
$fixtureRoot = 'D:\Temp\shipwright-provenance-test'

Describe 'verify-provenance.ps1' {
    BeforeEach {
        if (Test-Path -LiteralPath $fixtureRoot) {
            Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'docs\android') -Force | Out-Null
        git -C $fixtureRoot init --quiet
        git -C $fixtureRoot config user.name 'Provenance Test'
        git -C $fixtureRoot config user.email 'provenance-test@example.invalid'
        Set-Content -LiteralPath (Join-Path $fixtureRoot 'fixture.txt') -Value 'fixture'
        git -C $fixtureRoot add fixture.txt
        git -C $fixtureRoot commit --quiet -m 'fixture'
        $script:fixtureSha = (git -C $fixtureRoot rev-parse HEAD).Trim()
    }

    AfterEach {
        if (Test-Path -LiteralPath $fixtureRoot) {
            Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
        }
    }

    It 'prints PASS when every recorded ref resolves to the full expected SHA' {
        @"
# Provenance

| Repository | Ref | Commit |
|---|---|---|
| local | HEAD | $fixtureSha |
"@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'docs\android\PROVENANCE.md')

        $output = & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $scriptUnderTest -Repository $fixtureRoot

        $LASTEXITCODE | Should Be 0
        $output | Should Be "PASS local HEAD $fixtureSha"
    }

    It 'throws when a recorded ref resolves to another SHA' {
        $wrongSha = '0000000000000000000000000000000000000000'
        @"
# Provenance

| Repository | Ref | Commit |
|---|---|---|
| local | HEAD | $wrongSha |
"@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'docs\android\PROVENANCE.md')

        $previousErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $scriptUnderTest -Repository $fixtureRoot 2>&1 | Out-Null
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        $exitCode | Should Not Be 0
    }

    It 'rejects abbreviated commit IDs' {
        $shortSha = $fixtureSha.Substring(0, 8)
        @"
# Provenance

| Repository | Ref | Commit |
|---|---|---|
| local | HEAD | $shortSha |
"@ | Set-Content -LiteralPath (Join-Path $fixtureRoot 'docs\android\PROVENANCE.md')

        $previousErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $scriptUnderTest -Repository $fixtureRoot 2>&1 | Out-Null
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        $exitCode | Should Not Be 0
    }
}
