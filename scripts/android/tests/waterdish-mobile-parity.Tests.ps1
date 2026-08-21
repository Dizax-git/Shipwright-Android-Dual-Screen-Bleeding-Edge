$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Waterdish mobile behavior retained by the modern Android layer' {
    BeforeAll {
        $script:sdlControllerManager = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'Android\app\src\main\java\org\libsdl\app\SDLControllerManager.java') -Raw
        $script:physicalDeviceManager = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'libultraship\src\ship\controller\physicaldevice\ConnectedPhysicalDeviceManager.cpp') -Raw
        $script:gui = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'libultraship\src\ship\window\gui\Gui.cpp') -Raw
        $script:glesShader = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'libultraship\src\fast\shaders\opengl\default.shader.vs') -Raw
        $script:interpreter = Get-Content -LiteralPath (
            Join-Path $repositoryRoot 'libultraship\src\fast\interpreter.cpp') -Raw
    }

    It 'rejects known Android pseudo-gamepad input devices before SDL registration' {
        $sdlControllerManager | Should Match 'isInvalidGamepadName\(device\.getName\(\)\)'
        foreach ($name in @('uinput-fpc', 'uinput-fortsense', 'uinput-goodix', 'uinput-synaptics',
                            'uinput-elan', 'uinput-vfs', 'uinput-atrus')) {
            $sdlControllerManager | Should Match ([regex]::Escape($name))
        }
        $sdlControllerManager | Should Match '(?s)isDeviceSDLJoystick.*?if \(isInvalidGamepadName\(device\.getName\(\)\)\) \{\s*return false;'
    }

    It 'refreshes physical controllers after both add and remove events' {
        $physicalDeviceManager | Should Match '(?s)HandlePhysicalDeviceConnect.*?RefreshConnectedSDLGamepads\(\);'
        $physicalDeviceManager | Should Match '(?s)HandlePhysicalDeviceDisconnect.*?RefreshConnectedSDLGamepads\(\);'
    }

    It 'retains Android-scale ImGui controls and controller mapping windows' {
        $gui | Should Match '(?s)#if defined\(__ANDROID__\).*?ScaleAllSizes\(2\.0f\).*?FontGlobalScale = 2\.0f;'
    }

    It 'retains the GLES near-plane workaround and disables CPU near clipping' {
        $glesShader | Should Match '(?s)@if\(opengles\).*?gl_Position\.z \*= 0\.3f;'
        $interpreter | Should Match '// if \(z < -w\) d->clip_rej \|= 16; // CLIP_NEAR'
    }
}
