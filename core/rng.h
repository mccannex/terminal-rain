#pragma once
#include <cstdint>

// Small self-contained xorshift32 PRNG. Each StreamField owns one, so every
// simulation has an independent, seedable random stream rather than sharing
// the global srand/rand state that three separate entry points each used to
// seed. Also sidesteps rand()'s implementation-defined RAND_MAX (only 32767
// on some platforms, which would bias column/glyph selection on wide atlases
// and large displays).
class Rng
{
public:
    explicit Rng(uint32_t seed) : state_(seed ? seed : 0x9e3779b9u) {}

    uint32_t next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    // Uniform in [0, bound). Caller guarantees bound > 0.
    int below(int bound)
    {
        return static_cast<int>(next() % static_cast<uint32_t>(bound));
    }

private:
    uint32_t state_;
};
