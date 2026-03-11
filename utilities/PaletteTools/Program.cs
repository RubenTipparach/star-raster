using System;
using System.Drawing;
using System.Windows.Forms;

namespace PaletteTools;

static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        if (args.Length > 0)
        {
            RunCli(args);
            return;
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }

    static void RunCli(string[] args)
    {
        switch (args[0].ToLower())
        {
            case "convert":
                // convert <image> <palette.hex> <output.png> [--dither] [--idx <output.idx>]
                CliConvert(args);
                break;
            case "lut":
                // lut <palette.hex> <output.h> [--bright N] [--dark N]
                CliLut(args);
                break;
            case "batch":
                // batch <palette.hex> <output_dir> [--dither] <image1> <image2> ...
                CliBatch(args);
                break;
            default:
                Console.WriteLine("Usage:");
                Console.WriteLine("  PaletteTools convert <image> <palette.hex> <output.png> [--dither] [--idx output.idx]");
                Console.WriteLine("  PaletteTools lut <palette.hex> <output.h> [--bright N] [--dark N]");
                Console.WriteLine("  PaletteTools batch <palette.hex> <output_dir> [--dither] <images...>");
                break;
        }
    }

    static Color[] LoadPalette(string path)
    {
        if (path.EndsWith(".hex", StringComparison.OrdinalIgnoreCase))
            return PaletteCore.LoadPaletteFromHex(path);
        return PaletteCore.LoadPaletteFromImage(path);
    }

    static void CliConvert(string[] args)
    {
        if (args.Length < 4) { Console.WriteLine("convert <image> <palette> <output.png> [--dither] [--idx output.idx]"); return; }
        string imagePath = args[1], palPath = args[2], outPath = args[3];
        bool dither = Array.Exists(args, a => a == "--dither");
        string? idxPath = null;
        int idxArgPos = Array.IndexOf(args, "--idx");
        if (idxArgPos >= 0 && idxArgPos + 1 < args.Length) idxPath = args[idxArgPos + 1];

        var palette = LoadPalette(palPath);
        using var source = new Bitmap(imagePath);
        Console.WriteLine($"Converting {imagePath} ({source.Width}x{source.Height}) with {palette.Length} colors{(dither ? " +dither" : "")}");

        using var result = PaletteCore.ConvertToIndexed(source, palette, dither);
        result.Save(outPath, System.Drawing.Imaging.ImageFormat.Png);
        Console.WriteLine($"  -> {outPath}");

        if (idxPath != null)
        {
            var indices = PaletteCore.ConvertToIndices(source, palette, dither);
            PaletteCore.ExportIndexedBinary(idxPath, source.Width, source.Height, indices);
            Console.WriteLine($"  -> {idxPath} (indexed binary)");
        }
    }

    static void CliLut(string[] args)
    {
        if (args.Length < 3) { Console.WriteLine("lut <palette> <output.h> [--bright N] [--dark N]"); return; }
        string palPath = args[1], outPath = args[2];
        int bright = 8, dark = 8;
        for (int i = 3; i < args.Length - 1; i++)
        {
            if (args[i] == "--bright") bright = int.Parse(args[i + 1]);
            if (args[i] == "--dark") dark = int.Parse(args[i + 1]);
        }

        var palette = LoadPalette(palPath);
        Console.WriteLine($"Generating LUT: {palette.Length} colors, {dark} dark + 1 mid + {bright} bright = {dark + 1 + bright} rows");
        PaletteCore.ExportCHeader(outPath, palette, bright, dark);
        Console.WriteLine($"  -> {outPath}");

        // Also export LUT preview PNG
        var table = PaletteCore.GenerateShiftTable(palette, bright, dark);
        var preview = PaletteCore.RenderShiftTable(table, 8);
        string previewPath = Path.ChangeExtension(outPath, ".png");
        preview.Save(previewPath, System.Drawing.Imaging.ImageFormat.Png);
        Console.WriteLine($"  -> {previewPath} (preview)");
    }

    static void CliBatch(string[] args)
    {
        if (args.Length < 4) { Console.WriteLine("batch <palette> <output_dir> [--dither] <images...>"); return; }
        string palPath = args[1], outDir = args[2];
        bool dither = false;
        var images = new System.Collections.Generic.List<string>();
        for (int i = 3; i < args.Length; i++)
        {
            if (args[i] == "--dither") { dither = true; continue; }
            images.Add(args[i]);
        }

        Directory.CreateDirectory(outDir);
        var palette = LoadPalette(palPath);
        Console.WriteLine($"Batch converting {images.Count} images with {palette.Length} colors{(dither ? " +dither" : "")}");

        foreach (var img in images)
        {
            string name = Path.GetFileNameWithoutExtension(img);
            using var source = new Bitmap(img);

            // Export palette-converted PNG
            using var result = PaletteCore.ConvertToIndexed(source, palette, dither);
            string pngOut = Path.Combine(outDir, name + ".png");
            result.Save(pngOut, System.Drawing.Imaging.ImageFormat.Png);

            // Export indexed binary
            var indices = PaletteCore.ConvertToIndices(source, palette, dither);
            string idxOut = Path.Combine(outDir, name + ".idx");
            PaletteCore.ExportIndexedBinary(idxOut, source.Width, source.Height, indices);

            Console.WriteLine($"  {img} -> {pngOut}, {idxOut}");
        }
    }
}
