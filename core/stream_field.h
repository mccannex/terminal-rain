#pragma once
#include <SDL.h>
#include <vector>

// Tunables, ported from the original matrix.cpp globals of the same names.
struct StreamFieldConfig
{
    int maxStreams = 3000;  // max number of concurrently active streams
    int backTrace = 200;    // rows behind the head guaranteed fully erased
    int leading = 50;       // min rows behind the head before random erase can happen
    int spacePad = 30;      // range added to leading for the random erase point
    int speedDelay = 5;     // max ticks a stream can wait between row-advances
    Uint8 r = 150, g = 255, b = 125; // base head color
};

// Owns one independent falling-code simulation, sized to one surface
// (window/display). Renders into its own persistent off-screen texture that
// is never cleared between frames -- only the head/dim/erase draws touch it,
// matching the original GDI version's behavior of never clearing the whole
// screen and relying on selective erase-draws for the trail effect.
class StreamField
{
public:
    StreamField(SDL_Renderer* renderer, SDL_Texture* glyphAtlas,
                int surfaceWidth, int surfaceHeight, const StreamFieldConfig& config);
    ~StreamField();

    StreamField(const StreamField&) = delete;
    StreamField& operator=(const StreamField&) = delete;

    // Advances stream lifecycle/movement and draws this tick's deltas onto
    // the persistent target texture. Does not touch the screen.
    void tick();

    SDL_Texture* targetTexture() const { return target_; }

private:
    struct Stream
    {
        int col = 0;
        int y = 0;         // head position, in pixels
        int speed = 0;     // ticks remaining before next row-advance
        int origSpeed = 0; // resets speed, and drives brightness
        bool active = false;
    };

    void spawnDespawn();
    void updateMovement();
    void render();
    void drawGlyphCell(int px, int py, unsigned char glyphIndex, Uint8 r, Uint8 g, Uint8 b);
    void eraseCell(int px, int py);

    SDL_Renderer* renderer_;
    SDL_Texture* atlas_;
    SDL_Texture* target_;
    int surfaceWidth_;
    int surfaceHeight_;
    int cols_;
    StreamFieldConfig config_;
    std::vector<Stream> streams_;
    int activeCount_ = 0;
};
