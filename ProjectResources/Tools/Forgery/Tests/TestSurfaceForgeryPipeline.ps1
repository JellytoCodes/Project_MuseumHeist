[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PythonExecutable,
    [string]$ResultDirectory = (Join-Path $env:TEMP ('HeistForgeryFixtures-' + [Guid]::NewGuid().ToString('N')))
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$toolRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $toolRoot '..\..\..'))
$sourceRoot = Join-Path $projectRoot 'ProjectResources/SourceArt/Forgery/M01'
$validator = Join-Path $toolRoot 'ValidateSurfaceForgeryPack.ps1'
if (Test-Path -LiteralPath $ResultDirectory) { throw "Fixture output already exists: $ResultDirectory" }
New-Item -ItemType Directory -Path $ResultDirectory | Out-Null
$results = [System.Collections.Generic.List[object]]::new()
function Assert-Fixture {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}
function Test-Fixture {
    param([string]$Name, [scriptblock]$Action)
    $errorText = $null
    try { & $Action | Out-Null } catch { $errorText = $_.Exception.Message }
    $results.Add([pscustomobject]@{ name = $Name; pass = $null -eq $errorText; error = $errorText })
}
function Invoke-SourceValidator {
    param([string]$Root, [string]$LogName)
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $validator -PoolId M01 -SourceRoot $Root 2>&1
    $code = $LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $ResultDirectory $LogName)
    [pscustomobject]@{ exitCode = $code; text = $output -join "`n" }
}

$baseline = Invoke-SourceValidator -Root $sourceRoot -LogName 'baseline.log'
Test-Fixture 'existing_catalog_has_40_candidates_and_rows' {
    Assert-Fixture ($baseline.text -match 'Candidates=40 References=40 Masks=40 Palettes=40 DataRows=40') $baseline.text
}
$fixtureSource = Join-Path $ResultDirectory 'M01'
New-Item -ItemType Directory -Path (Join-Path $fixtureSource 'Candidates') -Force | Out-Null
Get-ChildItem -LiteralPath $sourceRoot -File | Copy-Item -Destination $fixtureSource
Test-Fixture 'empty_candidates_are_rejected' {
    $result = Invoke-SourceValidator -Root $fixtureSource -LogName 'empty-candidates.log'
    Assert-Fixture ($result.exitCode -ne 0 -and $result.text -match 'Missing candidate image for Template_M01_' -and $result.text -match 'Candidates=0') $result.text
}
Get-ChildItem -LiteralPath (Join-Path $sourceRoot 'Candidates') -File | Copy-Item -Destination (Join-Path $fixtureSource 'Candidates')
Copy-Item -LiteralPath (Join-Path $fixtureSource 'M01_Landscape_01.asset.json') -Destination (Join-Path $fixtureSource 'M01_Landscape_02.asset.json')
Test-Fixture 'duplicate_template_and_missing_metadata_are_rejected' {
    $result = Invoke-SourceValidator -Root $fixtureSource -LogName 'duplicate-template.log'
    Assert-Fixture ($result.exitCode -ne 0 -and $result.text -match 'Duplicate asset metadata TemplateId: Template_M01_Landscape_01' -and
        $result.text -match 'Missing asset metadata for source manifest TemplateId: Template_M01_Landscape_02' -and $result.text -match 'DataRows=39') $result.text
}
Test-Fixture 'import_failure_preserves_existing_directories' {
    $output = & $PythonExecutable (Join-Path $PSScriptRoot 'mock_import_failure.py') 2>&1
    $output | Set-Content -LiteralPath (Join-Path $ResultDirectory 'import-failure-mock.log')
    Assert-Fixture ($LASTEXITCODE -eq 0) ($output -join "`n")
}

