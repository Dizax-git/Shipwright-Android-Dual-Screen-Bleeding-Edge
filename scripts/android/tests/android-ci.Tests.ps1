$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$workflowPath = Join-Path $repositoryRoot '.github\workflows\android.yml'

Describe 'Android continuous-integration parity contract' {
    BeforeAll {
        $script:workflow = Get-Content -LiteralPath $workflowPath -Raw
    }

    It 'checks out the complete Android-compatible source graph' {
        $workflow | Should Match 'actions/checkout@v4'
        $workflow | Should Match 'submodules:\s*recursive'
    }

    It 'uses the GitHub-hosted Windows toolchain without a container' {
        $workflow | Should Match 'runs-on:\s*windows-2022'
        $workflow | Should Not Match '(?m)^\s*container:'
    }

    It 'installs the NDK required by the shared builder' {
        $builder = Get-Content (Join-Path $repositoryRoot 'scripts/android/WindowsNativeBuild.psm1') -Raw
        $pin = [regex]::Match($builder, '\$script:NdkVersion = ''([^'']+)''').Groups[1].Value
        $pin | Should Not BeNullOrEmpty
        $workflow | Should Match ([regex]::Escape("'ndk;$pin'"))
    }

    It 'does not request the removed legacy Android tools package' {
        $workflow | Should Match '(?s)uses: android-actions/setup-android@v3\s+with:\s+packages: platform-tools'
    }

    It 'uses the tested Windows-native builder through a short isolated clone' {
        $workflow | Should Match 'Build-WindowsAndroidRelease\.ps1'
        $workflow | Should Match "Join-Path \`$env:RUNNER_TEMP 'w'"
        $workflow | Should Not Match 'UseCurrentCheckout'
    }

    It 'does not invoke Docker or duplicate build implementation in YAML' {
        $workflow | Should Not Match '(?i)docker|android-builder-soh'
        $workflow | Should Not Match '(?s)cmake --build.*GenerateSohOtr'
        $workflow | Should Not Match '(?s)gradlew.*assembleRelease'
    }

    It 'uploads the unsigned candidate without replacing the persistent local signer' {
        $workflow | Should Match 'actions/upload-artifact@v4'
        $workflow | Should Match 'windows-native-output/\*\.apk'
        $workflow | Should Not Match 'SIGNING_KEYSTORE|storePassword|keyPassword|gh release create'
    }
}
