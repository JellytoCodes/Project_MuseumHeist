#include "Character/Components/HeistForgeryComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

#include <vector>

#if WITH_OPENCV
// IWYU pragma: begin_keep
#include "PreOpenCVHeaders.h"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/quality/qualityssim.hpp"
#include "PostOpenCVHeaders.h"
// IWYU pragma: end_keep
#endif

namespace
{
constexpr int32 MinimumSubmittedStrokePointCount = 2;
constexpr int32 MaximumSubmittedStrokeCount = 256;
constexpr int32 MaximumStrokePayloadBytes = 48 * 1024;
constexpr int32 ForgeryScoreGridResolution = 128;
constexpr uint8 ReferenceMaskThreshold = 128;
constexpr uint8 EmptyPaletteIndex = MAX_uint8;
constexpr float SubmissionTimeoutToleranceSeconds = 0.05f;
constexpr float BrushSizeValidationTolerance = 0.0001f;
constexpr int32 OpenCVMorphologyKernelSize = 3;
constexpr float OpenCVDistanceTolerancePixels = 10.0f;
constexpr float OpenCVStructuralColorWeight = 0.65f;
constexpr float OpenCVHistogramColorWeight = 0.35f;
constexpr float OpenCVShapeScoreExponent = 1.15f;
constexpr float OpenCVColorScoreExponent = 1.10f;
constexpr float PaintCompletenessExponent = 0.65f;
constexpr float OpenCVPaletteFidelityBonusWeight = 0.75f;
constexpr float OpenCVPaletteFidelityBonusExponent = 3.0f;

struct FOpenCVForgeryMetrics
{
	float ReferenceCoverage = 0.0f;
	float SubmittedPrecision = 0.0f;
	float BidirectionalShapeSimilarity = 0.0f;
	float MaskPrecision = 0.0f;
	float MaskRecall = 0.0f;
	float MaskIntersectionOverUnion = 0.0f;
	float MaskDiceSimilarity = 0.0f;
	float StructuralColorSimilarity = 0.0f;
	float HistogramColorSimilarity = 0.0f;
	float ColorSimilarity = 0.0f;
	int32 ReferencePixelCount = 0;
	int32 SubmittedPixelCount = 0;
};

uint8 ColorLuminance(const FColor& Color)
{
	return FMath::Max3(Color.R, Color.G, Color.B);
}

FColor DecodeRgb565(const uint16 Value)
{
	const uint8 Red = static_cast<uint8>(((Value >> 11) & 31) * 255 / 31);
	const uint8 Green = static_cast<uint8>(((Value >> 5) & 63) * 255 / 63);
	const uint8 Blue = static_cast<uint8>((Value & 31) * 255 / 31);
	return FColor(Red, Green, Blue, 255);
}

void DecodeDxtColorBlock(const uint8* BlockData, const bool bForceFourColor, FColor OutColors[16])
{
	const uint16 Color0 = static_cast<uint16>(BlockData[0] | (BlockData[1] << 8));
	const uint16 Color1 = static_cast<uint16>(BlockData[2] | (BlockData[3] << 8));
	FColor Palette[4];
	Palette[0] = DecodeRgb565(Color0);
	Palette[1] = DecodeRgb565(Color1);
	if (Color0 > Color1 || bForceFourColor)
	{
		Palette[2] = FColor(static_cast<uint8>((2 * Palette[0].R + Palette[1].R) / 3), static_cast<uint8>((2 * Palette[0].G + Palette[1].G) / 3),
							static_cast<uint8>((2 * Palette[0].B + Palette[1].B) / 3), 255);
		Palette[3] = FColor(static_cast<uint8>((Palette[0].R + 2 * Palette[1].R) / 3), static_cast<uint8>((Palette[0].G + 2 * Palette[1].G) / 3),
							static_cast<uint8>((Palette[0].B + 2 * Palette[1].B) / 3), 255);
	}
	else
	{
		Palette[2] = FColor(static_cast<uint8>((Palette[0].R + Palette[1].R) / 2), static_cast<uint8>((Palette[0].G + Palette[1].G) / 2), static_cast<uint8>((Palette[0].B + Palette[1].B) / 2), 255);
		Palette[3] = FColor(0, 0, 0, 0);
	}

	const uint32 Indices = static_cast<uint32>(BlockData[4]) | (static_cast<uint32>(BlockData[5]) << 8) | (static_cast<uint32>(BlockData[6]) << 16) | (static_cast<uint32>(BlockData[7]) << 24);
	for (int32 PixelIndex = 0; PixelIndex < 16; ++PixelIndex)
	{
		OutColors[PixelIndex] = Palette[(Indices >> (PixelIndex * 2)) & 3];
	}
}

void DecodeBcChannelBlock(const uint8* BlockData, uint8 OutValues[16])
{
	uint8 Palette[8];
	Palette[0] = BlockData[0];
	Palette[1] = BlockData[1];
	if (Palette[0] > Palette[1])
	{
		for (int32 Index = 1; Index <= 6; ++Index)
		{
			Palette[Index + 1] = static_cast<uint8>(((7 - Index) * Palette[0] + Index * Palette[1]) / 7);
		}
	}
	else
	{
		for (int32 Index = 1; Index <= 4; ++Index)
		{
			Palette[Index + 1] = static_cast<uint8>(((5 - Index) * Palette[0] + Index * Palette[1]) / 5);
		}
		Palette[6] = 0;
		Palette[7] = 255;
	}

	uint64 Indices = 0;
	for (int32 ByteIndex = 0; ByteIndex < 6; ++ByteIndex)
	{
		Indices |= static_cast<uint64>(BlockData[2 + ByteIndex]) << (ByteIndex * 8);
	}
	for (int32 PixelIndex = 0; PixelIndex < 16; ++PixelIndex)
	{
		OutValues[PixelIndex] = Palette[(Indices >> (PixelIndex * 3)) & 7];
	}
}

bool DecodePlatformTextureIntensity(UTexture2D* Texture, TArray<uint8>& OutIntensity, int32& OutWidth, int32& OutHeight, EPixelFormat& OutPixelFormat)
{
	OutIntensity.Reset();
	OutWidth = 0;
	OutHeight = 0;
	OutPixelFormat = PF_Unknown;
	if (!IsValid(Texture))
	{
		return false;
	}

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (PlatformData == nullptr || PlatformData->Mips.IsEmpty())
	{
		return false;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	OutWidth = Mip.SizeX;
	OutHeight = Mip.SizeY;
	OutPixelFormat = PlatformData->PixelFormat;
	if (OutWidth <= 0 || OutHeight <= 0)
	{
		return false;
	}

	const void* LockedData = Mip.BulkData.LockReadOnly();
	const int64 BulkDataBytes = Mip.BulkData.GetBulkDataSize();
	if (LockedData == nullptr || BulkDataBytes <= 0)
	{
		Mip.BulkData.Unlock();
		return false;
	}

	const uint8* Bytes = static_cast<const uint8*>(LockedData);
	OutIntensity.SetNumZeroed(OutWidth * OutHeight);
	bool bDecoded = true;
	switch (OutPixelFormat)
	{
	case PF_B8G8R8A8:
		if (BulkDataBytes < static_cast<int64>(OutWidth) * OutHeight * 4)
		{
			bDecoded = false;
			break;
		}
		for (int32 PixelIndex = 0; PixelIndex < OutWidth * OutHeight; ++PixelIndex)
		{
			OutIntensity[PixelIndex] = FMath::Max3(Bytes[PixelIndex * 4 + 2], Bytes[PixelIndex * 4 + 1], Bytes[PixelIndex * 4]);
		}
		break;
	case PF_R8G8B8A8:
		if (BulkDataBytes < static_cast<int64>(OutWidth) * OutHeight * 4)
		{
			bDecoded = false;
			break;
		}
		for (int32 PixelIndex = 0; PixelIndex < OutWidth * OutHeight; ++PixelIndex)
		{
			OutIntensity[PixelIndex] = FMath::Max3(Bytes[PixelIndex * 4], Bytes[PixelIndex * 4 + 1], Bytes[PixelIndex * 4 + 2]);
		}
		break;
	case PF_G8:
	case PF_R8:
	case PF_R8_UINT:
	case PF_L8:
		if (BulkDataBytes < static_cast<int64>(OutWidth) * OutHeight)
		{
			bDecoded = false;
			break;
		}
		FMemory::Memcpy(OutIntensity.GetData(), Bytes, static_cast<SIZE_T>(OutWidth) * OutHeight);
		break;
	case PF_DXT1:
	case PF_DXT5:
	case PF_BC4:
	case PF_BC5:
	{
		const int32 BlockBytes = OutPixelFormat == PF_DXT1 || OutPixelFormat == PF_BC4 ? 8 : 16;
		const int32 BlocksX = FMath::DivideAndRoundUp(OutWidth, 4);
		const int32 BlocksY = FMath::DivideAndRoundUp(OutHeight, 4);
		if (BulkDataBytes < static_cast<int64>(BlocksX) * BlocksY * BlockBytes)
		{
			bDecoded = false;
			break;
		}

		for (int32 BlockY = 0; BlockY < BlocksY; ++BlockY)
		{
			for (int32 BlockX = 0; BlockX < BlocksX; ++BlockX)
			{
				const uint8* Block = Bytes + (BlockY * BlocksX + BlockX) * BlockBytes;
				FColor Colors[16];
				uint8 ChannelValues[16];
				if (OutPixelFormat == PF_DXT1)
				{
					DecodeDxtColorBlock(Block, false, Colors);
				}
				else if (OutPixelFormat == PF_DXT5)
				{
					DecodeDxtColorBlock(Block + 8, true, Colors);
				}
				else
				{
					DecodeBcChannelBlock(Block, ChannelValues);
				}

				for (int32 LocalY = 0; LocalY < 4; ++LocalY)
				{
					for (int32 LocalX = 0; LocalX < 4; ++LocalX)
					{
						const int32 X = BlockX * 4 + LocalX;
						const int32 Y = BlockY * 4 + LocalY;
						if (X >= OutWidth || Y >= OutHeight)
						{
							continue;
						}
						const int32 LocalIndex = LocalY * 4 + LocalX;
						OutIntensity[Y * OutWidth + X] = OutPixelFormat == PF_DXT1 || OutPixelFormat == PF_DXT5 ? ColorLuminance(Colors[LocalIndex]) : ChannelValues[LocalIndex];
					}
				}
			}
		}
		break;
	}
	default:
		bDecoded = false;
		break;
	}

	Mip.BulkData.Unlock();
	if (!bDecoded)
	{
		OutIntensity.Reset();
		OutWidth = 0;
		OutHeight = 0;
	}
	return bDecoded;
}

bool DecodeReferenceMaskIntensity(UTexture2D* Texture, TArray<uint8>& OutIntensity, int32& OutWidth, int32& OutHeight, EPixelFormat& OutPixelFormat)
{
	if (DecodePlatformTextureIntensity(Texture, OutIntensity, OutWidth, OutHeight, OutPixelFormat))
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	FImage SourceImage;
	if (IsValid(Texture) && FImageUtils::GetTexture2DSourceImage(Texture, SourceImage) && SourceImage.SizeX > 0 && SourceImage.SizeY > 0)
	{
		SourceImage.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::Linear);
		const TArrayView64<FColor> Pixels = SourceImage.AsBGRA8();
		OutWidth = static_cast<int32>(SourceImage.SizeX);
		OutHeight = static_cast<int32>(SourceImage.SizeY);
		OutPixelFormat = PF_B8G8R8A8;
		OutIntensity.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 PixelIndex = 0; PixelIndex < OutIntensity.Num(); ++PixelIndex)
		{
			OutIntensity[PixelIndex] = ColorLuminance(Pixels[PixelIndex]);
		}
		return true;
	}
#endif
	return false;
}

