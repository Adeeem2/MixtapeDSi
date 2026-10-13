Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
}
"@
$emu = "$env:TEMP\desmume\DeSmuME_0.9.13_x64.exe"
$rom = "D:\PROJECT\ndspotify\mixtapedsi.nds"
$fat = "$env:TEMP\emusdfat"
$p = Start-Process -FilePath $emu -ArgumentList "`"$rom`" --slot1-fat-dir `"$fat`" --disable-sound" -PassThru
Start-Sleep 8
[Win]::ShowWindow($p.MainWindowHandle, 9)
[Win]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep 2
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$bmp.Save("D:\PROJECT\ndspotify\emu_shot.png")
$g.Dispose(); $bmp.Dispose()
if (-not $p.HasExited) { echo "EMU-ALIVE"; Stop-Process -Id $p.Id -Force } else { echo "EMU-EXITED rc=$($p.ExitCode)" }
