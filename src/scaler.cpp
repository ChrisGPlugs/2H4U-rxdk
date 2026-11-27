#include "scaler.h"

#include <stdlib.h>
#include <string.h>

namespace {

enum Mode { MODE_COPY, MODE_NEAREST, MODE_BILINEAR };

Mode      g_mode = MODE_COPY;
int       g_srcW = 0, g_srcH = 0;
ScaleRect g_rect = { 0, 0, 0, 0 };

// Nearest: source column/row for each destination column/row.
int*      g_xmap = NULL;
int*      g_ymap = NULL;

// Bilinear: left/top source index and 0..256 weight of the next pixel.
int*      g_x0 = NULL;
int*      g_fx = NULL;
int*      g_y0 = NULL;
int*      g_fy = NULL;

// Cached scratch rows (never read from video memory).
uint32_t* g_row  = NULL;
uint32_t* g_rowA = NULL;
uint32_t* g_rowB = NULL;

void freeTables()
{
    free(g_xmap); free(g_ymap); free(g_x0); free(g_fx); free(g_y0); free(g_fy);
    free(g_row); free(g_rowA); free(g_rowB);
    g_xmap = g_ymap = g_x0 = g_fx = g_y0 = g_fy = NULL;
    g_row = g_rowA = g_rowB = NULL;
}

// Blend two XRGB pixels, f = weight of b in 0..256.
inline uint32_t lerp(uint32_t a, uint32_t b, uint32_t f)
{
    uint32_t inv = 256 - f;
    uint32_t rb = (((a & 0x00FF00FF) * inv + (b & 0x00FF00FF) * f) >> 8) & 0x00FF00FF;
    uint32_t g  = (((a >> 8) & 0x00FF00FF) * inv + ((b >> 8) & 0x00FF00FF) * f) & 0xFF00FF00;
    return rb | g;
}

// Map destination coordinate d (0..dstLen-1) to source space, pixel centres aligned.
// Result is 24.8 fixed point.
inline int srcPos256(int d, int srcLen, int dstLen)
{
    return (int)(((2LL * d + 1) * srcLen * 256) / (2LL * dstLen)) - 128;
}

} // namespace

ScaleRect Scaler_Configure(int srcW, int srcH, int outW, int outH)
{
    freeTables();
    g_srcW = srcW;
    g_srcH = srcH;

    // Largest 4:3 (srcW:srcH) rectangle that fits, centred.
    int w = outW;
    int h = (int)((long long)outW * srcH / srcW);
    if (h > outH)
    {
        h = outH;
        w = (int)((long long)outH * srcW / srcH);
    }
    w &= ~1;
    h &= ~1;
    g_rect.w = w;
    g_rect.h = h;
    g_rect.x = (outW - w) / 2;
    g_rect.y = (outH - h) / 2;

    if (w == srcW && h == srcH)
    {
        g_mode = MODE_COPY;
    }
    else if (w > srcW)
    {
        g_mode = MODE_NEAREST;
        g_xmap = (int*)malloc(sizeof(int) * w);
        g_ymap = (int*)malloc(sizeof(int) * h);
        g_row  = (uint32_t*)malloc(sizeof(uint32_t) * w);
        for (int x = 0; x < w; x++) g_xmap[x] = (int)((long long)x * srcW / w);
        for (int y = 0; y < h; y++) g_ymap[y] = (int)((long long)y * srcH / h);
    }
    else
    {
        g_mode = MODE_BILINEAR;
        g_x0 = (int*)malloc(sizeof(int) * w);
        g_fx = (int*)malloc(sizeof(int) * w);
        g_y0 = (int*)malloc(sizeof(int) * h);
        g_fy = (int*)malloc(sizeof(int) * h);
        g_row  = (uint32_t*)malloc(sizeof(uint32_t) * w);
        g_rowA = (uint32_t*)malloc(sizeof(uint32_t) * w);
        g_rowB = (uint32_t*)malloc(sizeof(uint32_t) * w);
        for (int x = 0; x < w; x++)
        {
            int p = srcPos256(x, srcW, w);
            if (p < 0) p = 0;
            int i = p >> 8, f = p & 255;
            if (i >= srcW - 1) { i = srcW - 2; f = 256; }
            g_x0[x] = i; g_fx[x] = f;
        }
        for (int y = 0; y < h; y++)
        {
            int p = srcPos256(y, srcH, h);
            if (p < 0) p = 0;
            int i = p >> 8, f = p & 255;
            if (i >= srcH - 1) { i = srcH - 2; f = 256; }
            g_y0[y] = i; g_fy[y] = f;
        }
    }
    return g_rect;
}

static void hlerpRow(const uint32_t* s, uint32_t* out, int w)
{
    for (int x = 0; x < w; x++)
    {
        int i = g_x0[x];
        out[x] = lerp(s[i], s[i + 1], g_fx[x]);
    }
}

void Scaler_Run(const uint32_t* src, int srcPitch, uint32_t* dst, int dstPitch)
{
    const int w = g_rect.w, h = g_rect.h;
    uint32_t* out = dst + g_rect.y * dstPitch + g_rect.x;

    switch (g_mode)
    {
    case MODE_COPY:
        for (int y = 0; y < h; y++)
            memcpy(out + y * dstPitch, src + y * srcPitch, w * sizeof(uint32_t));
        break;

    case MODE_NEAREST:
    {
        int lastSy = -1;
        for (int y = 0; y < h; y++)
        {
            int sy = g_ymap[y];
            if (sy != lastSy)
            {
                const uint32_t* s = src + sy * srcPitch;
                for (int x = 0; x < w; x++) g_row[x] = s[g_xmap[x]];
                lastSy = sy;
            }
            memcpy(out + y * dstPitch, g_row, w * sizeof(uint32_t));
        }
        break;
    }

    case MODE_BILINEAR:
    {
        // rowA/rowB hold horizontally-filtered source rows y0 and y0+1.
        int haveA = -1, haveB = -1;
        for (int y = 0; y < h; y++)
        {
            int y0 = g_y0[y];
            if (haveA != y0)
            {
                if (haveB == y0) { uint32_t* t = g_rowA; g_rowA = g_rowB; g_rowB = t; haveA = y0; haveB = -1; }
                else { hlerpRow(src + y0 * srcPitch, g_rowA, w); haveA = y0; }
            }
            if (haveB != y0 + 1)
            {
                hlerpRow(src + (y0 + 1) * srcPitch, g_rowB, w);
                haveB = y0 + 1;
            }
            int f = g_fy[y];
            for (int x = 0; x < w; x++) g_row[x] = lerp(g_rowA[x], g_rowB[x], f);
            memcpy(out + y * dstPitch, g_row, w * sizeof(uint32_t));
        }
        break;
    }
    }
}

void Scaler_Shutdown()
{
    freeTables();
}
