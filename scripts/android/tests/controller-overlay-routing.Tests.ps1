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

    It 'preserves Waterdish right-stick C-button gameplay axes' {
        $mainActivity | Should Match 'setupCButtons\(buttonDpadUp, ControllerButtons\.AXIS_RY, 1, 0\);'
        $mainActivity | Should Match 'setupCButtons\(buttonDpadDown, ControllerButtons\.AXIS_RY, -1, 1\);'
        $mainActivity | Should Match 'setupCButtons\(buttonDpadLeft, ControllerButtons\.AXIS_RX, 1, 2\);'
        $mainActivity | Should Match 'setupCButtons\(buttonDpadRight, ControllerButtons\.AXIS_RX, -1, 3\);'
        $mainActivity | Should Match 'private void setupCButtons\(Button button, int axis, int direction, int navDir\)'
        $mainActivity | Should Match 'setAxis\(axis, direction < 0 \? Short\.MAX_VALUE : Short\.MIN_VALUE\);'
        $mainActivity | Should Match 'setAxis\(axis, \(short\) 0\);'
    }

    It 'retains direct menu navigation for every C-button touch state' {
        $mainActivity | Should Match '(?s)private void setupCButtons.*?ACTION_DOWN:.*?nativeMenuNavKey\(navDir, true\).*?ACTION_UP:.*?nativeMenuNavKey\(navDir, false\).*?ACTION_CANCEL:.*?nativeMenuNavKey\(navDir, false\)'
    }
}
