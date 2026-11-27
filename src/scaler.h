/*
    Software scaler: draws the 800x600 game frame into a 32-bit XRGB output
    buffer of any size, keeping the 4:3 aspect ratio and centering it.

    - Upscaling (720p, 1080i) uses nearest-neighbour, which is cheap enough
      for the Xbox CPU at 60 fps.
    - Downscaling (640x480 SD) uses bilinear filtering so thin lines and text
      don't drop out.
    - 1:1 (800x600 over VGA) is a straight row copy.

    The destination may be write-combined video memory, so the scaler only
    ever writes to it sequentially and never reads it back.
*/
#ifndef SCALER_H
#define SCALER_H

#include <stdint.h>

struct ScaleRect { int x, y, w, h; };

// Work out the destination rectangle and lookup tables for an output size.
// Returns the rectangle the picture will occupy (the rest stays black).
ScaleRect Scaler_Configure(int srcW, int srcH, int outW, int outH);

// Scale one frame. src/dst pitches are in pixels.
void Scaler_Run(const uint32_t* src, int srcPitch, uint32_t* dst, int dstPitch);

void Scaler_Shutdown();

#endif