bool DecodePlatformTextureColors(UTexture2D* Texture, TArray<FColor>& OutColors, int32& OutWidth, int32& OutHeight, EPixelFormat& OutPixelFormat)
{
	OutColors.Reset();
	OutWidth = 0;
	OutHeight = 0;
	OutPixelFormat = PF_Unknown;
	if (!IsValid(Texture))
	{
		return false;
	}

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (PlatformData == nullptr || PlatformData->Mips.IsEmpty())
	{
		return false;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	OutWidth = Mip.SizeX;
	OutHeight = Mip.SizeY;
	OutPixelFormat = PlatformData->PixelFormat;
	if (OutWidth <= 0 || OutHeight <= 0)
	{
		return false;
	}

	const void* LockedData = Mip.BulkData.LockReadOnly();
	const int64 BulkDataBytes = Mip.BulkData.GetBulkDataSize();
	if (LockedData == nullptr || BulkDataBytes <= 0)
	{
		Mip.BulkData.Unlock();
		return false;
	}

	const uint8* Bytes = static_cast<const uint8*>(LockedData);
	OutColors.SetNumUninitialized(OutWidth * OutHeight);
	bool bDecoded = true;
	switch (OutPixelFormat)
	{
	case PF_B8G8R8A8:
	case PF_R8G8B8A8:
		if (BulkDataBytes < static_cast<int64>(OutWidth) * OutHeight * 4)
		{
			bDecoded = false;
			break;
		}
		for (int32 PixelIndex = 0; PixelIndex < OutColors.Num(); ++PixelIndex)
		{
			const int32 ByteIndex = PixelIndex * 4;
			OutColors[PixelIndex] = OutPixelFormat == PF_B8G8R8A8 ? FColor(Bytes[ByteIndex + 2], Bytes[ByteIndex + 1], Bytes[ByteIndex], Bytes[ByteIndex + 3])
																  : FColor(Bytes[ByteIndex], Bytes[ByteIndex + 1], Bytes[ByteIndex + 2], Bytes[ByteIndex + 3]);
		}
		break;
	case PF_G8:
	case PF_R8:
	case PF_R8_UINT:
	case PF_L8:
		if (BulkDataBytes < static_cast<int64>(OutWidth) * OutHeight)
		{
			bDecoded = false;
			break;
		}
		for (int32 PixelIndex = 0; PixelIndex < OutColors.Num(); ++PixelIndex)
		{
			OutColors[PixelIndex] = FColor(Bytes[PixelIndex], Bytes[PixelIndex], Bytes[PixelIndex], 255);
		}
		break;
	case PF_DXT1:
	case PF_DXT5:
	case PF_BC4:
	case PF_BC5:
	{
		const int32 BlockBytes = OutPixelFormat == PF_DXT1 || OutPixelFormat == PF_BC4 ? 8 : 16;
		const int32 BlocksX = FMath::DivideAndRoundUp(OutWidth, 4);
		const int32 BlocksY = FMath::DivideAndRoundUp(OutHeight, 4);
		if (BulkDataBytes < static_cast<int64>(BlocksX) * BlocksY * BlockBytes)
		{
			bDecoded = false;
			break;
		}

		for (int32 BlockY = 0; BlockY < BlocksY; ++BlockY)
		{
			for (int32 BlockX = 0; BlockX < BlocksX; ++BlockX)
			{
				const uint8* Block = Bytes + (BlockY * BlocksX + BlockX) * BlockBytes;
				FColor Colors[16];
				uint8 FirstChannel[16];
				uint8 SecondChannel[16] = {};
				if (OutPixelFormat == PF_DXT1)
				{
					DecodeDxtColorBlock(Block, false, Colors);
				}
				else if (OutPixelFormat == PF_DXT5)
				{
					DecodeDxtColorBlock(Block + 8, true, Colors);
				}
				else
				{
					DecodeBcChannelBlock(Block, FirstChannel);
					if (OutPixelFormat == PF_BC5)
					{
						DecodeBcChannelBlock(Block + 8, SecondChannel);
					}
				}

				for (int32 LocalY = 0; LocalY < 4; ++LocalY)
				{
					for (int32 LocalX = 0; LocalX < 4; ++LocalX)
					{
						const int32 X = BlockX * 4 + LocalX;
						const int32 Y = BlockY * 4 + LocalY;
						if (X >= OutWidth || Y >= OutHeight)
						{
							continue;
						}

						const int32 LocalIndex = LocalY * 4 + LocalX;
						OutColors[Y * OutWidth + X] = OutPixelFormat == PF_DXT1 || OutPixelFormat == PF_DXT5 ? Colors[LocalIndex]
													  : OutPixelFormat == PF_BC5							 ? FColor(FirstChannel[LocalIndex], SecondChannel[LocalIndex], 0, 255)
																				 : FColor(FirstChannel[LocalIndex], FirstChannel[LocalIndex], FirstChannel[LocalIndex], 255);
					}
				}
			}
		}
		break;
	}
	default:
		bDecoded = false;
		break;
	}

	Mip.BulkData.Unlock();
	if (!bDecoded)
	{
		OutColors.Reset();
		OutWidth = 0;
		OutHeight = 0;
	}
	return bDecoded;
}

bool DecodeReferenceImageColors(UTexture2D* Texture, TArray<FColor>& OutColors, int32& OutWidth, int32& OutHeight, EPixelFormat& OutPixelFormat)
{
	if (DecodePlatformTextureColors(Texture, OutColors, OutWidth, OutHeight, OutPixelFormat))
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	FImage SourceImage;
	if (IsValid(Texture) && FImageUtils::GetTexture2DSourceImage(Texture, SourceImage) && SourceImage.SizeX > 0 && SourceImage.SizeY > 0)
	{
		SourceImage.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const TArrayView64<FColor> Pixels = SourceImage.AsBGRA8();
		OutWidth = static_cast<int32>(SourceImage.SizeX);
		OutHeight = static_cast<int32>(SourceImage.SizeY);
		OutPixelFormat = PF_B8G8R8A8;
		OutColors.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 PixelIndex = 0; PixelIndex < OutColors.Num(); ++PixelIndex)
		{
			OutColors[PixelIndex] = Pixels[PixelIndex];
		}
		return true;
	}
#endif
	return false;
}

void BuildLowResolutionReferenceMask(const TArray<uint8>& SourceIntensity, const int32 SourceWidth, const int32 SourceHeight, TArray<uint8>& OutMask)
{
	OutMask.SetNumZeroed(ForgeryScoreGridResolution * ForgeryScoreGridResolution);
	for (int32 GridY = 0; GridY < ForgeryScoreGridResolution; ++GridY)
	{
		const int32 SourceMinY = GridY * SourceHeight / ForgeryScoreGridResolution;
		const int32 SourceMaxY = FMath::Max(SourceMinY + 1, (GridY + 1) * SourceHeight / ForgeryScoreGridResolution);
		for (int32 GridX = 0; GridX < ForgeryScoreGridResolution; ++GridX)
		{
			const int32 SourceMinX = GridX * SourceWidth / ForgeryScoreGridResolution;
			const int32 SourceMaxX = FMath::Max(SourceMinX + 1, (GridX + 1) * SourceWidth / ForgeryScoreGridResolution);
			uint8 MaximumIntensity = 0;
			for (int32 SourceY = SourceMinY; SourceY < FMath::Min(SourceMaxY, SourceHeight); ++SourceY)
			{
				for (int32 SourceX = SourceMinX; SourceX < FMath::Min(SourceMaxX, SourceWidth); ++SourceX)
				{
					MaximumIntensity = FMath::Max(MaximumIntensity, SourceIntensity[SourceY * SourceWidth + SourceX]);
				}
			}
			OutMask[GridY * ForgeryScoreGridResolution + GridX] = MaximumIntensity >= ReferenceMaskThreshold ? 1 : 0;
		}
	}
}

void BuildLowResolutionBackgroundFilteredMask(const TArray<FColor>& SourceColors, const int32 SourceWidth, const int32 SourceHeight, const EHeistForgeryBackgroundFilter FilterMode,
											  const float BackgroundColorTolerance, TArray<uint8>& OutMask)
{
	OutMask.SetNumZeroed(ForgeryScoreGridResolution * ForgeryScoreGridResolution);
	const float ClampedTolerance = FMath::Clamp(BackgroundColorTolerance, 0.0f, 0.49f);
	const float BlackThreshold = ClampedTolerance * 255.0f;
	const float WhiteThreshold = (1.0f - ClampedTolerance) * 255.0f;

	for (int32 GridY = 0; GridY < ForgeryScoreGridResolution; ++GridY)
	{
		const int32 SourceMinY = GridY * SourceHeight / ForgeryScoreGridResolution;
		const int32 SourceMaxY = FMath::Max(SourceMinY + 1, (GridY + 1) * SourceHeight / ForgeryScoreGridResolution);
		for (int32 GridX = 0; GridX < ForgeryScoreGridResolution; ++GridX)
		{
			const int32 SourceMinX = GridX * SourceWidth / ForgeryScoreGridResolution;
			const int32 SourceMaxX = FMath::Max(SourceMinX + 1, (GridX + 1) * SourceWidth / ForgeryScoreGridResolution);
			uint64 RedTotal = 0;
			uint64 GreenTotal = 0;
			uint64 BlueTotal = 0;
			int32 SampleCount = 0;
			for (int32 SourceY = SourceMinY; SourceY < FMath::Min(SourceMaxY, SourceHeight); ++SourceY)
			{
				for (int32 SourceX = SourceMinX; SourceX < FMath::Min(SourceMaxX, SourceWidth); ++SourceX)
				{
					const FColor& Color = SourceColors[SourceY * SourceWidth + SourceX];
					RedTotal += Color.R;
					GreenTotal += Color.G;
					BlueTotal += Color.B;
					++SampleCount;
				}
			}

			if (SampleCount <= 0)
			{
				continue;
			}

			const float AverageRed = static_cast<float>(RedTotal) / SampleCount;
			const float AverageGreen = static_cast<float>(GreenTotal) / SampleCount;
			const float AverageBlue = static_cast<float>(BlueTotal) / SampleCount;
			const bool bForeground = FilterMode == EHeistForgeryBackgroundFilter::Black	  ? FMath::Max3(AverageRed, AverageGreen, AverageBlue) > BlackThreshold
									 : FilterMode == EHeistForgeryBackgroundFilter::White ? FMath::Min3(AverageRed, AverageGreen, AverageBlue) < WhiteThreshold
																						  : false;
			OutMask[GridY * ForgeryScoreGridResolution + GridX] = bForeground ? 1 : 0;
		}
	}
}

uint8 FindNearestPaletteIndex(const FColor& Color, const TArray<FLinearColor>& Palette)
{
	int32 BestIndex = 0;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const FLinearColor LinearColor = FLinearColor::FromSRGBColor(Color);
	for (int32 PaletteIndex = 0; PaletteIndex < Palette.Num(); ++PaletteIndex)
	{
		const FVector3f Delta(LinearColor.R - Palette[PaletteIndex].R, LinearColor.G - Palette[PaletteIndex].G, LinearColor.B - Palette[PaletteIndex].B);
		const float DistanceSquared = Delta.SizeSquared();
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestIndex = PaletteIndex;
		}
	}
	return static_cast<uint8>(BestIndex);
}

void BuildLowResolutionReferencePaletteMap(const TArray<FColor>& SourceColors, const int32 SourceWidth, const int32 SourceHeight, const TArray<uint8>& ReferenceMask,
										   const TArray<FLinearColor>& Palette, TArray<uint8>& OutPaletteMap)
{
	OutPaletteMap.Init(EmptyPaletteIndex, ForgeryScoreGridResolution * ForgeryScoreGridResolution);
	for (int32 GridY = 0; GridY < ForgeryScoreGridResolution; ++GridY)
	{
		for (int32 GridX = 0; GridX < ForgeryScoreGridResolution; ++GridX)
		{
			const int32 GridIndex = GridY * ForgeryScoreGridResolution + GridX;
			if (!ReferenceMask.IsValidIndex(GridIndex) || ReferenceMask[GridIndex] == 0)
			{
				continue;
			}

			const int32 SourceX = FMath::Clamp((GridX * SourceWidth + ForgeryScoreGridResolution / 2) / ForgeryScoreGridResolution, 0, SourceWidth - 1);
			const int32 SourceY = FMath::Clamp((GridY * SourceHeight + ForgeryScoreGridResolution / 2) / ForgeryScoreGridResolution, 0, SourceHeight - 1);
			OutPaletteMap[GridIndex] = FindNearestPaletteIndex(SourceColors[SourceY * SourceWidth + SourceX], Palette);
		}
	}
}

void StampForgeryPaletteBrush(TArray<uint8>& PlayerPaletteMap, const FVector2D& NormalizedPoint, const float BrushSize, const uint8 PaletteIndex)
{
	const float CenterX = FMath::Clamp(NormalizedPoint.X, 0.0, 1.0) * (ForgeryScoreGridResolution - 1);
	const float CenterY = FMath::Clamp(NormalizedPoint.Y, 0.0, 1.0) * (ForgeryScoreGridResolution - 1);
	const float RadiusPixels = FMath::Max(1.0f, BrushSize * ForgeryScoreGridResolution * 0.5f);
	const int32 MinimumX = FMath::Max(0, FMath::FloorToInt(CenterX - RadiusPixels));
	const int32 MaximumX = FMath::Min(ForgeryScoreGridResolution - 1, FMath::CeilToInt(CenterX + RadiusPixels));
	const int32 MinimumY = FMath::Max(0, FMath::FloorToInt(CenterY - RadiusPixels));
	const int32 MaximumY = FMath::Min(ForgeryScoreGridResolution - 1, FMath::CeilToInt(CenterY + RadiusPixels));
	const float RadiusSquared = FMath::Square(RadiusPixels);
	for (int32 Y = MinimumY; Y <= MaximumY; ++Y)
	{
		for (int32 X = MinimumX; X <= MaximumX; ++X)
		{
			if (FMath::Square(X - CenterX) + FMath::Square(Y - CenterY) <= RadiusSquared)
			{
				PlayerPaletteMap[Y * ForgeryScoreGridResolution + X] = PaletteIndex;
			}
		}
	}
}

void RasterizeForgeryPaletteStrokes(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices, const float BrushSize,
									TArray<uint8>& OutPlayerPaletteMap)
{
	OutPlayerPaletteMap.Init(EmptyPaletteIndex, ForgeryScoreGridResolution * ForgeryScoreGridResolution);
	int32 PointOffset = 0;
	for (int32 StrokeIndex = 0; StrokeIndex < StrokePointCounts.Num(); ++StrokeIndex)
	{
		const int32 StrokePointCount = StrokePointCounts[StrokeIndex];
		if (StrokePointCount <= 0 || PointOffset + StrokePointCount > NormalizedPoints.Num() || !StrokePaletteIndices.IsValidIndex(StrokeIndex))
		{
			break;
		}

		const uint8 PaletteIndex = StrokePaletteIndices[StrokeIndex];
		StampForgeryPaletteBrush(OutPlayerPaletteMap, NormalizedPoints[PointOffset], BrushSize, PaletteIndex);
		for (int32 StrokePointIndex = 1; StrokePointIndex < StrokePointCount; ++StrokePointIndex)
		{
			const FVector2D& Start = NormalizedPoints[PointOffset + StrokePointIndex - 1];
			const FVector2D& End = NormalizedPoints[PointOffset + StrokePointIndex];
			const int32 SampleCount = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(Start, End) * ForgeryScoreGridResolution * 2.0f));
			for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
			{
				StampForgeryPaletteBrush(OutPlayerPaletteMap, FMath::Lerp(Start, End, static_cast<float>(SampleIndex) / SampleCount), BrushSize, PaletteIndex);
			}
		}
		PointOffset += StrokePointCount;
	}
}

