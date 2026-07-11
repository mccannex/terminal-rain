#include "stream_field.h"
#include "glyph_atlas.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

StreamField::StreamField(SDL_Renderer* renderer, SDL_Texture* glyphAtlas,
                          int surfaceWidth, int surfaceHeight, const StreamFieldConfig& config)
    : renderer_(renderer)
    , atlas_(glyphAtlas)
    , surfaceWidth_(surfaceWidth)
    , surfaceHeight_(surfaceHeight)
    , glyphW_(std::max(1, static_cast<int>(std::lround(kGlyphW * config.contentScale))))
    , glyphH_(std::max(1, static_cast<int>(std::lround(kGlyphH * config.contentScale))))
    , cols_(surfaceWidth / glyphW_)
    , config_(config)
    , streams_(config.maxStreams)
{
    target_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_TARGET, surfaceWidth_, surfaceHeight_);
    SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND);

    // Persistent texture starts fully black, then is never cleared again --
    // only the selective head/dim/erase draws in render() touch it from here.
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, target_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_SetRenderTarget(renderer_, previousTarget);
}

StreamField::~StreamField()
{
    if (target_) SDL_DestroyTexture(target_);
}

void StreamField::spawnDespawn()
{
    if (activeCount_ < config_.maxStreams)
    {
        for (auto& s : streams_)
        {
            if (!s.active)
            {
                s.active = true;
                activeCount_++;
                s.y = 0;
                s.col = rand() % cols_;
                s.origSpeed = rand() % (config_.speedDelay + 1);
                s.speed = s.origSpeed;
                break;
            }
        }
    }

    int despawnRow = surfaceHeight_ + config_.backTrace * glyphH_;
    for (auto& s : streams_)
    {
        if (s.active && s.y > despawnRow)
        {
            s.active = false;
            activeCount_--;
            s.y = 0;
        }
    }
}

void StreamField::updateMovement()
{
    for (auto& s : streams_)
    {
        if (!s.active) continue;

        if (s.speed == 0)
        {
            s.y += glyphH_;
            s.speed = s.origSpeed;
        }
        else
        {
            s.speed--;
        }
    }
}

void StreamField::drawGlyphCell(int px, int py, int glyphIndex, Uint8 r, Uint8 g, Uint8 b)
{
    // Opaque cell overwrite: black-fill the cell first (matching GDI's
    // opaque-background TextOutW), then blit the color-keyed glyph on top.
    // Without the black-fill first, old bright pixels from a previous frame
    // would show through the gaps of the new glyph's shape.
    SDL_Rect dst{ px, py, glyphW_, glyphH_ };
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &dst);

    SDL_Rect src = glyphSrcRect(glyphIndex);
    SDL_SetTextureColorMod(atlas_, r, g, b);
    SDL_RenderCopy(renderer_, atlas_, &src, &dst);
}

void StreamField::eraseCell(int px, int py)
{
    SDL_Rect dst{ px, py, glyphW_, glyphH_ };
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &dst);
}

void StreamField::render()
{
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, target_);

    for (auto& s : streams_)
    {
        if (!s.active) continue;
        if (s.y < 0 || s.y > surfaceHeight_ + config_.backTrace * glyphH_) continue;

        int px = s.col * glyphW_;

        // Head: brightest, slower streams (higher origSpeed) are dimmer.
        int incR = config_.r / (config_.speedDelay + 1);
        int incG = config_.g / (config_.speedDelay + 1);
        int incB = config_.b / (config_.speedDelay + 1);
        Uint8 headR = static_cast<Uint8>(config_.r - s.origSpeed * incR);
        Uint8 headG = static_cast<Uint8>(config_.g - s.origSpeed * incG);
        Uint8 headB = static_cast<Uint8>(config_.b - s.origSpeed * incB);
        drawGlyphCell(px, s.y, 1 + (rand() % (glyphAtlasGlyphCount() - 1)), headR, headG, headB);

        // One row up: dimmer, same falloff scaled to a third.
        int dimIncR = (config_.r / 3) / (config_.speedDelay + 1);
        int dimIncG = (config_.g / 3) / (config_.speedDelay + 1);
        int dimIncB = (config_.b / 3) / (config_.speedDelay + 1);
        Uint8 dimR = static_cast<Uint8>(config_.r / 3 - s.origSpeed * dimIncR);
        Uint8 dimG = static_cast<Uint8>(config_.g / 3 - s.origSpeed * dimIncG);
        Uint8 dimB = static_cast<Uint8>(config_.b / 3 - s.origSpeed * dimIncB);
        drawGlyphCell(px, s.y - glyphH_, 1 + (rand() % (glyphAtlasGlyphCount() - 1)), dimR, dimG, dimB);

        // Erase behind the head: one randomized point within the leading
        // window, and one fixed point at backTrace -- the guaranteed wipe.
        int randomErase = rand() % (config_.spacePad + 1) + config_.leading;
        eraseCell(px, s.y - randomErase * glyphH_);
        eraseCell(px, s.y - config_.backTrace * glyphH_);
    }

    SDL_SetRenderTarget(renderer_, previousTarget);
}

void StreamField::tick()
{
    spawnDespawn();
    updateMovement();
    render();
}