$wrapperRoot = Join-Path $ResultDirectory 'wrapper-project'
$wrapperTools = Join-Path $wrapperRoot 'ProjectResources/Tools/Forgery'
New-Item -ItemType Directory -Path $wrapperTools, (Join-Path $wrapperRoot 'ProjectResources/DataTableImports') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $toolRoot 'ImportSurfaceForgeryPack.ps1') -Destination $wrapperTools
Set-Content -LiteralPath (Join-Path $wrapperRoot 'Project_MuseumHeist.uproject') -Value '{}'
Set-Content -LiteralPath (Join-Path $wrapperRoot 'ProjectResources/DataTableImports/DT_ForgeryTemplateRow.json') -Value '[]'
foreach ($pool in @('M01', 'M02', 'M03')) {
    $directory = Join-Path $wrapperRoot "ProjectResources/SourceArt/Forgery/$pool"
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    foreach ($index in 1..40) {
        foreach ($suffix in @('', '_Mask')) {
            Set-Content -LiteralPath (Join-Path $directory ("{0}_{1}{2}.png" -f $pool, $index, $suffix)) -Value 'count-only fixture; never imported'
        }
    }
}
$mockEditor = Join-Path $wrapperRoot 'mock-editor.ps1'
@'
if ($env:HEIST_FORGERY_FIXTURE_MODE -eq 'nonzero') { exit 7 }
if ($env:HEIST_FORGERY_FIXTURE_MODE -eq 'confirmed') {
    $logArgument = @($args | Where-Object { $_ -like '-abslog=*' })[0]
    Set-Content -LiteralPath $logArgument.Substring(8) -Value 'SurfaceForgeryImport Result=PASS Resolution=1024x1024 Textures=240 DataRows=120'
}
exit 0
'@ | Set-Content -LiteralPath $mockEditor
function Invoke-ImportWrapper {
    param([string]$Mode)
    $previousMode = $env:HEIST_FORGERY_FIXTURE_MODE
    try {
        $env:HEIST_FORGERY_FIXTURE_MODE = $Mode
        $output = & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $wrapperTools 'ImportSurfaceForgeryPack.ps1') -UnrealEditorCmd $mockEditor 2>&1
        $code = $LASTEXITCODE
        $output | Set-Content -LiteralPath (Join-Path $ResultDirectory ("wrapper-{0}.log" -f $Mode))
        Assert-Fixture (-not (Test-Path -LiteralPath (Join-Path $wrapperRoot 'Saved/Codex/ImportSurfaceForgeryPack.py'))) 'Temporary Python was not cleaned up.'
        [pscustomobject]@{ exitCode = $code; text = $output -join "`n" }
    }
    finally { $env:HEIST_FORGERY_FIXTURE_MODE = $previousMode }
}
Test-Fixture 'zero_exit_without_import_confirmation_is_rejected' {
    $result = Invoke-ImportWrapper -Mode 'missing'
    Assert-Fixture ($result.exitCode -ne 0 -and $result.text -match 'did not confirm 240 textures') $result.text
}
Test-Fixture 'nonzero_import_exit_is_reported' {
    $result = Invoke-ImportWrapper -Mode 'nonzero'
    Assert-Fixture ($result.exitCode -ne 0 -and $result.text -match 'failed with exit code 7') $result.text
}
Test-Fixture 'fresh_import_confirmation_is_accepted' {
    $result = Invoke-ImportWrapper -Mode 'confirmed'
    Assert-Fixture ($result.exitCode -eq 0 -and $result.text -match 'Result=PASS') $result.text
}
Test-Fixture 'previous_success_log_cannot_confirm_a_new_run' {
    $result = Invoke-ImportWrapper -Mode 'missing'
    Assert-Fixture ($result.exitCode -ne 0 -and $result.text -match 'did not confirm 240 textures') $result.text
}

$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $ResultDirectory 'results.json')
$failed = @($results | Where-Object { -not $_.pass })
Write-Output ("Forgery pipeline fixtures: Passed={0} Failed={1} Results={2}" -f ($results.Count - $failed.Count), $failed.Count, (Join-Path $ResultDirectory 'results.json'))
if ($failed.Count -gt 0) { $failed | Format-List; exit 1 }
exit 0