#if WITH_OPENCV
bool BuildOpenCVBinaryMasks(const TArray<uint8>& ReferenceMask, const TArray<uint8>& SubmittedPaletteMap, cv::Mat& OutReferenceMask, cv::Mat& OutSubmittedMask)
{
	const int32 ExpectedPixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	if (ReferenceMask.Num() != ExpectedPixelCount || SubmittedPaletteMap.Num() != ExpectedPixelCount)
	{
		return false;
	}

	OutReferenceMask = cv::Mat::zeros(ForgeryScoreGridResolution, ForgeryScoreGridResolution, CV_8UC1);
	OutSubmittedMask = cv::Mat::zeros(ForgeryScoreGridResolution, ForgeryScoreGridResolution, CV_8UC1);
	for (int32 PixelIndex = 0; PixelIndex < ExpectedPixelCount; ++PixelIndex)
	{
		OutReferenceMask.data[PixelIndex] = ReferenceMask[PixelIndex] != 0 ? 255 : 0;
		OutSubmittedMask.data[PixelIndex] = SubmittedPaletteMap[PixelIndex] != EmptyPaletteIndex ? 255 : 0;
	}
	return true;
}

bool BuildOpenCVPaletteImages(const TArray<uint8>& ReferencePaletteMap, const TArray<uint8>& SubmittedPaletteMap, const TArray<FLinearColor>& Palette, cv::Mat& OutReferenceBgr,
							  cv::Mat& OutSubmittedBgr, cv::Mat& OutReferenceHistogram, cv::Mat& OutSubmittedHistogram)
{
	const int32 ExpectedPixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	if (ReferencePaletteMap.Num() != ExpectedPixelCount || SubmittedPaletteMap.Num() != ExpectedPixelCount || !FMath::IsWithinInclusive(Palette.Num(), 2, 8))
	{
		return false;
	}

	const cv::Scalar NeutralBackground(127, 127, 127);
	OutReferenceBgr = cv::Mat(ForgeryScoreGridResolution, ForgeryScoreGridResolution, CV_8UC3, NeutralBackground);
	OutSubmittedBgr = cv::Mat(ForgeryScoreGridResolution, ForgeryScoreGridResolution, CV_8UC3, NeutralBackground);
	OutReferenceHistogram = cv::Mat::zeros(1, Palette.Num(), CV_32F);
	OutSubmittedHistogram = cv::Mat::zeros(1, Palette.Num(), CV_32F);

	for (int32 PixelIndex = 0; PixelIndex < ExpectedPixelCount; ++PixelIndex)
	{
		const uint8 ReferencePaletteIndex = ReferencePaletteMap[PixelIndex];
		const uint8 SubmittedPaletteIndex = SubmittedPaletteMap[PixelIndex];
		if (ReferencePaletteIndex != EmptyPaletteIndex)
		{
			if (!Palette.IsValidIndex(ReferencePaletteIndex))
			{
				return false;
			}
			const FColor Color = Palette[ReferencePaletteIndex].ToFColorSRGB();
			OutReferenceBgr.at<cv::Vec3b>(PixelIndex / ForgeryScoreGridResolution, PixelIndex % ForgeryScoreGridResolution) = cv::Vec3b(Color.B, Color.G, Color.R);
			OutReferenceHistogram.at<float>(0, ReferencePaletteIndex) += 1.0f;
		}

		if (SubmittedPaletteIndex != EmptyPaletteIndex)
		{
			if (!Palette.IsValidIndex(SubmittedPaletteIndex))
			{
				return false;
			}
			const FColor Color = Palette[SubmittedPaletteIndex].ToFColorSRGB();
			OutSubmittedBgr.at<cv::Vec3b>(PixelIndex / ForgeryScoreGridResolution, PixelIndex % ForgeryScoreGridResolution) = cv::Vec3b(Color.B, Color.G, Color.R);
			OutSubmittedHistogram.at<float>(0, SubmittedPaletteIndex) += 1.0f;
		}
	}

	cv::normalize(OutReferenceHistogram, OutReferenceHistogram, 1.0, 0.0, cv::NORM_L1);
	cv::normalize(OutSubmittedHistogram, OutSubmittedHistogram, 1.0, 0.0, cv::NORM_L1);
	return true;
}

float CalculateOpenCVDistanceSimilarity(const cv::Mat& SourceMask, const cv::Mat& TargetMask)
{
	cv::Mat InvertedTargetMask;
	cv::bitwise_not(TargetMask, InvertedTargetMask);
	cv::Mat DistanceMap;
	cv::distanceTransform(InvertedTargetMask, DistanceMap, cv::DIST_L2, cv::DIST_MASK_5);

	double SimilarityTotal = 0.0;
	int32 SampleCount = 0;
	for (int32 Y = 0; Y < SourceMask.rows; ++Y)
	{
		const uint8* SourceRow = SourceMask.ptr<uint8>(Y);
		const float* DistanceRow = DistanceMap.ptr<float>(Y);
		for (int32 X = 0; X < SourceMask.cols; ++X)
		{
			if (SourceRow[X] == 0)
			{
				continue;
			}

			const float NormalizedDistance = DistanceRow[X] / OpenCVDistanceTolerancePixels;
			SimilarityTotal += 1.0 / (1.0 + NormalizedDistance * NormalizedDistance);
			++SampleCount;
		}
	}
	return SampleCount > 0 ? static_cast<float>(SimilarityTotal / SampleCount) : 0.0f;
}

bool CalculateOpenCVForgeryMetrics(const TArray<uint8>& ReferenceMask, const TArray<uint8>& ReferencePaletteMap, const TArray<uint8>& SubmittedPaletteMap, const TArray<FLinearColor>& Palette,
								   FOpenCVForgeryMetrics& OutMetrics)
{
	OutMetrics = FOpenCVForgeryMetrics();
	cv::Mat ReferenceBinaryMask;
	cv::Mat SubmittedBinaryMask;
	if (!BuildOpenCVBinaryMasks(ReferenceMask, SubmittedPaletteMap, ReferenceBinaryMask, SubmittedBinaryMask))
	{
		return false;
	}

	OutMetrics.ReferencePixelCount = cv::countNonZero(ReferenceBinaryMask);
	OutMetrics.SubmittedPixelCount = cv::countNonZero(SubmittedBinaryMask);
	if (OutMetrics.ReferencePixelCount <= 0 || OutMetrics.SubmittedPixelCount <= 0)
	{
		return false;
	}

	const cv::Mat MorphologyKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(OpenCVMorphologyKernelSize, OpenCVMorphologyKernelSize));
	cv::Mat ReferenceShapeMask;
	cv::Mat SubmittedShapeMask;
	cv::morphologyEx(ReferenceBinaryMask, ReferenceShapeMask, cv::MORPH_CLOSE, MorphologyKernel);
	cv::morphologyEx(SubmittedBinaryMask, SubmittedShapeMask, cv::MORPH_CLOSE, MorphologyKernel);

	OutMetrics.ReferenceCoverage = CalculateOpenCVDistanceSimilarity(ReferenceShapeMask, SubmittedShapeMask);
	OutMetrics.SubmittedPrecision = CalculateOpenCVDistanceSimilarity(SubmittedShapeMask, ReferenceShapeMask);
	const float ShapeSimilarityTotal = OutMetrics.ReferenceCoverage + OutMetrics.SubmittedPrecision;
	OutMetrics.BidirectionalShapeSimilarity = ShapeSimilarityTotal > UE_SMALL_NUMBER ? 2.0f * OutMetrics.ReferenceCoverage * OutMetrics.SubmittedPrecision / ShapeSimilarityTotal : 0.0f;

	cv::Mat IntersectionMask;
	cv::Mat UnionMask;
	cv::bitwise_and(ReferenceShapeMask, SubmittedShapeMask, IntersectionMask);
	cv::bitwise_or(ReferenceShapeMask, SubmittedShapeMask, UnionMask);
	const int32 ReferenceShapePixels = cv::countNonZero(ReferenceShapeMask);
	const int32 SubmittedShapePixels = cv::countNonZero(SubmittedShapeMask);
	const int32 IntersectionPixels = cv::countNonZero(IntersectionMask);
	const int32 UnionPixels = cv::countNonZero(UnionMask);
	OutMetrics.MaskPrecision = SubmittedShapePixels > 0 ? static_cast<float>(IntersectionPixels) / SubmittedShapePixels : 0.0f;
	OutMetrics.MaskRecall = ReferenceShapePixels > 0 ? static_cast<float>(IntersectionPixels) / ReferenceShapePixels : 0.0f;
	OutMetrics.MaskIntersectionOverUnion = UnionPixels > 0 ? static_cast<float>(IntersectionPixels) / UnionPixels : 0.0f;
	const int32 ShapePixelTotal = ReferenceShapePixels + SubmittedShapePixels;
	OutMetrics.MaskDiceSimilarity = ShapePixelTotal > 0 ? 2.0f * IntersectionPixels / ShapePixelTotal : 0.0f;

	cv::Mat ReferenceBgr;
	cv::Mat SubmittedBgr;
	cv::Mat ReferenceHistogram;
	cv::Mat SubmittedHistogram;
	if (!BuildOpenCVPaletteImages(ReferencePaletteMap, SubmittedPaletteMap, Palette, ReferenceBgr, SubmittedBgr, ReferenceHistogram, SubmittedHistogram))
	{
		return false;
	}

	std::vector<cv::Point> ForegroundPoints;
	cv::findNonZero(UnionMask, ForegroundPoints);
	if (ForegroundPoints.empty())
	{
		return false;
	}

	cv::Rect ComparisonBounds = cv::boundingRect(ForegroundPoints);
	constexpr int32 ComparisonPaddingPixels = 4;
	ComparisonBounds.x = FMath::Max(0, ComparisonBounds.x - ComparisonPaddingPixels);
	ComparisonBounds.y = FMath::Max(0, ComparisonBounds.y - ComparisonPaddingPixels);
	ComparisonBounds.width = FMath::Min(ForgeryScoreGridResolution - ComparisonBounds.x, ComparisonBounds.width + ComparisonPaddingPixels * 2);
	ComparisonBounds.height = FMath::Min(ForgeryScoreGridResolution - ComparisonBounds.y, ComparisonBounds.height + ComparisonPaddingPixels * 2);

	cv::Mat ReferenceLab;
	cv::Mat SubmittedLab;
	cv::cvtColor(ReferenceBgr(ComparisonBounds), ReferenceLab, cv::COLOR_BGR2Lab);
	cv::cvtColor(SubmittedBgr(ComparisonBounds), SubmittedLab, cv::COLOR_BGR2Lab);
	cv::Mat StructuralSimilarityMap;
	cv::quality::QualitySSIM::compute(ReferenceLab, SubmittedLab, StructuralSimilarityMap);
	const cv::Scalar StructuralSimilarity = cv::mean(StructuralSimilarityMap, UnionMask(ComparisonBounds));
	OutMetrics.StructuralColorSimilarity = FMath::Clamp(static_cast<float>((StructuralSimilarity[0] + StructuralSimilarity[1] + StructuralSimilarity[2]) / 3.0), 0.0f, 1.0f);
	OutMetrics.HistogramColorSimilarity = FMath::Clamp(1.0f - static_cast<float>(cv::compareHist(ReferenceHistogram, SubmittedHistogram, cv::HISTCMP_BHATTACHARYYA)), 0.0f, 1.0f);
	OutMetrics.ColorSimilarity =
		FMath::Clamp(FMath::Pow(OutMetrics.StructuralColorSimilarity, OpenCVStructuralColorWeight) * FMath::Pow(OutMetrics.HistogramColorSimilarity, OpenCVHistogramColorWeight), 0.0f, 1.0f);
	return true;
}
#endif

}

UHeistForgeryComponent::UHeistForgeryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeistForgeryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bSessionActive)
	{
		ClearSession(FName(TEXT("OwnerEndPlay")), true);
	}
	else
	{
		UnbindActiveDisplayCase();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool UHeistForgeryComponent::TryBeginForgerySession(AHeistPaintingDisplayCaseActor* TargetDisplayCase, const float DurationSeconds)
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=InvalidAuthorityContext"), *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (bSessionActive || bSubmitPending || IsValid(ActiveDisplayCase.Get()))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s ActiveCase=%s Reason=SessionAlreadyActive"), *GetNameSafe(HeistCharacter),
			   *GetNameSafe(TargetDisplayCase), *GetNameSafe(ActiveDisplayCase.Get()));
		return false;
	}

	float IgnoredObservationDuration = 0.0f;
	if ((!bTemplatePrepared || PreparedDisplayCase.Get() != TargetDisplayCase) && !TryPrepareForgeryTemplate(TargetDisplayCase, IgnoredObservationDuration))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=TemplatePreparationFailed"), *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (!TargetDisplayCase->IsSessionLocked() || TargetDisplayCase->GetSessionOwner() != HeistPlayerState)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s CaseOwner=%s Reason=CaseOwnershipMismatch"), *GetNameSafe(HeistCharacter),
			   *GetNameSafe(TargetDisplayCase), *GetNameSafe(TargetDisplayCase->GetSessionOwner()));
		return false;
	}

	if (TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Observed)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s CaseState=%s Reason=CaseNotObserved"), *GetNameSafe(HeistCharacter),
			   *GetNameSafe(TargetDisplayCase), *UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState()));
		return false;
	}

	if (HeistPlayerState->IsArrested() || HeistPlayerState->IsEscaped())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=PlayerStateBlocked"), *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	const UHeistInventoryComponent* InventoryComponent = HeistCharacter->GetInventoryComponent();
	if (IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=InventoryOpen"), *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (FVector::DistSquared(HeistCharacter->GetActorLocation(), TargetDisplayCase->GetActorLocation()) > FMath::Square(TargetDisplayCase->GetMaximumSessionDistance()))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=OutOfRange"), *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (!TargetDisplayCase->TryTransitionToDisplayCaseState(EHeistDisplayCaseState::ForgeryInProgress))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=CaseTransitionRejected"), *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	ActiveDisplayCase = TargetDisplayCase;
	ActiveDisplayCase->OnDisplayCaseSessionChanged.AddDynamic(this, &UHeistForgeryComponent::HandleDisplayCaseSessionChanged);
	ResetStrokeTransportState(true);
	ResetForgeryScoreState();
	bSessionActive = true;
	bSubmitPending = false;
	LastCleanupReason = NAME_None;

	const float SafeDurationSeconds = DurationSeconds > 0.0f ? DurationSeconds : FMath::Max(1.0f, TemplateForgeryDuration > 0.0f ? TemplateForgeryDuration : DefaultSessionDurationSeconds);
	ActiveSessionDurationSeconds = SafeDurationSeconds;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	SessionEndServerTime = ServerWorldTime + SafeDurationSeconds;
	++SessionRevision;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SessionTimeoutTimerHandle, this, &UHeistForgeryComponent::HandleSessionTimeout, SafeDurationSeconds, false);
	}

	HeistCharacter->ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("ServerBegin"), FName(TEXT("BeginAccepted")));
	return true;
}

