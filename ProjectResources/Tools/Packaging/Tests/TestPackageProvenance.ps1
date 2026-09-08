[CmdletBinding()]
param([string]$ResultDirectory = (Join-Path $env:TEMP ('HeistPackagingFixtures-' + [Guid]::NewGuid().ToString('N'))))

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$packagingRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $packagingRoot 'PackageProvenance.ps1')
if (Test-Path -LiteralPath $ResultDirectory) { throw "Fixture output already exists: $ResultDirectory" }
New-Item -ItemType Directory -Path $ResultDirectory | Out-Null
$fixtureRoot = Join-Path $ResultDirectory 'project'
$externalRoot = Join-Path $fixtureRoot 'Content/Assets/MapAssets/Pack'
New-Item -ItemType Directory -Path $externalRoot -Force | Out-Null
$sourcePath = Join-Path $fixtureRoot 'source.cpp'
$externalPath = Join-Path $externalRoot 'one.bin'
$externalSecondPath = Join-Path $externalRoot 'two.bin'
Set-Content -LiteralPath $sourcePath -Value 'base' -NoNewline
Set-Content -LiteralPath (Join-Path $fixtureRoot '.gitignore') -Value '/Content/Assets/MapAssets/'
[System.IO.File]::WriteAllBytes($externalPath, [byte[]](1, 2, 3, 4))
[System.IO.File]::WriteAllBytes($externalSecondPath, [byte[]](5, 6))
& git -C $fixtureRoot init --quiet
if ($LASTEXITCODE -ne 0) { throw 'Fixture git init failed.' }
& git -C $fixtureRoot add -- .
& git -C $fixtureRoot -c user.name=PackagingFixture -c user.email=fixture@example.invalid commit --quiet -m baseline
if ($LASTEXITCODE -ne 0) { throw 'Fixture git commit failed.' }
$lockPath = Join-Path $ResultDirectory 'ExternalContentLock.json'
Get-PackageExternalContentSnapshot -ProjectRoot $fixtureRoot | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $lockPath -Encoding utf8
$results = [System.Collections.Generic.List[object]]::new()

function Assert-Fixture {
	param([bool]$Condition, [string]$Message)
	if (-not $Condition) { throw $Message }
}

function Test-Fixture {
	param([string]$Name, [scriptblock]$Action, [string]$ExpectedError = '')
	$errorText = $null
	try { & $Action | Out-Null } catch { $errorText = $_.Exception.Message }
	$pass = if ($ExpectedError) { $null -ne $errorText -and $errorText -like "*$ExpectedError*" } else { $null -eq $errorText }
	$results.Add([pscustomobject]@{ name = $Name; pass = $pass; error = $errorText })
}

