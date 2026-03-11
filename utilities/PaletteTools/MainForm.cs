using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;

namespace PaletteTools;

public class MainForm : Form
{
    private TabControl tabs = null!;

    // === Palette Shift Tab ===
    private PictureBox palShiftPreview = null!;
    private Label palShiftDropLabel = null!;
    private Panel palShiftDropZone = null!;
    private NumericUpDown nudBright = null!;
    private NumericUpDown nudDark = null!;
    private Button btnExportShift = null!;
    private Color[]? shiftPalette;
    private Color[,]? shiftTable;

    // === Color Convert Tab ===
    private PictureBox convertSourcePreview = null!;
    private PictureBox convertResultPreview = null!;
    private Panel convertImageDrop = null!;
    private Panel convertPaletteDrop = null!;
    private Label convertImageLabel = null!;
    private Label convertPaletteLabel = null!;
    private CheckBox chkDither = null!;
    private Button btnConvert = null!;
    private Button btnExportConverted = null!;
    private Bitmap? convertSource;
    private Color[]? convertPalette;
    private Bitmap? convertResult;

    public MainForm()
    {
        Text = "StarRaster Palette Tools";
        Size = new Size(960, 700);
        MinimumSize = new Size(800, 600);
        StartPosition = FormStartPosition.CenterScreen;

        tabs = new TabControl { Dock = DockStyle.Fill };
        Controls.Add(tabs);

        BuildPaletteShiftTab();
        BuildColorConvertTab();
    }

    // ───────────────────────── Palette Shift Tab ─────────────────────────

    void BuildPaletteShiftTab()
    {
        var tab = new TabPage("Palette Shift LUT");
        tabs.TabPages.Add(tab);

        // Drop zone
        palShiftDropZone = new Panel
        {
            Location = new Point(12, 12),
            Size = new Size(300, 80),
            BorderStyle = BorderStyle.FixedSingle,
            AllowDrop = true,
            BackColor = Color.FromArgb(40, 40, 40),
        };
        palShiftDropLabel = new Label
        {
            Text = "Drop palette image or .hex file here",
            ForeColor = Color.LightGray,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleCenter,
        };
        palShiftDropZone.Controls.Add(palShiftDropLabel);
        palShiftDropZone.DragEnter += DropZone_DragEnter;
        palShiftDropZone.DragDrop += PalShiftDrop_DragDrop;
        tab.Controls.Add(palShiftDropZone);

        // Settings
        var lblBright = new Label { Text = "Bright steps:", Location = new Point(330, 16), AutoSize = true };
        nudBright = new NumericUpDown { Location = new Point(430, 14), Width = 60, Minimum = 1, Maximum = 32, Value = 8 };
        nudBright.ValueChanged += (_, _) => RegenerateShiftTable();

        var lblDark = new Label { Text = "Dark steps:", Location = new Point(330, 48), AutoSize = true };
        nudDark = new NumericUpDown { Location = new Point(430, 46), Width = 60, Minimum = 1, Maximum = 32, Value = 8 };
        nudDark.ValueChanged += (_, _) => RegenerateShiftTable();

        btnExportShift = new Button { Text = "Export PNG", Location = new Point(510, 14), Size = new Size(100, 30), Enabled = false };
        btnExportShift.Click += BtnExportShift_Click;

        var btnExportC = new Button { Text = "Export C Header", Location = new Point(510, 50), Size = new Size(100, 30), Enabled = false };
        btnExportC.Tag = "exportC";
        btnExportC.Click += BtnExportShiftC_Click;

        tab.Controls.AddRange(new Control[] { lblBright, nudBright, lblDark, nudDark, btnExportShift, btnExportC });

        // Preview
        palShiftPreview = new PictureBox
        {
            Location = new Point(12, 100),
            Size = new Size(920, 400),
            SizeMode = PictureBoxSizeMode.Zoom,
            BorderStyle = BorderStyle.FixedSingle,
            BackColor = Color.FromArgb(30, 30, 30),
        };
        tab.Controls.Add(palShiftPreview);
    }

