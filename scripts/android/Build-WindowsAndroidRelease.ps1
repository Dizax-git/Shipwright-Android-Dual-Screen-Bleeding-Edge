[CmdletBinding()]
param(
    [string] $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [string] $WorkingRoot,
    [string] $AndroidSdkRoot,
    [string] $CMakePath,
    [string] $ReferenceSohO2r,
    [switch] $UseCurrentCheckout,
    [switch] $KeepWorkingDirectoryOnFailure
)

$ErrorActionPreference = 'Stop'
$modulePath = Join-Path $PSScriptRoot 'WindowsNativeBuild.psm1'
Import-Module $modulePath -Force

Invoke-SohWindowsAndroidBuild @PSBoundParameters