$externalHash = Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $lockPath
$baseline = Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot
Test-Fixture 'unchanged_sources_and_lock' {
	Assert-Fixture (-not $baseline.GitDirty) 'Expected a clean baseline.'
	Assert-PackageInputsUnchanged -Before $baseline -After (Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot) -ExternalLockBefore $externalHash -ExternalLockAfter $externalHash
}
$ownedOutput = Join-Path $fixtureRoot 'custom-output'
$beforeOutput = Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot -OutputPath $ownedOutput
New-Item -ItemType Directory -Path $ownedOutput | Out-Null
Set-Content -LiteralPath (Join-Path $ownedOutput 'generated.json') -Value '{}'
Test-Fixture 'owned_archive_is_not_a_source_input' {
	Assert-PackageInputsUnchanged -Before $beforeOutput -After (Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot -OutputPath $ownedOutput) -ExternalLockBefore $externalHash -ExternalLockAfter $externalHash
}
Set-Content -LiteralPath $sourcePath -Value 'dirty-one' -NoNewline
$dirtyBefore = Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot
$statusBefore = (& git -C $fixtureRoot status --porcelain=v1) -join "`n"
Set-Content -LiteralPath $sourcePath -Value 'dirty-two' -NoNewline
Test-Fixture 'same_porcelain_same_size_source_edit' {
	Assert-Fixture ((& git -C $fixtureRoot status --porcelain=v1) -join "`n").Equals($statusBefore) 'Fixture did not preserve porcelain.'
	Assert-PackageInputsUnchanged -Before $dirtyBefore -After (Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot) -ExternalLockBefore $externalHash -ExternalLockAfter $externalHash
} 'Packaging inputs changed'
$untrackedPath = Join-Path $fixtureRoot 'untracked source.cpp'
Set-Content -LiteralPath $untrackedPath -Value 'one' -NoNewline
$untrackedBefore = Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot
Set-Content -LiteralPath $untrackedPath -Value 'two' -NoNewline
Test-Fixture 'untracked_source_content_edit' {
	Assert-PackageInputsUnchanged -Before $untrackedBefore -After (Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot) -ExternalLockBefore $externalHash -ExternalLockAfter $externalHash
} 'Packaging inputs changed'
$beforeCommit = Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot
& git -C $fixtureRoot -c user.name=PackagingFixture -c user.email=fixture@example.invalid commit --quiet --allow-empty -m changed-revision
if ($LASTEXITCODE -ne 0) { throw 'Fixture revision change failed.' }
Test-Fixture 'head_change_without_file_change' {
	Assert-PackageInputsUnchanged -Before $beforeCommit -After (Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot) -ExternalLockBefore $externalHash -ExternalLockAfter $externalHash
} 'Packaging inputs changed'
$beforeDelete = Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot
Remove-Item -LiteralPath $sourcePath
Test-Fixture 'tracked_source_deletion' {
	Assert-PackageInputsUnchanged -Before $beforeDelete -After (Get-PackageSourceSnapshot -ProjectRoot $fixtureRoot) -ExternalLockBefore $externalHash -ExternalLockAfter $externalHash
} 'Packaging inputs changed'
Test-Fixture 'external_lock_unchanged' { Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $lockPath }
[System.IO.File]::WriteAllBytes($externalPath, [byte[]](4, 3, 2, 1))
Test-Fixture 'external_same_size_content_change' { Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $lockPath } 'differs from the lock'
[System.IO.File]::WriteAllBytes($externalPath, [byte[]](1, 2, 3, 4))
Remove-Item -LiteralPath $externalSecondPath
Test-Fixture 'external_missing_file' { Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $lockPath } 'Locked external content is missing'
[System.IO.File]::WriteAllBytes($externalSecondPath, [byte[]](5, 6))
$extraPath = Join-Path $externalRoot 'extra.bin'
Set-Content -LiteralPath $extraPath -Value 'extra'
Test-Fixture 'external_unlocked_file' { Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $lockPath } 'External content is not locked'
Remove-Item -LiteralPath $extraPath
Test-Fixture 'lock_changed_during_build' {
	Assert-PackageInputsUnchanged -Before $baseline -After $baseline -ExternalLockBefore $externalHash -ExternalLockAfter ('f' * 64)
} 'Packaging inputs changed'
$invalidLockPath = Join-Path $ResultDirectory 'InvalidExternalContentLock.json'
$invalidLock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
$invalidLock.files += $invalidLock.files[0]
$invalidLock | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $invalidLockPath -Encoding utf8
Test-Fixture 'external_lock_duplicate_entry' { Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $invalidLockPath } 'invalid or duplicate entry'
foreach ($invalidPath in @('Content/Assets/MapAssets/../outside.bin', 'Content/Assets/MapAssets/Pack\one.bin')) {
	$invalidLock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
	$invalidLock.files[0].path = $invalidPath
	$invalidLock | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $invalidLockPath -Encoding utf8
	Test-Fixture ("external_lock_invalid_path_{0}" -f $invalidPath) { Assert-PackageExternalContent -ProjectRoot $fixtureRoot -LockPath $invalidLockPath } 'invalid or duplicate entry'
}

