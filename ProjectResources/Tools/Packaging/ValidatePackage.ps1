[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$PackageRoot,

	[ValidateSet('', 'Development', 'Shipping')]
	[string]$ExpectedConfiguration = '',

	[string]$ExpectedVersion = '',

	[string]$ExpectedGitCommit = '',

	[switch]$RequireClean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedPackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure {
	param([Parameter(Mandatory = $true)][string]$Message)
	$script:failures.Add($Message)
}

$bootstrapExecutables = @(Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'Project_MuseumHeist.exe' |
	Where-Object { $_.FullName -notmatch '[\\/]Project_MuseumHeist[\\/]Binaries[\\/]Win64[\\/]Project_MuseumHeist\.exe$' })
if ($bootstrapExecutables.Count -ne 1) {
	Write-Output ("Packaging validation failure: Expected exactly one bootstrap executable; found {0}." -f $bootstrapExecutables.Count)
	Write-Output ("Packaging validation: Root={0} Failures=1 Result=FAIL" -f $resolvedPackageRoot)
	exit 1
}
$gameExecutable = $bootstrapExecutables[0]
$resolvedPackageRoot = $gameExecutable.DirectoryName

$buildInfoPath = Join-Path $resolvedPackageRoot 'BuildInfo.json'
$buildInfoFile = if (Test-Path -LiteralPath $buildInfoPath -PathType Leaf) { Get-Item -LiteralPath $buildInfoPath } else { $null }
if ($null -eq $buildInfoFile) {
	Add-Failure 'BuildInfo.json is missing beside the bootstrap executable.'
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

$configuration = ''
if ($null -ne $buildInfo) {
	$configurationProperty = $buildInfo.PSObject.Properties['configuration']
	if ($null -ne $configurationProperty) {
		$configuration = [string]$configurationProperty.Value
	}
	if ($configuration -notin @('Development', 'Shipping')) {
		Add-Failure ("BuildInfo.json has an unsupported configuration: {0}" -f $configuration)
	}
	if (-not [string]::IsNullOrWhiteSpace($ExpectedConfiguration) -and $configuration -ne $ExpectedConfiguration) {
		Add-Failure ("Configuration mismatch. Expected={0} Actual={1}" -f $ExpectedConfiguration, $configuration)
	}
	if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion) -and $buildInfo.projectVersion -ne $ExpectedVersion) {
		Add-Failure ("Version mismatch. Expected={0} Actual={1}" -f $ExpectedVersion, $buildInfo.projectVersion)
	}
	if ([string]::IsNullOrWhiteSpace([string]$buildInfo.projectVersion)) {
		Add-Failure 'BuildInfo.json has an empty projectVersion.'
	}
	if (-not [string]::IsNullOrWhiteSpace($ExpectedGitCommit)) {
		$gitCommitProperty = $buildInfo.PSObject.Properties['gitCommit']
		$actualGitCommit = if ($null -ne $gitCommitProperty) { [string]$gitCommitProperty.Value } else { '' }
		if ($actualGitCommit -ne $ExpectedGitCommit) {
			Add-Failure ("Git commit mismatch. Expected={0} Actual={1}" -f $ExpectedGitCommit, $actualGitCommit)
		}
	}
	if ($RequireClean) {
		$gitDirtyProperty = $buildInfo.PSObject.Properties['gitDirty']
		if ($null -eq $gitDirtyProperty -or $gitDirtyProperty.Value -isnot [bool]) {
			Add-Failure 'BuildInfo.json must contain a boolean gitDirty value when RequireClean is specified.'
		}
		elseif ($gitDirtyProperty.Value) {
			Add-Failure 'Package was built from a dirty working tree.'
		}
	}
	$schemaVersionProperty = $buildInfo.PSObject.Properties['schemaVersion']
	if ($null -eq $schemaVersionProperty -or
		($schemaVersionProperty.Value -isnot [int] -and $schemaVersionProperty.Value -isnot [long]) -or
		$schemaVersionProperty.Value -notin @(1, 2)) {
		Add-Failure 'BuildInfo.json requires a supported numeric schemaVersion (1 or 2).'
	}
	elseif ($schemaVersionProperty.Value -eq 2) {
		foreach ($fingerprintName in @('sourceInputSha256', 'externalContentLockSha256')) {
			$fingerprint = $buildInfo.PSObject.Properties[$fingerprintName]
			if ($null -eq $fingerprint -or [string]$fingerprint.Value -notmatch '^[0-9a-f]{64}$') {
				Add-Failure ("BuildInfo.json requires a SHA256 {0} for schemaVersion 2." -f $fingerprintName)
			}
		}
		$fullCommit = $buildInfo.PSObject.Properties['gitCommitFull']
		$shortCommit = $buildInfo.PSObject.Properties['gitCommit']
		if ($null -eq $fullCommit -or [string]$fullCommit.Value -notmatch '^(?:[0-9a-f]{40}|[0-9a-f]{64})$' -or $null -eq $shortCommit -or
			([string]$fullCommit.Value).Substring(0, 12) -ne [string]$shortCommit.Value) {
			Add-Failure 'BuildInfo.json requires consistent full and short Git revisions for schemaVersion 2.'
		}
		$externalLockFile = Join-Path $buildInfoFile.DirectoryName 'ExternalContentLock.json'
		$lockFingerprint = $buildInfo.PSObject.Properties['externalContentLockSha256']
		if (-not (Test-Path -LiteralPath $externalLockFile -PathType Leaf)) {
			Add-Failure 'SchemaVersion 2 package is missing ExternalContentLock.json.'
		}
		elseif ($null -eq $lockFingerprint -or (Get-FileHash -LiteralPath $externalLockFile -Algorithm SHA256).Hash -ne [string]$lockFingerprint.Value) {
			Add-Failure 'ExternalContentLock.json does not match the BuildInfo.json fingerprint.'
		}
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

exit 0
