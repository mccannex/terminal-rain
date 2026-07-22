#include "stream_field.h"
#include "glyph_atlas.h"
#include <algorithm>
#include <cmath>
#include <ctime>

using namespace streamsim;

namespace
{
    // Brightness falls off linearly with a stream's advanceDelay (slower
    // streams are dimmer). Every term here is built from compile-time
    // constants, so the per-channel increments are constants too -- no reason
    // to recompute them inside the render loop as the original did.
    constexpr int kHeadIncR = kHeadR / (kSpeedDelay + 1);
    constexpr int kHeadIncG = kHeadG / (kSpeedDelay + 1);
    constexpr int kHeadIncB = kHeadB / (kSpeedDelay + 1);

    // The trailing "dim" glyph one row up is a third as bright, same falloff.
    constexpr int kDimBaseR = kHeadR / 3;
    constexpr int kDimBaseG = kHeadG / 3;
    constexpr int kDimBaseB = kHeadB / 3;
    constexpr int kDimIncR = kDimBaseR / (kSpeedDelay + 1);
    constexpr int kDimIncG = kDimBaseG / (kSpeedDelay + 1);
    constexpr int kDimIncB = kDimBaseB / (kSpeedDelay + 1);

    // Distinct seed per StreamField, so multiple displays created in the same
    // second still get independent rain rather than identical patterns.
    uint32_t makeSeed()
    {
        static uint32_t counter = 0;
        return static_cast<uint32_t>(time(nullptr)) * 2654435761u + (++counter) * 40503u;
    }

    // Scale the stream count by how a surface's pixel area compares to the
    // 1920x1080 reference, so density stays visually consistent across
    // differently-sized displays (issue #1), floored so a small preview still
    // shows rain.
    int scaledStreamCount(int surfaceWidth, int surfaceHeight)
    {
        const double area = static_cast<double>(surfaceWidth) * surfaceHeight;
        const double refArea = static_cast<double>(kReferenceWidth) * kReferenceHeight;
        const int scaled = static_cast<int>(std::lround(kMaxStreams * area / refArea));
        return std::max(kMinStreams, scaled);
    }
}

StreamField::StreamField(SDL_Renderer* renderer, SDL_Texture* glyphAtlas,
                          int surfaceWidth, int surfaceHeight, float contentScale)
    : renderer_(renderer)
    , atlas_(glyphAtlas)
    , surfaceWidth_(surfaceWidth)
    , surfaceHeight_(surfaceHeight)
    , glyphW_(std::max(1, static_cast<int>(std::lround(kGlyphW * contentScale))))
    , glyphH_(std::max(1, static_cast<int>(std::lround(kGlyphH * contentScale))))
    , cols_(std::max(1, surfaceWidth / glyphW_))
    , despawnRow_(surfaceHeight / glyphH_ + kBackTrace)
    , maxStreams_(scaledStreamCount(surfaceWidth, surfaceHeight))
    , rng_(makeSeed())
    , streams_(maxStreams_)
{
    blackCells_.reserve(static_cast<size_t>(maxStreams_) * 4);
    glyphDraws_.reserve(static_cast<size_t>(maxStreams_) * 2);

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
    if (activeCount_ < maxStreams_)
    {
        for (auto& s : streams_)
        {
            if (!s.active)
            {
                s.active = true;
                activeCount_++;
                s.headRow = 0;
                s.col = rng_.below(cols_);
                s.advanceDelay = rng_.below(kSpeedDelay + 1);
                s.ticksUntilAdvance = s.advanceDelay;
                break;
            }
        }
    }

    for (auto& s : streams_)
    {
        if (s.active && s.headRow > despawnRow_)
        {
            s.active = false;
            activeCount_--;
            s.headRow = 0;
        }
    }
}

void StreamField::updateMovement()
{
    for (auto& s : streams_)
    {
        if (!s.active) continue;

        if (s.ticksUntilAdvance == 0)
        {
            s.headRow++;
            s.ticksUntilAdvance = s.advanceDelay;
        }
        else
        {
            s.ticksUntilAdvance--;
        }
    }
}

