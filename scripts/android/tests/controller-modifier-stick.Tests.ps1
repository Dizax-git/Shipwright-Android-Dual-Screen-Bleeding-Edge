$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$padManagerPath = Join-Path $repositoryRoot 'soh\src\code\padmgr.c'
$bridgeHeaderPath = Join-Path $repositoryRoot 'soh\soh\Enhancements\controls\ModifierStickCButtons.h'
$bridgeSourcePath = Join-Path $repositoryRoot 'soh\soh\Enhancements\controls\ModifierStickCButtons.cpp'
$inputEditorPath = Join-Path $repositoryRoot 'soh\soh\Enhancements\controls\SohInputEditorWindow.cpp'

Describe 'Shipwright modifier-stick controller integration' {
    It 'transforms each delayed controller sample before PadMgr derives button edges' {
        Test-Path -LiteralPath $bridgeHeaderPath | Should Be $true
        Test-Path -LiteralPath $bridgeSourcePath | Should Be $true

        $padManager = Get-Content -LiteralPath $padManagerPath -Raw
        $padManager | Should Match '#include "soh/Enhancements/controls/ModifierStickCButtons.h"'
        $padManager | Should Match 'ModifierStickCButtons_Apply\(&padMgr->pads\[i\], i\);'

        $readPosition = $padManager.IndexOf('osContGetReadData(padMgr->pads);')
        $applyPosition = $padManager.IndexOf('ModifierStickCButtons_Apply(&padMgr->pads[i], i);')
        $processPosition = $padManager.IndexOf('PadMgr_ProcessInputs(padMgr);', $readPosition)

        $readPosition | Should BeGreaterThan -1
        $applyPosition | Should BeGreaterThan $readPosition
        $processPosition | Should BeGreaterThan $applyPosition
    }

    It 'keeps independent state per port and reads the per-port enable and modifier CVars' {
        $bridge = Get-Content -LiteralPath $bridgeSourcePath -Raw
        $bridge | Should Match 'std::array<LUS::ModifierStickCButtonsState, MAXCONTROLLERS>'
        $bridge | Should Match 'gSettings\.Controllers\.Port%d\.ModifierStickCButtons\.%s'
        $bridge | Should Match 'GetCVarName\(portIndex, "Enabled"\)'
        $bridge | Should Match 'GetCVarName\(portIndex, "Modifier"\)'
        $bridge | Should Match 'BTN_CUSTOM_MODIFIER1'
        $bridge | Should Match 'BTN_CUSTOM_MODIFIER2'
    }

    It 'exposes an enable switch and M1 or M2 selector beside the modifier mappings' {
        $inputEditor = Get-Content -LiteralPath $inputEditorPath -Raw
        $inputEditor | Should Match 'Left stick becomes C-buttons while modifier is held'
        $inputEditor | Should Match 'ModifierStickCButtons\.Enabled'
        $inputEditor | Should Match 'ModifierStickCButtons\.Modifier'
        $inputEditor | Should Match '"M1\\0M2\\0\\0"'
    }
}