bool UHeistForgeryComponent::TryPrepareForgeryTemplate(AHeistPaintingDisplayCaseActor* TargetDisplayCase, float& OutObservationDuration)
{
	OutObservationDuration = 0.0f;
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(TargetDisplayCase) || bSessionActive || bSubmitPending)
	{
		return false;
	}

	if (bTemplatePrepared && PreparedDisplayCase.Get() == TargetDisplayCase)
	{
		OutObservationDuration = TemplateObservationDuration;
		return true;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	FHeistForgeryTemplateRow TemplateDefinition;
	const FName ArtifactId = TargetDisplayCase->GetTargetArtifactId();
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(ArtifactId, ArtifactDefinition) || ArtifactDefinition.ForgeryType != EHeistForgeryType::Drawing ||
		ArtifactDefinition.ForgeryTemplateId.IsNone() || !HeistGameMode->TryGetForgeryTemplateDefinition(ArtifactDefinition.ForgeryTemplateId, TemplateDefinition))
	{
		UE_LOG(LogHeist, Error, TEXT("Forgery template preparation rejected: Character=%s Case=%s Artifact=%s Reason=ArtifactOrTemplateLookupFailed"), *GetNameSafe(HeistCharacter),
			   *GetNameSafe(TargetDisplayCase), *ArtifactId.ToString());
		return false;
	}

	if (!FMath::IsWithinInclusive(TemplateDefinition.AllowedPalette.Num(), 2, 8) || TemplateDefinition.ShapeAccuracyWeight + TemplateDefinition.ColorAccuracyWeight <= 0.0f ||
		TemplateDefinition.MaximumPaintToReferenceRatio < 1.0f || !FMath::IsWithinInclusive(TemplateDefinition.BackgroundColorTolerance, 0.0f, 0.49f) ||
		!FMath::IsWithinInclusive(TemplateDefinition.OverpaintScoreCap, 0.0f, 100.0f))
	{
		UE_LOG(LogHeist, Error, TEXT("Forgery template preparation rejected: Character=%s Case=%s Artifact=%s Template=%s PaletteColors=%d Reason=InvalidPaletteScoreContract"),
			   *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase), *ArtifactId.ToString(), *TemplateDefinition.TemplateId.ToString(), TemplateDefinition.AllowedPalette.Num());
		return false;
	}

	UTexture2D* LoadedReferenceImage = TemplateDefinition.ReferenceImage.LoadSynchronous();
	UTexture2D* LoadedReferenceMask = nullptr;
	if (TemplateDefinition.BackgroundFilterMode == EHeistForgeryBackgroundFilter::None)
	{
		LoadedReferenceMask = TemplateDefinition.ReferenceMask.LoadSynchronous();
	}
	if (!IsValid(LoadedReferenceImage) || (TemplateDefinition.BackgroundFilterMode == EHeistForgeryBackgroundFilter::None && !IsValid(LoadedReferenceMask)))
	{
		UE_LOG(LogHeist, Error, TEXT("Forgery template preparation rejected: Character=%s Case=%s Artifact=%s Template=%s ReferenceImage=%s ReferenceMask=%s Reason=ReferenceAssetLoadFailed"),
			   *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase), *ArtifactId.ToString(), *TemplateDefinition.TemplateId.ToString(),
			   *TemplateDefinition.ReferenceImage.ToSoftObjectPath().ToString(), *TemplateDefinition.ReferenceMask.ToSoftObjectPath().ToString());
		return false;
	}

	ResetPreparedTemplateSnapshot();
	PreparedDisplayCase = TargetDisplayCase;
	bTemplatePrepared = true;
	ActiveArtifactId = ArtifactDefinition.ArtifactId;
	ActiveTemplateId = TemplateDefinition.TemplateId;
	ReferenceImageAsset = TemplateDefinition.ReferenceImage;
	ReferenceMaskAsset = TemplateDefinition.ReferenceMask;
	TemplateObservationDuration = TemplateDefinition.ObservationDuration;
	TemplateForgeryDuration = TemplateDefinition.ForgeryDuration;
	TemplateStrokeLimit = TemplateDefinition.StrokeLimit;
	TemplateBrushSize = TemplateDefinition.BrushSize;
	TemplateAllowedPalette = TemplateDefinition.AllowedPalette;
	TemplateBackgroundFilterMode = TemplateDefinition.BackgroundFilterMode;
	TemplateBackgroundColorTolerance = TemplateDefinition.BackgroundColorTolerance;
	TemplateCoverageWeight = TemplateDefinition.CoverageWeight;
	TemplateMajorShapeWeight = TemplateDefinition.MajorShapeWeight;
	TemplateExtraStrokePenaltyWeight = TemplateDefinition.ExtraStrokePenaltyWeight;
	TemplateTimeoutPenalty = TemplateDefinition.TimeoutPenalty;
	TemplateShapeAccuracyWeight = TemplateDefinition.ShapeAccuracyWeight;
	TemplateColorAccuracyWeight = TemplateDefinition.ColorAccuracyWeight;
	TemplateMaximumPaintToReferenceRatio = TemplateDefinition.MaximumPaintToReferenceRatio;
	TemplateOverpaintScoreCap = TemplateDefinition.OverpaintScoreCap;
	OutObservationDuration = TemplateObservationDuration;
	LastCleanupReason = NAME_None;
	++SessionRevision;
	HeistCharacter->ForceNetUpdate();

	UE_LOG(
		LogHeist, Log,
		TEXT(
			"Forgery template prepared: Character=%s Case=%s Artifact=%s Template=%s ReferenceImage=%s ReferenceMask=%s BackgroundFilter=%s BackgroundTolerance=%.3f PaletteColors=%d Observation=%.2f Forgery=%.2f StrokeLimit=%d Brush=%.4f CoverageWeight=%.3f MajorShapeWeight=%.3f ExtraStrokePenaltyWeight=%.3f TimeoutPenalty=%.3f ShapeWeight=%.3f ColorWeight=%.3f MaxPaintRatio=%.2f OverpaintCap=%.2f Result=PASS"),
		*GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase), *ActiveArtifactId.ToString(), *ActiveTemplateId.ToString(), *ReferenceImageAsset.ToSoftObjectPath().ToString(),
		*ReferenceMaskAsset.ToSoftObjectPath().ToString(), *StaticEnum<EHeistForgeryBackgroundFilter>()->GetNameStringByValue(static_cast<int64>(TemplateBackgroundFilterMode)),
		TemplateBackgroundColorTolerance, TemplateAllowedPalette.Num(), TemplateObservationDuration, TemplateForgeryDuration, TemplateStrokeLimit, TemplateBrushSize, TemplateCoverageWeight,
		TemplateMajorShapeWeight, TemplateExtraStrokePenaltyWeight, TemplateTimeoutPenalty, TemplateShapeAccuracyWeight, TemplateColorAccuracyWeight, TemplateMaximumPaintToReferenceRatio,
		TemplateOverpaintScoreCap);
	BroadcastSessionSnapshot(TEXT("TemplatePrepared"), FName(TEXT("TemplateLoaded")));
	return true;
}

bool UHeistForgeryComponent::ClearPreparedForgeryTemplate(const FName Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bSessionActive || bSubmitPending || !bTemplatePrepared)
	{
		return false;
	}

	ResetPreparedTemplateSnapshot();
	LastCleanupReason = Reason;
	++SessionRevision;
	GetOwner()->ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("TemplateCleared"), Reason);
	return true;
}

bool UHeistForgeryComponent::TryBeginSubmit()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	FName RejectReason = NAME_None;
	if (!ValidateActiveSession(RejectReason) || bSubmitPending || !bHasValidatedStrokePayload)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Forgery submit rejected: Character=%s Case=%s SubmitPending=%s Reason=%s"), *GetNameSafe(GetOwner()), *GetNameSafe(ActiveDisplayCase.Get()),
			   bSubmitPending ? TEXT("true") : TEXT("false"),
			   bSubmitPending				 ? TEXT("DuplicateSubmit")
			   : !bHasValidatedStrokePayload ? TEXT("MissingValidatedStrokePayload")
											 : *RejectReason.ToString());
		return false;
	}

	bSubmitPending = true;
	++SessionRevision;
	GetOwner()->ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("ServerSubmit"), FName(TEXT("SubmitPending")));
	return true;
}

bool UHeistForgeryComponent::TrySubmitStrokePayload(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
													const float ClientBrushSize, const int32 ClientSessionRevision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	FName RejectReason = NAME_None;
	int32 PayloadBytes = 0;
	if (!ValidateStrokePayload(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, ClientBrushSize, ClientSessionRevision, RejectReason, PayloadBytes))
	{
		RecordStrokeValidationResult(false, RejectReason);
		UE_LOG(
			LogHeistNetwork, Warning,
			TEXT(
				"Forgery stroke payload rejected: Character=%s Case=%s Strokes=%d Points=%d PayloadBytes=%d ClientBrush=%.4f ServerBrush=%.4f ClientSessionRevision=%d ServerSessionRevision=%d SubmitPending=%s Reason=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(ActiveDisplayCase.Get()), StrokePointCounts.Num(), NormalizedPoints.Num(), PayloadBytes, ClientBrushSize, TemplateBrushSize, ClientSessionRevision,
			SessionRevision, bSubmitPending ? TEXT("true") : TEXT("false"), *RejectReason.ToString());

		if (RejectReason == FName(TEXT("SessionExpired")))
		{
			ClearSession(FName(TEXT("Timeout")), true);
		}
		return false;
	}

	ValidatedStrokePoints = NormalizedPoints;
	ValidatedStrokePointCounts = StrokePointCounts;
	ValidatedStrokePaletteIndices = StrokePaletteIndices;
	ValidatedStrokeCount = StrokePointCounts.Num();
	ValidatedPointCount = NormalizedPoints.Num();
	ValidatedPayloadBytes = PayloadBytes;
	ValidatedBrushSize = ClientBrushSize;
	bHasValidatedStrokePayload = true;
	RecordStrokeValidationResult(true, FName(TEXT("Accepted")));

	if (!TryBeginSubmit())
	{
		ResetStrokeTransportState(false);
		RecordStrokeValidationResult(false, FName(TEXT("SubmitStateTransitionRejected")));
		return false;
	}

	if (!TryCalculateAndCommitForgeryScore())
	{
		bSubmitPending = false;
		ResetStrokeTransportState(false);
		RecordStrokeValidationResult(false, FName(TEXT("ScoreCalculationFailed")));
		++SessionRevision;
		GetOwner()->ForceNetUpdate();
		BroadcastSessionSnapshot(TEXT("ServerScoreRejected"), FName(TEXT("ScoreCalculationFailed")));
		return false;
	}

	AHeistPaintingDisplayCaseActor* SubmittedDisplayCase = ActiveDisplayCase.Get();
	UE_LOG(
		LogHeistNetwork, Log,
		TEXT(
			"Forgery stroke payload accepted: Character=%s Case=%s Strokes=%d Points=%d PayloadBytes=%d Brush=%.4f ValidatedForSessionRevision=%d SubmitPending=%s RenderTargetReplicated=false Authority=true Result=PASS"),
		*GetNameSafe(GetOwner()), *GetNameSafe(SubmittedDisplayCase), ValidatedStrokeCount, ValidatedPointCount, ValidatedPayloadBytes, ValidatedBrushSize, ClientSessionRevision,
		bSubmitPending ? TEXT("true") : TEXT("false"));
	CompleteSuccessfulForgerySession();
	return true;
}

bool UHeistForgeryComponent::CancelForgerySession(const FName Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || (!bSessionActive && !bSubmitPending && !IsValid(ActiveDisplayCase.Get())))
	{
		return false;
	}

	ClearSession(Reason.IsNone() ? FName(TEXT("OwnerCancelled")) : Reason, true);
	return true;
}

bool UHeistForgeryComponent::ForceTimeoutForDebug()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bSessionActive)
	{
		return false;
	}

	HandleSessionTimeout();
	return true;
}

bool UHeistForgeryComponent::ForceExpireSubmissionWindowForDebug()
{
#if !UE_BUILD_SHIPPING
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bSessionActive || bSubmitPending)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
		const AHeistGameState* HeistGameState = World->GetGameState<AHeistGameState>();
		const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
		SessionEndServerTime = ServerWorldTime - SubmissionTimeoutToleranceSeconds - 0.1f;
		GetOwner()->ForceNetUpdate();
		return true;
	}
#endif
	return false;
}

bool UHeistForgeryComponent::ForceNearExpirySubmissionWindowForDebug()
{
#if !UE_BUILD_SHIPPING
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bSessionActive || bSubmitPending)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		constexpr float NearExpiryWindowSeconds = 0.01f;
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
		const AHeistGameState* HeistGameState = World->GetGameState<AHeistGameState>();
		const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
		SessionEndServerTime = ServerWorldTime + NearExpiryWindowSeconds;
		World->GetTimerManager().SetTimer(SessionTimeoutTimerHandle, this, &UHeistForgeryComponent::HandleSessionTimeout, NearExpiryWindowSeconds, false);
		GetOwner()->ForceNetUpdate();
		return true;
	}
