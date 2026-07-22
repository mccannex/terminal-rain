#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <cctype>
#include <string>
#include "core/app_loop.h"

// Standard Win32 screensaver command-line convention (4.0+):
//   /s          run fullscreen (the actual screensaver)
//   /c, /c ###  show config dialog (parented to foreground window, or HWND ###)
//   /p ###      preview mode: render into a child window of HWND ###
//   /a ###      Windows 95/Plus!-era password-change dialog; vestigial today
//   (none)      same as /c with a NULL parent
// Callers vary in case, '/' vs '-' prefix, and space vs ':' before the value,
// so parsing tolerates all of that. See planning/SCR_SHELL_PLAN.md.

namespace
{
    enum class Mode { Show, Configure, Preview, Password };

    struct ParsedArgs
    {
        Mode mode = Mode::Configure;
        HWND parentHwnd = nullptr;
    };

    ParsedArgs parseCommandLine(LPSTR lpCmdLine)
    {
        ParsedArgs result;
        std::string cmd(lpCmdLine ? lpCmdLine : "");

        size_t pos = cmd.find_first_not_of(" \t");
        if (pos == std::string::npos) return result; // empty -> Configure, no parent
        if (cmd[pos] != '/' && cmd[pos] != '-') return result; // malformed -> Configure

        pos++;
        if (pos >= cmd.size()) return result;

        char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(cmd[pos])));
        pos++;

        if (pos < cmd.size() && (cmd[pos] == ':' || cmd[pos] == ' '))
            pos++;

        long hwndValue = 0;
        std::string rest = cmd.substr(pos);
        size_t valueStart = rest.find_first_not_of(" \t");
        if (valueStart != std::string::npos)
        {
            try { hwndValue = std::stol(rest.substr(valueStart)); }
            catch (...) { hwndValue = 0; }
        }
        HWND hwnd = reinterpret_cast<HWND>(static_cast<intptr_t>(hwndValue));

        switch (letter)
        {
        case 's': result.mode = Mode::Show; break;
        case 'p': result.mode = Mode::Preview; result.parentHwnd = hwnd; break;
        case 'a': result.mode = Mode::Password; result.parentHwnd = hwnd; break;
        case 'c':
        default:  result.mode = Mode::Configure; result.parentHwnd = hwnd; break;
        }
        return result;
    }

    int runShow()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

        // No per-display content-scale query on Windows yet (unlike Linux/KDE,
        // see src/sdl_app/main.cpp) -- each display renders at contentScale
        // 1.0 for now.
        int result = runMultiDisplayStreamLoop(nullptr);

        SDL_Quit();
        return result;
    }

    int runPreview(HWND parentHwnd)
    {
        if (!parentHwnd || !IsWindow(parentHwnd)) return 0;

        RECT clientRect;
        GetClientRect(parentHwnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;
        if (width <= 0 || height <= 0) return 0;

        if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

        // parentHwnd belongs to another process/thread (the Display Settings
        // dialog), so it can't be adopted directly as our own SDL window --
        // SDL_CreateWindowFrom() on a foreign-process HWND deadlocked the
        // dialog in testing. Instead, create our own window and reparent it
        // into the given HWND, same as any native Win32 screensaver preview.
        SDL_Window* window = SDL_CreateWindow(
            "Glyph Rain Preview",
            0, 0, width, height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
        if (!window)
        {
            SDL_Quit();
            return 1;
        }

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            HWND ourHwnd = wmInfo.info.win.window;
            SetParent(ourHwnd, parentHwnd);

            // Switch from a top-level popup to a real child window so it
            // behaves as embedded content rather than a window merely
            // positioned over the parent.
            LONG_PTR style = GetWindowLongPtr(ourHwnd, GWL_STYLE);
            style = (style & ~WS_POPUP) | WS_CHILD;
            SetWindowLongPtr(ourHwnd, GWL_STYLE, style);
            SetWindowPos(ourHwnd, nullptr, 0, 0, width, height,
                SWP_NOZORDER | SWP_FRAMECHANGED);
        }

        int result = runStreamLoop(window, true);

        SDL_DestroyWindow(window);
        SDL_Quit();
        return result;
    }

    int runConfigure(HWND parentHwnd)
    {
        // No settings are configurable yet -- this build uses fixed defaults.
        MessageBoxA(parentHwnd,
            "Glyph Rain has no configurable settings yet.",
            "Glyph Rain",
            MB_OK | MB_ICONINFORMATION);
        return 0;
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    ParsedArgs args = parseCommandLine(lpCmdLine);

    switch (args.mode)
    {
    case Mode::Show:     return runShow();
    case Mode::Preview:  return runPreview(args.parentHwnd);
    case Mode::Password: return 0; // vestigial, no-op
    case Mode::Configure:
    default:             return runConfigure(args.parentHwnd);
    }
}
