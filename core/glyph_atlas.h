#pragma once
#include <SDL.h>
#include <string>

// CP437 glyph atlas: 16x16 grid of 8x16 cells (128x256 px total), generated
// by tools/generate_glyph_atlas.py from the oldschool-vga-8x16 bitmap font
// data (assets/fonts/oldschool-vga-8x16-fontlist.js). Glyph N lives at cell
// (N % 16, N / 16).
constexpr int kGlyphW = 8;
constexpr int kGlyphH = 16;
constexpr int kGridCols = 16;

SDL_Rect glyphSrcRect(unsigned char glyphIndex);

// Loads the atlas BMP from next to the running executable, color-keys black
// as transparent, and sets nearest-neighbor scaling. Returns nullptr on
// failure (check SDL_GetError()).
SDL_Texture* loadGlyphAtlas(SDL_Renderer* renderer, const char* fileName);
