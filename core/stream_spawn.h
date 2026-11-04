#pragma once
#include <cstdint>

namespace streamsim
{
    constexpr int kTickMilliseconds = 50;
    constexpr int kBaselineColumns = 160; // 1280 pixels with native 8-pixel glyphs
    constexpr int kBaselineStreamsPerSecond = 20;

    // Scale arrivals by glyph columns, rather than scaling only the capacity.
    // Integer credit preserves fractional rates without drift or random bursts.
    class StreamSpawnRate
    {
    public:
        explicit StreamSpawnRate(int columns) : columns_(columns) {}

        int next()
        {
            constexpr int denominator = kBaselineColumns * 1000;
            credit_ += static_cast<int64_t>(columns_) *
                kBaselineStreamsPerSecond * kTickMilliseconds;
            const int count = static_cast<int>(credit_ / denominator);
            credit_ %= denominator;
            return count;
        }

    private:
        int columns_;
        int64_t credit_ = 0;
    };
}
