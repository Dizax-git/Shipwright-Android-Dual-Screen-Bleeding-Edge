$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Dual-screen native renderer contract' {
    BeforeAll {
        $headerPath = Join-Path $repositoryRoot 'soh\soh\Enhancements\DualScreenHUD\DualScreenHUD.h'
        $sourcePath = Join-Path $repositoryRoot 'soh\soh\Enhancements\DualScreenHUD\DualScreenHUD.cpp'
        $cmakePath = Join-Path $repositoryRoot 'soh\CMakeLists.txt'
        $parameterPath = Join-Path $repositoryRoot 'soh\src\code\z_parameter.c'

        $script:header = Get-Content -LiteralPath $headerPath -Raw
        $script:source = Get-Content -LiteralPath $sourcePath -Raw
        $script:cmake = Get-Content -LiteralPath $cmakePath -Raw
        $script:parameter = Get-Content -LiteralPath $parameterPath -Raw
    }

    It 'exposes only the approved narrow renderer operations' {
        $header | Should Match 'void DualScreenHUD_InitFramebuffer\(void\);'
        $header | Should Match 'bool DualScreenHUD_BeginHudPass\(void\);'
        $header | Should Match 'bool DualScreenHUD_PauseHudPass\(void\);'
        $header | Should Match 'bool DualScreenHUD_ResumeHudPass\(void\);'
        $header | Should Match 'void DualScreenHUD_EndHudPass\(void\);'
        $header | Should Match 'void DualScreenHUD_PresentFrame\(void\);'
        $header | Should Match 'bool DualScreenHUD_IsActive\(void\);'
        $header | Should Not Match 'GetFrameBufferId'
    }

    It 'uses the maintained interpreter and rendering API' {
        $source | Should Match 'CreateFrameBuffer\('
        $source | Should Match 'GetCurrentRenderingAPI\('
        $source | Should Match 'GetFramebufferTextureId\('
        $source | Should Not Match 'gfx_get_current_rendering_api'
    }

    It 'checks shader compilation and program linking' {
        $source | Should Match 'GL_COMPILE_STATUS'
        $source | Should Match 'GL_LINK_STATUS'
    }

    It 'restores the primary EGL bindings after secondary presentation' {
        $source | Should Match 'RestorePrimaryContext\('
        $source | Should Match 'eglGetCurrentSurface\(EGL_DRAW\)'
        $source | Should Match 'eglGetCurrentSurface\(EGL_READ\)'
    }

    It 'links the required Android graphics libraries' {
        $cmake | Should Match '(?ms)elseif\(CMAKE_SYSTEM_NAME STREQUAL "Android"\).*?\bandroid\b.*?\bEGL\b.*?\bGLESv2\b'
    }

    It 'temporarily restores the primary HUD target without starting a second clearing pass' {
        $parameter | Should Match '(?s)DualScreenHUD_PauseHudPass\(\).*?func_8002C124.*?Interface_DrawEnemyHealthBar.*?DualScreenHUD_ResumeHudPass\(\)'
        $parameter | Should Not Match '(?s)DualScreenHUD_EndHudPass\(\).*?func_8002C124.*?DualScreenHUD_BeginHudPass\(\)'
    }

    It 'clears only when beginning a frame and never when resuming the HUD target' {
        $begin = [regex]::Match($source, '(?s)bool DualScreenHUD_BeginHudPass\(void\) \{(.*?)\n\}').Groups[1].Value
        $resume = [regex]::Match($source, '(?s)bool DualScreenHUD_ResumeHudPass\(void\) \{(.*?)\n\}').Groups[1].Value
        $begin | Should Match 'gDPFillRectangle'
        $resume | Should Not Match 'gDPFillRectangle|gDPSetFillColor'
        $resume | Should Match 'gsSPSetFB'
    }
}
