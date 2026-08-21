$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$resolverPath = Join-Path $repositoryRoot 'scripts\android\Get-AndroidReleaseVersion.ps1'
$versionPath = Join-Path $repositoryRoot 'Android\version.properties'

Describe 'Shipwright Android release version contract' {
    It 'uses one parser-safe monotonic version manifest' {
        $version = & $resolverPath -Properties $versionPath

        $version.UpstreamVersion | Should Be '9.2.3'
        $version.PortRevision | Should Be 3
        $version.VersionName | Should Be '9.2.3.3'
        $version.VersionCode | Should Be 902003003
        $version.ReleaseTag | Should Be 'v9.2.3-dual-screen.3'
    }

    It 'makes the Gradle build consume the shared version manifest' {
        $gradle = Get-Content -LiteralPath (Join-Path $repositoryRoot 'Android\app\build.gradle') -Raw
        $gradle | Should Match "rootProject\.file\('version\.properties'\)"
        $gradle | Should Match 'versionCode\s+androidVersionCode'
        $gradle | Should Match 'versionName\s+androidVersionName'
        $gradle | Should Match 'GIT_TAG.*releaseTag'
    }

    It 'does not leave conflicting version metadata in the Android manifest' {
        $manifest = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'Android\app\src\main\AndroidManifest.xml') -Raw
        $manifest | Should Not Match 'android:versionCode='
        $manifest | Should Not Match 'android:versionName='
    }
}
