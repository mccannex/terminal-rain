// Offline frame capture for the README's animated preview.
//
// Runs the real StreamField against SDL's software renderer (no window, no
// GPU, no display needed), so every frame is pixel-identical in content to
// what the screensaver draws, at an exact 20 fps tick rate. Skips a warm-up
// period so the capture starts from a filled screen, then writes a
// seamlessly looping sequence as raw RGB24 frames for
// tools/make_preview.py to encode.
//
// Seamless loop: render loopFrames + fadeFrames frames A[0..]. Output frame
// i is A[i], except the first fadeFrames, which crossfade from A[loop + i]
// (the natural successor of the last output frame) to A[i]. Since output
// frame 0 is the end of that crossfade, it can't be written until the run
// finishes, so frames are written starting at fadeFrames and the crossfaded
// block goes last. The order is still one continuous cycle.
//
// Usage: terminal_rain_capture <width> <height> <warmupSeconds>
//            <loopSeconds> <fadeSeconds> <out.rgb>

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "core/glyph_atlas.h"
#include "core/stream_field.h"

namespace
{
    constexpr int kFps = 20; // matches kFrameIntervalMs (50 ms) in core/app_loop.cpp

    // Draws the field's persistent texture onto the output surface and
    // copies it out as tightly packed RGB24.
    void grabFrame(SDL_Renderer* renderer, SDL_Surface* surface, StreamField& field,
                   std::vector<Uint8>& rgb)
    {
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderCopy(renderer, field.targetTexture(), nullptr, nullptr);
        SDL_RenderFlush(renderer);

        const int w = surface->w;
        const int h = surface->h;
        rgb.resize(static_cast<size_t>(w) * h * 3);
        for (int y = 0; y < h; ++y)
        {
            const Uint32* row = reinterpret_cast<const Uint32*>(
                static_cast<const Uint8*>(surface->pixels) + y * surface->pitch);
            Uint8* out = &rgb[static_cast<size_t>(y) * w * 3];
            for (int x = 0; x < w; ++x)
            {
                SDL_GetRGB(row[x], surface->format, &out[x * 3], &out[x * 3 + 1], &out[x * 3 + 2]);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 7)
    {
        std::fprintf(stderr,
            "usage: %s <width> <height> <warmupSeconds> <loopSeconds> <fadeSeconds> <out.rgb>\n",
            argv[0]);
        return 2;
    }
    const int width = std::atoi(argv[1]);
    const int height = std::atoi(argv[2]);
    const int warmupFrames = static_cast<int>(std::atof(argv[3]) * kFps);
    const int loopFrames = static_cast<int>(std::atof(argv[4]) * kFps);
    const int fadeFrames = static_cast<int>(std::atof(argv[5]) * kFps);
    if (width <= 0 || height <= 0 || loopFrames <= 0 || fadeFrames < 1 ||
        fadeFrames > loopFrames)
    {
        std::fprintf(stderr, "invalid arguments\n");
        return 2;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                          SDL_PIXELFORMAT_RGBA8888);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    SDL_Texture* atlas = renderer ? loadGlyphAtlas(renderer) : nullptr;
    if (!atlas)
    {
        std::fprintf(stderr, "SDL setup failed: %s\n", SDL_GetError());
        return 1;
    }

    FILE* out = std::fopen(argv[6], "wb");
    if (!out)
    {
        std::perror(argv[6]);
        return 1;
    }

    {
        StreamField field(renderer, atlas, width, height);
        if (!field.valid()) return 1;
        std::fprintf(stderr, "%dx%d, streamCap=%d, warmup=%d frames, loop=%d, fade=%d\n",
                     width, height, field.streamCap(), warmupFrames, loopFrames, fadeFrames);

        for (int i = 0; i < warmupFrames; ++i)
            if (!field.tick()) return 1;

        std::vector<Uint8> frame;
        std::vector<std::vector<Uint8>> head(fadeFrames); // A[0 .. fadeFrames)
        const size_t frameBytes = static_cast<size_t>(width) * height * 3;

        for (int i = 0; i < loopFrames + fadeFrames; ++i)
        {
            if (!field.tick()) return 1;
            grabFrame(renderer, surface, field, frame);

            if (i < fadeFrames)
            {
                head[i] = frame;
            }
            else if (i < loopFrames)
            {
                std::fwrite(frame.data(), 1, frameBytes, out);
            }
            else
            {
                // Crossfade: weight on A[k] rises from 0 at k = 0 (pure
                // A[loop], continuing from A[loop - 1]) toward 1 at
                // k = fadeFrames, where A[fadeFrames] follows naturally.
                const int k = i - loopFrames;
                const float w = static_cast<float>(k) / fadeFrames;
                std::vector<Uint8>& a = head[k];
                for (size_t p = 0; p < frameBytes; ++p)
                {
                    a[p] = static_cast<Uint8>(frame[p] * (1.0f - w) + a[p] * w + 0.5f);
                }
            }
        }
        for (const auto& a : head) std::fwrite(a.data(), 1, frameBytes, out);
    }

    std::fclose(out);
    SDL_DestroyTexture(atlas);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    return 0;
}
