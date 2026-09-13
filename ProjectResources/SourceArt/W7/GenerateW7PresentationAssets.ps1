param(
	[string]$OutputDirectory = (Join-Path $PSScriptRoot 'Generated'),
	[switch]$VentFeedbackOnly,
	[switch]$FloorPlanOnly,
	[string]$EditorExecutable
)

$ErrorActionPreference = 'Stop'

if ($VentFeedbackOnly -and $FloorPlanOnly)
{
	throw 'VentFeedbackOnly and FloorPlanOnly cannot be used together.'
}

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $OutputDirectory))
{
	New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
}

function New-FloorPlanTexture
{
	param(
		[pscustomobject]$Geometry,
		[System.Drawing.Color]$AccentColor,
		[System.Drawing.Color]$SecondaryColor
	)

	$width = 1024
	$height = 640
	$spanX = [double]$Geometry.worldMax[0] - [double]$Geometry.worldMin[0]
	$spanY = [double]$Geometry.worldMax[1] - [double]$Geometry.worldMin[1]
	$resources = @()
	try
	{
		$bitmap = [System.Drawing.Bitmap]::new($width, $height)
		$resources += $bitmap
		$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
		$resources += $graphics
		$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
		$graphics.Clear([System.Drawing.Color]::FromArgb(255, 20, 18, 16))

		$floorBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(46, $AccentColor))
		$resources += $floorBrush
		$wallBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(225, $AccentColor))
		$resources += $wallBrush
		$wallEdgePen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(150, $SecondaryColor), 4.0)
		$resources += $wallEdgePen
		$doorBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 238, 194, 91))
		$resources += $doorBrush

		foreach ($layer in @('floors', 'doors', 'walls'))
		{
			$brush = switch ($layer)
			{
				'floors' { $floorBrush }
				'walls' { $wallBrush }
				'doors' { $doorBrush }
			}
			foreach ($shape in $Geometry.$layer)
			{
				$points = [System.Drawing.PointF[]]@(foreach ($point in $shape.polygon)
				{
					# Match PositiveY WorldMin/WorldMax projection and UV * CanvasSize; no texture inset.
					$u = ([double]$point[0] - [double]$Geometry.worldMin[0]) / $spanX
					$v = 1.0 - ([double]$point[1] - [double]$Geometry.worldMin[1]) / $spanY
					[System.Drawing.PointF]::new([single]($u * $width), [single]($v * $height))
				})
				$graphics.FillPolygon($brush, $points)
				if ($layer -eq 'walls')
				{
					$graphics.DrawPolygon($wallEdgePen, $points)
				}
			}
		}

		$outputPath = Join-Path $OutputDirectory ("T_FloorPlan_{0}.png" -f $Geometry.mapId)
		$bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
	}
	finally
	{
		for ($resourceIndex = $resources.Count - 1; $resourceIndex -ge 0; --$resourceIndex)
		{
			$resources[$resourceIndex].Dispose()
		}
	}
}

