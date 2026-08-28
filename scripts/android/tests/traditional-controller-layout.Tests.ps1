$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$scriptPath = Join-Path $repositoryRoot 'scripts\android\Set-TraditionalControllerLayout.ps1'

Describe 'Traditional Shipwright controller layout updater' {
    BeforeEach {
        $script:testPath = Join-Path $TestDrive 'shipofharkinian.json'
        @'
{
  "CVars": {
    "Unrelated": { "Keep": 7 },
    "gSettings": {
      "Controllers": {
        "AxisDirectionMappings": {
          "P0-S0-D0-KB30": { "AxisDirectionMappingClass": "KeyboardKeyToAxisDirectionMapping", "Direction": 0, "KeyboardScancode": 30, "Stick": 0 },
          "P0-S0-D0-SDLA9-ADN": { "AxisDirection": -1, "AxisDirectionMappingClass": "SDLAxisDirectionToAxisDirectionMapping", "Direction": 0, "SDLControllerAxis": 9, "Stick": 0 }
        },
        "ButtonMappings": {
          "P0-B32768-SDLB10": { "Bitmask": 32768, "ButtonMappingClass": "SDLButtonToButtonMapping", "SDLControllerButton": 10 },
          "P0-B256-KB35": { "Bitmask": 256, "ButtonMappingClass": "KeyboardKeyToButtonMapping", "KeyboardScancode": 35 },
          "P0-B8-SDLB3": { "Bitmask": 8, "ButtonMappingClass": "SDLButtonToButtonMapping", "SDLControllerButton": 3 }
        },
        "Port1": {
          "Buttons": {
            "32768ButtonMappingIds": "P0-B32768-SDLB10,",
            "256ButtonMappingIds": "P0-B256-KB35,",
            "8ButtonMappingIds": "P0-B8-SDLB3,"
          },
          "HasConfig": 1,
          "LeftStick": {
            "DeadzonePercentage": 20,
            "LeftAxisDirectionMappingIds": "P0-S0-D0-KB30,P0-S0-D0-SDLA9-ADN,",
            "SensitivityPercentage": 100
          },
          "RightStick": { "DeadzonePercentage": 20, "SensitivityPercentage": 100 }
        }
      },
      "Controls": { "RightStickAim": 0 },
      "FreeLook": { "Enabled": 0 }
    }
  },
  "ConfigVersion": 6
}
'@ | Set-Content -LiteralPath $script:testPath -Encoding utf8
    }

    It 'builds the approved Xbox-style SDL mapping while preserving non-SDL settings' {
        Test-Path -LiteralPath $scriptPath | Should Be $true
        & $scriptPath -Path $script:testPath
        $config = Get-Content -LiteralPath $script:testPath -Raw | ConvertFrom-Json
        $controllers = $config.CVars.gSettings.Controllers

        $config.CVars.Unrelated.Keep | Should Be 7
        $controllers.ButtonMappings.'P0-B256-KB35'.KeyboardScancode | Should Be 35
        $controllers.AxisDirectionMappings.'P0-S0-D0-KB30'.KeyboardScancode | Should Be 30

        $controllers.ButtonMappings.'P0-B32768-SDLB0'.SDLControllerButton | Should Be 0
        $controllers.ButtonMappings.'P0-B16384-SDLB1'.SDLControllerButton | Should Be 1
        $controllers.ButtonMappings.'P0-B8192-SDLA4-ADP'.SDLControllerAxis | Should Be 4
        $controllers.ButtonMappings.'P0-B16-SDLB10'.SDLControllerButton | Should Be 10
        $controllers.ButtonMappings.'P0-B32-SDLB4'.SDLControllerButton | Should Be 4
        $controllers.ButtonMappings.'P0-B4096-SDLB6'.SDLControllerButton | Should Be 6
        $controllers.ButtonMappings.'P0-B64-SDLB9'.SDLControllerButton | Should Be 9

        ($controllers.ButtonMappings.PSObject.Properties.Name -contains 'P0-B32768-SDLB10') | Should Be $false
        ($controllers.ButtonMappings.PSObject.Properties.Name -contains 'P0-B8-SDLB3') | Should Be $false
    }

    It 'maps both physical sticks and enables free look, right-stick aim, and M1 plus stick C-buttons' {
        & $scriptPath -Path $script:testPath
        $config = Get-Content -LiteralPath $script:testPath -Raw | ConvertFrom-Json
        $settings = $config.CVars.gSettings
        $controllers = $settings.Controllers

        $controllers.AxisDirectionMappings.'P0-S0-D0-SDLA0-ADN'.SDLControllerAxis | Should Be 0
        $controllers.AxisDirectionMappings.'P0-S0-D3-SDLA1-ADP'.SDLControllerAxis | Should Be 1
        $controllers.AxisDirectionMappings.'P0-S1-D0-SDLA2-ADN'.SDLControllerAxis | Should Be 2
        $controllers.AxisDirectionMappings.'P0-S1-D3-SDLA3-ADP'.SDLControllerAxis | Should Be 3
        $settings.FreeLook.Enabled | Should Be 1
        $settings.Controls.RightStickAim | Should Be 1
        $controllers.Port1.ModifierStickCButtons.Enabled | Should Be 1
        $controllers.Port1.ModifierStickCButtons.Modifier | Should Be 1
    }

    It 'is idempotent and leaves no sibling temporary file' {
        & $scriptPath -Path $script:testPath
        $first = Get-Content -LiteralPath $script:testPath -Raw
        & $scriptPath -Path $script:testPath
        $second = Get-Content -LiteralPath $script:testPath -Raw

        $second | Should Be $first
        @(Get-ChildItem -LiteralPath $TestDrive -Filter '*.tmp').Count | Should Be 0
    }
}
