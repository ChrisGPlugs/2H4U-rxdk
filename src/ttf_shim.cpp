#include "ttf_shim.h"
#include "platform.h"

#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#define STBTT_malloc(x, u) ((void)(u), SDL_malloc(x))
#define STBTT_free(x, u)   ((void)(u), SDL_free(x))
#define STBTT_assert(x)    SDL_assert(x)
#include "../third_party/stb_truetype.h"

struct TTF_Font
{
    void*          data;
    stbtt_fontinfo info;
    float          scale;
    int            ascent;   // pixels above the baseline
    int            height;   // ascent - descent, in pixels
};

namespace {

// Next code point; invalid UTF-8 falls back to the byte as Latin-1.
Uint32 nextCodepoint(const unsigned char*& s)
{
    const unsigned char c = *s;
    int extra = 0;
    Uint32 cp = 0;
    if (c < 0x80)                { s++; return c; }
    else if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else                         { s++; return c; }

    for (int i = 1; i <= extra; i++)
    {
        if ((s[i] & 0xC0) != 0x80) { s++; return c; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    s += extra + 1;
    return cp;
}

} // namespace

int TTF_Init(void)
{
    return 0;
}

void TTF_Quit(void)
{
}

TTF_Font* TTF_OpenFont(const char* file, int ptsize)
{
    size_t size = 0;
    void* data = SDL_LoadFile(file, &size);
    if (!data)
    {
        Platform_Log("2H4U: can't read font %s: %s\n", file, SDL_GetError());
        return NULL;
    }

    TTF_Font* font = (TTF_Font*)SDL_calloc(1, sizeof(TTF_Font));
    if (!font || !stbtt_InitFont(&font->info, (const unsigned char*)data,
                                 stbtt_GetFontOffsetForIndex((const unsigned char*)data, 0)))
    {
        Platform_Log("2H4U: bad font %s\n", file);
        SDL_free(font);
        SDL_free(data);
        return NULL;
    }
    font->data = data;
    // FreeType's character size at 72 dpi, as SDL_ttf sets it: an em of ptsize pixels.
    font->scale = stbtt_ScaleForMappingEmToPixels(&font->info, (float)ptsize);

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &lineGap);
    font->ascent = (int)ceilf(ascent * font->scale);
    font->height = font->ascent + (int)ceilf(-descent * font->scale);
    if (font->height < 1) font->height = 1;
    return font;
}

void TTF_CloseFont(TTF_Font* font)
{
    if (!font) return;
    SDL_free(font->data);
    SDL_free(font);
}

SDL_Surface* TTF_RenderText_Blended(TTF_Font* font, const char* text, SDL_Color fg)
{
    if (!font || !text) return NULL;

    // First pass: width (advances and kerning, plus anything a glyph's
    // bitmap sticks out past the pen at either end).
    float pen = 0.0f;
    int minX = 0, maxX = 0;
    Uint32 prev = 0;
    for (const unsigned char* s = (const unsigned char*)text; *s; )
    {
        const Uint32 cp = nextCodepoint(s);
        if (prev) pen += font->scale * stbtt_GetCodepointKernAdvance(&font->info, prev, cp);
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font->info, cp, &advance, &lsb);
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBoxSubpixel(&font->info, cp, font->scale, font->scale,
                                            pen - floorf(pen), 0.0f, &x0, &y0, &x1, &y1);
        const int left = (int)floorf(pen) + x0, right = (int)floorf(pen) + x1;
        if (left < minX) minX = left;
        if (right > maxX) maxX = right;
        pen += advance * font->scale;
        prev = cp;
    }
    if ((int)ceilf(pen) > maxX) maxX = (int)ceilf(pen);
    const int width = (maxX - minX) > 0 ? (maxX - minX) : 1;   // SDL_ttf fails on empty text; the game expects a surface
    const int height = font->height;

    Uint8* coverage = (Uint8*)SDL_calloc((size_t)width * height, 1);
    SDL_Surface* surf = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    if (!coverage || !surf)
    {
        SDL_free(coverage);
        SDL_DestroySurface(surf);
        return NULL;
    }

    // Second pass: rasterise each glyph and keep the strongest coverage
    // where neighbours overlap.
    pen = (float)-minX;
    prev = 0;
    for (const unsigned char* s = (const unsigned char*)text; *s; )
    {
        const Uint32 cp = nextCodepoint(s);
        if (prev) pen += font->scale * stbtt_GetCodepointKernAdvance(&font->info, prev, cp);
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font->info, cp, &advance, &lsb);

        const float shift = pen - floorf(pen);
        int gw = 0, gh = 0, xoff = 0, yoff = 0;
        unsigned char* glyph = stbtt_GetCodepointBitmapSubpixel(&font->info, font->scale, font->scale,
                                                                shift, 0.0f, cp, &gw, &gh, &xoff, &yoff);
        if (glyph)
        {
            const int gx = (int)floorf(pen) + xoff;
            const int gy = font->ascent + yoff;
            for (int y = 0; y < gh; y++)
            {
                const int ty = gy + y;
                if (ty < 0 || ty >= height) continue;
                for (int x = 0; x < gw; x++)
                {
                    const int tx = gx + x;
                    if (tx < 0 || tx >= width) continue;
                    Uint8& c = coverage[(size_t)ty * width + tx];
                    const Uint8 g = glyph[y * gw + x];
                    if (g > c) c = g;
                }
            }
            stbtt_FreeBitmap(glyph, NULL);
        }
        pen += advance * font->scale;
        prev = cp;
    }

    const Uint32 rgb = ((Uint32)fg.r << 16) | ((Uint32)fg.g << 8) | fg.b;
    for (int y = 0; y < height; y++)
    {
        Uint32* row = (Uint32*)((Uint8*)surf->pixels + (size_t)y * surf->pitch);
        const Uint8* cov = coverage + (size_t)y * width;
        for (int x = 0; x < width; x++)
            row[x] = ((Uint32)cov[x] << 24) | rgb;
    }
    SDL_free(coverage);
    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);
    return surf;
}
