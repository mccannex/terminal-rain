#include "stream_field.h"
#include "glyph_atlas.h"
#include <algorithm>
#include <cmath>
#include <ctime>

using namespace streamsim;

namespace
{
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
                          int surfaceWidth, int surfaceHeight, float contentScale,
                          bool compactTrails)
    : renderer_(renderer)
    , atlas_(glyphAtlas)
    , surfaceWidth_(surfaceWidth)
    , surfaceHeight_(surfaceHeight)
    , glyphW_(std::max(1, static_cast<int>(std::lround(kGlyphW * contentScale))))
    , glyphH_(std::max(1, static_cast<int>(std::lround(kGlyphH * contentScale))))
    , cols_(std::max(1, surfaceWidth / glyphW_))
    , backTrace_(compactTrails ? std::clamp(surfaceHeight / glyphH_, 1, kBackTrace) : kBackTrace)
    , leading_(compactTrails ? std::min(kLeading, std::max(1, backTrace_ / 3)) : kLeading)
    , spacePad_(compactTrails ? std::min(kSpacePad, std::max(1, backTrace_ / 6)) : kSpacePad)
    , despawnRow_(surfaceHeight / glyphH_ + backTrace_)
    , maxStreams_(scaledStreamCount(surfaceWidth, surfaceHeight))
    , rng_(makeSeed())
    , streams_(maxStreams_)
{
    blackCells_.reserve(static_cast<size_t>(maxStreams_) * 4);
    glyphDraws_.reserve(static_cast<size_t>(maxStreams_) * 2);

    target_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_TARGET, surfaceWidth_, surfaceHeight_);
    if (!target_)
    {
        SDL_Log("Persistent texture creation failed (%dx%d): %s",
                surfaceWidth_, surfaceHeight_, SDL_GetError());
        return;
    }

    // Persistent texture starts fully black, then is never cleared again --
    // only the selective head/dim/erase draws in render() touch it from here.
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    initialized_ = SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND) == 0 &&
                   SDL_SetRenderTarget(renderer_, target_) == 0 &&
                   SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) == 0 &&
                   SDL_RenderClear(renderer_) == 0;
    if (!initialized_)
        SDL_Log("Persistent texture initialization failed: %s", SDL_GetError());
    if (SDL_SetRenderTarget(renderer_, previousTarget) != 0)
    {
        SDL_Log("Restoring render target failed: %s", SDL_GetError());
        initialized_ = false;
    }

    // Every field observes the event before any view drains the shared queue.
    // Conservatively stop all fields on either reset: their persistent image
    // cannot be assumed intact, and a device reset also invalidates the atlas.
    if (initialized_) SDL_AddEventWatch(watchRendererReset, this);
}

StreamField::~StreamField()
{
    SDL_DelEventWatch(watchRendererReset, this);
    if (target_) SDL_DestroyTexture(target_);
}