$packageRoot = Join-Path $ResultDirectory 'package'
$runtimeRoot = Join-Path $packageRoot 'Project_MuseumHeist/Binaries/Win64'
New-Item -ItemType Directory -Path $runtimeRoot -Force | Out-Null
foreach ($name in @('Project_MuseumHeist.exe', 'content.pak', 'content.utoc', 'content.ucas', 'steam_api64.dll', 'opencv_world_fixture.dll', 'vc_redist.x64.exe', 'steam_appid.txt')) {
	Set-Content -LiteralPath (Join-Path $packageRoot $name) -Value 'fixture'
}
Set-Content -LiteralPath (Join-Path $runtimeRoot 'Project_MuseumHeist.exe') -Value 'fixture'
$packageLock = Join-Path $packageRoot 'ExternalContentLock.json'
Copy-Item -LiteralPath $lockPath -Destination $packageLock
$infoPath = Join-Path $packageRoot 'BuildInfo.json'
$info = [ordered]@{ schemaVersion = 1; projectVersion = '0.0.1'; configuration = 'Development'; gitCommit = 'aaaaaaaaaaaa'; gitDirty = $false }
function Save-FixtureInfo { $info | ConvertTo-Json | Set-Content -LiteralPath $infoPath -Encoding utf8 }
function Invoke-FixtureValidator {
	param([int]$ExpectedExit = 0, [string]$ExpectedText = 'Result=PASS')
	$output = & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $packagingRoot 'ValidatePackage.ps1') -PackageRoot $packageRoot -ExpectedConfiguration Development -ExpectedVersion 0.0.1 -ExpectedGitCommit aaaaaaaaaaaa -RequireClean 2>&1
	Assert-Fixture ($LASTEXITCODE -eq $ExpectedExit) ("Validator exit mismatch: {0}" -f ($output -join "`n"))
	Assert-Fixture (($output -join "`n") -like "*$ExpectedText*") 'Expected validation result was absent.'
}
Save-FixtureInfo
Test-Fixture 'legacy_schema1_compatible' { Invoke-FixtureValidator }
foreach ($invalidSchema in @(3, 'invalid', '2')) {
	$info.schemaVersion = $invalidSchema
	Save-FixtureInfo
	Test-Fixture ("unsupported_schema_{0}" -f $invalidSchema) { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'supported numeric schemaVersion' }
}
$info.Remove('schemaVersion')
Save-FixtureInfo
Test-Fixture 'missing_schema_rejected' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'supported numeric schemaVersion' }
$info.schemaVersion = 2
$info.gitCommitFull = 'a' * 40
$info.sourceInputSha256 = 'b' * 64
$info.externalContentLockSha256 = $externalHash
Save-FixtureInfo
Test-Fixture 'schema2_valid_fingerprints_and_lock' { Invoke-FixtureValidator }
$info.sourceInputSha256 = 'bad'
Save-FixtureInfo
Test-Fixture 'schema2_invalid_source_fingerprint' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'requires a SHA256 sourceInputSha256' }
$info.sourceInputSha256 = 'b' * 64
$info.gitCommitFull = 'c' * 40
Save-FixtureInfo
Test-Fixture 'schema2_revision_inconsistent' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'consistent full and short Git revisions' }
$info.gitCommitFull = 'a' * 41
Save-FixtureInfo
Test-Fixture 'schema2_invalid_full_revision_length' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'consistent full and short Git revisions' }
$info.gitCommitFull = 'a' * 64
Save-FixtureInfo
Test-Fixture 'schema2_sha256_git_revision_supported' { Invoke-FixtureValidator }
$info.gitCommitFull = 'a' * 40
Save-FixtureInfo
Add-Content -LiteralPath $packageLock -Value 'tampered'
Test-Fixture 'schema2_packaged_lock_tampered' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'does not match' }
Remove-Item -LiteralPath $packageLock
Test-Fixture 'schema2_packaged_lock_missing' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'missing ExternalContentLock.json' }
Copy-Item -LiteralPath $lockPath -Destination $packageLock
$info.gitDirty = $true
Save-FixtureInfo
Test-Fixture 'schema2_require_clean_rejects_dirty' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'dirty working tree' }
$info.gitDirty = 'false'
Save-FixtureInfo
Test-Fixture 'schema2_require_clean_rejects_string_false' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'boolean gitDirty value' }
$info.gitDirty = $false
$info.configuration = 'Unknown'
Save-FixtureInfo
Test-Fixture 'schema2_configuration_still_validated' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'unsupported configuration' }
$info.configuration = 'Development'
$info.gitCommit = 'dddddddddddd'
Save-FixtureInfo
Test-Fixture 'schema2_expected_revision_still_validated' { Invoke-FixtureValidator -ExpectedExit 1 -ExpectedText 'Git commit mismatch' }
$info.gitCommit = 'aaaaaaaaaaaa'
Save-FixtureInfo
$candidateOutput = Join-Path $ResultDirectory 'depot'
Test-Fixture 'prepare_depot_preserves_schema2_provenance' {
	$output = & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $packagingRoot 'PrepareSteamDepot.ps1') -PackageRoot $packageRoot -AppId 480 -DepotId 4801 -OutputRoot $candidateOutput -ExpectedConfiguration Development -ExpectedVersion 0.0.1 -ExpectedGitCommit aaaaaaaaaaaa -RequireClean 2>&1
	Assert-Fixture ($LASTEXITCODE -eq 0) ("Prepare failed: {0}" -f ($output -join "`n"))
	$copiedLock = Join-Path $candidateOutput 'MuseumHeist-0.0.1-Development-Windows/content/windows/ExternalContentLock.json'
	Assert-Fixture ((Get-FileHash -LiteralPath $copiedLock -Algorithm SHA256).Hash -eq $externalHash) 'Depot lock changed.'
	$depotInfo = Get-Content -LiteralPath (Join-Path $candidateOutput 'MuseumHeist-0.0.1-Development-Windows/DepotCandidate.json') -Raw | ConvertFrom-Json
	Assert-Fixture $depotInfo.previewOnly 'Depot fixture must remain preview-only.'
}
$fakeEngine = Join-Path $ResultDirectory 'engine'
$batchDirectory = Join-Path $fakeEngine 'Engine/Build/BatchFiles'
New-Item -ItemType Directory -Path $batchDirectory -Force | Out-Null
Set-Content -LiteralPath (Join-Path $batchDirectory 'RunUAT.bat') -Value '@exit /b 87'
$planOutput = Join-Path $ResultDirectory 'plan-output'
Test-Fixture 'plan_only_writes_nothing_and_does_not_invoke_uat' {
	$output = & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $packagingRoot 'PackageProject.ps1') -EngineRoot $fakeEngine -OutputRoot $planOutput -PlanOnly 2>&1
	Assert-Fixture ($LASTEXITCODE -eq 0) ("PlanOnly failed: {0}" -f ($output -join "`n"))
	Assert-Fixture (-not (Test-Path -LiteralPath $planOutput)) 'PlanOnly created output.'
}

$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $ResultDirectory 'results.json') -Encoding utf8
$failed = @($results | Where-Object { -not $_.pass })
Write-Output ("Packaging provenance fixtures: Passed={0} Failed={1} Results={2}" -f ($results.Count - $failed.Count), $failed.Count, (Join-Path $ResultDirectory 'results.json'))
if ($failed.Count -gt 0) { $failed | Format-List; exit 1 }
exit 0
