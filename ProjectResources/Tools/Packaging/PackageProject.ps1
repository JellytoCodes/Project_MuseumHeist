[CmdletBinding()]
param(
	[ValidateSet('Development', 'Shipping')]
	[string]$Configuration = 'Development',

	[Parameter(Mandatory = $true)]
	[string]$EngineRoot,

	[string]$OutputRoot = '',

	[ValidateRange(0, [int]::MaxValue)]
	[int]$SteamAppId = 480,

	[switch]$PlanOnly,

	[switch]$Clean,

	[switch]$CleanCook
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'PackageProvenance.ps1')

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
		throw "Refusing to modify a path outside the packaging root: $normalizedPath"
	}
}

$projectRoot = Get-NormalizedPath -Path (Join-Path $PSScriptRoot '..\..\..')
$projectFile = Join-Path $projectRoot 'Project_MuseumHeist.uproject'
$defaultGameIni = Join-Path $projectRoot 'Config\DefaultGame.ini'
$runUat = Join-Path (Get-NormalizedPath -Path $EngineRoot) 'Engine\Build\BatchFiles\RunUAT.bat'

foreach ($requiredFile in @($projectFile, $defaultGameIni, $runUat)) {
	if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
		throw "Required file is missing: $requiredFile"
	}
}

$versionMatch = Select-String -LiteralPath $defaultGameIni -Pattern '^ProjectVersion=(.+)$' | Select-Object -First 1
if ($null -eq $versionMatch) {
	throw 'ProjectVersion is missing from Config\DefaultGame.ini.'
}

$projectVersion = $versionMatch.Matches[0].Groups[1].Value.Trim()
if ($projectVersion -notmatch '^[0-9A-Za-z][0-9A-Za-z._-]*$') {
	throw "ProjectVersion contains unsupported packaging path characters: $projectVersion"
}

