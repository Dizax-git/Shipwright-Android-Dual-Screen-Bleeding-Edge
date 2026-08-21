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
}
