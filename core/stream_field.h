#pragma once
#include <SDL.h>
#include <cstdint>
#include <vector>
#include "rng.h"

// Fixed simulation tunables, ported from the original matrix.cpp globals of
// the same names. These were briefly matrix.cfg keys, but the screensaver
// exposes no user-facing configuration, so they live here as compile-time
// constants rather than being loaded from disk at runtime.
namespace streamsim
{
    constexpr int   kMaxStreams = 3000; // active streams at the reference resolution below
    constexpr int   kBackTrace  = 200;  // rows behind the head guaranteed fully erased
    constexpr int   kLeading    = 50;   // min rows behind the head before a random erase can happen
    constexpr int   kSpacePad   = 30;   // range added to kLeading for the random erase point
    constexpr int   kSpeedDelay = 5;    // max ticks a stream can wait between row-advances
    constexpr Uint8 kHeadR = 150, kHeadG = 255, kHeadB = 125; // base head color

    // Density reference: kMaxStreams is the count intended for a 1920x1080
    // surface. StreamField scales it by a display's pixel area so density
    // stays visually consistent -- a smaller screen isn't overcrowded and a
    // larger one isn't sparse (issue #1). kMinStreams keeps a tiny preview
    // window from ending up nearly empty.
    constexpr int   kReferenceWidth  = 1920;
    constexpr int   kReferenceHeight = 1080;
    constexpr int   kMinStreams      = 24;
}

// Owns one independent falling-code simulation, sized to one surface
// (window/display). Renders into its own persistent off-screen texture that
// is never cleared between frames -- only the head/dim/erase draws touch it,
// matching the original GDI version's behavior of never clearing the whole
// screen and relying on selective erase-draws for the trail effect.
class StreamField
{
public:
    // contentScale multiplies the glyph cell size so content stays a
    // perceptually consistent physical size across displays of differing
    // pixel density; 1.0 (the default) leaves the atlas's native 8x12 cell.
    StreamField(SDL_Renderer* renderer, SDL_Texture* glyphAtlas,
                int surfaceWidth, int surfaceHeight, float contentScale = 1.0f);
    ~StreamField();

    StreamField(const StreamField&) = delete;
    StreamField& operator=(const StreamField&) = delete;

    // Advances stream lifecycle/movement and draws this tick's deltas onto
    // the persistent target texture. Does not touch the screen.
    void tick();

    SDL_Texture* targetTexture() const { return target_; }

    // Area-scaled active-stream cap for this surface (see the density note in
    // streamsim). Exposed mainly for diagnostics/logging.
    int streamCap() const { return maxStreams_; }

private:
    struct Stream
    {
        int col = 0;               // column index
        int headRow = 0;           // head position, in glyph rows from the top
        int ticksUntilAdvance = 0; // ticks left before the head drops one row
        int advanceDelay = 0;      // reload for ticksUntilAdvance; higher = slower and dimmer
        bool active = false;
    };

    // One color-modulated glyph blit queued during render(), so all the
    // opaque black cell-fills can be flushed in a single batched pass before
    // any glyphs are drawn (see render()).
    struct GlyphDraw
    {
        SDL_Rect dst;
        int glyphIndex;
        uint32_t colorKey; // packed rgb, sorted on to minimize color-mod changes
        Uint8 r, g, b;
    };

    void spawnDespawn();
    void updateMovement();
    void render();

    SDL_Renderer* renderer_;
    SDL_Texture* atlas_;
    SDL_Texture* target_;
    int surfaceWidth_;
    int surfaceHeight_;
    int glyphW_;
    int glyphH_;
    int cols_;
    int despawnRow_;   // headRow past which a stream is recycled
    int maxStreams_;   // active stream cap for this surface
    Rng rng_;
    std::vector<Stream> streams_;
    int activeCount_ = 0;

    // Reused each frame to avoid per-frame heap churn (reserved in the ctor).
    std::vector<SDL_Rect> blackCells_;
    std::vector<GlyphDraw> glyphDraws_;
};
