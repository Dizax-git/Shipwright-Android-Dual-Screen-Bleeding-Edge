[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Repository,
    [Parameter(Mandatory = $true)][string] $StableRef,
    [Parameter(Mandatory = $true)][string] $WaterdishRef,
    [Parameter(Mandatory = $true)][string] $DualScreenRef
)

$ErrorActionPreference = 'Stop'
$repositoryPath = (Resolve-Path -LiteralPath $Repository -ErrorAction Stop).Path

function Get-RepositoryFile {
    param([Parameter(Mandatory = $true)][string] $RelativePath)
    $path = Join-Path $repositoryPath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required feature file is missing: $RelativePath"
    }
    return Get-Content -LiteralPath $path -Raw
}

function Assert-Pattern {
    param(
        [Parameter(Mandatory = $true)][string] $Text,
        [Parameter(Mandatory = $true)][string] $Pattern,
        [Parameter(Mandatory = $true)][string] $Feature
    )
    if ($Text -notmatch $Pattern) {
        throw "Missing feature marker for ${Feature}: $Pattern"
    }
}

function Resolve-Commit {
    param([Parameter(Mandatory = $true)][string] $Ref)
    $output = @(& git -C $repositoryPath rev-parse --verify "$Ref^{commit}" 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Source ref does not resolve: $Ref ($($output -join ' '))"
    }
    return ([string] $output[0]).Trim()
}

$stableCommit = Resolve-Commit $StableRef
$waterdishCommit = Resolve-Commit $WaterdishRef
$dualScreenCommit = Resolve-Commit $DualScreenRef
& git -C $repositoryPath merge-base --is-ancestor $stableCommit HEAD
if ($LASTEXITCODE -ne 0) {
    throw "Current branch is not based on official stable commit $stableCommit"
}

$provenance = Get-RepositoryFile 'docs\android\PROVENANCE.md'
foreach ($commit in @($stableCommit, $waterdishCommit, $dualScreenCommit)) {
    Assert-Pattern $provenance ([regex]::Escape($commit)) "recorded source commit $commit"
}
Write-Output "PASS official-base commit=$stableCommit"

$mainActivity = Get-RepositoryFile 'Android\app\src\main\java\com\dishii\soh\MainActivity.java'
$manifest = Get-RepositoryFile 'Android\app\src\main\AndroidManifest.xml'
$buildGradle = Get-RepositoryFile 'Android\app\build.gradle'
$inputEditor = Get-RepositoryFile 'soh\soh\Enhancements\controls\SohInputEditorWindow.cpp'
$waterdishMarkers = @(
    @($mainActivity, 'FLAG_KEEP_SCREEN_ON', 'keep-screen-awake'),
    @($mainActivity, 'setupControllerOverlay\(\)', 'touch-controller-overlay'),
    @($mainActivity, 'nativeGamepadBackPressed', 'controller-back-menu'),
    @($mainActivity, 'setCameraState', 'touch-free-look'),
    @($mainActivity, 'setItemButtonHeld', 'first-person-touch-action'),
    @($mainActivity, 'startRumble', 'controller-rumble'),
    @($mainActivity, 'AssetCopyUtil\.copyAssetsToExternal', 'asset-copy'),
    @($mainActivity, 'targetModsDir', 'mods-folder'),
    @($mainActivity, 'Intent\.ACTION_OPEN_DOCUMENT', 'system-file-picker'),
    @($manifest, 'MANAGE_EXTERNAL_STORAGE', 'shared-SOH-storage'),
    @($manifest, 'android\.permission\.INTERNET', 'Anchor-network-permission'),
    @($manifest, 'android\.permission\.VIBRATE', 'rumble-permission'),
    @($buildGradle, "abiFilters\s+'arm64-v8a'", 'arm64-release'),
    @($inputEditor, 'Touch Controls', 'touch-control-settings')
)
foreach ($marker in $waterdishMarkers) {
    Assert-Pattern -Text $marker[0] -Pattern $marker[1] -Feature $marker[2]
}
foreach ($path in @(
    'Android\app\src\main\assets\gamecontrollerdb.txt',
    'Android\app\src\main\res\layout\touchcontrol_overlay.xml',
    'scripts\android\Build-WindowsAndroidRelease.ps1',
    'scripts\android\WindowsNativeBuild.psm1',
    '.github\workflows\android.yml'
)) {
    $null = Get-RepositoryFile $path
}
Write-Output "PASS waterdish-android ref=$waterdishCommit"

$manager = Get-RepositoryFile 'Android\app\src\main\java\com\dishii\soh\DualScreenManager.java'
$presentation = Get-RepositoryFile 'Android\app\src\main\java\com\dishii\soh\DualScreenHudPresentation.java'
$renderer = Get-RepositoryFile 'soh\soh\Enhancements\DualScreenHUD\DualScreenHUD.cpp'
$routing = Get-RepositoryFile 'soh\src\code\z_play.c'
$targeting = Get-RepositoryFile 'soh\src\code\z_parameter.c'
$menu = Get-RepositoryFile 'soh\soh\SohGui\SohMenuEnhancements.cpp'
$dualMarkers = @(
    @($manager, 'Display\.FLAG_PRESENTATION', 'presentation-display-discovery'),
    @($presentation, 'SurfaceView', 'secondary-surface'),
    @($presentation, 'nativeSecondaryDisplaySurfaceChanged', 'secondary-surface-JNI'),
    @($renderer, 'CreateFrameBuffer', 'secondary-HUD-framebuffer'),
    @($renderer, 'eglCreateWindowSurface', 'shared-EGL-presentation'),
    @($renderer, 'surfaceAspect.*hudAspect', 'four-by-three-letterboxing'),
    @($routing, 'DualScreenHUD_BeginHudPass', 'HUD-redirection'),
    @($targeting, 'Interface_DrawEnemyHealthBar', 'primary-enemy-health'),
    @($menu, 'Dual-Screen HUD', 'dual-screen-menu-toggle')
)
foreach ($marker in $dualMarkers) {
    Assert-Pattern -Text $marker[0] -Pattern $marker[1] -Feature $marker[2]
}
Write-Output "PASS chilhaps-dual-screen ref=$dualScreenCommit"

Assert-Pattern $presentation 'activity\.attachControllerOverlay\(root\)' 'lower-display-touch-controls'
Assert-Pattern $presentation 'activity\.attachControllerOverlayToPrimary\(\)' 'single-screen-controller-fallback'
Assert-Pattern $mainActivity 'RomImportService' 'safe-archive-ROM-import'
Write-Output 'PASS thor-composition controls=secondary fallback=primary archive-import=enabled'

$version = & (Join-Path $repositoryPath 'scripts\android\Get-AndroidReleaseVersion.ps1') `
    -Properties (Join-Path $repositoryPath 'Android\version.properties')
Write-Output "PASS release-version name=$($version.VersionName) code=$($version.VersionCode) tag=$($version.ReleaseTag)"