    void PalShiftDrop_DragDrop(object? sender, DragEventArgs e)
    {
        var files = (string[]?)e.Data?.GetData(DataFormats.FileDrop);
        if (files == null || files.Length == 0) return;
        string path = files[0];

        try
        {
            if (path.EndsWith(".hex", StringComparison.OrdinalIgnoreCase))
                shiftPalette = PaletteCore.LoadPaletteFromHex(path);
            else
                shiftPalette = PaletteCore.LoadPaletteFromImage(path);

            palShiftDropLabel.Text = $"{Path.GetFileName(path)}  ({shiftPalette.Length} colors)";
            RegenerateShiftTable();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to load palette:\n{ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    void RegenerateShiftTable()
    {
        if (shiftPalette == null) return;
        int bright = (int)nudBright.Value;
        int dark = (int)nudDark.Value;

        shiftTable = PaletteCore.GenerateShiftTable(shiftPalette, bright, dark);
        var bmp = PaletteCore.RenderShiftTable(shiftTable, cellSize: 16);

        palShiftPreview.Image?.Dispose();
        palShiftPreview.Image = bmp;
        btnExportShift.Enabled = true;

        // Enable the C header export button
        foreach (Control c in tabs.TabPages[0].Controls)
            if (c is Button b && b.Tag as string == "exportC") b.Enabled = true;
    }

    void BtnExportShift_Click(object? sender, EventArgs e)
    {
        if (palShiftPreview.Image == null) return;
        using var dlg = new SaveFileDialog { Filter = "PNG|*.png", FileName = "palette_shift_lut.png" };
        if (dlg.ShowDialog() == DialogResult.OK)
            palShiftPreview.Image.Save(dlg.FileName, ImageFormat.Png);
    }

    void BtnExportShiftC_Click(object? sender, EventArgs e)
    {
        if (shiftTable == null || shiftPalette == null) return;
        using var dlg = new SaveFileDialog { Filter = "C Header|*.h", FileName = "palette_lut.h" };
        if (dlg.ShowDialog() != DialogResult.OK) return;

        int rows = shiftTable.GetLength(0);
        int cols = shiftTable.GetLength(1);

        using var w = new StreamWriter(dlg.FileName);
        w.WriteLine("// Auto-generated palette shift LUT");
        w.WriteLine($"// {cols} colors x {rows} shade levels");
        w.WriteLine($"// Middle row (index {rows / 2}) = default shade");
        w.WriteLine($"#pragma once");
        w.WriteLine();
        w.WriteLine($"#define PAL_COLORS {cols}");
        w.WriteLine($"#define PAL_SHADES {rows}");
        w.WriteLine($"#define PAL_MID_ROW {rows / 2}");
        w.WriteLine();

        // Palette index table (each entry is the index into the base palette)
        w.WriteLine($"// Each entry is the palette index for [shade][color]");
        w.WriteLine($"static const unsigned char pal_shift_lut[{rows}][{cols}] = {{");
        for (int r = 0; r < rows; r++)
        {
            w.Write("    { ");
            for (int c = 0; c < cols; c++)
            {
                // Find which palette index this color maps to
                Color cell = shiftTable[r, c];
                int idx = 0;
                for (int i = 0; i < shiftPalette.Length; i++)
                    if (shiftPalette[i].ToArgb() == cell.ToArgb()) { idx = i; break; }
                w.Write(idx.ToString().PadLeft(2));
                if (c < cols - 1) w.Write(", ");
            }
            w.WriteLine(r < rows - 1 ? " }," : " }");
        }
        w.WriteLine("};");
        w.WriteLine();

        // Also emit the palette RGB values
        w.WriteLine("// Base palette RGB values (0xRRGGBB)");
        w.WriteLine($"static const unsigned int pal_colors[{cols}] = {{");
        w.Write("    ");
        for (int i = 0; i < shiftPalette.Length; i++)
        {
            Color c = shiftPalette[i];
            w.Write($"0x{c.R:X2}{c.G:X2}{c.B:X2}");
            if (i < shiftPalette.Length - 1) w.Write(", ");
        }
        w.WriteLine();
        w.WriteLine("};");
    }

    // ───────────────────────── Color Convert Tab ─────────────────────────

    void BuildColorConvertTab()
    {
        var tab = new TabPage("Image → Indexed Color");
        tabs.TabPages.Add(tab);

        // Image drop zone
        convertImageDrop = new Panel
        {
            Location = new Point(12, 12),
            Size = new Size(220, 70),
            BorderStyle = BorderStyle.FixedSingle,
            AllowDrop = true,
            BackColor = Color.FromArgb(40, 40, 40),
        };
        convertImageLabel = new Label
        {
            Text = "Drop source image here",
            ForeColor = Color.LightGray,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleCenter,
        };
        convertImageDrop.Controls.Add(convertImageLabel);
        convertImageDrop.DragEnter += DropZone_DragEnter;
        convertImageDrop.DragDrop += ConvertImageDrop_DragDrop;
        tab.Controls.Add(convertImageDrop);

        // Palette drop zone
        convertPaletteDrop = new Panel
        {
            Location = new Point(250, 12),
            Size = new Size(220, 70),
            BorderStyle = BorderStyle.FixedSingle,
            AllowDrop = true,
            BackColor = Color.FromArgb(40, 40, 40),
        };
        convertPaletteLabel = new Label
        {
            Text = "Drop palette (.png/.hex) here",
            ForeColor = Color.LightGray,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleCenter,
        };
        convertPaletteDrop.Controls.Add(convertPaletteLabel);
        convertPaletteDrop.DragEnter += DropZone_DragEnter;
        convertPaletteDrop.DragDrop += ConvertPaletteDrop_DragDrop;
        tab.Controls.Add(convertPaletteDrop);

        // Controls
        chkDither = new CheckBox { Text = "Floyd-Steinberg Dithering", Location = new Point(490, 16), AutoSize = true, Checked = true };
        btnConvert = new Button { Text = "Convert", Location = new Point(490, 46), Size = new Size(80, 28), Enabled = false };
        btnConvert.Click += BtnConvert_Click;
        btnExportConverted = new Button { Text = "Export PNG", Location = new Point(580, 46), Size = new Size(90, 28), Enabled = false };
        btnExportConverted.Click += BtnExportConverted_Click;
        tab.Controls.AddRange(new Control[] { chkDither, btnConvert, btnExportConverted });

        // Source preview
        convertSourcePreview = new PictureBox
        {
            Location = new Point(12, 90),
            Size = new Size(450, 400),
            SizeMode = PictureBoxSizeMode.Zoom,
            BorderStyle = BorderStyle.FixedSingle,
            BackColor = Color.FromArgb(30, 30, 30),
        };
        tab.Controls.Add(convertSourcePreview);

        // Result preview
        convertResultPreview = new PictureBox
        {
            Location = new Point(470, 90),
            Size = new Size(450, 400),
            SizeMode = PictureBoxSizeMode.Zoom,
            BorderStyle = BorderStyle.FixedSingle,
            BackColor = Color.FromArgb(30, 30, 30),
        };
        tab.Controls.Add(convertResultPreview);
    }

    void ConvertImageDrop_DragDrop(object? sender, DragEventArgs e)
    {
        var files = (string[]?)e.Data?.GetData(DataFormats.FileDrop);
        if (files == null || files.Length == 0) return;
        try
        {
            convertSource?.Dispose();
            convertSource = new Bitmap(files[0]);
            convertSourcePreview.Image?.Dispose();
            convertSourcePreview.Image = new Bitmap(convertSource);
            convertImageLabel.Text = $"{Path.GetFileName(files[0])}\n{convertSource.Width}x{convertSource.Height}";
            UpdateConvertButton();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to load image:\n{ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    void ConvertPaletteDrop_DragDrop(object? sender, DragEventArgs e)
    {
        var files = (string[]?)e.Data?.GetData(DataFormats.FileDrop);
        if (files == null || files.Length == 0) return;
        string path = files[0];
        try
        {
            if (path.EndsWith(".hex", StringComparison.OrdinalIgnoreCase))
                convertPalette = PaletteCore.LoadPaletteFromHex(path);
            else
                convertPalette = PaletteCore.LoadPaletteFromImage(path);

            convertPaletteLabel.Text = $"{Path.GetFileName(path)}\n{convertPalette.Length} colors";
            UpdateConvertButton();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to load palette:\n{ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    void UpdateConvertButton()
    {
        btnConvert.Enabled = convertSource != null && convertPalette != null;
    }

    void BtnConvert_Click(object? sender, EventArgs e)
    {
        if (convertSource == null || convertPalette == null) return;

        Cursor = Cursors.WaitCursor;
        try
        {
            convertResult?.Dispose();
            convertResult = PaletteCore.ConvertToIndexed(convertSource, convertPalette, chkDither.Checked);
            convertResultPreview.Image?.Dispose();
            convertResultPreview.Image = new Bitmap(convertResult);
            btnExportConverted.Enabled = true;
        }
        finally
        {
            Cursor = Cursors.Default;
        }
    }

    void BtnExportConverted_Click(object? sender, EventArgs e)
    {
        if (convertResult == null) return;
        using var dlg = new SaveFileDialog { Filter = "PNG|*.png", FileName = "converted.png" };
        if (dlg.ShowDialog() == DialogResult.OK)
            convertResult.Save(dlg.FileName, ImageFormat.Png);
    }

    // ───────────────────────── Shared ─────────────────────────

    void DropZone_DragEnter(object? sender, DragEventArgs e)
    {
        if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
            e.Effect = DragDropEffects.Copy;
    }
}