#endif
	return false;
}

bool UHeistForgeryComponent::IsSessionActive() const
{
	return bSessionActive;
}

bool UHeistForgeryComponent::IsSubmitPending() const
{
	return bSubmitPending;
}

float UHeistForgeryComponent::GetSessionEndServerTime() const
{
	return SessionEndServerTime;
}

int32 UHeistForgeryComponent::GetSessionRevision() const
{
	return SessionRevision;
}

AHeistPaintingDisplayCaseActor* UHeistForgeryComponent::GetActiveDisplayCase() const
{
	return ActiveDisplayCase.Get();
}

FName UHeistForgeryComponent::GetLastCleanupReason() const
{
	return LastCleanupReason;
}

bool UHeistForgeryComponent::HasPreparedForgeryTemplate() const
{
	return bTemplatePrepared;
}

FName UHeistForgeryComponent::GetActiveArtifactId() const
{
	return ActiveArtifactId;
}

FName UHeistForgeryComponent::GetActiveTemplateId() const
{
	return ActiveTemplateId;
}

const TSoftObjectPtr<UTexture2D>& UHeistForgeryComponent::GetReferenceImageAsset() const
{
	return ReferenceImageAsset;
}

const TSoftObjectPtr<UTexture2D>& UHeistForgeryComponent::GetReferenceMaskAsset() const
{
	return ReferenceMaskAsset;
}

UTexture2D* UHeistForgeryComponent::LoadReferenceImage() const
{
	return ReferenceImageAsset.LoadSynchronous();
}

UTexture2D* UHeistForgeryComponent::LoadReferenceMask() const
{
	return ReferenceMaskAsset.LoadSynchronous();
}

float UHeistForgeryComponent::GetTemplateObservationDuration() const
{
	return TemplateObservationDuration;
}

float UHeistForgeryComponent::GetTemplateForgeryDuration() const
{
	return TemplateForgeryDuration;
}

int32 UHeistForgeryComponent::GetTemplateStrokeLimit() const
{
	return TemplateStrokeLimit;
}

float UHeistForgeryComponent::GetTemplateBrushSize() const
{
	return TemplateBrushSize;
}

const TArray<FLinearColor>& UHeistForgeryComponent::GetTemplateAllowedPalette() const
{
	return TemplateAllowedPalette;
}

bool UHeistForgeryComponent::HasValidatedStrokePayload() const
{
	return bHasValidatedStrokePayload;
}

bool UHeistForgeryComponent::WasLastStrokeValidationAccepted() const
{
	return bLastStrokeValidationAccepted;
}

FName UHeistForgeryComponent::GetLastStrokeValidationReason() const
{
	return LastStrokeValidationReason;
}

int32 UHeistForgeryComponent::GetStrokeValidationRevision() const
{
	return StrokeValidationRevision;
}

int32 UHeistForgeryComponent::GetValidatedStrokeCount() const
{
	return ValidatedStrokeCount;
}

int32 UHeistForgeryComponent::GetValidatedPointCount() const
{
	return ValidatedPointCount;
}

int32 UHeistForgeryComponent::GetValidatedPayloadBytes() const
{
	return ValidatedPayloadBytes;
}

float UHeistForgeryComponent::GetValidatedBrushSize() const
{
	return ValidatedBrushSize;
}

const TArray<FVector2D>& UHeistForgeryComponent::GetValidatedStrokePoints() const
{
	return ValidatedStrokePoints;
}

const TArray<int32>& UHeistForgeryComponent::GetValidatedStrokePointCounts() const
{
	return ValidatedStrokePointCounts;
}

const TArray<uint8>& UHeistForgeryComponent::GetValidatedStrokePaletteIndices() const
{
	return ValidatedStrokePaletteIndices;
}

bool UHeistForgeryComponent::HasAuthoritativeForgeryResult() const
{
	return bHasAuthoritativeForgeryResult;
}

const FHeistForgeryResult& UHeistForgeryComponent::GetAuthoritativeForgeryResult() const
{
	return AuthoritativeForgeryResult;
}

int32 UHeistForgeryComponent::GetForgeryScoreRevision() const
{
	return ForgeryScoreRevision;
}

int32 UHeistForgeryComponent::GetForgeryScoreResolution() const
{
	return ForgeryScoreResolution;
}

int32 UHeistForgeryComponent::GetReferenceMaskPixelCount() const
{
	return ReferenceMaskPixelCount;
}

int32 UHeistForgeryComponent::GetSubmittedMaskPixelCount() const
{
	return SubmittedMaskPixelCount;
}

bool UHeistForgeryComponent::RecalculateValidatedForgeryScoreForDebug(FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const
{
#if !UE_BUILD_SHIPPING
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bHasValidatedStrokePayload)
	{
		return false;
	}

	return CalculateForgeryScore(ValidatedStrokePoints, ValidatedStrokePointCounts, ValidatedStrokePaletteIndices, ValidatedBrushSize, OutResult, OutReferenceMaskPixels, OutSubmittedMaskPixels);
#else
	return false;
#endif
}

bool UHeistForgeryComponent::CalculateLocalForgeryPreview(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
														  const float BrushSize, FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const
{
	if (!bSessionActive || bSubmitPending)
	{
		return false;
	}

	return CalculateForgeryScore(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, BrushSize, OutResult, OutReferenceMaskPixels, OutSubmittedMaskPixels);
}

FHeistForgerySessionStateChanged& UHeistForgeryComponent::GetSessionStateChangedDelegate()
{
	return SessionStateChangedDelegate;
}

bool UHeistForgeryComponent::ValidateStrokePayload(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
												   const float ClientBrushSize, const int32 ClientSessionRevision, FName& OutRejectReason, int32& OutPayloadBytes) const
{
	OutRejectReason = NAME_None;
	const int64 EstimatedPayloadBytes = static_cast<int64>(NormalizedPoints.Num()) * sizeof(FVector2D) + static_cast<int64>(StrokePointCounts.Num()) * sizeof(int32) +
										static_cast<int64>(StrokePaletteIndices.Num()) * sizeof(uint8) + sizeof(ClientBrushSize) + sizeof(ClientSessionRevision);
	OutPayloadBytes = EstimatedPayloadBytes > MAX_int32 ? MAX_int32 : static_cast<int32>(EstimatedPayloadBytes);

	if (bSubmitPending)
	{
		OutRejectReason = FName(TEXT("DuplicateSubmit"));
		return false;
	}

	FName SessionRejectReason = NAME_None;
	if (!ValidateActiveSession(SessionRejectReason))
	{
		OutRejectReason = SessionRejectReason;
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	if (SessionEndServerTime <= 0.0f || ServerWorldTime > SessionEndServerTime + SubmissionTimeoutToleranceSeconds)
	{
		OutRejectReason = FName(TEXT("SessionExpired"));
		return false;
	}

	if (ClientSessionRevision != SessionRevision)
	{
		OutRejectReason = FName(TEXT("SessionRevisionMismatch"));
		return false;
	}

	if (StrokePointCounts.IsEmpty() || NormalizedPoints.IsEmpty())
	{
		OutRejectReason = FName(TEXT("EmptyPayload"));
		return false;
	}
	if (StrokePaletteIndices.Num() != StrokePointCounts.Num())
	{
		OutRejectReason = FName(TEXT("PaletteLayoutMismatch"));
		return false;
	}
	if (StrokePointCounts.Num() > MaximumSubmittedStrokeCount)
	{
		OutRejectReason = FName(TEXT("StrokeCountLimit"));
		return false;
	}
	if (NormalizedPoints.Num() > TemplateStrokeLimit)
	{
		OutRejectReason = FName(TEXT("PointCountLimit"));
		return false;
	}
	if (EstimatedPayloadBytes <= 0 || EstimatedPayloadBytes > MaximumStrokePayloadBytes)
	{
		OutRejectReason = FName(TEXT("PayloadSizeLimit"));
		return false;
	}

	int64 CountedPointTotal = 0;
	for (const int32 StrokePointCount : StrokePointCounts)
	{
		if (StrokePointCount < MinimumSubmittedStrokePointCount)
		{
			OutRejectReason = FName(TEXT("StrokeTooShort"));
			return false;
		}
		CountedPointTotal += StrokePointCount;
		if (CountedPointTotal > NormalizedPoints.Num())
		{
			OutRejectReason = FName(TEXT("StrokeLayoutMismatch"));
			return false;
		}
	}
	if (CountedPointTotal != NormalizedPoints.Num())
	{
		OutRejectReason = FName(TEXT("StrokeLayoutMismatch"));
		return false;
	}
	for (const uint8 PaletteIndex : StrokePaletteIndices)
	{
		if (!TemplateAllowedPalette.IsValidIndex(PaletteIndex))
		{
			OutRejectReason = FName(TEXT("PaletteIndexOutOfBounds"));
			return false;
		}
	}

	for (const FVector2D& Point : NormalizedPoints)
	{
		if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
		{
			OutRejectReason = FName(TEXT("NonFinitePoint"));
			return false;
		}
		if (!FMath::IsWithinInclusive(Point.X, 0.0, 1.0) || !FMath::IsWithinInclusive(Point.Y, 0.0, 1.0))
		{
			OutRejectReason = FName(TEXT("PointOutOfBounds"));
			return false;
		}
	}

	if (!FMath::IsFinite(ClientBrushSize) || !FMath::IsWithinInclusive(ClientBrushSize, 0.001f, 0.25f))
	{
		OutRejectReason = FName(TEXT("InvalidBrushSize"));
		return false;
	}
	if (!FMath::IsNearlyEqual(ClientBrushSize, TemplateBrushSize, BrushSizeValidationTolerance))
	{
		OutRejectReason = FName(TEXT("BrushSizeMismatch"));
		return false;
	}

	return true;
}

void UHeistForgeryComponent::RecordStrokeValidationResult(const bool bAccepted, const FName Reason)
{
	bLastStrokeValidationAccepted = bAccepted;
	LastStrokeValidationReason = Reason;
	++StrokeValidationRevision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Forgery stroke validation result: Character=%s Accepted=%s HasValidatedPayload=%s Strokes=%d Points=%d PayloadBytes=%d Brush=%.4f Reason=%s ValidationRevision=%d Authority=%s"),
		   *GetNameSafe(GetOwner()), bAccepted ? TEXT("true") : TEXT("false"), bHasValidatedStrokePayload ? TEXT("true") : TEXT("false"), ValidatedStrokeCount, ValidatedPointCount,
		   ValidatedPayloadBytes, ValidatedBrushSize, Reason.IsNone() ? TEXT("None") : *Reason.ToString(), StrokeValidationRevision,
		   GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
}

void UHeistForgeryComponent::ResetStrokeTransportState(const bool bResetLastValidation)
{
	ValidatedStrokePoints.Reset();
	ValidatedStrokePointCounts.Reset();
	ValidatedStrokePaletteIndices.Reset();
	bHasValidatedStrokePayload = false;
	ValidatedStrokeCount = 0;
	ValidatedPointCount = 0;
	ValidatedPayloadBytes = 0;
	ValidatedBrushSize = 0.0f;
	if (bResetLastValidation)
	{
		bLastStrokeValidationAccepted = false;
		LastStrokeValidationReason = NAME_None;
	}
}

bool UHeistForgeryComponent::TryCalculateAndCommitForgeryScore()
{
	FHeistForgeryResult CalculatedResult;
	int32 CalculatedReferenceMaskPixels = 0;
	int32 CalculatedSubmittedMaskPixels = 0;
	const double ScoreCalculationStartSeconds = FPlatformTime::Seconds();
	if (!CalculateForgeryScore(ValidatedStrokePoints, ValidatedStrokePointCounts, ValidatedStrokePaletteIndices, ValidatedBrushSize, CalculatedResult, CalculatedReferenceMaskPixels,
							   CalculatedSubmittedMaskPixels))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score calculation rejected: Character=%s Case=%s Artifact=%s Template=%s ReferenceMask=%s Reason=MaskDecodeOrScoreContractFailed"),
			   *GetNameSafe(GetOwner()), *GetNameSafe(ActiveDisplayCase.Get()), *ActiveArtifactId.ToString(), *ActiveTemplateId.ToString(), *ReferenceMaskAsset.ToSoftObjectPath().ToString());
		return false;
	}
	const double ScoreCalculationMilliseconds = (FPlatformTime::Seconds() - ScoreCalculationStartSeconds) * 1000.0;

	FHeistReplicaPaintingData PaintingData;
	if (!BuildReplicaPaintingData(PaintingData))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Forgery painting data build rejected: Character=%s Case=%s Template=%s Reason=PaletteRasterPackingFailed"), *GetNameSafe(GetOwner()),
			   *GetNameSafe(ActiveDisplayCase.Get()), *ActiveTemplateId.ToString());
		return false;
	}

	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	AHeistPaintingDisplayCaseActor* TargetDisplayCase = ActiveDisplayCase.Get();
	if (!IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score commit rejected: Character=%s Case=%s Reason=MissingOwnerOrDisplayCase"), *GetNameSafe(GetOwner()), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	bHandlingCaseSessionCallback = true;
	const bool bReplicaCommitted = TargetDisplayCase->TryCommitReplicaPlacement(HeistPlayerState, CalculatedResult, PaintingData);
	bHandlingCaseSessionCallback = false;
	if (!bReplicaCommitted)
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score commit rejected: Character=%s Case=%s Reason=ReplicaPlacementRejected"), *GetNameSafe(GetOwner()), *GetNameSafe(TargetDisplayCase));
		return false;
	}

	AuthoritativeForgeryResult = TargetDisplayCase->GetCommittedForgeryResult();
	bHasAuthoritativeForgeryResult = true;
	ForgeryScoreResolution = ForgeryScoreGridResolution;
	ReferenceMaskPixelCount = CalculatedReferenceMaskPixels;
	SubmittedMaskPixelCount = CalculatedSubmittedMaskPixels;
	++ForgeryScoreRevision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}

	const int32 TotalScorePixels = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	UE_LOG(
		LogHeistNetwork, Log,
		TEXT(
			"Forgery score committed: Character=%s Case=%s Artifact=%s Template=%s Backend=OpenCV ShapeMetric=DistanceDiceGeometric ColorMetric=LabSSIMHistogramGeometric Fusion=WeightedGeometricBottleneck Calibration=OpenCVHistogramBonus0.75x3 ScoreCurve=Shape1.15Color1.10 PaintCompletenessExponent=0.65 Score=%.2f Coverage=%.2f MajorShape=%.2f ColorAccuracy=%.2f MissingPenalty=%.2f ExtraPenalty=%.2f TimeoutPenalty=%.2f CompletionTime=%.2f PaintToReference=%.2f PaintCompleteness=%.3f AntiFill=%s Resolution=%dx%d ReferencePixels=%d SubmittedPixels=%d ReferenceRatio=%.4f SubmittedRatio=%.4f Cache=%s ReferenceMs=%.3f OpenCVMs=%.3f TotalMs=%.3f ScoreRevision=%d Authority=true Deterministic=true Result=PASS"),
		*GetNameSafe(GetOwner()), *GetNameSafe(ActiveDisplayCase.Get()), *AuthoritativeForgeryResult.ArtifactId.ToString(), *AuthoritativeForgeryResult.TemplateId.ToString(),
		AuthoritativeForgeryResult.SimilarityScore, AuthoritativeForgeryResult.CoverageScore, AuthoritativeForgeryResult.MajorShapeScore, AuthoritativeForgeryResult.ColorAccuracyScore,
		AuthoritativeForgeryResult.MissingShapePenalty, AuthoritativeForgeryResult.ExtraStrokePenalty, AuthoritativeForgeryResult.TimeoutPenalty, AuthoritativeForgeryResult.CompletionTime,
		AuthoritativeForgeryResult.PaintToReferenceRatio, FMath::Pow(FMath::Clamp(AuthoritativeForgeryResult.PaintToReferenceRatio, 0.0f, 1.0f), PaintCompletenessExponent),
		AuthoritativeForgeryResult.bAntiFillTriggered ? TEXT("true") : TEXT("false"), ForgeryScoreResolution, ForgeryScoreResolution, ReferenceMaskPixelCount, SubmittedMaskPixelCount,
		static_cast<float>(ReferenceMaskPixelCount) / TotalScorePixels, static_cast<float>(SubmittedMaskPixelCount) / TotalScorePixels, bLastScoringReferenceCacheHit ? TEXT("Hit") : TEXT("Miss"),
		LastScoringReferenceMilliseconds, LastOpenCVScoringMilliseconds, ScoreCalculationMilliseconds, ForgeryScoreRevision);
	SessionStateChangedDelegate.Broadcast();
	return true;
}

