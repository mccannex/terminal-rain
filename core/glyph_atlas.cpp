#include "glyph_atlas.h"
#include "glyph_atlas_data.h"

SDL_Rect glyphSrcRect(unsigned char glyphIndex)
{
    SDL_Rect rect;
    rect.x = (glyphIndex % kGridCols) * kGlyphW;
    rect.y = (glyphIndex / kGridCols) * kGlyphH;
    rect.w = kGlyphW;
    rect.h = kGlyphH;
    return rect;
}

SDL_Texture* loadGlyphAtlas(SDL_Renderer* renderer)
{
    SDL_RWops* rw = SDL_RWFromConstMem(kGlyphAtlasBmpData, static_cast<int>(kGlyphAtlasBmpSize));
    if (!rw)
    {
        SDL_Log("SDL_RWFromConstMem failed: %s", SDL_GetError());
        return nullptr;
    }

    SDL_Surface* atlasSurface = SDL_LoadBMP_RW(rw, 1); // 1 = SDL closes/frees rw for us
    if (!atlasSurface)
    {
        SDL_Log("SDL_LoadBMP_RW failed: %s", SDL_GetError());
        return nullptr;
    }
    SDL_SetColorKey(atlasSurface, SDL_TRUE, SDL_MapRGB(atlasSurface->format, 0, 0, 0));

    SDL_Texture* atlasTexture = SDL_CreateTextureFromSurface(renderer, atlasSurface);
    SDL_FreeSurface(atlasSurface);
    if (atlasTexture)
    {
        SDL_SetTextureScaleMode(atlasTexture, SDL_ScaleModeNearest);
    }
    return atlasTexture;
}
