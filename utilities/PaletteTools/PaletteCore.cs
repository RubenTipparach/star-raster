using System.Drawing;
using System.Drawing.Imaging;

namespace PaletteTools;

public static class PaletteCore
{
    /// <summary>
    /// Generate a palette shift lookup table.
    /// Returns a 2D array [shadeRow, colorIndex] of Colors.
    /// Row 0 = darkest, row <paramref name="brighterSteps"/>+<paramref name="darkerSteps"/> = brightest.
    /// Middle row = original palette.
    /// </summary>
    public static Color[,] GenerateShiftTable(Color[] palette, int brighterSteps = 8, int darkerSteps = 8)
    {
        int cols = palette.Length;
        int rows = darkerSteps + 1 + brighterSteps; // dark rows + default + bright rows
        int midRow = darkerSteps; // 0-indexed middle

        var table = new Color[rows, cols];

        for (int c = 0; c < cols; c++)
        {
            table[midRow, c] = palette[c];

            // Darker rows: lerp toward black
            for (int d = 1; d <= darkerSteps; d++)
            {
                float t = d / (float)darkerSteps;
                Color dark = LerpColor(palette[c], Color.Black, t);
                // Snap to nearest palette color
                table[midRow - d, c] = FindNearest(dark, palette);
            }

            // Brighter rows: lerp toward white
            for (int b = 1; b <= brighterSteps; b++)
            {
                float t = b / (float)brighterSteps;
                Color bright = LerpColor(palette[c], Color.White, t);
                table[midRow + b, c] = FindNearest(bright, palette);
            }
        }

        return table;
    }

    /// <summary>
    /// Render the shift table as a bitmap for preview/export.
    /// Each cell is <paramref name="cellSize"/> pixels wide/tall.
    /// </summary>
    public static Bitmap RenderShiftTable(Color[,] table, int cellSize = 16)
    {
        int rows = table.GetLength(0);
        int cols = table.GetLength(1);
        var bmp = new Bitmap(cols * cellSize, rows * cellSize, PixelFormat.Format32bppArgb);

        using var g = Graphics.FromImage(bmp);
        for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
        {
            using var brush = new SolidBrush(table[r, c]);
            g.FillRectangle(brush, c * cellSize, r * cellSize, cellSize, cellSize);
        }

        return bmp;
    }

    /// <summary>
    /// Convert an image to indexed colors using the given palette.
    /// Optionally applies Floyd-Steinberg dithering.
    /// </summary>
    public static Bitmap ConvertToIndexed(Bitmap source, Color[] palette, bool dither)
    {
        int w = source.Width, h = source.Height;
        // Work in float RGB to accumulate dither error
        float[,] bufR = new float[h, w];
        float[,] bufG = new float[h, w];
        float[,] bufB = new float[h, w];

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            Color px = source.GetPixel(x, y);
            bufR[y, x] = px.R;
            bufG[y, x] = px.G;
            bufB[y, x] = px.B;
        }

        var result = new Bitmap(w, h, PixelFormat.Format32bppArgb);

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            float r = Clamp(bufR[y, x]);
            float g = Clamp(bufG[y, x]);
            float b = Clamp(bufB[y, x]);

            Color oldColor = Color.FromArgb((int)r, (int)g, (int)b);
            Color newColor = FindNearest(oldColor, palette);
            result.SetPixel(x, y, newColor);

