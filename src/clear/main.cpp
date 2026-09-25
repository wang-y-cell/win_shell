#include "utils/output.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

int main() {
    utils::output::init();
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (handle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(handle, &info)) {
        const DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
        DWORD written = 0;
        COORD origin{0, 0};
        FillConsoleOutputCharacterW(handle, L' ', cells, origin, &written);
        FillConsoleOutputAttribute(handle, info.wAttributes, cells, origin, &written);
        SetConsoleCursorPosition(handle, origin);
        return 0;
    }
#endif
    utils::output::write("\033[2J\033[H");
    utils::output::flush();
    return 0;
}
