# DEV: screenshot of ONE process window (the game), never the whole desktop.
# usage: powershell -File window_shot.ps1 -ProcessId PID -Out C:\path\shot.png
param([int]$ProcessId, [string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class EliteWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
$h = (Get-Process -Id $ProcessId).MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Error "no window"; exit 1 }
$r = New-Object EliteWin+RECT
[void][EliteWin]::GetClientRect($h, [ref]$r)
$p = New-Object EliteWin+POINT
[void][EliteWin]::ClientToScreen($h, [ref]$p)
$w = $r.R - $r.L; $hgt = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hgt
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($p.X, $p.Y, 0, 0, (New-Object System.Drawing.Size $w, $hgt))
$bmp.Save($Out)
