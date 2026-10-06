#pragma once
#include <cmath>

// Pure timing logic; callers supply monotonic seconds and global HID idle age.
// Grace-only input cannot be distinguished from hot-corner activation, so it
// deliberately remains ignored until another post-grace event occurs.
class ZombieConfirmation
{
public:
    void start(double now) { start_ = now; active_ = true; pending_ = false; }
    void stop() { active_ = false; pending_ = false; }

    bool update(double now, double hidIdle, bool locked, bool preview)
    {
        if (!active_ || locked || preview)
        {
            pending_ = false;
            return false;
        }
        if (!pending_ && std::isfinite(hidIdle) && hidIdle >= 0 &&
            now - hidIdle > start_ + kStartupGraceSeconds)
        {
            pending_ = true;
            confirmationStart_ = now;
        }
        return pending_ && now - confirmationStart_ >= kConfirmationSeconds;
    }

private:
    static constexpr double kStartupGraceSeconds = 5.0;
    static constexpr double kConfirmationSeconds = 2.0;
    double start_ = 0;
    double confirmationStart_ = 0;
    bool active_ = false;
    bool pending_ = false;
};