            if (dither)
            {
                float errR = r - newColor.R;
                float errG = g - newColor.G;
                float errB = b - newColor.B;

                DistributeError(bufR, bufG, bufB, w, h, x + 1, y,     errR, errG, errB, 7f / 16f);
                DistributeError(bufR, bufG, bufB, w, h, x - 1, y + 1, errR, errG, errB, 3f / 16f);
                DistributeError(bufR, bufG, bufB, w, h, x,     y + 1, errR, errG, errB, 5f / 16f);
                DistributeError(bufR, bufG, bufB, w, h, x + 1, y + 1, errR, errG, errB, 1f / 16f);
            }
        }

        return result;
    }

    /// <summary>
    /// Load a palette from an image file (reads unique colors left-to-right, top-to-bottom).
    /// Typically a 1-row or small swatch image.
    /// </summary>
    public static Color[] LoadPaletteFromImage(string path)
    {
        using var bmp = new Bitmap(path);
        var colors = new List<Color>();
        var seen = new HashSet<int>();

        for (int y = 0; y < bmp.Height; y++)
        for (int x = 0; x < bmp.Width; x++)
        {
            Color px = bmp.GetPixel(x, y);
            int key = px.ToArgb();
            if (seen.Add(key))
                colors.Add(Color.FromArgb(255, px.R, px.G, px.B));
        }

        return colors.ToArray();
    }

    /// <summary>
    /// Parse a .hex palette file (one hex color per line, e.g. "1a1c2c").
    /// </summary>
    public static Color[] LoadPaletteFromHex(string path)
    {
        var colors = new List<Color>();
        foreach (var raw in File.ReadAllLines(path))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#"))
                continue;
            // strip leading # if present
            string hex = line.TrimStart('#');
            if (hex.Length == 6 && int.TryParse(hex, System.Globalization.NumberStyles.HexNumber, null, out int val))
            {
                colors.Add(Color.FromArgb(255, (val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF));
            }
        }
        return colors.ToArray();
    }

    public static int FindNearestIndex(Color target, Color[] palette)
    {
        int bestIdx = 0;
        int bestDist = int.MaxValue;
        for (int i = 0; i < palette.Length; i++)
        {
            int dr = target.R - palette[i].R;
            int dg = target.G - palette[i].G;
            int db = target.B - palette[i].B;
            int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
        return bestIdx;
    }

    public static Color FindNearest(Color target, Color[] palette)
    {
        return palette[FindNearestIndex(target, palette)];
    }

    /// <summary>
    /// Convert image to indexed palette, returning per-pixel palette indices.
    /// Optionally applies Floyd-Steinberg dithering.
    /// </summary>
    public static byte[] ConvertToIndices(Bitmap source, Color[] palette, bool dither)
    {
        int w = source.Width, h = source.Height;
        float[,] bufR = new float[h, w];
        float[,] bufG = new float[h, w];
        float[,] bufB = new float[h, w];

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            Color px = source.GetPixel(x, y);
            bufR[y, x] = px.R;
            bufG[y, x] = px.G;
            bufB[y, x] = px.B;
        }

        var indices = new byte[w * h];

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            float r = Clamp(bufR[y, x]);
            float g = Clamp(bufG[y, x]);
            float b = Clamp(bufB[y, x]);

            Color oldColor = Color.FromArgb((int)r, (int)g, (int)b);
            int idx = FindNearestIndex(oldColor, palette);
            indices[y * w + x] = (byte)idx;

            if (dither)
            {
                Color newColor = palette[idx];
                float errR = r - newColor.R;
                float errG = g - newColor.G;
                float errB = b - newColor.B;

                DistributeError(bufR, bufG, bufB, w, h, x + 1, y,     errR, errG, errB, 7f / 16f);
                DistributeError(bufR, bufG, bufB, w, h, x - 1, y + 1, errR, errG, errB, 3f / 16f);
                DistributeError(bufR, bufG, bufB, w, h, x,     y + 1, errR, errG, errB, 5f / 16f);
                DistributeError(bufR, bufG, bufB, w, h, x + 1, y + 1, errR, errG, errB, 1f / 16f);
            }
        }

        return indices;
    }

    /// <summary>
    /// Generate the shift LUT as index-to-index mapping (byte array).
    /// Returns byte[rows * cols] where lut[row * cols + col] = palette index.
    /// </summary>
    public static byte[] GenerateShiftTableIndices(Color[] palette, int brighterSteps = 8, int darkerSteps = 8)
    {
        var table = GenerateShiftTable(palette, brighterSteps, darkerSteps);
        int rows = table.GetLength(0);
        int cols = table.GetLength(1);
        var lut = new byte[rows * cols];

        for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
        {
            lut[r * cols + c] = (byte)FindNearestIndex(table[r, c], palette);
        }
        return lut;
    }

    /// <summary>
    /// Export indexed texture as .idx binary: [u16 width][u16 height][w*h bytes of indices]
    /// </summary>
    public static void ExportIndexedBinary(string path, int w, int h, byte[] indices)
    {
        using var fs = File.Create(path);
        using var bw = new BinaryWriter(fs);
        bw.Write((ushort)w);
        bw.Write((ushort)h);
        bw.Write(indices);
    }

    /// <summary>
    /// Export palette + shift LUT as a C header.
    /// </summary>
    public static void ExportCHeader(string path, Color[] palette, int brightSteps, int darkSteps)
    {
        var lut = GenerateShiftTableIndices(palette, brightSteps, darkSteps);
        int rows = darkSteps + 1 + brightSteps;
        int cols = palette.Length;

        using var w = new StreamWriter(path);
        w.WriteLine("// Auto-generated palette shift LUT");
        w.WriteLine($"// {cols} colors x {rows} shade levels");
        w.WriteLine($"#pragma once");
        w.WriteLine($"#include <stdint.h>");
        w.WriteLine();
        w.WriteLine($"#define PAL_COLORS {cols}");
        w.WriteLine($"#define PAL_SHADES {rows}");
        w.WriteLine($"#define PAL_MID_ROW {darkSteps}");
        w.WriteLine();

        // RGB palette
        w.WriteLine($"static const uint32_t pal_colors[PAL_COLORS] = {{");
        w.Write("    ");
        for (int i = 0; i < palette.Length; i++)
        {
            Color c = palette[i];
            // ABGR format matching engine convention (0xAABBGGRR)
            w.Write($"0xFF{c.B:X2}{c.G:X2}{c.R:X2}");
            if (i < palette.Length - 1) w.Write(", ");
            if ((i + 1) % 8 == 0 && i < palette.Length - 1) { w.WriteLine(); w.Write("    "); }
        }
        w.WriteLine();
        w.WriteLine("};");
        w.WriteLine();

        // Shift LUT
        w.WriteLine($"static const uint8_t pal_shift_lut[PAL_SHADES][PAL_COLORS] = {{");
        for (int r = 0; r < rows; r++)
        {
            w.Write("    { ");
            for (int c = 0; c < cols; c++)
            {
                w.Write($"{lut[r * cols + c],2}");
                if (c < cols - 1) w.Write(", ");
            }
            w.Write(r < rows - 1 ? " },\n" : " }\n");
        }
        w.WriteLine("};");
    }

    static Color LerpColor(Color a, Color b, float t)
    {
        return Color.FromArgb(
            255,
            (int)(a.R + (b.R - a.R) * t),
            (int)(a.G + (b.G - a.G) * t),
            (int)(a.B + (b.B - a.B) * t));
    }

    static void DistributeError(float[,] r, float[,] g, float[,] b,
        int w, int h, int x, int y, float er, float eg, float eb, float factor)
    {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        r[y, x] += er * factor;
        g[y, x] += eg * factor;
        b[y, x] += eb * factor;
    }

    static float Clamp(float v) => Math.Max(0, Math.Min(255, v));
}