function Confirm-CurrentFloorPlanGeometry
{
	param([string]$GeometryPath, [string]$ExpectedSourceHash)

	$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
	$projectPath = Join-Path $projectRoot 'Project_MuseumHeist.uproject'
	$verificationScript = Join-Path $projectRoot 'ProjectResources/Scripts/Editor/export_floor_plan_geometry.py'
	$editorPath = $EditorExecutable
	if ([string]::IsNullOrWhiteSpace($editorPath))
	{
		$engineAssociation = (Get-Content -Raw -LiteralPath $projectPath | ConvertFrom-Json).EngineAssociation
		$engineInstallation = Get-ItemProperty -LiteralPath "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$engineAssociation" -ErrorAction SilentlyContinue
		if ($engineInstallation.InstalledDirectory)
		{
			$editorPath = Join-Path $engineInstallation.InstalledDirectory 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
		}
	}
	if ([string]::IsNullOrWhiteSpace($editorPath) -or -not (Test-Path -LiteralPath $editorPath -PathType Leaf))
	{
		throw 'Current floor-plan geometry requires Unreal Editor verification. Supply -EditorExecutable with the UnrealEditor-Cmd.exe path. Existing PNGs were not changed.'
	}
	$editorPath = (Resolve-Path -LiteralPath $editorPath).Path
	$verificationId = [guid]::NewGuid().ToString('N')
	$reportPath = Join-Path ([IO.Path]::GetTempPath()) "MuseumHeist-FloorPlanVerify-$verificationId.json"
	$logPath = Join-Path ([IO.Path]::GetTempPath()) "MuseumHeist-FloorPlanVerify-$verificationId.log"
	# Start-Process joins ArgumentList into one native command line. Quote each path.
	foreach ($path in @($projectPath, $verificationScript, $reportPath, $logPath))
	{
		if ($path.Contains('"')) { throw 'Unreal verification paths cannot contain a double quote.' }
	}
	$arguments = @(
		('"' + $projectPath + '"'), '-Unattended', '-NullRHI', '-NoSound', '-NoSplash',
		('-ExecutePythonScript="' + $verificationScript + '"'), '-MuseumFloorPlanMode=verify',
		('-MuseumFloorPlanReport="' + $reportPath + '"'), '-MuseumFloorPlanQuit', ('-abslog="' + $logPath + '"')
	)
	Write-Host "Checking current map geometry in a read-only Editor process. Log: $logPath"
	# Wait only for this Editor. Start-Process -Wait also waits for descendants,
	# including shared Zen services that can outlive the verification process.
	$verificationProcess = Start-Process -FilePath $editorPath -ArgumentList $arguments -WindowStyle Hidden -PassThru
	$verificationProcess.WaitForExit()
	if ($verificationProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $reportPath -PathType Leaf))
	{
		throw "Floor-plan Editor verification failed (exit $($verificationProcess.ExitCode)). Existing PNGs were not changed. See $logPath"
	}
	$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
	if ($report.status -cne 'PASS' -or $report.mode -cne 'verify' -or $report.packages_saved -ne $false -or
		$report.map_hashes_unchanged -ne $true -or $report.maps -ne 3 -or
		$report.current_geometry_sha256 -cne $report.saved_geometry_sha256 -or
		$report.source_file_sha256 -ine $ExpectedSourceHash -or
		(Get-FileHash -LiteralPath $GeometryPath -Algorithm SHA256).Hash -ine $ExpectedSourceHash)
	{
		throw "Current map geometry verification did not pass, or source JSON changed during verification. Existing PNGs were not changed. See $reportPath"
	}
	Write-Host "Current map geometry verified: $($report.current_geometry_sha256). Report: $reportPath"
}

function New-HeistLoopWave
{
	param(
		[string]$FileName,
		[ValidateSet('Suspense', 'Alarm')]
		[string]$Mode,
		[int]$DurationSeconds = 12
	)

	$sampleRate = 48000
	$channels = 2
	$bitsPerSample = 16
	$sampleCount = $sampleRate * $DurationSeconds
	$blockAlign = $channels * ($bitsPerSample / 8)
	$dataSize = $sampleCount * $blockAlign
	$outputPath = Join-Path $OutputDirectory $FileName

	$stream = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
	$writer = [System.IO.BinaryWriter]::new($stream)
	try
	{
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF'))
		$writer.Write([int](36 + $dataSize))
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('WAVE'))
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('fmt '))
		$writer.Write([int]16)
		$writer.Write([short]1)
		$writer.Write([short]$channels)
		$writer.Write([int]$sampleRate)
		$writer.Write([int]($sampleRate * $blockAlign))
		$writer.Write([short]$blockAlign)
		$writer.Write([short]$bitsPerSample)
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('data'))
		$writer.Write([int]$dataSize)

		$fadeFrames = [int]($sampleRate * 0.02)
		for ($i = 0; $i -lt $sampleCount; ++$i)
		{
			$t = $i / [double]$sampleRate
			$edgeEnvelope = 1.0
			if ($i -lt $fadeFrames)
			{
				$edgeEnvelope = 0.5 - 0.5 * [Math]::Cos([Math]::PI * $i / [double]$fadeFrames)
			}
			elseif ($i -ge $sampleCount - $fadeFrames)
			{
				$framesFromEnd = $sampleCount - 1 - $i
				$edgeEnvelope = 0.5 - 0.5 * [Math]::Cos([Math]::PI * $framesFromEnd / [double]$fadeFrames)
			}
			if ($Mode -eq 'Suspense')
			{
				$lfo = 0.72 + 0.28 * [Math]::Sin(2.0 * [Math]::PI * 0.25 * $t)
				$signal = 0.34 * [Math]::Sin(2.0 * [Math]::PI * 55.0 * $t)
				$signal += 0.22 * [Math]::Sin(2.0 * [Math]::PI * 82.5 * $t + 0.4)
				$signal += 0.09 * [Math]::Sin(2.0 * [Math]::PI * 165.0 * $t + 1.1)
				$left = $signal * $lfo * 0.46
				$right = ($signal + 0.06 * [Math]::Sin(2.0 * [Math]::PI * 110.0 * $t + 0.8)) * $lfo * 0.44
			}
			else
			{
				$pulse = 0.60 + 0.40 * [Math]::Sin(2.0 * [Math]::PI * 1.0 * $t)
				$phaseMod = 38.0 * [Math]::Sin(2.0 * [Math]::PI * 0.25 * $t)
				$signal = 0.50 * [Math]::Sin(2.0 * [Math]::PI * 220.0 * $t + $phaseMod)
				$signal += 0.22 * [Math]::Sin(2.0 * [Math]::PI * 330.0 * $t)
				$signal += 0.12 * [Math]::Sin(2.0 * [Math]::PI * 440.0 * $t)
				$left = $signal * $pulse * 0.42
				$right = ($signal + 0.08 * [Math]::Sin(2.0 * [Math]::PI * 275.0 * $t + 0.6)) * $pulse * 0.40
			}

			$left *= $edgeEnvelope
			$right *= $edgeEnvelope
			$writer.Write([short]([Math]::Round([Math]::Max(-1.0, [Math]::Min(1.0, $left)) * 32767.0)))
			$writer.Write([short]([Math]::Round([Math]::Max(-1.0, [Math]::Min(1.0, $right)) * 32767.0)))
		}
	}
	finally
	{
		$writer.Dispose()
		$stream.Dispose()
	}
}

