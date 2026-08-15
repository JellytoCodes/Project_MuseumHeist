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

	$west = [System.Drawing.Rectangle]::new(72, 218, 238, 204)
	$central = [System.Drawing.Rectangle]::new(310, 172, 404, 296)
	$east = [System.Drawing.Rectangle]::new(714, 218, 238, 204)
	$north = [System.Drawing.Rectangle]::new(382, 48, 260, 124)
	$south = [System.Drawing.Rectangle]::new(382, 468, 260, 124)

	foreach ($room in @($west, $central, $east))
	{
		$graphics.FillRectangle($roomFill, $room)
		$graphics.DrawRectangle($wallPen, $room)
	}
	foreach ($room in @($north, $south))
	{
		$graphics.FillRectangle($roomFillSecondary, $room)
		$graphics.DrawRectangle($wallPen, $room)
	}

	$graphics.DrawRectangle($innerPen, 350, 212, 324, 216)
	$graphics.DrawLine($doorPen, 310, 286, 310, 354)
	$graphics.DrawLine($doorPen, 714, 286, 714, 354)
	$graphics.DrawLine($doorPen, 478, 172, 546, 172)
	$graphics.DrawLine($doorPen, 478, 468, 546, 468)
	$graphics.DrawLine($doorPen, 478, 48, 546, 48)

	$titleFont = [System.Drawing.Font]::new('Malgun Gothic', 24, [System.Drawing.FontStyle]::Bold)
	$roomFont = [System.Drawing.Font]::new('Malgun Gothic', 17, [System.Drawing.FontStyle]::Bold)
	$smallFont = [System.Drawing.Font]::new('Malgun Gothic', 12, [System.Drawing.FontStyle]::Regular)
	$titleBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(245, 231, 244, 255))
	$textBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(238, $AccentColor))
	$mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(190, $SecondaryColor))

	$graphics.DrawString("$MapId 박물관 도면", $titleFont, $titleBrush, 42, 24)
	$graphics.DrawString('서관', $roomFont, $textBrush, 160, 303)
	$graphics.DrawString('중앙 전시관', $roomFont, $textBrush, 452, 303)
	$graphics.DrawString('동관', $roomFont, $textBrush, 800, 303)
	$graphics.DrawString('북관', $roomFont, $mutedBrush, 480, 92)
	$graphics.DrawString('로비', $roomFont, $mutedBrush, 484, 518)
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

New-FloorPlanTexture -MapId 'M01' -AccentColor ([System.Drawing.Color]::FromArgb(255, 75, 210, 236)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 128, 162, 205))
New-FloorPlanTexture -MapId 'M02' -AccentColor ([System.Drawing.Color]::FromArgb(255, 130, 170, 255)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 173, 137, 226))
New-FloorPlanTexture -MapId 'M03' -AccentColor ([System.Drawing.Color]::FromArgb(255, 91, 232, 181)) -SecondaryColor ([System.Drawing.Color]::FromArgb(255, 103, 181, 209))

New-HeistLoopWave -FileName 'SW_HeistSuspenseLoop.wav' -Mode 'Suspense'
New-HeistLoopWave -FileName 'SW_HeistAlarmLoop.wav' -Mode 'Alarm'

Write-Host "Generated W7 presentation sources in $OutputDirectory"
