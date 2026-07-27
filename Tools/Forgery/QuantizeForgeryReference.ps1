param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$MaskOutputPath = "",

    [string]$BackgroundHex = "#000000",

    [Parameter(Mandatory = $true)]
    [string[]]$PaletteHex,

    [ValidateRange(0, 255)]
    [int]$BackgroundThreshold = 24
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if (-not ([System.Management.Automation.PSTypeName]"HeistForgeryReferenceQuantizer").Type)
{
    Add-Type -ReferencedAssemblies "System.Drawing" -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;

public static class HeistForgeryReferenceQuantizer
{
    private static Color ParseHex(string value)
    {
        string hex = value.Trim().TrimStart('#');
        if (hex.Length != 6)
        {
            throw new ArgumentException("Color must use #RRGGBB format: " + value);
        }

        return Color.FromArgb(
            255,
            int.Parse(hex.Substring(0, 2), NumberStyles.HexNumber),
            int.Parse(hex.Substring(2, 2), NumberStyles.HexNumber),
            int.Parse(hex.Substring(4, 2), NumberStyles.HexNumber));
    }

    private static int DistanceSquared(byte red, byte green, byte blue, Color target)
    {
        int deltaRed = red - target.R;
        int deltaGreen = green - target.G;
        int deltaBlue = blue - target.B;
        return deltaRed * deltaRed + deltaGreen * deltaGreen + deltaBlue * deltaBlue;
    }

    public static string Quantize(
        string inputPath,
        string outputPath,
        string maskOutputPath,
        string backgroundHex,
        string[] paletteHex,
        int backgroundThreshold)
    {
        if (paletteHex == null || paletteHex.Length < 2 || paletteHex.Length > 8)
        {
            throw new ArgumentException("PaletteHex must contain between 2 and 8 colors.");
        }

        Color background = ParseHex(backgroundHex);
        Color[] palette = new Color[paletteHex.Length];
        for (int index = 0; index < paletteHex.Length; ++index)
        {
            palette[index] = ParseHex(paletteHex[index]);
        }

        using (Bitmap source = new Bitmap(inputPath))
        using (Bitmap output = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb))
        using (Graphics graphics = Graphics.FromImage(output))
        {
            graphics.DrawImageUnscaled(source, 0, 0);

            Rectangle bounds = new Rectangle(0, 0, output.Width, output.Height);
            BitmapData outputData = output.LockBits(bounds, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
            int byteCount = Math.Abs(outputData.Stride) * outputData.Height;
            byte[] pixels = new byte[byteCount];
            Marshal.Copy(outputData.Scan0, pixels, 0, pixels.Length);

            byte[] maskPixels = new byte[byteCount];
            int thresholdSquared = backgroundThreshold * backgroundThreshold * 3;
            int foregroundPixels = 0;

            for (int offset = 0; offset < pixels.Length; offset += 4)
            {
                byte blue = pixels[offset];
                byte green = pixels[offset + 1];
                byte red = pixels[offset + 2];
                bool isBackground = DistanceSquared(red, green, blue, background) <= thresholdSquared;
                Color result = background;

                if (!isBackground)
                {
                    int bestDistance = int.MaxValue;
                    for (int paletteIndex = 0; paletteIndex < palette.Length; ++paletteIndex)
                    {
                        int candidateDistance = DistanceSquared(red, green, blue, palette[paletteIndex]);
                        if (candidateDistance < bestDistance)
                        {
                            bestDistance = candidateDistance;
                            result = palette[paletteIndex];
                        }
                    }

                    ++foregroundPixels;
                }

                pixels[offset] = result.B;
                pixels[offset + 1] = result.G;
                pixels[offset + 2] = result.R;
                pixels[offset + 3] = 255;

                byte maskValue = isBackground ? (byte)0 : (byte)255;
                maskPixels[offset] = maskValue;
                maskPixels[offset + 1] = maskValue;
                maskPixels[offset + 2] = maskValue;
                maskPixels[offset + 3] = 255;
            }

            Marshal.Copy(pixels, 0, outputData.Scan0, pixels.Length);
            output.UnlockBits(outputData);

            string outputDirectory = Path.GetDirectoryName(Path.GetFullPath(outputPath));
            Directory.CreateDirectory(outputDirectory);
            output.Save(outputPath, ImageFormat.Png);

            if (!String.IsNullOrWhiteSpace(maskOutputPath))
            {
                using (Bitmap mask = new Bitmap(output.Width, output.Height, PixelFormat.Format32bppArgb))
                {
                    BitmapData maskData = mask.LockBits(bounds, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                    Marshal.Copy(maskPixels, 0, maskData.Scan0, maskPixels.Length);
                    mask.UnlockBits(maskData);

                    string maskDirectory = Path.GetDirectoryName(Path.GetFullPath(maskOutputPath));
                    Directory.CreateDirectory(maskDirectory);
                    mask.Save(maskOutputPath, ImageFormat.Png);
                }
            }

            double foregroundRatio = (double)foregroundPixels / (output.Width * output.Height);
            return String.Format(
                CultureInfo.InvariantCulture,
                "Width={0} Height={1} Palette={2} ForegroundRatio={3:F4} Result=PASS",
                output.Width,
                output.Height,
                palette.Length,
                foregroundRatio);
        }
    }
}
'@
}

$resolvedInputPath = (Resolve-Path -LiteralPath $InputPath).Path
$resolvedOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$resolvedMaskOutputPath = if ([string]::IsNullOrWhiteSpace($MaskOutputPath))
{
    ""
}
else
{
    [System.IO.Path]::GetFullPath($MaskOutputPath)
}

$result = [HeistForgeryReferenceQuantizer]::Quantize(
    $resolvedInputPath,
    $resolvedOutputPath,
    $resolvedMaskOutputPath,
    $BackgroundHex,
    $PaletteHex,
    $BackgroundThreshold)

Write-Output "Forgery reference quantization: Input=$resolvedInputPath Output=$resolvedOutputPath Mask=$resolvedMaskOutputPath $result"
