$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Dual-screen HUD routing contract' {
    BeforeAll {
        $script:play = Get-Content -LiteralPath (Join-Path $repositoryRoot 'soh\src\code\z_play.c') -Raw
        $script:interface = Get-Content -LiteralPath (Join-Path $repositoryRoot 'soh\src\code\z_parameter.c') -Raw
        $script:renderLoop = Get-Content -LiteralPath (Join-Path $repositoryRoot 'soh\soh\OTRGlobals.cpp') -Raw
        $script:menu = Get-Content -LiteralPath (Join-Path $repositoryRoot 'soh\soh\SohGui\SohMenuEnhancements.cpp') -Raw
    }

    It 'redirects only Interface_Draw while leaving pause and messages outside the HUD pass' {
        $play | Should Match '(?s)KaleidoScopeCall_Draw\(play\);.*?DualScreenHUD_BeginHudPass\(\).*?Interface_Draw\(play\);.*?DualScreenHUD_EndHudPass\(\).*?Message_Draw\(play\);'
    }

    It 'returns target feedback and enemy health to the primary framebuffer' {
        $interface | Should Match '(?s)DualScreenHUD_EndHudPass\(\);.*?func_8002C124\(&play->actorCtx\.targetCtx, play\).*?Interface_DrawEnemyHealthBar\(&play->actorCtx\.targetCtx, play\);.*?DualScreenHUD_BeginHudPass\(\);'
    }

    It 'presents once after all interpolation samples' {
        $renderLoop | Should Match '(?s)for \(const auto& m : mtx_replacements\).*?DrawAndRunGraphicsCommands\(Commands, m\);.*?mInterpolationIndex\+\+;.*?\}\s*DualScreenHUD_PresentFrame\(\);\s*ImGui::PopStyleColor'
    }

    It 'offers the Android UI setting enabled by default' {
        $menu | Should Match 'Dual-Screen HUD'
        $menu | Should Match 'CVAR_ENHANCEMENT\("DualScreenHUD"\)'
        $menu | Should Match '(?s)Dual-Screen HUD.*?DefaultValue\(true\).*?eligible secondary display.*?no secondary display is available'
    }
}