bool UHeistForgeryComponent::BuildReplicaPaintingData(FHeistReplicaPaintingData& OutPaintingData) const
{
	OutPaintingData = FHeistReplicaPaintingData();
	if (ValidatedStrokePoints.IsEmpty() || ValidatedStrokePointCounts.IsEmpty() || ValidatedStrokePaletteIndices.Num() != ValidatedStrokePointCounts.Num() ||
		!FMath::IsWithinInclusive(TemplateAllowedPalette.Num(), 2, 8) || ValidatedBrushSize <= 0.0f)
	{
		return false;
	}

	TArray<uint8> SubmittedPaletteMap;
	RasterizeForgeryPaletteStrokes(ValidatedStrokePoints, ValidatedStrokePointCounts, ValidatedStrokePaletteIndices, ValidatedBrushSize, SubmittedPaletteMap);

	const int32 ExpectedPixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	if (SubmittedPaletteMap.Num() != ExpectedPixelCount)
	{
		return false;
	}

	OutPaintingData.Resolution = ForgeryScoreGridResolution;
	OutPaintingData.Palette.Reserve(TemplateAllowedPalette.Num());
	for (const FLinearColor& PaletteColor : TemplateAllowedPalette)
	{
		FColor SrgbColor = PaletteColor.ToFColorSRGB();
		SrgbColor.A = 255;
		OutPaintingData.Palette.Add(SrgbColor);
	}

	OutPaintingData.PackedPaletteIndices.SetNumZeroed(FMath::DivideAndRoundUp(ExpectedPixelCount, 2));
	for (int32 PixelIndex = 0; PixelIndex < SubmittedPaletteMap.Num(); ++PixelIndex)
	{
		const uint8 SourcePaletteIndex = SubmittedPaletteMap[PixelIndex];
		if (SourcePaletteIndex != EmptyPaletteIndex && !OutPaintingData.Palette.IsValidIndex(SourcePaletteIndex))
		{
			OutPaintingData = FHeistReplicaPaintingData();
			return false;
		}

		const uint8 PackedIndex = SourcePaletteIndex == EmptyPaletteIndex ? 0 : static_cast<uint8>(SourcePaletteIndex + 1);
		uint8& PackedByte = OutPaintingData.PackedPaletteIndices[PixelIndex / 2];
		if ((PixelIndex & 1) == 0)
		{
			PackedByte = PackedIndex;
		}
		else
		{
			PackedByte |= static_cast<uint8>(PackedIndex << 4);
		}
	}

	return true;
}

bool UHeistForgeryComponent::BuildScoringReferenceCache() const
{
	const int32 ExpectedPixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	if (CachedScoringTemplateId == ActiveTemplateId && CachedReferenceMask.Num() == ExpectedPixelCount && CachedReferencePaletteMap.Num() == ExpectedPixelCount)
	{
		return true;
	}

	ResetScoringReferenceCache();
	TArray<uint8> SourceIntensity;
	int32 SourceWidth = 0;
	int32 SourceHeight = 0;
	EPixelFormat SourcePixelFormat = PF_Unknown;
	if (TemplateBackgroundFilterMode == EHeistForgeryBackgroundFilter::None)
	{
		UTexture2D* ReferenceMaskTexture = ReferenceMaskAsset.LoadSynchronous();
		if (!DecodeReferenceMaskIntensity(ReferenceMaskTexture, SourceIntensity, SourceWidth, SourceHeight, SourcePixelFormat))
		{
			UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score mask decode failed: Character=%s Texture=%s PixelFormat=%d Source=%dx%d"), *GetNameSafe(GetOwner()), *GetNameSafe(ReferenceMaskTexture),
				   static_cast<int32>(SourcePixelFormat), SourceWidth, SourceHeight);
			return false;
		}
	}

	UTexture2D* ReferenceImageTexture = ReferenceImageAsset.LoadSynchronous();
	TArray<FColor> SourceColors;
	int32 ImageWidth = 0;
	int32 ImageHeight = 0;
	EPixelFormat ImagePixelFormat = PF_Unknown;
	if (!DecodeReferenceImageColors(ReferenceImageTexture, SourceColors, ImageWidth, ImageHeight, ImagePixelFormat))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score image decode failed: Character=%s Texture=%s PixelFormat=%d Source=%dx%d"), *GetNameSafe(GetOwner()), *GetNameSafe(ReferenceImageTexture),
			   static_cast<int32>(ImagePixelFormat), ImageWidth, ImageHeight);
		return false;
	}

	if (TemplateBackgroundFilterMode == EHeistForgeryBackgroundFilter::None)
	{
		BuildLowResolutionReferenceMask(SourceIntensity, SourceWidth, SourceHeight, CachedReferenceMask);
	}
	else
	{
		BuildLowResolutionBackgroundFilteredMask(SourceColors, ImageWidth, ImageHeight, TemplateBackgroundFilterMode, TemplateBackgroundColorTolerance, CachedReferenceMask);
	}
	BuildLowResolutionReferencePaletteMap(SourceColors, ImageWidth, ImageHeight, CachedReferenceMask, TemplateAllowedPalette, CachedReferencePaletteMap);
	CachedScoringTemplateId = ActiveTemplateId;
	return CachedReferenceMask.Num() == ExpectedPixelCount && CachedReferencePaletteMap.Num() == ExpectedPixelCount;
}

void UHeistForgeryComponent::ResetScoringReferenceCache() const
{
	CachedScoringTemplateId = NAME_None;
	CachedReferenceMask.Reset();
	CachedReferencePaletteMap.Reset();
}

bool UHeistForgeryComponent::CalculateForgeryScore(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices, const float BrushSize,
												   FHeistForgeryResult& OutResult, int32& OutReferenceMaskPixels, int32& OutSubmittedMaskPixels) const
{
	OutResult = FHeistForgeryResult();
	OutReferenceMaskPixels = 0;
	OutSubmittedMaskPixels = 0;
	if (!GetOwner() || ActiveArtifactId.IsNone() || ActiveTemplateId.IsNone() || ReferenceImageAsset.IsNull() ||
		(TemplateBackgroundFilterMode == EHeistForgeryBackgroundFilter::None && ReferenceMaskAsset.IsNull()) || NormalizedPoints.IsEmpty() || StrokePointCounts.IsEmpty() ||
		StrokePaletteIndices.Num() != StrokePointCounts.Num() || !FMath::IsWithinInclusive(TemplateAllowedPalette.Num(), 2, 8) || BrushSize <= 0.0f || TemplateCoverageWeight < 0.0f ||
		TemplateMajorShapeWeight < 0.0f || TemplateExtraStrokePenaltyWeight < 0.0f || TemplateTimeoutPenalty < 0.0f || TemplateShapeAccuracyWeight < 0.0f || TemplateColorAccuracyWeight < 0.0f ||
		TemplateShapeAccuracyWeight + TemplateColorAccuracyWeight <= 0.0f || !FMath::IsWithinInclusive(TemplateBackgroundColorTolerance, 0.0f, 0.49f) || TemplateMaximumPaintToReferenceRatio < 1.0f ||
		!FMath::IsWithinInclusive(TemplateOverpaintScoreCap, 0.0f, 100.0f))
	{
		return false;
	}

	const int32 ExpectedScorePixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	bLastScoringReferenceCacheHit = CachedScoringTemplateId == ActiveTemplateId && CachedReferenceMask.Num() == ExpectedScorePixelCount && CachedReferencePaletteMap.Num() == ExpectedScorePixelCount;
	LastOpenCVScoringMilliseconds = 0.0;
	const double ReferenceScoringStartSeconds = FPlatformTime::Seconds();
	if (!BuildScoringReferenceCache())
	{
		LastScoringReferenceMilliseconds = (FPlatformTime::Seconds() - ReferenceScoringStartSeconds) * 1000.0;
		return false;
	}
	LastScoringReferenceMilliseconds = (FPlatformTime::Seconds() - ReferenceScoringStartSeconds) * 1000.0;

	const TArray<uint8>& ReferenceMask = CachedReferenceMask;
	const TArray<uint8>& ReferencePaletteMap = CachedReferencePaletteMap;
	TArray<uint8> SubmittedPaletteMap;
	RasterizeForgeryPaletteStrokes(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, BrushSize, SubmittedPaletteMap);

#if WITH_OPENCV
	FOpenCVForgeryMetrics OpenCVMetrics;
	const double OpenCVScoringStartSeconds = FPlatformTime::Seconds();
	if (!CalculateOpenCVForgeryMetrics(ReferenceMask, ReferencePaletteMap, SubmittedPaletteMap, TemplateAllowedPalette, OpenCVMetrics))
	{
		LastOpenCVScoringMilliseconds = (FPlatformTime::Seconds() - OpenCVScoringStartSeconds) * 1000.0;
		return false;
	}
	LastOpenCVScoringMilliseconds = (FPlatformTime::Seconds() - OpenCVScoringStartSeconds) * 1000.0;
	OutReferenceMaskPixels = OpenCVMetrics.ReferencePixelCount;
	OutSubmittedMaskPixels = OpenCVMetrics.SubmittedPixelCount;
	if (GetOwner()->HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork, Log,
			TEXT(
				"Forgery OpenCV metrics: Character=%s Template=%s DistanceRecall=%.4f DistancePrecision=%.4f BidirectionalDistance=%.4f MaskPrecision=%.4f MaskRecall=%.4f MaskIoU=%.4f MaskDice=%.4f LabSSIM=%.4f PaletteHistogram=%.4f ColorGeometric=%.4f PaletteBonus=%.4f Result=PASS"),
			*GetNameSafe(GetOwner()), *ActiveTemplateId.ToString(), OpenCVMetrics.ReferenceCoverage, OpenCVMetrics.SubmittedPrecision, OpenCVMetrics.BidirectionalShapeSimilarity,
			OpenCVMetrics.MaskPrecision, OpenCVMetrics.MaskRecall, OpenCVMetrics.MaskIntersectionOverUnion, OpenCVMetrics.MaskDiceSimilarity, OpenCVMetrics.StructuralColorSimilarity,
			OpenCVMetrics.HistogramColorSimilarity, OpenCVMetrics.ColorSimilarity,
			OpenCVPaletteFidelityBonusWeight * FMath::Pow(OpenCVMetrics.HistogramColorSimilarity, OpenCVPaletteFidelityBonusExponent));
	}
#else
	UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score calculation rejected: Character=%s Template=%s Reason=OpenCVUnavailable"), *GetNameSafe(GetOwner()), *ActiveTemplateId.ToString());
	return false;
