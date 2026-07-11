#include "kde_display_scale.h"

// Non-KDE platforms (currently: Windows) have no KWin/D-Bus to query --
// queryKdeOutputScale() is documented as safe to call unconditionally and
// fall back to 1.0 outside a live Plasma session, so this stub keeps that
// contract true on platforms that never had a real implementation, rather
// than requiring every caller to #ifdef around it.
float queryKdeOutputScale(const char*)
{
    return 1.0f;
}