int SDLCALL StreamField::watchRendererReset(void* userdata, SDL_Event* event)
{
    if (event->type == SDL_RENDER_TARGETS_RESET || event->type == SDL_RENDER_DEVICE_RESET)
        static_cast<StreamField*>(userdata)->resetRequested_.store(true);
    return 0; // Event-watch return values do not filter the event queue.
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

bool StreamField::render()
{
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    if (SDL_SetRenderTarget(renderer_, target_) != 0)
    {
        SDL_Log("Binding persistent render target failed: %s", SDL_GetError());
        SDL_SetRenderTarget(renderer_, previousTarget);
        return false;
    }

    blackCells_.clear();
    glyphDraws_.clear();

    const int glyphChoices = glyphAtlasGlyphCount() - 1;

    for (auto& s : streams_)
    {
        if (!s.active || s.headRow > despawnRow_) continue;

        const int px = s.col * glyphW_;
        const int headPy = s.headRow * glyphH_;
        const auto visible = [this](int py)
        {
            return py < surfaceHeight_ && py + glyphH_ > 0;
        };
        const auto eraseCell = [&](int py)
        {
            if (visible(py)) blackCells_.push_back({ px, py, glyphW_, glyphH_ });
        };

        // Opaque black cell-fills: head cell, the dim cell one row up, and the
        // two erase points behind the head (one randomized within the leading
        // window, one fixed at backTrace_ -- the guaranteed wipe). Queued now,
        // flushed together below before any glyph is drawn.
        const int randomErase = rng_.below(spacePad_ + 1) + leading_;
        eraseCell(headPy);
        eraseCell(headPy - glyphH_);
        eraseCell(headPy - randomErase * glyphH_);
        eraseCell(headPy - backTrace_ * glyphH_);

        // Consume the same random choices even when a glyph is invisible,
        // preserving future visible animation. Below-screen heads still have
        // visible trailing erases, so only cull individual cells, not streams.
        const int headGlyph = 1 + rng_.below(glyphChoices);
        const int dimGlyph = 1 + rng_.below(glyphChoices);

        // Head: brightest; slower streams (higher advanceDelay) are dimmer.
        const auto head = kHeadColors[s.advanceDelay];
        const Uint8 headR = head.r;
        const Uint8 headG = head.g;
        const Uint8 headB = head.b;
        if (visible(headPy))
            glyphDraws_.push_back({ { px, headPy, glyphW_, glyphH_ },
                                    headGlyph,
                                    static_cast<uint32_t>((headR << 16) | (headG << 8) | headB),
                                    headR, headG, headB });

        // Trailing dim glyph one row up.
        const auto trail = kTrailColors[s.advanceDelay];
        const Uint8 dimR = trail.r;
        const Uint8 dimG = trail.g;
        const Uint8 dimB = trail.b;
        if (visible(headPy - glyphH_))
            glyphDraws_.push_back({ { px, headPy - glyphH_, glyphW_, glyphH_ },
                                    dimGlyph,
                                    static_cast<uint32_t>((dimR << 16) | (dimG << 8) | dimB),
                                    dimR, dimG, dimB });
    }

    // Pass 1: every opaque black fill in one batched call. Matching GDI's
    // opaque-background TextOutW, this clears each glyph cell (and erases the
    // trail points) before any glyph is blitted on top.
    bool success = SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) == 0;
    if (success && !blackCells_.empty())
        success = SDL_RenderFillRects(renderer_, blackCells_.data(),
                                     static_cast<int>(blackCells_.size())) == 0;

    // Pass 2: glyphs, sorted by color so the color-key only changes a handful
    // of times (there are just 2*(kSpeedDelay+1) distinct brightnesses), which
    // lets SDL batch long runs of copies instead of breaking on every blit.
    std::sort(glyphDraws_.begin(), glyphDraws_.end(),
              [](const GlyphDraw& a, const GlyphDraw& b) { return a.colorKey < b.colorKey; });

    uint32_t currentColor = 0xffffffffu; // force a set on the first glyph
    for (const GlyphDraw& gd : glyphDraws_)
    {
        if (!success) break;
        if (gd.colorKey != currentColor)
        {
            success = SDL_SetTextureColorMod(atlas_, gd.r, gd.g, gd.b) == 0;
            if (!success) break;
            currentColor = gd.colorKey;
        }
        SDL_Rect src = glyphSrcRect(gd.glyphIndex);
        success = SDL_RenderCopy(renderer_, atlas_, &src, &gd.dst) == 0;
    }

    if (!success) SDL_Log("Drawing stream field failed: %s", SDL_GetError());
    if (SDL_SetRenderTarget(renderer_, previousTarget) != 0)
    {
        SDL_Log("Restoring render target failed: %s", SDL_GetError());
        success = false;
    }
    return success;
}

bool StreamField::tick()
{
    if (!valid())
    {
        if (initialized_) SDL_Log("Renderer reset: stopping stream field");
        initialized_ = false;
        return false;
    }
    spawnDespawn();
    updateMovement();
    initialized_ = render();
    return valid();
}
