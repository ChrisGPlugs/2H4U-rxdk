/*
    Stand-in for the parts of SDL_ttf that 2H4U uses (RXDK has no SDL3_ttf
    or FreeType). Fonts are rasterised with stb_truetype.

    Sizes follow SDL_ttf: TTF_OpenFont(file, n) gives an em of n pixels, and
    rendered surfaces are as tall as the font (ascent - descent), with the
    baseline at the ascent.
*/
#ifndef TTF_SHIM_H
#define TTF_SHIM_H

#include <SDL3/SDL.h>

typedef struct TTF_Font TTF_Font;

int          TTF_Init(void);
void         TTF_Quit(void);
TTF_Font*    TTF_OpenFont(const char* file, int ptsize);
void         TTF_CloseFont(TTF_Font* font);

// Text is UTF-8 (bytes that aren't valid UTF-8 are read as Latin-1).
// Returns an ARGB8888 surface in the given colour, with the glyph coverage
// in alpha and blending enabled.
SDL_Surface* TTF_RenderText_Blended(TTF_Font* font, const char* text, SDL_Color fg);

#endif
