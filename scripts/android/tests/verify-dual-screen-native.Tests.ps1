$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Dual-screen native renderer contract' {
    BeforeAll {
        $headerPath = Join-Path $repositoryRoot 'soh\soh\Enhancements\DualScreenHUD\DualScreenHUD.h'
        $sourcePath = Join-Path $repositoryRoot 'soh\soh\Enhancements\DualScreenHUD\DualScreenHUD.cpp'
        $cmakePath = Join-Path $repositoryRoot 'soh\CMakeLists.txt'

        $script:header = Get-Content -LiteralPath $headerPath -Raw
        $script:source = Get-Content -LiteralPath $sourcePath -Raw
        $script:cmake = Get-Content -LiteralPath $cmakePath -Raw
    }

    It 'exposes only the approved narrow renderer operations' {
        $header | Should Match 'void DualScreenHUD_InitFramebuffer\(void\);'
        $header | Should Match 'bool DualScreenHUD_BeginHudPass\(void\);'
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
}