function New-HeistStatusIcon
{
	param(
		[ValidateSet('Stunned', 'Arrested', 'CarryingOriginal', 'Heavy')]
		[string]$StatusName
	)

	$size = 256
	$bitmap = [System.Drawing.Bitmap]::new($size, $size)
	$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
	$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
	$graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
	$graphics.Clear([System.Drawing.Color]::Transparent)

	switch ($StatusName)
	{
		'Stunned'
		{
			$backgroundColor = [System.Drawing.Color]::FromArgb(238, 83, 56, 148)
			$accentColor = [System.Drawing.Color]::FromArgb(255, 248, 218, 88)
		}
		'Arrested'
		{
			$backgroundColor = [System.Drawing.Color]::FromArgb(238, 151, 45, 58)
			$accentColor = [System.Drawing.Color]::FromArgb(255, 244, 246, 250)
		}
		'CarryingOriginal'
		{
			$backgroundColor = [System.Drawing.Color]::FromArgb(238, 32, 111, 134)
			$accentColor = [System.Drawing.Color]::FromArgb(255, 242, 192, 76)
		}
		'Heavy'
		{
			$backgroundColor = [System.Drawing.Color]::FromArgb(238, 157, 86, 31)
			$accentColor = [System.Drawing.Color]::FromArgb(255, 245, 239, 224)
		}
	}

	$shadowBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(72, 0, 0, 0))
	$backgroundBrush = [System.Drawing.SolidBrush]::new($backgroundColor)
	$symbolBrush = [System.Drawing.SolidBrush]::new($accentColor)
	$symbolPen = [System.Drawing.Pen]::new($accentColor, 14.0)
	$symbolPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
	$symbolPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
	$ringPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(220, $accentColor), 7.0)

	$graphics.FillEllipse($shadowBrush, 20, 25, 224, 224)
	$graphics.FillEllipse($backgroundBrush, 16, 16, 224, 224)
	$graphics.DrawEllipse($ringPen, 20, 20, 216, 216)

	switch ($StatusName)
	{
		'Stunned'
		{
			$bolt = [System.Drawing.PointF[]]@(
				[System.Drawing.PointF]::new(139, 42),
				[System.Drawing.PointF]::new(84, 132),
				[System.Drawing.PointF]::new(121, 132),
				[System.Drawing.PointF]::new(101, 211),
				[System.Drawing.PointF]::new(177, 111),
				[System.Drawing.PointF]::new(139, 111)
			)
			$graphics.FillPolygon($symbolBrush, $bolt)
			$graphics.FillEllipse($symbolBrush, 58, 67, 18, 18)
			$graphics.FillEllipse($symbolBrush, 182, 61, 13, 13)
			$graphics.FillEllipse($symbolBrush, 183, 171, 17, 17)
		}
		'Arrested'
		{
			$graphics.DrawEllipse($symbolPen, 52, 75, 66, 66)
			$graphics.DrawEllipse($symbolPen, 138, 115, 66, 66)
			$graphics.DrawLine($symbolPen, 109, 127, 151, 144)
			$graphics.DrawLine($symbolPen, 112, 145, 145, 158)
		}
		'CarryingOriginal'
		{
			$framePen = [System.Drawing.Pen]::new($accentColor, 12.0)
			$graphics.DrawRectangle($framePen, 62, 59, 132, 105)
			$mountain = [System.Drawing.PointF[]]@(
				[System.Drawing.PointF]::new(79, 145),
				[System.Drawing.PointF]::new(111, 107),
				[System.Drawing.PointF]::new(133, 129),
				[System.Drawing.PointF]::new(154, 96),
				[System.Drawing.PointF]::new(180, 145)
			)
			$graphics.FillPolygon($symbolBrush, $mountain)
			$graphics.FillEllipse($symbolBrush, 91, 79, 17, 17)
			$graphics.DrawLine($symbolPen, 78, 181, 178, 203)
			$framePen.Dispose()
		}
		'Heavy'
		{
			$graphics.DrawArc($symbolPen, 88, 55, 80, 78, 190, 160)
			$weight = [System.Drawing.PointF[]]@(
				[System.Drawing.PointF]::new(71, 110),
				[System.Drawing.PointF]::new(185, 110),
				[System.Drawing.PointF]::new(205, 193),
				[System.Drawing.PointF]::new(51, 193)
			)
			$graphics.FillPolygon($symbolBrush, $weight)
			$cutoutBrush = [System.Drawing.SolidBrush]::new($backgroundColor)
			$graphics.FillRectangle($cutoutBrush, 111, 127, 34, 47)
			$cutoutBrush.Dispose()
		}
	}

	$outputPath = Join-Path $OutputDirectory ("T_HeistStatus_{0}.png" -f $StatusName)
	$bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

	foreach ($resource in @($shadowBrush, $backgroundBrush, $symbolBrush, $symbolPen, $ringPen, $graphics, $bitmap))
	{
		$resource.Dispose()
	}
}

