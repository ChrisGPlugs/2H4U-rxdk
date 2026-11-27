# Converts a PNG/JPEG/BMP into the XPR0 texture imagebld embeds as the XBE's
# title image (the thumbnail dashboards show), e.g.
#   pwsh -File tools/make-titleimage.ps1 -In original/titleimage-source.jpg -Out original/TITLEIMAGE.XBX
#
# The picture is scaled to fit (never up) and centred on a black canvas of
# -Width x -Height (powers of two; swizzled textures need them), then written
# as one swizzled X8R8G8B8 level after a 2 KB XPR0 header, the same layout as
# the XBX files the nxdk port shipped.
param(
    [Parameter(Mandatory)][string]$In,
    [Parameter(Mandatory)][string]$Out,
    [int]$Width = 256,
    [int]$Height = 128
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Log2([int]$n) {
    $l = 0; while ((1 -shl $l) -lt $n) { $l++ }
    if ((1 -shl $l) -ne $n) { throw "$n is not a power of two" }
    return $l
}
$lu = Log2 $Width; $lv = Log2 $Height

# Draw the source centred on black.
$src = [System.Drawing.Image]::FromFile((Resolve-Path $In))
$bmp = New-Object System.Drawing.Bitmap $Width, $Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Black)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$scale = [Math]::Min(1.0, [Math]::Min($Width / $src.Width, $Height / $src.Height))
$w = [int][Math]::Round($src.Width * $scale); $h = [int][Math]::Round($src.Height * $scale)
$g.DrawImage($src, [int](($Width - $w) / 2), [int](($Height - $h) / 2), $w, $h)
$g.Dispose(); $src.Dispose()

# Linear BGRA (memory order of X8R8G8B8).
$rect = New-Object System.Drawing.Rectangle 0, 0, $Width, $Height
$lock = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $bmp.PixelFormat)
$linear = New-Object byte[] ($Width * $Height * 4)
for ($y = 0; $y -lt $Height; $y++) {
    [System.Runtime.InteropServices.Marshal]::Copy([IntPtr]($lock.Scan0.ToInt64() + $y * $lock.Stride), $linear, $y * $Width * 4, $Width * 4)
}
$bmp.UnlockBits($lock); $bmp.Dispose()

# Xbox swizzle: interleave x and y bits (x in the lower bit) while both
# dimensions have bits left, then the larger one's remaining bits on top.
$xbits = New-Object System.Collections.Generic.List[int]; $ybits = New-Object System.Collections.Generic.List[int]
$next = 0
for ($b = 0; $b -lt [Math]::Max($lu, $lv); $b++) {
    if ($b -lt $lu) { $xbits.Add($next); $next++ }
    if ($b -lt $lv) { $ybits.Add($next); $next++ }
}
function Spread([int]$v, $bits) { $r = 0; for ($i = 0; $i -lt $bits.Count; $i++) { if ($v -band (1 -shl $i)) { $r = $r -bor (1 -shl $bits[$i]) } }; return $r }
$sx = foreach ($x in 0..($Width - 1)) { Spread $x $xbits }
$sy = foreach ($y in 0..($Height - 1)) { Spread $y $ybits }
$pixels = New-Object byte[] ($Width * $Height * 4)
for ($y = 0; $y -lt $Height; $y++) {
    for ($x = 0; $x -lt $Width; $x++) {
        $s = ($y * $Width + $x) * 4; $d = ($sx[$x] -bor $sy[$y]) * 4
        $pixels[$d] = $linear[$s]; $pixels[$d + 1] = $linear[$s + 1]; $pixels[$d + 2] = $linear[$s + 2]; $pixels[$d + 3] = 0xFF
    }
}

# XPR0 header: magic, total size, header size, then one D3DTexture resource
# (Common, Data, Lock, Format, Size), an 0xFFFFFFFF end marker, 0xAD padding.
$headerSize = 0x800
$format = 0x1 -bor 0x8 -bor (2 -shl 4) -bor (0x07 -shl 8) -bor (1 -shl 16) -bor ($lu -shl 20) -bor ($lv -shl 24)  # DMA A, border colour, 2D, X8R8G8B8 swizzled, 1 level
$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter $ms
$bw.Write([byte[]][char[]]'XPR0')
$bw.Write([uint32]($headerSize + $pixels.Length)); $bw.Write([uint32]$headerSize)
$bw.Write([uint32]0x00040001); $bw.Write([uint32]0); $bw.Write([uint32]0); $bw.Write([uint32]$format); $bw.Write([uint32]0)
$bw.Write([uint32]::MaxValue)
while ($ms.Length -lt $headerSize) { $bw.Write([byte]0xAD) }
$bw.Write($pixels); $bw.Flush()
[System.IO.File]::WriteAllBytes([System.IO.Path]::GetFullPath($Out, (Get-Location).Path), $ms.ToArray())
"wrote $Out ({0}x{1}, picture {2}x{3}, {4} bytes)" -f $Width, $Height, $w, $h, $ms.Length
