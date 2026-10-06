#include "platform/macos/zombie_confirmation.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

struct Sample { double now, idle; bool locked, preview, expected; };
struct Case { const char* name; std::vector<Sample> samples; };

int main()
{
    const Case cases[] = {
        {"continuous input", {{6,0,false,false,false}, {7,0,false,false,false},
                              {7.99,0,false,false,false}, {8,0,false,false,true}}},
        {"single input", {{6,0,false,false,false}, {7,1,false,false,false},
                          {8,2,false,false,true}}},
        {"delayed unlock retains single event", {{6,0,true,false,false},
            {9,3,true,false,false}, {12,6,false,false,false},
            {13,7,false,false,false}, {14,8,false,false,true}}},
        {"password entry stays safe", {{6,0,true,false,false},
            {9,0,true,false,false}, {20,11,true,false,false}}},
        {"lock cancels pending deadline", {{6,0,false,false,false},
            {7,0,true,false,false}, {10,3,true,false,false},
            {12,5,false,false,false}, {13,6,false,false,false},
            {14,7,false,false,true}}},
        {"preview", {{6,0,false,true,false}, {20,0,false,true,false}}},
        {"preview cancels confirmation", {{6,0,false,false,false},
            {8,0,false,true,false}, {9,0,false,false,false},
            {11,0,false,false,true}}},
        {"idle without input", {{6,20,false,false,false}, {100,114,false,false,false}}},
        {"startup grace only", {{2,0,false,false,false}, {5,3,false,false,false},
            {20,18,false,false,false}}},
        {"grace boundary and subsequent input", {{5,0,false,false,false},
            {7,2,false,false,false}, {8,0,false,false,false}, {10,0,false,false,true}}},
        {"invalid HID ages", {{6,-1,false,false,false},
            {7,std::numeric_limits<double>::infinity(),false,false,false},
            {8,std::numeric_limits<double>::quiet_NaN(),false,false,false},
            {9,0,false,false,false}, {11,0,false,false,true}}},
    };
    for (const auto& test : cases)
    {
        ZombieConfirmation state;
        state.start(0);
        for (const auto& sample : test.samples)
            if (state.update(sample.now, sample.idle, sample.locked, sample.preview)
                != sample.expected)
            {
                std::fprintf(stderr, "FAIL: %s at %.2f\n", test.name, sample.now);
                return 1;
            }
    }
    ZombieConfirmation state;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        const double start = 100 + cycle * 20;
        state.start(start);
        if (state.update(start + 6, 0, false, false)) return 1;
        state.stop(); // Healthy stop must discard pending evidence.
        if (state.update(start + 10, 4, false, false)) return 1;
        state.start(start + 10);
        if (state.update(start + 13, 7, false, false)) return 1;
        if (state.update(start + 16, 0, false, false)) return 1;
        if (!state.update(start + 18, 0, false, false)) return 1;
    }
    std::puts("Zombie confirmation cases and repeated lifecycle checks passed.");
}