#endif

	const int32 TotalPixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	if (OutReferenceMaskPixels <= 0 || OutReferenceMaskPixels >= TotalPixelCount)
	{
		const FString MaskSource =
			TemplateBackgroundFilterMode == EHeistForgeryBackgroundFilter::None ? ReferenceMaskAsset.ToSoftObjectPath().ToString() : ReferenceImageAsset.ToSoftObjectPath().ToString();
		UE_LOG(LogHeistNetwork, Error, TEXT("Forgery score mask rejected: Character=%s Texture=%s ReferencePixels=%d TotalPixels=%d Reason=DegenerateReferenceMask"), *GetNameSafe(GetOwner()),
			   *MaskSource, OutReferenceMaskPixels, TotalPixelCount);
		return false;
	}

	float CoverageRatio = 0.0f;
	float MissingShapeRatio = 0.0f;
	float ExtraStrokeRatio = 0.0f;
	float MajorShapeRatio = 0.0f;
	float ColorSimilarityRatio = 0.0f;
#if WITH_OPENCV
	const float ShapeMetricWeightTotal = TemplateCoverageWeight + TemplateMajorShapeWeight;
	const float RawShapeSimilarity = FMath::Pow(FMath::Clamp(OpenCVMetrics.MaskDiceSimilarity, 0.0f, 1.0f), TemplateCoverageWeight / ShapeMetricWeightTotal) *
									 FMath::Pow(FMath::Clamp(OpenCVMetrics.BidirectionalShapeSimilarity, 0.0f, 1.0f), TemplateMajorShapeWeight / ShapeMetricWeightTotal);
	CoverageRatio = FMath::Pow(FMath::Clamp(OpenCVMetrics.MaskRecall, 0.0f, 1.0f), OpenCVShapeScoreExponent);
	MissingShapeRatio = 1.0f - OpenCVMetrics.MaskRecall;
	ExtraStrokeRatio = 1.0f - OpenCVMetrics.MaskPrecision;
	MajorShapeRatio = FMath::Pow(FMath::Clamp(RawShapeSimilarity, 0.0f, 1.0f), OpenCVShapeScoreExponent);
	ColorSimilarityRatio = FMath::Pow(FMath::Clamp(OpenCVMetrics.ColorSimilarity, 0.0f, 1.0f), OpenCVColorScoreExponent);
#endif

	const float CoveragePoints = CoverageRatio * TemplateCoverageWeight * 100.0f;
	const float MajorShapePoints = MajorShapeRatio * TemplateMajorShapeWeight * 100.0f;
	const float MissingShapePenaltyPoints = MissingShapeRatio * TemplateExtraStrokePenaltyWeight * 100.0f;
	const float ExtraStrokePenaltyPoints = ExtraStrokeRatio * TemplateExtraStrokePenaltyWeight * 100.0f;
	const float TimeoutPenaltyPoints = 0.0f;
	const float ColorAccuracyScore = ColorSimilarityRatio * 100.0f;
	const float AccuracyWeightTotal = TemplateShapeAccuracyWeight + TemplateColorAccuracyWeight;
	const float WeightedGeometricAccuracy =
		FMath::Pow(MajorShapeRatio, TemplateShapeAccuracyWeight / AccuracyWeightTotal) * FMath::Pow(ColorSimilarityRatio, TemplateColorAccuracyWeight / AccuracyWeightTotal);
	const float BottleneckSimilarity = FMath::Min(MajorShapeRatio, ColorSimilarityRatio);
	float FinalScore = FMath::Clamp(WeightedGeometricAccuracy * BottleneckSimilarity * 100.0f - TimeoutPenaltyPoints, 0.0f, 100.0f);
	const float PaintToReferenceRatio = static_cast<float>(OutSubmittedMaskPixels) / OutReferenceMaskPixels;
	const float PaintCompletenessFactor = FMath::Pow(FMath::Clamp(PaintToReferenceRatio, 0.0f, 1.0f), PaintCompletenessExponent);
	FinalScore *= PaintCompletenessFactor;
#if WITH_OPENCV
	const float PaletteFidelityBonus = OpenCVPaletteFidelityBonusWeight * FMath::Pow(FMath::Clamp(OpenCVMetrics.HistogramColorSimilarity, 0.0f, 1.0f), OpenCVPaletteFidelityBonusExponent);
	FinalScore = FMath::Lerp(FinalScore, 100.0f, PaletteFidelityBonus);
#endif
	const bool bAntiFillTriggered = PaintToReferenceRatio > TemplateMaximumPaintToReferenceRatio;
	if (bAntiFillTriggered)
	{
		FinalScore = FMath::Min(FinalScore, TemplateOverpaintScoreCap);
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	const float SessionStartServerTime = SessionEndServerTime - ActiveSessionDurationSeconds;
	const float CompletionTime = FMath::Clamp(ServerWorldTime - SessionStartServerTime, 0.0f, ActiveSessionDurationSeconds);
	const auto RoundScore = [](const float Value) { return FMath::RoundToFloat(Value * 100.0f) / 100.0f; };

	OutResult.ArtifactId = ActiveArtifactId;
	OutResult.TemplateId = ActiveTemplateId;
	OutResult.ForgeryType = EHeistForgeryType::Drawing;
	OutResult.SimilarityScore = RoundScore(FinalScore);
	OutResult.CoverageScore = RoundScore(CoveragePoints);
	OutResult.MajorShapeScore = RoundScore(MajorShapePoints);
	OutResult.ColorAccuracyScore = RoundScore(ColorAccuracyScore);
	OutResult.PaintToReferenceRatio = RoundScore(PaintToReferenceRatio);
	OutResult.bAntiFillTriggered = bAntiFillTriggered;
	OutResult.MissingShapePenalty = RoundScore(MissingShapePenaltyPoints);
	OutResult.ExtraStrokePenalty = RoundScore(ExtraStrokePenaltyPoints);
	OutResult.TimeoutPenalty = RoundScore(TimeoutPenaltyPoints);
	OutResult.CompletionTime = RoundScore(CompletionTime);
	OutResult.bReplicaPlaced = false;
	return true;
}

bool UHeistForgeryComponent::RunOpenCVScoringSelfTestForDebug(FString& OutSummary) const
{
#if WITH_OPENCV
	const int32 PixelCount = ForgeryScoreGridResolution * ForgeryScoreGridResolution;
	TArray<uint8> ReferenceMask;
	TArray<uint8> ReferencePaletteMap;
	TArray<uint8> ExactPaletteMap;
	TArray<uint8> ShiftedPaletteMap;
	TArray<uint8> WrongColorPaletteMap;
	TArray<uint8> SingleColorPaletteMap;
	TArray<uint8> FilledPaletteMap;
	ReferenceMask.Init(0, PixelCount);
	ReferencePaletteMap.Init(EmptyPaletteIndex, PixelCount);
	ExactPaletteMap.Init(EmptyPaletteIndex, PixelCount);
	ShiftedPaletteMap.Init(EmptyPaletteIndex, PixelCount);
	WrongColorPaletteMap.Init(EmptyPaletteIndex, PixelCount);
	SingleColorPaletteMap.Init(EmptyPaletteIndex, PixelCount);
	FilledPaletteMap.Init(0, PixelCount);

	constexpr int32 ShapeMinimum = 32;
	constexpr int32 ShapeMaximum = 95;
	constexpr int32 ShiftPixels = 5;
	for (int32 Y = ShapeMinimum; Y <= ShapeMaximum; ++Y)
	{
		for (int32 X = ShapeMinimum; X <= ShapeMaximum; ++X)
		{
			const int32 PixelIndex = Y * ForgeryScoreGridResolution + X;
			const uint8 PaletteIndex = X < ForgeryScoreGridResolution / 2 ? 0 : 1;
			ReferenceMask[PixelIndex] = 1;
			ReferencePaletteMap[PixelIndex] = PaletteIndex;
			ExactPaletteMap[PixelIndex] = PaletteIndex;
			WrongColorPaletteMap[PixelIndex] = PaletteIndex == 0 ? 1 : 0;
			SingleColorPaletteMap[PixelIndex] = 0;

			const int32 ShiftedX = X + ShiftPixels;
			if (ShiftedX < ForgeryScoreGridResolution)
			{
				ShiftedPaletteMap[Y * ForgeryScoreGridResolution + ShiftedX] = PaletteIndex;
			}
		}
	}

	const TArray<FLinearColor> TestPalette = {FLinearColor(0.95f, 0.08f, 0.05f, 1.0f), FLinearColor(0.04f, 0.15f, 0.95f, 1.0f)};
	FOpenCVForgeryMetrics ExactMetrics;
	FOpenCVForgeryMetrics ShiftedMetrics;
	FOpenCVForgeryMetrics WrongColorMetrics;
	FOpenCVForgeryMetrics SingleColorMetrics;
	FOpenCVForgeryMetrics FilledMetrics;
	const bool bCalculated = CalculateOpenCVForgeryMetrics(ReferenceMask, ReferencePaletteMap, ExactPaletteMap, TestPalette, ExactMetrics) &&
							 CalculateOpenCVForgeryMetrics(ReferenceMask, ReferencePaletteMap, ShiftedPaletteMap, TestPalette, ShiftedMetrics) &&
							 CalculateOpenCVForgeryMetrics(ReferenceMask, ReferencePaletteMap, WrongColorPaletteMap, TestPalette, WrongColorMetrics) &&
							 CalculateOpenCVForgeryMetrics(ReferenceMask, ReferencePaletteMap, SingleColorPaletteMap, TestPalette, SingleColorMetrics) &&
							 CalculateOpenCVForgeryMetrics(ReferenceMask, ReferencePaletteMap, FilledPaletteMap, TestPalette, FilledMetrics);
	const float FilledAreaRatio = bCalculated ? static_cast<float>(FilledMetrics.SubmittedPixelCount) / ExactMetrics.ReferencePixelCount : 0.0f;
	const auto CalculateOpenCVOnlyScore = [](const FOpenCVForgeryMetrics& Metrics)
	{
		constexpr float CoverageWeight = 0.45f;
		constexpr float DistanceWeight = 0.55f;
		constexpr float ShapeWeight = 0.65f;
		constexpr float ColorWeight = 0.35f;
		const float RawShape = FMath::Pow(Metrics.MaskDiceSimilarity, CoverageWeight) * FMath::Pow(Metrics.BidirectionalShapeSimilarity, DistanceWeight);
		const float Shape = FMath::Pow(RawShape, OpenCVShapeScoreExponent);
		const float Color = FMath::Pow(Metrics.ColorSimilarity, OpenCVColorScoreExponent);
		const float WeightedGeometric = FMath::Pow(Shape, ShapeWeight) * FMath::Pow(Color, ColorWeight);
		const float BaseScore = 100.0f * WeightedGeometric * FMath::Min(Shape, Color);
		const float PaletteFidelityBonus = OpenCVPaletteFidelityBonusWeight * FMath::Pow(Metrics.HistogramColorSimilarity, OpenCVPaletteFidelityBonusExponent);
		return FMath::Lerp(BaseScore, 100.0f, PaletteFidelityBonus);
	};
	const float ExactScore = bCalculated ? CalculateOpenCVOnlyScore(ExactMetrics) : 0.0f;
	const float ShiftedScore = bCalculated ? CalculateOpenCVOnlyScore(ShiftedMetrics) : 100.0f;
	const float WrongColorScore = bCalculated ? CalculateOpenCVOnlyScore(WrongColorMetrics) : 100.0f;
	const float SingleColorScore = bCalculated ? CalculateOpenCVOnlyScore(SingleColorMetrics) : 100.0f;
	const float FilledScore = bCalculated ? FMath::Min(CalculateOpenCVOnlyScore(FilledMetrics), 20.0f) : 100.0f;
	const bool bContractPassed = bCalculated && ExactMetrics.BidirectionalShapeSimilarity >= 0.999f && ExactMetrics.MaskDiceSimilarity >= 0.999f && ExactMetrics.ColorSimilarity >= 0.999f &&
								 ExactScore >= 99.9f && ShiftedMetrics.BidirectionalShapeSimilarity < ExactMetrics.BidirectionalShapeSimilarity && ShiftedMetrics.BidirectionalShapeSimilarity > 0.0f &&
								 ShiftedMetrics.MaskDiceSimilarity < ExactMetrics.MaskDiceSimilarity && ShiftedScore < ExactScore && WrongColorMetrics.BidirectionalShapeSimilarity >= 0.999f &&
								 WrongColorMetrics.ColorSimilarity < ExactMetrics.ColorSimilarity && WrongColorScore < ExactScore &&
								 SingleColorMetrics.HistogramColorSimilarity < ExactMetrics.HistogramColorSimilarity && SingleColorScore < 60.0f &&
								 FilledMetrics.MaskPrecision < ExactMetrics.MaskPrecision && FilledMetrics.MaskDiceSimilarity < ExactMetrics.MaskDiceSimilarity && FilledAreaRatio > 1.0f &&
								 FilledScore <= 30.0f;

	OutSummary = FString::Printf(
		TEXT(
			"Calculated=%s ExactDistance=%.4f ExactDice=%.4f ExactColor=%.4f ExactScore=%.2f ShiftedDistance=%.4f ShiftedDice=%.4f ShiftedScore=%.2f WrongColor=%.4f WrongColorScore=%.2f SingleHistogram=%.4f SingleColor=%.4f SingleScore=%.2f FillPrecision=%.4f FillDice=%.4f FillAreaRatio=%.2f FillScore=%.2f Contract=%s"),
		bCalculated ? TEXT("true") : TEXT("false"), ExactMetrics.BidirectionalShapeSimilarity, ExactMetrics.MaskDiceSimilarity, ExactMetrics.ColorSimilarity, ExactScore,
		ShiftedMetrics.BidirectionalShapeSimilarity, ShiftedMetrics.MaskDiceSimilarity, ShiftedScore, WrongColorMetrics.ColorSimilarity, WrongColorScore, SingleColorMetrics.HistogramColorSimilarity,
		SingleColorMetrics.ColorSimilarity, SingleColorScore, FilledMetrics.MaskPrecision, FilledMetrics.MaskDiceSimilarity, FilledAreaRatio, FilledScore,
		bContractPassed ? TEXT("PASS") : TEXT("FAIL"));
	return bContractPassed;
#else
	OutSummary = TEXT("Calculated=false Reason=OpenCVUnavailable Contract=FAIL");
	return false;
#endif
}