function New-HeistCueWave
{
	param(
		[string]$FileName,
		[ValidateSet('Arrested', 'Rescue', 'CarryFootstep', 'HeavyFootstep', 'VentOpened', 'VentSettlement')]
		[string]$Mode
	)

	$durationSeconds = switch ($Mode)
	{
		'Arrested' { 1.10 }
		'Rescue' { 0.85 }
		'CarryFootstep' { 0.32 }
		'HeavyFootstep' { 0.46 }
		'VentOpened' { 0.70 }
		'VentSettlement' { 0.72 }
	}
	$sampleRate = 48000
	$channels = 2
	$bitsPerSample = 16
	$sampleCount = [int]($sampleRate * $durationSeconds)
	$blockAlign = $channels * ($bitsPerSample / 8)
	$dataSize = $sampleCount * $blockAlign
	$outputPath = Join-Path $OutputDirectory $FileName

	$stream = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
	$writer = [System.IO.BinaryWriter]::new($stream)
	try
	{
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF'))
		$writer.Write([int](36 + $dataSize))
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('WAVE'))
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('fmt '))
		$writer.Write([int]16)
		$writer.Write([short]1)
		$writer.Write([short]$channels)
		$writer.Write([int]$sampleRate)
		$writer.Write([int]($sampleRate * $blockAlign))
		$writer.Write([short]$blockAlign)
		$writer.Write([short]$bitsPerSample)
		$writer.Write([System.Text.Encoding]::ASCII.GetBytes('data'))
		$writer.Write([int]$dataSize)

		$fadeInFrames = [int]($sampleRate * 0.004)
		$fadeOutFrames = [int]($sampleRate * 0.025)
		for ($i = 0; $i -lt $sampleCount; ++$i)
		{
			$t = $i / [double]$sampleRate
			$edgeEnvelope = 1.0
			if ($i -lt $fadeInFrames)
			{
				$edgeEnvelope = $i / [double]$fadeInFrames
			}
			elseif ($i -ge $sampleCount - $fadeOutFrames)
			{
				$edgeEnvelope = ($sampleCount - 1 - $i) / [double]$fadeOutFrames
			}

			$left = 0.0
			$right = 0.0
			switch ($Mode)
			{
				'Arrested'
				{
					$firstHit = [Math]::Exp(-9.0 * $t) * (0.46 * [Math]::Sin(2.0 * [Math]::PI * 790.0 * $t) + 0.24 * [Math]::Sin(2.0 * [Math]::PI * 1270.0 * $t))
					$secondTime = $t - 0.27
					$secondHit = if ($secondTime -ge 0.0) { [Math]::Exp(-11.0 * $secondTime) * (0.40 * [Math]::Sin(2.0 * [Math]::PI * 650.0 * $secondTime) + 0.22 * [Math]::Sin(2.0 * [Math]::PI * 1040.0 * $secondTime)) } else { 0.0 }
					$impact = 0.28 * [Math]::Exp(-15.0 * $t) * [Math]::Sin(2.0 * [Math]::PI * 92.0 * $t)
					$left = ($firstHit + $secondHit + $impact) * 0.62
					$right = ($firstHit + 0.94 * $secondHit + 0.90 * $impact) * 0.60
				}
				'Rescue'
				{
					$signal = 0.0
					foreach ($note in @(@(0.00, 440.0), @(0.18, 659.25), @(0.36, 880.0)))
					{
						$noteTime = $t - $note[0]
						if ($noteTime -ge 0.0)
						{
							$signal += 0.40 * [Math]::Exp(-7.5 * $noteTime) * [Math]::Sin(2.0 * [Math]::PI * $note[1] * $noteTime)
						}
					}
					$left = $signal * 0.66
					$right = ($signal + 0.07 * [Math]::Exp(-5.0 * $t) * [Math]::Sin(2.0 * [Math]::PI * 1320.0 * $t)) * 0.64
				}
				'CarryFootstep'
				{
					$thump = [Math]::Exp(-18.0 * $t) * (0.62 * [Math]::Sin(2.0 * [Math]::PI * 92.0 * $t) + 0.18 * [Math]::Sin(2.0 * [Math]::PI * 184.0 * $t))
					$contact = 0.15 * [Math]::Exp(-42.0 * $t) * [Math]::Sin(2.0 * [Math]::PI * 760.0 * $t)
					$left = ($thump + $contact) * 0.58
					$right = ($thump + 0.85 * $contact) * 0.55
				}
				'HeavyFootstep'
				{
					$thump = [Math]::Exp(-12.0 * $t) * (0.72 * [Math]::Sin(2.0 * [Math]::PI * 58.0 * $t) + 0.28 * [Math]::Sin(2.0 * [Math]::PI * 116.0 * $t))
					$contact = 0.18 * [Math]::Exp(-34.0 * $t) * [Math]::Sin(2.0 * [Math]::PI * 520.0 * $t)
					$left = ($thump + $contact) * 0.64
					$right = (0.96 * $thump + 0.78 * $contact) * 0.62
				}
				'VentOpened'
				{
					# A restrained single bell identifies access opening; it has no loop metadata.
					$bellEnvelope = (1.0 - [Math]::Exp(-250.0 * $t)) * [Math]::Exp(-7.5 * $t)
					$signal = [Math]::Sin(2.0 * [Math]::PI * 523.25 * $t)
					$signal += 0.18 * [Math]::Sin(2.0 * [Math]::PI * 1046.50 * $t)
					$left = $signal * $bellEnvelope * 0.36
					$right = $left
				}
				'VentSettlement'
				{
					# Two rising sine bells confirm a secured deposit without borrowing the rescue cue.
					$signal = 0.0
					foreach ($note in @(@(0.00, 659.25), @(0.16, 987.77)))
					{
						$noteTime = $t - $note[0]
						if ($noteTime -ge 0.0)
						{
							$bellEnvelope = (1.0 - [Math]::Exp(-250.0 * $noteTime)) * [Math]::Exp(-8.5 * $noteTime)
							$bell = [Math]::Sin(2.0 * [Math]::PI * $note[1] * $noteTime)
							$bell += 0.10 * [Math]::Sin(4.0 * [Math]::PI * $note[1] * $noteTime)
							$signal += 0.30 * $bellEnvelope * $bell
						}
					}
					$left = $signal
					$right = $signal
				}
			}

			$left *= $edgeEnvelope
			$right *= $edgeEnvelope
			$writer.Write([short]([Math]::Round([Math]::Max(-1.0, [Math]::Min(1.0, $left)) * 32767.0)))
			$writer.Write([short]([Math]::Round([Math]::Max(-1.0, [Math]::Min(1.0, $right)) * 32767.0)))
		}
	}
	finally
	{
		$writer.Dispose()
		$stream.Dispose()
	}
}

