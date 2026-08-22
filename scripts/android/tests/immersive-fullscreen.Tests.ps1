$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Android dual-display immersive fullscreen contract' {
    BeforeAll {
        $script:helperPath = Join-Path $repositoryRoot `
            'Android\app\src\main\java\com\dishii\soh\ImmersiveMode.java'
        $script:helper = if (Test-Path -LiteralPath $helperPath) {
            Get-Content -LiteralPath $helperPath -Raw
        } else {
            ''
        }
        $script:mainActivity = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'Android\app\src\main\java\com\dishii\soh\MainActivity.java') -Raw
        $script:presentation = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'Android\app\src\main\java\com\dishii\soh\DualScreenHudPresentation.java') -Raw
    }

    It 'centralizes a modern and legacy immersive system-bars policy' {
        (Test-Path -LiteralPath $helperPath) | Should Be $true
        $helper | Should Match 'static void apply\(Window window\)'
        $helper | Should Match 'WindowCompat\.setDecorFitsSystemWindows\(window, false\)'
        $helper | Should Match 'controller\.hide\(WindowInsetsCompat\.Type\.systemBars\(\)\)'
        $helper | Should Match 'BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE'
        $helper | Should Match 'SYSTEM_UI_FLAG_IMMERSIVE_STICKY'
        $helper | Should Match 'FLAG_FULLSCREEN'
        $helper | Should Match 'FLAG_FORCE_NOT_FULLSCREEN'
    }

    It 'restores immersive mode for the main SDL window throughout its lifecycle' {
        $mainActivity | Should Match '(?s)onCreate\(Bundle savedInstanceState\).*?super\.onCreate\(savedInstanceState\).*?ImmersiveMode\.apply\(getWindow\(\)\)'
        $mainActivity | Should Match '(?s)onResume\(\).*?super\.onResume\(\).*?ImmersiveMode\.apply\(getWindow\(\)\)'
        $mainActivity | Should Match '(?s)onWindowFocusChanged\(boolean hasFocus\).*?super\.onWindowFocusChanged\(hasFocus\).*?if \(hasFocus\).*?ImmersiveMode\.apply\(getWindow\(\)\)'
    }

    It 'applies the same policy to the independent secondary Presentation window' {
        $presentation | Should Match '(?s)onCreate\(Bundle savedInstanceState\).*?super\.onCreate\(savedInstanceState\).*?ImmersiveMode\.apply\(getWindow\(\)\)'
        $presentation | Should Match '(?s)onStart\(\).*?super\.onStart\(\).*?ImmersiveMode\.apply\(getWindow\(\)\)'
        $presentation | Should Match '(?s)onWindowFocusChanged\(boolean hasFocus\).*?super\.onWindowFocusChanged\(hasFocus\).*?if \(hasFocus\).*?ImmersiveMode\.apply\(getWindow\(\)\)'
    }
}