$requiredAlwaysCookDirectories = @(
	'/Game/Data/DataAsset',
	'/Game/Data/DataTable',
	'/Game/Data/Forgery/Textures'
)
$defaultGameConfig = Get-Content -LiteralPath $defaultGameIni
foreach ($requiredDirectory in $requiredAlwaysCookDirectories) {
	$escapedDirectory = [Regex]::Escape($requiredDirectory)
	if (-not ($defaultGameConfig -match "^\+DirectoriesToAlwaysCook=\(Path=`"$escapedDirectory`"\)$")) {
		throw "Required cook directory is missing from Config\DefaultGame.ini: $requiredDirectory"
	}
}

$packagesRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
	Join-Path $projectRoot 'Build\Packages'
}
else {
	Get-NormalizedPath -Path $OutputRoot
}

$archiveDirectory = Join-Path $packagesRoot ("MuseumHeist-{0}-{1}-Win64" -f $projectVersion, $Configuration)
Assert-PathWithinRoot -Path $archiveDirectory -Root $packagesRoot

$mapsToCook = @(
	'/Game/Maps/TitleMenuMap',
	'/Game/Maps/LobbyMap',
	'/Game/Maps/M01_ClassicalPrototype',
	'/Game/Maps/M02_MoonlitPrototype',
	'/Game/Maps/M03_GlasshousePrototype'
)

$uatArguments = @(
	'BuildCookRun',
	"-project=$projectFile",
	'-noP4',
	'-platform=Win64',
	'-target=Project_MuseumHeist',
	"-clientconfig=$Configuration",
	'-build',
	'-cook',
	("-map={0}" -f ($mapsToCook -join '+')),
	'-stage',
	'-pak',
	'-iostore',
	'-archive',
	"-archivedirectory=$archiveDirectory",
	'-prereqs',
	'-utf8output',
	'-unattended'
)

if ($Configuration -eq 'Shipping') {
	$uatArguments += '-distribution'
}

if ($CleanCook) {
	$uatArguments += '-clean'
}

if ($PlanOnly) {
	Write-Output ("Packaging plan: Version={0} Configuration={1} Output={2} Maps={3} Result=PASS" -f
		$projectVersion,
		$Configuration,
		$archiveDirectory,
		($mapsToCook -join ','))
	Write-Output ("Packaging data roots: {0} Result=PASS" -f ($requiredAlwaysCookDirectories -join ','))
	Write-Output ("RunUAT: {0} {1}" -f $runUat, ($uatArguments -join ' '))
	exit 0
}

$externalContentLockPath = Join-Path $PSScriptRoot 'ExternalContentLock.json'
$externalLockBefore = Assert-PackageExternalContent -ProjectRoot $projectRoot -LockPath $externalContentLockPath
$inputBefore = Get-PackageSourceSnapshot -ProjectRoot $projectRoot -OutputPath $archiveDirectory
$inputCapturedUtc = [DateTime]::UtcNow.ToString('o')

if (Test-Path -LiteralPath $archiveDirectory) {
	if (-not $Clean) {
		throw "Package output already exists. Re-run with -Clean after confirming the target: $archiveDirectory"
	}

	Remove-Item -LiteralPath $archiveDirectory -Recurse -Force
}

New-Item -ItemType Directory -Path $archiveDirectory -Force | Out-Null

Write-Output ("Packaging inputs captured: GitCommit={0} GitDirty={1} SourceInputSha256={2} ExternalContentLockSha256={3}" -f
	$inputBefore.GitCommit, $inputBefore.GitDirty, $inputBefore.InputSha256, $externalLockBefore)

Write-Output ("Packaging start: Version={0} Configuration={1} Output={2}" -f $projectVersion, $Configuration, $archiveDirectory)
& $runUat @uatArguments
if ($LASTEXITCODE -ne 0) {
	throw "RunUAT failed with exit code $LASTEXITCODE."
}

$externalLockAfter = Assert-PackageExternalContent -ProjectRoot $projectRoot -LockPath $externalContentLockPath
$inputAfter = Get-PackageSourceSnapshot -ProjectRoot $projectRoot -OutputPath $archiveDirectory
Assert-PackageInputsUnchanged -Before $inputBefore -After $inputAfter -ExternalLockBefore $externalLockBefore -ExternalLockAfter $externalLockAfter
Write-Output 'Packaging input verification: Git revision, source contents, and locked external contents unchanged Result=PASS'

$gameExecutable = Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter 'Project_MuseumHeist.exe' |
	Where-Object { $_.FullName -notmatch '[\\/]Project_MuseumHeist[\\/]Binaries[\\/]Win64[\\/]Project_MuseumHeist\.exe$' } |
	Select-Object -First 1
if ($null -eq $gameExecutable) {
	throw "Packaged bootstrap executable was not found under: $archiveDirectory"
}

if ($Configuration -eq 'Development' -and $SteamAppId -gt 0) {
	Set-Content -LiteralPath (Join-Path $gameExecutable.DirectoryName 'steam_appid.txt') -Value $SteamAppId -Encoding ASCII
}

$buildInfo = [ordered]@{
	schemaVersion = 2
	project = 'Project_MuseumHeist'
	displayName = 'Museum Heist'
	projectVersion = $projectVersion
	configuration = $Configuration
	platform = 'Win64'
	gitCommit = $inputBefore.GitCommit
	gitCommitFull = $inputBefore.GitCommitFull
	gitDirty = $inputBefore.GitDirty
	sourceInputSha256 = $inputBefore.InputSha256
	externalContentLockSha256 = $externalLockBefore
	inputCapturedUtc = $inputCapturedUtc
	createdUtc = [DateTime]::UtcNow.ToString('o')
	steamAppId = if ($Configuration -eq 'Development') { $SteamAppId } else { 0 }
	maps = $mapsToCook
}

$buildInfoPath = Join-Path $gameExecutable.DirectoryName 'BuildInfo.json'
Copy-Item -LiteralPath $externalContentLockPath -Destination (Join-Path $gameExecutable.DirectoryName 'ExternalContentLock.json')
$buildInfo | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $buildInfoPath -Encoding UTF8

$validator = Join-Path $PSScriptRoot 'ValidatePackage.ps1'
& $validator -PackageRoot $archiveDirectory -ExpectedConfiguration $Configuration -ExpectedVersion $projectVersion
if ($LASTEXITCODE -ne 0) {
	throw "Package validation failed with exit code $LASTEXITCODE."
}

Write-Output ("Packaging complete: Version={0} Configuration={1} Executable={2} Result=PASS" -f $projectVersion, $Configuration, $gameExecutable.FullName)
