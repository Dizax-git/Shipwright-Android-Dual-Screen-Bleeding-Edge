[CmdletBinding()]
param(
    [string] $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [string] $WorkingRoot,
    [string] $AndroidSdkRoot,
    [string] $CMakePath,
    [string] $ReferenceSohO2r,
    [switch] $UseCurrentCheckout,
    [string] $ThreadName,
    [switch] $DeferWorkingDirectoryCleanup,
    [switch] $KeepWorkingDirectoryOnFailure
)

$ErrorActionPreference = 'Stop'
$modulePath = Join-Path $PSScriptRoot 'WindowsNativeBuild.psm1'
Import-Module $modulePath -Force

$result = Invoke-SohWindowsAndroidBuild @PSBoundParameters

# PowerShell propagates the last native process exit code when this script is
# invoked by GitHub Actions. The build function has already thrown on every
# native-tool failure, so reaching this point is the successful contract.
$global:LASTEXITCODE = 0
$result
