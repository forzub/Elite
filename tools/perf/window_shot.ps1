# DEV: picture of ONE process window (the game) rendered by the window itself
# (PrintWindow, PW_RENDERFULLCONTENT). It never reads screen pixels, so other
# applications' windows can never end up in the file, even when they cover
# the game.
# usage: powershell -File window_shot.ps1 -ProcessId PID -Out C:\path\shot.png
param([int]$ProcessId, [string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class EliteWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
$h = (Get-Process -Id $ProcessId).MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Error "no window"; exit 1 }
$r = New-Object EliteWin+RECT
[void][EliteWin]::GetClientRect($h, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
# 1 = PW_CLIENTONLY, 2 = PW_RENDERFULLCONTENT
$ok = [EliteWin]::PrintWindow($h, $hdc, 3)
$g.ReleaseHdc($hdc)
if (-not $ok) { Write-Error "PrintWindow failed"; exit 1 }
$bmp.Save($Out)
