[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$PackageRoot,

	[ValidateSet('', 'Development', 'Shipping')]
	[string]$ExpectedConfiguration = '',

	[string]$ExpectedVersion = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedPackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure {
	param([Parameter(Mandatory = $true)][string]$Message)
	$script:failures.Add($Message)
}

$gameExecutable = Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'Project_MuseumHeist.exe' |
	Where-Object { $_.FullName -notmatch '[\\/]Project_MuseumHeist[\\/]Binaries[\\/]Win64[\\/]Project_MuseumHeist\.exe$' } |
	Select-Object -First 1
if ($null -eq $gameExecutable) {
	Add-Failure 'Project_MuseumHeist bootstrap executable is missing.'
}

$buildInfoFile = Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'BuildInfo.json' |
	Select-Object -First 1
if ($null -eq $buildInfoFile) {
	Add-Failure 'BuildInfo.json is missing.'
}

$buildInfo = $null
if ($null -ne $buildInfoFile) {
	try {
		$buildInfo = Get-Content -LiteralPath $buildInfoFile.FullName -Raw | ConvertFrom-Json
	}
	catch {
		Add-Failure ("BuildInfo.json is invalid: {0}" -f $_.Exception.Message)
	}
}

if ($null -ne $buildInfo) {
	if (-not [string]::IsNullOrWhiteSpace($ExpectedConfiguration) -and $buildInfo.configuration -ne $ExpectedConfiguration) {
		Add-Failure ("Configuration mismatch. Expected={0} Actual={1}" -f $ExpectedConfiguration, $buildInfo.configuration)
	}
	if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion) -and $buildInfo.projectVersion -ne $ExpectedVersion) {
		Add-Failure ("Version mismatch. Expected={0} Actual={1}" -f $ExpectedVersion, $buildInfo.projectVersion)
	}
	if ([string]::IsNullOrWhiteSpace([string]$buildInfo.projectVersion)) {
		Add-Failure 'BuildInfo.json has an empty projectVersion.'
	}
}

$requiredArtifactPatterns = [ordered]@{
	Pak = '*.pak'
	IoStoreToc = '*.utoc'
	IoStoreContainer = '*.ucas'
	SteamRuntime = 'steam_api64.dll'
	OpenCVRuntime = 'opencv_world*.dll'
}

foreach ($artifact in $requiredArtifactPatterns.GetEnumerator()) {
	$match = Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter $artifact.Value |
		Select-Object -First 1
	if ($null -eq $match) {
		Add-Failure ("{0} artifact is missing ({1})." -f $artifact.Key, $artifact.Value)
	}
}

$prerequisiteInstaller = @('UEPrereqSetup_x64.exe', 'vc_redist.x64.exe') |
	ForEach-Object {
		Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter $_ |
			Select-Object -First 1
	} |
	Where-Object { $null -ne $_ } |
	Select-Object -First 1
if ($null -eq $prerequisiteInstaller) {
	Add-Failure 'Prerequisite installer is missing (UEPrereqSetup_x64.exe or vc_redist.x64.exe).'
}

$developmentRuntimeExecutables = @(
	Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'Project_MuseumHeist.exe' |
		Where-Object { $_.FullName -match '[\\/]Project_MuseumHeist[\\/]Binaries[\\/]Win64[\\/]Project_MuseumHeist\.exe$' }
)
$shippingRuntimeExecutables = @(
	Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'Project_MuseumHeist-Win64-Shipping.exe'
)

$configuration = if ($null -ne $buildInfo) { [string]$buildInfo.configuration } else { $ExpectedConfiguration }
if ($configuration -eq 'Development') {
	if ($developmentRuntimeExecutables.Count -eq 0) {
		Add-Failure 'Development runtime executable is missing.'
	}
	if ($shippingRuntimeExecutables.Count -gt 0) {
		Add-Failure 'Development package contains a Shipping runtime executable.'
	}
}
if ($configuration -eq 'Shipping') {
	if ($shippingRuntimeExecutables.Count -eq 0) {
		Add-Failure 'Shipping runtime executable is missing.'
	}
	if ($developmentRuntimeExecutables.Count -gt 0) {
		Add-Failure 'Shipping package contains a Development runtime executable.'
	}
}

if ($null -ne $gameExecutable) {
	$steamAppIdFile = Join-Path $gameExecutable.DirectoryName 'steam_appid.txt'
	if ($configuration -eq 'Development' -and -not (Test-Path -LiteralPath $steamAppIdFile -PathType Leaf)) {
		Add-Failure 'Development package is missing steam_appid.txt.'
	}
	if ($configuration -eq 'Shipping' -and (Test-Path -LiteralPath $steamAppIdFile -PathType Leaf)) {
		Add-Failure 'Shipping package must not contain the local steam_appid.txt file.'
	}
}

if ($failures.Count -gt 0) {
	foreach ($failure in $failures) {
		Write-Output ("Packaging validation failure: {0}" -f $failure)
	}
	Write-Output ("Packaging validation: Root={0} Failures={1} Result=FAIL" -f $resolvedPackageRoot, $failures.Count)
	exit 1
}

Write-Output ("Packaging validation: Root={0} Configuration={1} Version={2} Executable={3} Result=PASS" -f
	$resolvedPackageRoot,
	$buildInfo.configuration,
	$buildInfo.projectVersion,
	$gameExecutable.FullName)
