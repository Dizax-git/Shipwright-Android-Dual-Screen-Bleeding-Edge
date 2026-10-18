$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Android libc++ portability includes' {
    It 'declares the standard header used by SohMenuRandomizer string streams' {
        $sourcePath = Join-Path $repositoryRoot 'soh\soh\SohGui\SohMenuRandomizer.cpp'
        $source = Get-Content -LiteralPath $sourcePath -Raw

        $source | Should Match '(?m)^#include <sstream>$'
    }
}

Describe 'Stable gameplay behavior retained by the Android overlay' {
    It 'does not import the undefined Android-reference talk-skip hook' {
        $sourcePath = Join-Path $repositoryRoot 'soh\src\overlays\actors\ovl_player_actor\z_player.c'
        $source = Get-Content -LiteralPath $sourcePath -Raw

        $source | Should Not Match 'VB_SKIP_TALKING'
        $source | Should Match 'this->currentMask = sSavedCurrentMask;\s+Player_StartTalking\(play, talkOfferActor\);'
    }
}

Describe 'Release signing handoff' {
    It 'only applies the Gradle release signer when credentials are supplied' {
        $gradlePath = Join-Path $repositoryRoot 'Android\app\build.gradle'
        $gradle = Get-Content -LiteralPath $gradlePath -Raw

        $gradle | Should Match "if \(project\.hasProperty\('storeFile'\)\) \{\s+signingConfig signingConfigs\.release\s+\}"
    }

    It 'generates canonical hash assets before merge and lint consumers' {
        $gradlePath = Join-Path $repositoryRoot 'Android\app\build.gradle'
        $gradle = Get-Content -LiteralPath $gradlePath -Raw

        $gradle | Should Match "task\.name\.startsWith\('merge'\)\s+&&\s+task\.name\.endsWith\('Assets'\)"
        $gradle | Should Match "task\.name\.toLowerCase\(\)\.contains\('lint'\)"
        $gradle | Should Match "task\.dependsOn\(syncSupportedRomHashes\)"
    }
}

Describe 'Android archived ROM import boundary' {
    BeforeAll {
        $mainActivityPath = Join-Path $repositoryRoot 'Android\app\src\main\java\com\dishii\soh\MainActivity.java'
        $script:mainActivity = Get-Content -LiteralPath $mainActivityPath -Raw
    }

    It 'refreshes only APK-owned Torch metadata and the project archive on updates' {
        $mainActivity | Should Match 'needsBundledAssetRefresh'
        $mainActivity | Should Match 'getInt\("versionCode", 0\)'
        $mainActivity | Should Match 'putInt\("versionCode", BuildConfig.VERSION_CODE\)'
        $mainActivity | Should Match 'copyBundledFile\(this, "soh\.o2r", targetOtrFile\)'
        $gradle = Get-Content -LiteralPath (Join-Path $repositoryRoot 'Android\app\build.gradle') -Raw
        $gradle | Should Match 'syncTorchExtractorAssets'
        $gradle | Should Match 'soh/assets/yml'
        $gradle | Should Match 'assets\.exclude\(''assets/extractor/\*\*''\)'
    }

    It 'advertises raw ZIP and 7z selections' {
        $mainActivity | Should Match 'application/octet-stream'
        $mainActivity | Should Match 'application/zip'
        $mainActivity | Should Match 'application/x-7z-compressed'
    }

    It 'loads canonical hashes and imports away from the UI thread' {
        $mainActivity | Should Match 'supportedHashes\.json'
        $mainActivity | Should Match 'romImportExecutor\.execute'
        $mainActivity | Should Match 'RomImportService'
    }

    It 'atomically promotes only a verified pending ROM' {
        $mainActivity | Should Match 'OOT\.z64\.pending'
        $mainActivity | Should Match 'Os\.rename'
        $mainActivity | Should Not Match 'destinationFile\.delete\(\)'
    }

    It 'reports failed selections back to the native extractor' {
        $mainActivity | Should Match 'nativeHandleSelectedFile\(null\)'
    }
}
