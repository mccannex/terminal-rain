#pragma once

// Queries KWin's live per-output content scale factor via the org.kde.KScreen
// D-Bus interface. KDE/Plasma-specific -- this project's Linux target is
// scoped to Fedora/KDE Plasma (see platform/linux/AGENT_CONTEXT.md), so this
// dependency is in scope, but it's isolated here rather than in core/ since
// core/ is shared with the Windows build.
//
// Falls back to 1.0 (no scaling) on any failure: D-Bus/KScreen unavailable,
// the call fails, or no output matches connectorName -- so this is safe to
// call unconditionally, including outside a live Plasma session.
float queryKdeOutputScale(const char* connectorName);
