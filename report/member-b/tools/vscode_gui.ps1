param([string]$Clicks='', [string]$Paste='', [string]$Output='gui-check.png', [int]$Delay=1)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;using System.Runtime.InteropServices;public class V{[DllImport("user32.dll")]public static extern bool SetProcessDPIAware();[DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);[DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);[DllImport("user32.dll")]public static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out RECT r); public struct RECT{public int Left,Top,Right,Bottom;}}
'@
[V]::SetProcessDPIAware()|Out-Null
$p=Get-Process Code|Where-Object {$_.MainWindowTitle -like '*NKU-OS-2026-lab2-integration*'}|Select-Object -First 1
[V]::SetForegroundWindow($p.MainWindowHandle)|Out-Null
foreach($point in $Clicks.Split(';')){if(!$point){continue};$xy=$point.Split(',');[V]::SetCursorPos([int]$xy[0],[int]$xy[1])|Out-Null;[V]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[V]::mouse_event(4,0,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 700}
if($Paste){[System.Windows.Forms.SendKeys]::SendWait('^u');Set-Clipboard -Value $Paste;[System.Windows.Forms.SendKeys]::SendWait('^v');Start-Sleep -Milliseconds 300;[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')}
Start-Sleep -Seconds $Delay
$r=New-Object V+RECT;[V]::GetWindowRect($p.MainWindowHandle,[ref]$r)|Out-Null
$b=New-Object System.Drawing.Bitmap(($r.Right-$r.Left),($r.Bottom-$r.Top));$g=[System.Drawing.Graphics]::FromImage($b);$g.CopyFromScreen($r.Left,$r.Top,0,0,$b.Size);$b.Save("D:\NKU-OS-2026-lab2-integration\report\images\member-b\$Output");$g.Dispose();$b.Dispose()