if ($VentFeedbackOnly)
{
	New-HeistCueWave -FileName 'SW_HeistVentOpened.wav' -Mode 'VentOpened'
	New-HeistCueWave -FileName 'SW_HeistVentSettlement.wav' -Mode 'VentSettlement'
	Write-Host "Generated only Vent feedback sources in $OutputDirectory"
	return
}

# Geometry is exported from placed floor/wall/door footprints in world XY.
# Each maps entry has mapId, worldMin:[x,y], worldMax:[x,y], and floors/walls/doors
# arrays of {label,polygon:[[x,y],...]}. Labels are provenance only, never raster text.
$floorPlanGeometryPath = Join-Path $PSScriptRoot 'FloorPlanGeometry.json'
if (-not (Test-Path -LiteralPath $floorPlanGeometryPath -PathType Leaf))
{
	throw "Missing floor-plan geometry: $floorPlanGeometryPath. Existing PNGs were not changed."
}
$floorPlanSourceHash = (Get-FileHash -LiteralPath $floorPlanGeometryPath -Algorithm SHA256).Hash
$floorPlanGeometry = Get-Content -Raw -LiteralPath $floorPlanGeometryPath | ConvertFrom-Json
$geometryMaps = @($floorPlanGeometry.maps)
if ($geometryMaps.Count -ne 3)
{
	throw 'Floor-plan geometry must contain exactly M01, M02 and M03. Existing PNGs were not changed.'
}
$mapPresentationPath = Join-Path $PSScriptRoot '../../DataTableImports/DT_MapPresentation.json'
$mapPresentationRows = @(Get-Content -Raw -LiteralPath $mapPresentationPath | ConvertFrom-Json)
$geometryByMap = @{}

