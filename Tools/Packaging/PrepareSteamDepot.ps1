[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$PackageRoot,

	[Parameter(Mandatory = $true)]
	[ValidateRange(1, [int]::MaxValue)]
	[int]$AppId,

	[Parameter(Mandatory = $true)]
	[ValidateRange(1, [int]::MaxValue)]
	[int]$DepotId,

	[string]$OutputRoot = '',

	[switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-NormalizedPath {
	param([Parameter(Mandatory = $true)][string]$Path)
	return [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Assert-PathWithinRoot {
	param(
		[Parameter(Mandatory = $true)][string]$Path,
		[Parameter(Mandatory = $true)][string]$Root
	)

	$normalizedPath = Get-NormalizedPath -Path $Path
	$normalizedRoot = Get-NormalizedPath -Path $Root
	$rootPrefix = $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar
	if (-not $normalizedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
		throw "Refusing to modify a path outside the Steam candidate root: $normalizedPath"
	}
}

$projectRoot = Get-NormalizedPath -Path (Join-Path $PSScriptRoot '..\..')
$resolvedPackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$validator = Join-Path $PSScriptRoot 'ValidatePackage.ps1'
& $validator -PackageRoot $resolvedPackageRoot
if ($LASTEXITCODE -ne 0) {
	throw "Package validation failed with exit code $LASTEXITCODE."
}

$gameExecutable = Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'Project_MuseumHeist.exe' |
	Select-Object -First 1
$buildInfoFile = Join-Path $gameExecutable.DirectoryName 'BuildInfo.json'
$buildInfo = Get-Content -LiteralPath $buildInfoFile -Raw | ConvertFrom-Json

$candidateRootBase = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
	Join-Path $projectRoot 'Build\SteamCandidate'
}
else {
	Get-NormalizedPath -Path $OutputRoot
}

$candidateName = "MuseumHeist-{0}-{1}-Windows" -f $buildInfo.projectVersion, $buildInfo.configuration
$candidateRoot = Join-Path $candidateRootBase $candidateName
Assert-PathWithinRoot -Path $candidateRoot -Root $candidateRootBase

if (Test-Path -LiteralPath $candidateRoot) {
	if (-not $Clean) {
		throw "Steam candidate already exists. Re-run with -Clean after confirming the target: $candidateRoot"
	}

	Remove-Item -LiteralPath $candidateRoot -Recurse -Force
}

$contentRoot = Join-Path $candidateRoot 'content'
$windowsContentRoot = Join-Path $contentRoot 'windows'
$scriptsRoot = Join-Path $candidateRoot 'scripts'
$outputRootPath = Join-Path $candidateRoot 'output'
New-Item -ItemType Directory -Path $windowsContentRoot, $scriptsRoot, $outputRootPath -Force | Out-Null

Copy-Item -Path (Join-Path $gameExecutable.DirectoryName '*') -Destination $windowsContentRoot -Recurse -Force

$localSteamAppIdFile = Join-Path $windowsContentRoot 'steam_appid.txt'
if (Test-Path -LiteralPath $localSteamAppIdFile -PathType Leaf) {
	Remove-Item -LiteralPath $localSteamAppIdFile -Force
}

$templateRoot = Join-Path $PSScriptRoot 'Templates'
$appTemplate = Get-Content -LiteralPath (Join-Path $templateRoot 'app_build.vdf.in') -Raw
$depotTemplate = Get-Content -LiteralPath (Join-Path $templateRoot 'depot_build_windows.vdf.in') -Raw

$appBuild = $appTemplate.
	Replace('__APP_ID__', [string]$AppId).
	Replace('__DEPOT_ID__', [string]$DepotId).
	Replace('__VERSION__', [string]$buildInfo.projectVersion).
	Replace('__CONFIGURATION__', [string]$buildInfo.configuration)
$depotBuild = $depotTemplate.Replace('__DEPOT_ID__', [string]$DepotId)

$appBuildPath = Join-Path $scriptsRoot 'app_build.vdf'
$depotBuildPath = Join-Path $scriptsRoot 'depot_build_windows.vdf'
Set-Content -LiteralPath $appBuildPath -Value $appBuild -Encoding ASCII
Set-Content -LiteralPath $depotBuildPath -Value $depotBuild -Encoding ASCII

$candidateInfo = [ordered]@{
	schemaVersion = 1
	appId = $AppId
	depotId = $DepotId
	projectVersion = $buildInfo.projectVersion
	configuration = $buildInfo.configuration
	sourcePackage = $resolvedPackageRoot
	contentRoot = $windowsContentRoot
	appBuildScript = $appBuildPath
	previewOnly = $true
	createdUtc = [DateTime]::UtcNow.ToString('o')
}
$candidateInfo | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $candidateRoot 'DepotCandidate.json') -Encoding UTF8

Write-Output ("Steam depot candidate: Version={0} Configuration={1} AppId={2} DepotId={3} Content={4} PreviewOnly=true Result=PASS" -f
	$buildInfo.projectVersion,
	$buildInfo.configuration,
	$AppId,
	$DepotId,
	$windowsContentRoot)
Write-Output ('Upload is intentionally not executed. Review the candidate, then use SteamCMD with: +login <account> +run_app_build "{0}" +quit' -f $appBuildPath)
