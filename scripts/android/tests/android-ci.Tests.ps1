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

    It 'runs Android tests before assembling the release candidate' {
        $workflow | Should Match '(?s):app:testReleaseUnitTest.*?:app:assembleRelease'
    }

    It 'restores the Gradle wrapper executable bit in the Linux runner' {
        $workflow | Should Match '(?s)chmod \+x Android/gradlew.*?cd Android.*?\./gradlew --no-daemon'
    }

    It 'generates the project-owned resource archive before packaging' {
        $workflow | Should Match 'GenerateSohOtr'
        $workflow | Should Match 'soh/soh\.o2r Android/app/src/main/assets/soh\.o2r'
    }

    It 'uploads the unsigned candidate without replacing the persistent local signer' {
        $workflow | Should Match 'actions/upload-artifact@v4'
        $workflow | Should Match 'Android/app/build/outputs/apk/release/\*\.apk'
        $workflow | Should Not Match 'SIGNING_KEYSTORE|storePassword|keyPassword|gh release create'
    }
}
