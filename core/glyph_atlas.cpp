#include "glyph_atlas.h"

SDL_Rect glyphSrcRect(unsigned char glyphIndex)
{
    SDL_Rect rect;
    rect.x = (glyphIndex % kGridCols) * kGlyphW;
    rect.y = (glyphIndex / kGridCols) * kGlyphH;
    rect.w = kGlyphW;
    rect.h = kGlyphH;
    return rect;
}

SDL_Texture* loadGlyphAtlas(SDL_Renderer* renderer, const char* fileName)
{
    char* basePath = SDL_GetBasePath();
    std::string atlasPath = std::string(basePath ? basePath : "./") + fileName;
    if (basePath) SDL_free(basePath);

    SDL_Surface* atlasSurface = SDL_LoadBMP(atlasPath.c_str());
    if (!atlasSurface)
    {
        SDL_Log("SDL_LoadBMP failed for '%s': %s", atlasPath.c_str(), SDL_GetError());
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