# Validate all maps before the first PNG write, including bounds shared with C++ marker projection.
foreach ($mapId in @('M01', 'M02', 'M03'))
{
	$geometryMatches = @($geometryMaps | Where-Object { $_.mapId -ceq $mapId })
	if ($geometryMatches.Count -ne 1)
	{
		throw "Floor-plan geometry requires exactly one $mapId entry. Existing PNGs were not changed."
	}
	$mapGeometry = $geometryMatches[0]
	foreach ($bound in @('worldMin', 'worldMax'))
	{
		if ($mapGeometry.$bound -isnot [System.Array] -or $mapGeometry.$bound.Count -ne 2)
		{
			throw "$mapId $bound must be a numeric [x,y] pair."
		}
		foreach ($coordinate in $mapGeometry.$bound)
		{
			if ($null -eq $coordinate -or $coordinate -is [bool] -or $coordinate -is [string] -or $coordinate -isnot [System.ValueType] -or
				[double]::IsNaN([double]$coordinate) -or [double]::IsInfinity([double]$coordinate))
			{
				throw "$mapId $bound contains a non-finite or non-numeric coordinate."
			}
		}
	}
	if ([double]$mapGeometry.worldMax[0] -le [double]$mapGeometry.worldMin[0] -or
		[double]$mapGeometry.worldMax[1] -le [double]$mapGeometry.worldMin[1])
	{
		throw "$mapId world bounds must have positive X and Y spans."
	}
	$presentationMatches = @($mapPresentationRows | Where-Object { $_.Name -ceq $mapId -and $_.MapId -ceq $mapId })
	if ($presentationMatches.Count -ne 1 -or $presentationMatches[0].MapNorthAxis -cne 'PositiveY')
	{
		throw "$mapId requires one PositiveY DT_MapPresentation row."
	}
	$presentation = $presentationMatches[0]
	if ([Math]::Abs([double]$mapGeometry.worldMin[0] - [double]$presentation.WorldMin.X) -gt 0.01 -or
		[Math]::Abs([double]$mapGeometry.worldMin[1] - [double]$presentation.WorldMin.Y) -gt 0.01 -or
		[Math]::Abs([double]$mapGeometry.worldMax[0] - [double]$presentation.WorldMax.X) -gt 0.01 -or
		[Math]::Abs([double]$mapGeometry.worldMax[1] - [double]$presentation.WorldMax.Y) -gt 0.01)
	{
		throw "$mapId geometry bounds differ from DT_MapPresentation. Existing PNGs were not changed."
	}

	foreach ($layer in @('floors', 'walls', 'doors'))
	{
		if ($mapGeometry.$layer -isnot [System.Array] -or ($layer -ne 'doors' -and $mapGeometry.$layer.Count -eq 0))
		{
			throw "$mapId $layer must be an array; floors and walls must not be empty."
		}
		foreach ($shape in $mapGeometry.$layer)
		{
			if ($shape.polygon -isnot [System.Array] -or $shape.polygon.Count -lt 3)
			{
				throw "$mapId $layer polygon requires at least three [x,y] points."
			}
			foreach ($point in $shape.polygon)
			{
				if ($point -isnot [System.Array] -or $point.Count -ne 2)
				{
					throw "$mapId $layer polygon contains an invalid [x,y] point."
				}
				foreach ($coordinate in $point)
				{
					if ($null -eq $coordinate -or $coordinate -is [bool] -or $coordinate -is [string] -or $coordinate -isnot [System.ValueType] -or
						[double]::IsNaN([double]$coordinate) -or [double]::IsInfinity([double]$coordinate))
					{
						throw "$mapId $layer polygon contains a non-finite or non-numeric coordinate."
					}
				}
			}
			$twiceArea = 0.0
			for ($pointIndex = 0; $pointIndex -lt $shape.polygon.Count; ++$pointIndex)
			{
				$a = $shape.polygon[$pointIndex]
				$b = $shape.polygon[($pointIndex + 1) % $shape.polygon.Count]
				$twiceArea += [double]$a[0] * [double]$b[1] - [double]$b[0] * [double]$a[1]
			}
			if ([double]::IsNaN($twiceArea) -or [double]::IsInfinity($twiceArea) -or [Math]::Abs($twiceArea) -le 0.0001)
			{
				throw "$mapId $layer polygon is degenerate."
			}
		}
	}
	$geometryByMap[$mapId] = $mapGeometry
}

