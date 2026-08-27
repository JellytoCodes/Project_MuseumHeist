param(
	[string]$OutputDirectory = (Join-Path $PSScriptRoot 'Generated')
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $OutputDirectory))
{
	New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
}

function New-FloorPlanTexture
{
	param(
		[string]$MapId,
		[System.Drawing.Color]$AccentColor,
		[System.Drawing.Color]$SecondaryColor
	)

	$width = 1024
	$height = 640
	$bitmap = [System.Drawing.Bitmap]::new($width, $height)
	$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
	$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
	$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

	$background = [System.Drawing.Color]::FromArgb(255, 9, 19, 31)
	$graphics.Clear($background)

	$minorGridPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(28, $AccentColor), 1.0)
	$majorGridPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(52, $AccentColor), 1.0)
	for ($x = 32; $x -lt $width; $x += 32)
	{
		$graphics.DrawLine(($x % 128 -eq 0) ? $majorGridPen : $minorGridPen, $x, 0, $x, $height)
	}
	for ($y = 32; $y -lt $height; $y += 32)
	{
		$graphics.DrawLine(($y % 128 -eq 0) ? $majorGridPen : $minorGridPen, 0, $y, $width, $y)
	}

	$roomFill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(46, $AccentColor))
	$roomFillSecondary = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(36, $SecondaryColor))
	$wallPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(225, $AccentColor), 8.0)
	$innerPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(150, $SecondaryColor), 3.0)
	$doorPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 238, 194, 91), 5.0)

	$titleFont = [System.Drawing.Font]::new('Malgun Gothic', 24, [System.Drawing.FontStyle]::Bold)
	$roomFont = [System.Drawing.Font]::new('Malgun Gothic', 17, [System.Drawing.FontStyle]::Bold)
	$smallFont = [System.Drawing.Font]::new('Malgun Gothic', 12, [System.Drawing.FontStyle]::Regular)
	$titleBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(245, 231, 244, 255))
	$textBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(238, $AccentColor))
	$mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(190, $SecondaryColor))

	$graphics.DrawString("$MapId 박물관 도면", $titleFont, $titleBrush, 42, 24)

	switch ($MapId)
	{
		'M01'
		{
			$northLoop = [System.Drawing.Rectangle]::new(150, 92, 714, 208)
			$southLoop = [System.Drawing.Rectangle]::new(150, 340, 714, 208)
			$westVent = [System.Drawing.Rectangle]::new(66, 248, 110, 144)
			$eastWing = [System.Drawing.Rectangle]::new(848, 218, 110, 204)
			foreach ($room in @($northLoop, $southLoop, $westVent, $eastWing))
			{
				$graphics.FillRectangle($roomFill, $room)
				$graphics.DrawRectangle($wallPen, $room)
			}
			$graphics.FillEllipse($roomFillSecondary, 422, 216, 180, 208)
			$graphics.DrawEllipse($wallPen, 422, 216, 180, 208)
			$graphics.DrawLine($doorPen, 150, 270, 150, 330)
			$graphics.DrawLine($doorPen, 864, 270, 864, 330)
			$graphics.DrawLine($doorPen, 474, 216, 550, 216)
			$graphics.DrawLine($doorPen, 474, 424, 550, 424)
			$graphics.DrawString('북측 순환 회랑', $roomFont, $textBrush, 402, 144)
			$graphics.DrawString('남측 순환 회랑', $roomFont, $textBrush, 402, 476)
			$graphics.DrawString('로툰다', $roomFont, $mutedBrush, 472, 304)
			$graphics.DrawString('귀환 벤트', $smallFont, $mutedBrush, 78, 304)
			$graphics.DrawString('타깃 윙', $smallFont, $mutedBrush, 872, 304)
		}
		'M02'
		{
			$body = [System.Drawing.Rectangle]::new(70, 82, 884, 476)
			$graphics.FillRectangle($roomFill, $body)
			$graphics.DrawRectangle($wallPen, $body)
			$serpentineWalls = @(
				@(206, 184, 206, 548), @(334, 82, 334, 438), @(462, 184, 462, 548),
				@(590, 82, 590, 438), @(718, 184, 718, 548), @(846, 82, 846, 438)
			)
			foreach ($wall in $serpentineWalls)
			{
				$graphics.DrawLine($wallPen, $wall[0], $wall[1], $wall[2], $wall[3])
			}
			foreach ($shortcut in @(@(206, 338), @(334, 248), @(462, 386), @(590, 232), @(718, 366), @(846, 244)))
			{
				$graphics.DrawLine($doorPen, $shortcut[0], $shortcut[1] - 24, $shortcut[0], $shortcut[1] + 24)
			}
			$graphics.FillEllipse($roomFillSecondary, 382, 214, 176, 128)
			$graphics.DrawEllipse($innerPen, 382, 214, 176, 128)
			$graphics.DrawString('진입', $smallFont, $mutedBrush, 86, 506)
			$graphics.DrawString('비대칭 달빛 정원', $roomFont, $textBrush, 394, 258)
			$graphics.DrawString('S자 안전 동선', $roomFont, $textBrush, 420, 494)
			$graphics.DrawString('감시 지름길', $smallFont, $mutedBrush, 694, 132)
			$graphics.DrawString('배출 벤트', $smallFont, $mutedBrush, 720, 92)
		}
		'M03'
		{
			$northLane = [System.Drawing.Rectangle]::new(70, 104, 884, 126)
			$centralLane = [System.Drawing.Rectangle]::new(70, 258, 884, 124)
			$southLane = [System.Drawing.Rectangle]::new(70, 410, 884, 126)
			foreach ($room in @($northLane, $centralLane, $southLane))
			{
				$graphics.FillRectangle(($room.Y -eq 258) ? $roomFill : $roomFillSecondary, $room)
				$graphics.DrawRectangle($wallPen, $room)
			}
			foreach ($x in @(150, 390, 600, 824))
			{
				$graphics.DrawLine($doorPen, $x, 230, $x, 258)
			}
			foreach ($x in @(260, 506, 724, 900))
			{
				$graphics.DrawLine($doorPen, $x, 382, $x, 410)
			}
			$graphics.DrawRectangle($innerPen, 474, 238, 92, 164)
			$graphics.DrawString('북측 유리 레인', $roomFont, $textBrush, 418, 146)
			$graphics.DrawString('중앙 스파인', $roomFont, $textBrush, 420, 278)
			$graphics.DrawString('남측 복원 레인', $roomFont, $textBrush, 412, 452)
			$graphics.DrawString('엇갈린 교차', $smallFont, $mutedBrush, 478, 342)
			$graphics.DrawString('진입/귀환 벤트', $smallFont, $mutedBrush, 78, 302)
			$graphics.DrawString('타깃', $smallFont, $mutedBrush, 900, 146)
		}
		default
		{
			throw "Unknown floor plan MapId: $MapId"
		}
	}

	$graphics.DrawString('N', $roomFont, $titleBrush, 936, 48)
	$graphics.DrawLine($wallPen, 947, 86, 947, 122)
	$graphics.DrawLine($wallPen, 947, 86, 934, 104)
	$graphics.DrawLine($wallPen, 947, 86, 960, 104)
	$graphics.DrawString('고정 도면 · 실제 위치는 마커로 갱신', $smallFont, $mutedBrush, 42, 596)

	$outputPath = Join-Path $OutputDirectory ("T_FloorPlan_{0}.png" -f $MapId)
	$bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

	foreach ($resource in @($minorGridPen, $majorGridPen, $roomFill, $roomFillSecondary, $wallPen, $innerPen, $doorPen, $titleFont, $roomFont, $smallFont, $titleBrush, $textBrush, $mutedBrush, $graphics, $bitmap))
	{
		$resource.Dispose()
	}
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
		[ValidateSet('Arrested', 'Rescue', 'CarryFootstep', 'HeavyFootstep')]
		[string]$Mode
	)

	$durationSeconds = switch ($Mode)
	{
		'Arrested' { 1.10 }
		'Rescue' { 0.85 }
		'CarryFootstep' { 0.32 }
		'HeavyFootstep' { 0.46 }
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

New-FloorPlanTexture -MapId 'M01' -AccentColor ([System.Drawing.Color]::FromArgb(255, 75, 210, 236)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 128, 162, 205))
New-FloorPlanTexture -MapId 'M02' -AccentColor ([System.Drawing.Color]::FromArgb(255, 130, 170, 255)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 173, 137, 226))
New-FloorPlanTexture -MapId 'M03' -AccentColor ([System.Drawing.Color]::FromArgb(255, 91, 232, 181)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 103, 181, 209))

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

Write-Host "Generated W7 presentation sources in $OutputDirectory"
