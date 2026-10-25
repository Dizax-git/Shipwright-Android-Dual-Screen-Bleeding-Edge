$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$verifierPath = Join-Path $repositoryRoot 'scripts\android\verify-feature-parity.ps1'

Describe 'Three-source Android feature parity' {
    It 'proves the official, Waterdish, Chilhaps, and combined Thor contracts' {
        $output = & $verifierPath -Repository $repositoryRoot -StableRef '9.3.1' `
            -WaterdishRef 'waterdish-reference/main' -DualScreenRef 'dual-screen-reference/main'
        $text = $output -join [Environment]::NewLine

        $text | Should Match 'PASS official-base'
        $text | Should Match 'PASS waterdish-android'
        $text | Should Match 'PASS chilhaps-dual-screen'
        $text | Should Match 'PASS thor-composition'
        $text | Should Match 'PASS release-version'
    }

    It 'retains the Waterdish and Chilhaps multi-ABI debug build while keeping the Thor release arm64-only' {
        $gradle = Get-Content -LiteralPath (Join-Path $repositoryRoot 'Android\app\build.gradle') -Raw
        $gradle | Should Match "(?s)debug\s*\{.*?abiFilters\s+'arm64-v8a',\s*'armeabi-v7a',\s*'x86',\s*'x86_64'.*?release\s*\{.*?abiFilters\s+'arm64-v8a'"
    }
}