Confirm-CurrentFloorPlanGeometry -GeometryPath $floorPlanGeometryPath -ExpectedSourceHash $floorPlanSourceHash

New-FloorPlanTexture -Geometry $geometryByMap['M01'] -AccentColor ([System.Drawing.Color]::FromArgb(255, 224, 213, 190)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 156, 144, 120))
New-FloorPlanTexture -Geometry $geometryByMap['M02'] -AccentColor ([System.Drawing.Color]::FromArgb(255, 224, 213, 190)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 156, 144, 120))
New-FloorPlanTexture -Geometry $geometryByMap['M03'] -AccentColor ([System.Drawing.Color]::FromArgb(255, 224, 213, 190)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 156, 144, 120))

if ($FloorPlanOnly)
{
	Write-Host "Generated only world-aligned Floor Plan sources in $OutputDirectory"
	return
}

New-HeistLoopWave -FileName 'SW_HeistSuspenseLoop.wav' -Mode 'Suspense'
New-HeistLoopWave -FileName 'SW_HeistAlarmLoop.wav' -Mode 'Alarm'

foreach ($statusName in @('Stunned', 'Arrested', 'CarryingOriginal', 'Heavy'))
{
	New-HeistStatusIcon -StatusName $statusName
}

New-HeistCueWave -FileName 'SW_HeistArrested.wav' -Mode 'Arrested'
New-HeistCueWave -FileName 'SW_HeistRescue.wav' -Mode 'Rescue'
New-HeistCueWave -FileName 'SW_HeistCarryFootstep.wav' -Mode 'CarryFootstep'
New-HeistCueWave -FileName 'SW_HeistHeavyFootstep.wav' -Mode 'HeavyFootstep'
New-HeistCueWave -FileName 'SW_HeistVentOpened.wav' -Mode 'VentOpened'
New-HeistCueWave -FileName 'SW_HeistVentSettlement.wav' -Mode 'VentSettlement'

Write-Host "Generated W7 presentation sources in $OutputDirectory"