void StreamField::render()
{
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, target_);

    blackCells_.clear();
    glyphDraws_.clear();

    const int glyphChoices = glyphAtlasGlyphCount() - 1;

    for (auto& s : streams_)
    {
        if (!s.active || s.headRow > despawnRow_) continue;

        const int px = s.col * glyphW_;
        const int headPy = s.headRow * glyphH_;

        // Opaque black cell-fills: head cell, the dim cell one row up, and the
        // two erase points behind the head (one randomized within the leading
        // window, one fixed at kBackTrace -- the guaranteed wipe). Queued now,
        // flushed together below before any glyph is drawn.
        const int randomErase = rng_.below(kSpacePad + 1) + kLeading;
        blackCells_.push_back({ px, headPy, glyphW_, glyphH_ });
        blackCells_.push_back({ px, headPy - glyphH_, glyphW_, glyphH_ });
        blackCells_.push_back({ px, headPy - randomErase * glyphH_, glyphW_, glyphH_ });
        blackCells_.push_back({ px, headPy - kBackTrace * glyphH_, glyphW_, glyphH_ });

        // Head: brightest; slower streams (higher advanceDelay) are dimmer.
        const Uint8 headR = static_cast<Uint8>(kHeadR - s.advanceDelay * kHeadIncR);
        const Uint8 headG = static_cast<Uint8>(kHeadG - s.advanceDelay * kHeadIncG);
        const Uint8 headB = static_cast<Uint8>(kHeadB - s.advanceDelay * kHeadIncB);
        glyphDraws_.push_back({ { px, headPy, glyphW_, glyphH_ },
                                1 + rng_.below(glyphChoices),
                                static_cast<uint32_t>((headR << 16) | (headG << 8) | headB),
                                headR, headG, headB });

        // Trailing dim glyph one row up.
        const Uint8 dimR = static_cast<Uint8>(kDimBaseR - s.advanceDelay * kDimIncR);
        const Uint8 dimG = static_cast<Uint8>(kDimBaseG - s.advanceDelay * kDimIncG);
        const Uint8 dimB = static_cast<Uint8>(kDimBaseB - s.advanceDelay * kDimIncB);
        glyphDraws_.push_back({ { px, headPy - glyphH_, glyphW_, glyphH_ },
                                1 + rng_.below(glyphChoices),
                                static_cast<uint32_t>((dimR << 16) | (dimG << 8) | dimB),
                                dimR, dimG, dimB });
    }

    // Pass 1: every opaque black fill in one batched call. Matching GDI's
    // opaque-background TextOutW, this clears each glyph cell (and erases the
    // trail points) before any glyph is blitted on top.
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    if (!blackCells_.empty())
        SDL_RenderFillRects(renderer_, blackCells_.data(), static_cast<int>(blackCells_.size()));

    // Pass 2: glyphs, sorted by color so the color-key only changes a handful
    // of times (there are just 2*(kSpeedDelay+1) distinct brightnesses), which
    // lets SDL batch long runs of copies instead of breaking on every blit.
    std::sort(glyphDraws_.begin(), glyphDraws_.end(),
              [](const GlyphDraw& a, const GlyphDraw& b) { return a.colorKey < b.colorKey; });

    uint32_t currentColor = 0xffffffffu; // force a set on the first glyph
    for (const GlyphDraw& gd : glyphDraws_)
    {
        if (gd.colorKey != currentColor)
        {
            SDL_SetTextureColorMod(atlas_, gd.r, gd.g, gd.b);
            currentColor = gd.colorKey;
        }
        SDL_Rect src = glyphSrcRect(gd.glyphIndex);
        SDL_RenderCopy(renderer_, atlas_, &src, &gd.dst);
    }

    SDL_SetRenderTarget(renderer_, previousTarget);
}

void StreamField::tick()
{
    spawnDespawn();
    updateMovement();
    render();
}
