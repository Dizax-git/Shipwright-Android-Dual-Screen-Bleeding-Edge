[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'

function Set-JsonProperty {
    param(
        [Parameter(Mandatory = $true)] [object]$Object,
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value
    )

    $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
}

function Add-SdlButtonMapping {
    param(
        [Parameter(Mandatory = $true)] [object]$Mappings,
        [Parameter(Mandatory = $true)] [int]$Bitmask,
        [Parameter(Mandatory = $true)] [int]$Button
    )

    $id = "P0-B$Bitmask-SDLB$Button"
    Set-JsonProperty -Object $Mappings -Name $id -Value ([pscustomobject][ordered]@{
        Bitmask = $Bitmask
        ButtonMappingClass = 'SDLButtonToButtonMapping'
        SDLControllerButton = $Button
    })
}

function Add-SdlTriggerMapping {
    param(
        [Parameter(Mandatory = $true)] [object]$Mappings,
        [Parameter(Mandatory = $true)] [int]$Bitmask,
        [Parameter(Mandatory = $true)] [int]$Axis
    )

    $id = "P0-B$Bitmask-SDLA$Axis-ADP"
    Set-JsonProperty -Object $Mappings -Name $id -Value ([pscustomobject][ordered]@{
        AxisDirection = 1
        Bitmask = $Bitmask
        ButtonMappingClass = 'SDLAxisDirectionToButtonMapping'
        SDLControllerAxis = $Axis
    })
}

function Add-SdlStickMapping {
    param(
        [Parameter(Mandatory = $true)] [object]$Mappings,
        [Parameter(Mandatory = $true)] [int]$Stick,
        [Parameter(Mandatory = $true)] [int]$Direction,
        [Parameter(Mandatory = $true)] [int]$Axis,
        [Parameter(Mandatory = $true)] [int]$AxisDirection
    )

    $suffix = if ($AxisDirection -lt 0) { 'ADN' } else { 'ADP' }
    $id = "P0-S$Stick-D$Direction-SDLA$Axis-$suffix"
    Set-JsonProperty -Object $Mappings -Name $id -Value ([pscustomobject][ordered]@{
        AxisDirection = $AxisDirection
        AxisDirectionMappingClass = 'SDLAxisDirectionToAxisDirectionMapping'
        Direction = $Direction
        SDLControllerAxis = $Axis
        Stick = $Stick
    })
}

function Rebuild-ButtonReferences {
    param(
        [Parameter(Mandatory = $true)] [object]$ButtonMappings,
        [Parameter(Mandatory = $true)] [object]$ButtonReferences
    )

    foreach ($property in @($ButtonReferences.PSObject.Properties)) {
        if ($property.Name -match '^\d+ButtonMappingIds$') {
            $ButtonReferences.PSObject.Properties.Remove($property.Name)
        }
    }

    $groupedMappings = @($ButtonMappings.PSObject.Properties) |
        Group-Object { [int]$_.Value.Bitmask } |
        Sort-Object { [int]$_.Name }
    foreach ($group in $groupedMappings) {
        $ids = (($group.Group | ForEach-Object Name) -join ',') + ','
        Set-JsonProperty -Object $ButtonReferences -Name "$($group.Name)ButtonMappingIds" -Value $ids
    }
}

function Rebuild-StickReferences {
    param(
        [Parameter(Mandatory = $true)] [object]$AxisMappings,
        [Parameter(Mandatory = $true)] [object]$StickConfig,
        [Parameter(Mandatory = $true)] [int]$Stick
    )

    $referenceNames = @('LeftAxisDirectionMappingIds', 'RightAxisDirectionMappingIds',
                        'UpAxisDirectionMappingIds', 'DownAxisDirectionMappingIds')
    for ($direction = 0; $direction -lt $referenceNames.Count; $direction++) {
        $ids = @($AxisMappings.PSObject.Properties | Where-Object {
            [int]$_.Value.Stick -eq $Stick -and [int]$_.Value.Direction -eq $direction
        } | ForEach-Object Name)
        Set-JsonProperty -Object $StickConfig -Name $referenceNames[$direction] -Value (($ids -join ',') + ',')
    }
}

$resolvedPath = (Resolve-Path -LiteralPath $Path).Path
$document = Get-Content -LiteralPath $resolvedPath -Raw | ConvertFrom-Json
$settings = $document.CVars.gSettings
$controllers = $settings.Controllers
$port = $controllers.Port1

if ($null -eq $settings -or $null -eq $controllers -or $null -eq $port -or
    $null -eq $controllers.ButtonMappings -or $null -eq $controllers.AxisDirectionMappings) {
    throw "The selected file is not a supported Shipwright configuration: $resolvedPath"
}

foreach ($property in @($controllers.ButtonMappings.PSObject.Properties)) {
    if ([string]$property.Value.ButtonMappingClass -like 'SDL*') {
        $controllers.ButtonMappings.PSObject.Properties.Remove($property.Name)
    }
}

# Xbox-style physical layout: A/B face buttons, L2=Z, R1=R, View=L,
# Start=Start, physical D-pad=N64 D-pad, and L1=M1 for the C-button layer.
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 32768 -Button 0
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 16384 -Button 1
Add-SdlTriggerMapping -Mappings $controllers.ButtonMappings -Bitmask 8192 -Axis 4
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 16 -Button 10
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 32 -Button 4
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 4096 -Button 6
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 64 -Button 9
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 256 -Button 14
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 512 -Button 13
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 1024 -Button 12
Add-SdlButtonMapping -Mappings $controllers.ButtonMappings -Bitmask 2048 -Button 11
Rebuild-ButtonReferences -ButtonMappings $controllers.ButtonMappings -ButtonReferences $port.Buttons

foreach ($property in @($controllers.AxisDirectionMappings.PSObject.Properties)) {
    if ([string]$property.Value.AxisDirectionMappingClass -like 'SDL*') {
        $controllers.AxisDirectionMappings.PSObject.Properties.Remove($property.Name)
    }
}

# SDL axes 0/1 are the movement stick and 2/3 are the camera/aim stick.
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 0 -Direction 0 -Axis 0 -AxisDirection -1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 0 -Direction 1 -Axis 0 -AxisDirection 1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 0 -Direction 2 -Axis 1 -AxisDirection -1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 0 -Direction 3 -Axis 1 -AxisDirection 1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 1 -Direction 0 -Axis 2 -AxisDirection -1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 1 -Direction 1 -Axis 2 -AxisDirection 1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 1 -Direction 2 -Axis 3 -AxisDirection -1
Add-SdlStickMapping -Mappings $controllers.AxisDirectionMappings -Stick 1 -Direction 3 -Axis 3 -AxisDirection 1
Rebuild-StickReferences -AxisMappings $controllers.AxisDirectionMappings -StickConfig $port.LeftStick -Stick 0
Rebuild-StickReferences -AxisMappings $controllers.AxisDirectionMappings -StickConfig $port.RightStick -Stick 1

Set-JsonProperty -Object $port -Name 'ModifierStickCButtons' -Value ([pscustomobject][ordered]@{
    Enabled = 1
    Modifier = 1
})
Set-JsonProperty -Object $settings.FreeLook -Name 'Enabled' -Value 1
Set-JsonProperty -Object $settings.Controls -Name 'RightStickAim' -Value 1

$temporaryPath = Join-Path ([IO.Path]::GetDirectoryName($resolvedPath)) (
    ([IO.Path]::GetFileName($resolvedPath)) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
try {
    $serialized = ($document | ConvertTo-Json -Depth 100) + [Environment]::NewLine
    [IO.File]::WriteAllText($temporaryPath, $serialized, [Text.UTF8Encoding]::new($false))
    $null = Get-Content -LiteralPath $temporaryPath -Raw | ConvertFrom-Json
    [IO.File]::Move($temporaryPath, $resolvedPath, $true)
} finally {
    if (Test-Path -LiteralPath $temporaryPath) {
        Remove-Item -LiteralPath $temporaryPath -Force
    }
}

[pscustomobject]@{
    Path = $resolvedPath
    Layout = 'Traditional Xbox-style'
    CButtonLayer = 'M1 + left stick'
    Camera = 'Right stick'
}
