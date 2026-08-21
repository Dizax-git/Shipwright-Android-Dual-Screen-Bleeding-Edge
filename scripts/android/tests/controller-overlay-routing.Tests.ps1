$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Dual-screen touch-controller placement contract' {
    BeforeAll {
        $script:mainActivity = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'Android\app\src\main\java\com\dishii\soh\MainActivity.java') -Raw
        $script:presentation = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'Android\app\src\main\java\com\dishii\soh\DualScreenHudPresentation.java') -Raw
    }

    It 'keeps the Waterdish touch controller available on the primary display fallback' {
        $mainActivity | Should Match '(?s)setupControllerOverlay\(\);.*?attachControllerOverlayToPrimary\(\);'
        $mainActivity | Should Match 'void attachControllerOverlayToPrimary\(\)'
    }

    It 'supports moving the single controller overlay between display hosts' {
        $mainActivity | Should Match 'void attachControllerOverlay\(ViewGroup host\)'
        $mainActivity | Should Match '(?s)overlayView\.getParent\(\).*?removeView\(overlayView\).*?host\.addView\(overlayView\)'
    }

    It 'layers the controller overlay above the native HUD surface on the secondary display' {
        $presentation | Should Match 'FrameLayout root'
        $presentation | Should Match '(?s)root\.addView\(surfaceView.*?setContentView\(root.*?activity\.attachControllerOverlay\(root\)'
    }

    It 'restores the controller overlay to the primary display when the presentation closes' {
        $presentation | Should Match '(?s)void dismiss\(\).*?activity\.attachControllerOverlayToPrimary\(\).*?super\.dismiss\(\)'
    }
}