void UHeistForgeryComponent::ResetForgeryScoreState()
{
	bHasAuthoritativeForgeryResult = false;
	AuthoritativeForgeryResult = FHeistForgeryResult();
	ForgeryScoreResolution = 0;
	ReferenceMaskPixelCount = 0;
	SubmittedMaskPixelCount = 0;
}

void UHeistForgeryComponent::CompleteSuccessfulForgerySession()
{
	AHeistPaintingDisplayCaseActor* PreviousDisplayCase = ActiveDisplayCase.Get();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
	}
	UnbindActiveDisplayCase();

	ActiveDisplayCase = nullptr;
	bSessionActive = false;
	bSubmitPending = false;
	SessionEndServerTime = 0.0f;
	ActiveSessionDurationSeconds = 0.0f;
	LastCleanupReason = FName(TEXT("ForgeryCompleted"));
	++SessionRevision;

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	UE_LOG(LogHeistNetwork, Log, TEXT("Forgery session completed: Character=%s PreviousCase=%s HasScore=%s Score=%.2f ReplicaPlaced=%s Revision=%d Authority=true Result=%s"), *GetNameSafe(GetOwner()),
		   *GetNameSafe(PreviousDisplayCase), bHasAuthoritativeForgeryResult ? TEXT("true") : TEXT("false"), AuthoritativeForgeryResult.SimilarityScore,
		   AuthoritativeForgeryResult.bReplicaPlaced ? TEXT("true") : TEXT("false"), SessionRevision,
		   bHasAuthoritativeForgeryResult && AuthoritativeForgeryResult.bReplicaPlaced ? TEXT("PASS") : TEXT("FAIL"));
	BroadcastSessionSnapshot(TEXT("ServerComplete"), LastCleanupReason);
}

bool UHeistForgeryComponent::ValidateActiveSession(FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistPaintingDisplayCaseActor* TargetDisplayCase = ActiveDisplayCase.Get();

	if (!bSessionActive)
	{
		OutRejectReason = FName(TEXT("SessionInactive"));
		return false;
	}
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState))
	{
		OutRejectReason = FName(TEXT("InvalidAuthorityContext"));
		return false;
	}
	if (!IsValid(TargetDisplayCase))
	{
		OutRejectReason = FName(TEXT("MissingDisplayCase"));
		return false;
	}
	if (!bTemplatePrepared || ActiveArtifactId.IsNone() || ActiveTemplateId.IsNone() || ReferenceImageAsset.IsNull() ||
		(TemplateBackgroundFilterMode == EHeistForgeryBackgroundFilter::None && ReferenceMaskAsset.IsNull()) || TemplateForgeryDuration <= 0.0f || TemplateStrokeLimit <= 0 ||
		TemplateBrushSize <= 0.0f || !FMath::IsWithinInclusive(TemplateAllowedPalette.Num(), 2, 8) || ActiveSessionDurationSeconds <= 0.0f || TemplateCoverageWeight < 0.0f ||
		TemplateMajorShapeWeight < 0.0f || TemplateExtraStrokePenaltyWeight < 0.0f || TemplateTimeoutPenalty < 0.0f || TemplateShapeAccuracyWeight < 0.0f || TemplateColorAccuracyWeight < 0.0f ||
		TemplateShapeAccuracyWeight + TemplateColorAccuracyWeight <= 0.0f || !FMath::IsWithinInclusive(TemplateBackgroundColorTolerance, 0.0f, 0.49f) || TemplateMaximumPaintToReferenceRatio < 1.0f ||
		!FMath::IsWithinInclusive(TemplateOverpaintScoreCap, 0.0f, 100.0f))
	{
		OutRejectReason = FName(TEXT("TemplateSnapshotInvalid"));
		return false;
	}
	if (!TargetDisplayCase->IsSessionLocked() || TargetDisplayCase->GetSessionOwner() != HeistPlayerState)
	{
		OutRejectReason = FName(TEXT("CaseOwnershipMismatch"));
		return false;
	}
	if (TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::ForgeryInProgress)
	{
		OutRejectReason = FName(TEXT("CaseStateMismatch"));
		return false;
	}
	if (HeistPlayerState->IsArrested() || HeistPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerStateBlocked"));
		return false;
	}
	if (FVector::DistSquared(HeistCharacter->GetActorLocation(), TargetDisplayCase->GetActorLocation()) > FMath::Square(TargetDisplayCase->GetMaximumSessionDistance()))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}

	return true;
}

void UHeistForgeryComponent::HandleSessionTimeout()
{
	if (GetOwner() && GetOwner()->HasAuthority() && bSessionActive)
	{
		ClearSession(FName(TEXT("Timeout")), true);
	}
}

void UHeistForgeryComponent::ClearSession(const FName Reason, const bool bReleaseCaseLock)
{
	AHeistPaintingDisplayCaseActor* PreviousDisplayCase = ActiveDisplayCase.Get();
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
	}
	UnbindActiveDisplayCase();

	ActiveDisplayCase = nullptr;
	bSessionActive = false;
	bSubmitPending = false;
	SessionEndServerTime = 0.0f;
	ActiveSessionDurationSeconds = 0.0f;
	ResetStrokeTransportState(false);
	const bool bHadForgeryResult = bHasAuthoritativeForgeryResult;
	ResetForgeryScoreState();
	if (bHadForgeryResult)
	{
		++ForgeryScoreRevision;
	}
	ResetPreparedTemplateSnapshot();
	LastCleanupReason = Reason;
	++SessionRevision;

	if (bReleaseCaseLock && IsValid(PreviousDisplayCase) && IsValid(HeistPlayerState) && PreviousDisplayCase->GetSessionOwner() == HeistPlayerState)
	{
		bHandlingCaseSessionCallback = true;
		PreviousDisplayCase->CancelSessionForOwner(HeistPlayerState, Reason);
		bHandlingCaseSessionCallback = false;
	}

	if (IsValid(HeistCharacter))
	{
		HeistCharacter->ForceNetUpdate();
	}

	UE_LOG(LogHeistNetwork, Log, TEXT("Forgery session cleared: Character=%s PreviousCase=%s Reason=%s Revision=%d"), *GetNameSafe(HeistCharacter), *GetNameSafe(PreviousDisplayCase),
		   *Reason.ToString(), SessionRevision);
	BroadcastSessionSnapshot(TEXT("ServerClear"), Reason);
}

void UHeistForgeryComponent::BroadcastSessionSnapshot(const TCHAR* ChangeSource, const FName Reason)
{
	SessionStateChangedDelegate.Broadcast();
	UE_LOG(
		LogHeistNetwork, Log,
		TEXT(
			"Forgery session %s: Character=%s Case=%s Active=%s SubmitPending=%s HasScore=%s Score=%.2f TemplatePrepared=%s Artifact=%s Template=%s EndServerTime=%.2f Revision=%d LastCleanup=%s Reason=%s Authority=%s"),
		ChangeSource, *GetNameSafe(GetOwner()), *GetNameSafe(ActiveDisplayCase.Get()), bSessionActive ? TEXT("true") : TEXT("false"), bSubmitPending ? TEXT("true") : TEXT("false"),
		bHasAuthoritativeForgeryResult ? TEXT("true") : TEXT("false"), AuthoritativeForgeryResult.SimilarityScore, bTemplatePrepared ? TEXT("true") : TEXT("false"), *ActiveArtifactId.ToString(),
		*ActiveTemplateId.ToString(), SessionEndServerTime, SessionRevision, LastCleanupReason.IsNone() ? TEXT("None") : *LastCleanupReason.ToString(),
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(), GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
}

void UHeistForgeryComponent::ResetPreparedTemplateSnapshot()
{
	ResetScoringReferenceCache();
	PreparedDisplayCase.Reset();
	bTemplatePrepared = false;
	ActiveArtifactId = NAME_None;
	ActiveTemplateId = NAME_None;
	ReferenceImageAsset.Reset();
	ReferenceMaskAsset.Reset();
	TemplateObservationDuration = 0.0f;
	TemplateForgeryDuration = 0.0f;
	TemplateStrokeLimit = 0;
	TemplateBrushSize = 0.0f;
	TemplateAllowedPalette.Reset();
	TemplateCoverageWeight = 0.0f;
	TemplateMajorShapeWeight = 0.0f;
	TemplateExtraStrokePenaltyWeight = 0.0f;
	TemplateTimeoutPenalty = 0.0f;
	TemplateBackgroundFilterMode = EHeistForgeryBackgroundFilter::None;
	TemplateBackgroundColorTolerance = 0.0f;
	TemplateShapeAccuracyWeight = 0.0f;
	TemplateColorAccuracyWeight = 0.0f;
	TemplateMaximumPaintToReferenceRatio = 0.0f;
	TemplateOverpaintScoreCap = 0.0f;
}

void UHeistForgeryComponent::UnbindActiveDisplayCase()
{
	if (IsValid(ActiveDisplayCase.Get()))
	{
		ActiveDisplayCase->OnDisplayCaseSessionChanged.RemoveDynamic(this, &UHeistForgeryComponent::HandleDisplayCaseSessionChanged);
	}
}

void UHeistForgeryComponent::HandleDisplayCaseSessionChanged(AHeistPlayerState* SessionOwner, const bool bLocked, const int32)
{
	if (bHandlingCaseSessionCallback || !GetOwner() || !GetOwner()->HasAuthority() || !bSessionActive)
	{
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!bLocked || !IsValid(SessionOwner) || SessionOwner != HeistPlayerState)
	{
		ClearSession(FName(TEXT("CaseSessionInvalidated")), false);
	}
}

void UHeistForgeryComponent::OnRep_SessionRevision()
{
	ResetScoringReferenceCache();
	BroadcastSessionSnapshot(TEXT("Replicated"), NAME_None);
}

void UHeistForgeryComponent::OnRep_StrokeValidationRevision()
{
	UE_LOG(
		LogHeistNetwork, Log,
		TEXT(
			"Forgery stroke validation replicated: Character=%s Accepted=%s HasValidatedPayload=%s Strokes=%d Points=%d PayloadBytes=%d Brush=%.4f Reason=%s ValidationRevision=%d Authority=false Result=%s"),
		*GetNameSafe(GetOwner()), bLastStrokeValidationAccepted ? TEXT("true") : TEXT("false"), bHasValidatedStrokePayload ? TEXT("true") : TEXT("false"), ValidatedStrokeCount, ValidatedPointCount,
		ValidatedPayloadBytes, ValidatedBrushSize, LastStrokeValidationReason.IsNone() ? TEXT("None") : *LastStrokeValidationReason.ToString(), StrokeValidationRevision,
		bLastStrokeValidationAccepted ? TEXT("PASS") : TEXT("REJECTED"));
}

void UHeistForgeryComponent::OnRep_ForgeryScoreRevision()
{
	SessionStateChangedDelegate.Broadcast();
	UE_LOG(
		LogHeistNetwork, Log,
		TEXT(
			"Forgery score replicated: Character=%s HasScore=%s Artifact=%s Template=%s Score=%.2f Coverage=%.2f MajorShape=%.2f ColorAccuracy=%.2f MissingPenalty=%.2f ExtraPenalty=%.2f TimeoutPenalty=%.2f CompletionTime=%.2f PaintToReference=%.2f AntiFill=%s Resolution=%dx%d ReferencePixels=%d SubmittedPixels=%d ScoreRevision=%d Authority=false Result=%s"),
		*GetNameSafe(GetOwner()), bHasAuthoritativeForgeryResult ? TEXT("true") : TEXT("false"), *AuthoritativeForgeryResult.ArtifactId.ToString(), *AuthoritativeForgeryResult.TemplateId.ToString(),
		AuthoritativeForgeryResult.SimilarityScore, AuthoritativeForgeryResult.CoverageScore, AuthoritativeForgeryResult.MajorShapeScore, AuthoritativeForgeryResult.ColorAccuracyScore,
		AuthoritativeForgeryResult.MissingShapePenalty, AuthoritativeForgeryResult.ExtraStrokePenalty, AuthoritativeForgeryResult.TimeoutPenalty, AuthoritativeForgeryResult.CompletionTime,
		AuthoritativeForgeryResult.PaintToReferenceRatio, AuthoritativeForgeryResult.bAntiFillTriggered ? TEXT("true") : TEXT("false"), ForgeryScoreResolution, ForgeryScoreResolution,
		ReferenceMaskPixelCount, SubmittedMaskPixelCount, ForgeryScoreRevision, bHasAuthoritativeForgeryResult ? TEXT("PASS") : TEXT("CLEARED"));
}

void UHeistForgeryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ActiveDisplayCase, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, bSessionActive, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, bSubmitPending, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, SessionEndServerTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, SessionRevision, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, bTemplatePrepared, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ActiveArtifactId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ActiveTemplateId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ReferenceImageAsset, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ReferenceMaskAsset, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateObservationDuration, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateForgeryDuration, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateStrokeLimit, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateBrushSize, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateAllowedPalette, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateCoverageWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateMajorShapeWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateExtraStrokePenaltyWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateTimeoutPenalty, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateBackgroundFilterMode, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateBackgroundColorTolerance, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateShapeAccuracyWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateColorAccuracyWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateMaximumPaintToReferenceRatio, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, TemplateOverpaintScoreCap, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, bHasValidatedStrokePayload, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, bLastStrokeValidationAccepted, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, LastStrokeValidationReason, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, StrokeValidationRevision, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ValidatedStrokeCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ValidatedPointCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ValidatedPayloadBytes, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ValidatedBrushSize, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, bHasAuthoritativeForgeryResult, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, AuthoritativeForgeryResult, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ForgeryScoreRevision, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ForgeryScoreResolution, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, ReferenceMaskPixelCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistForgeryComponent, SubmittedMaskPixelCount, COND_OwnerOnly);
}
