#include "game.hpp"
#include <windows.h>
#include <exception>

int main() {
    try {
        Game g;
        g.run();
    } catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Fatal exception", MB_OK | MB_ICONERROR);
    } catch (...) {
        MessageBoxA(NULL, "Unknown fatal error (non-C++ exception, e.g. access violation)",
                    "Fatal exception", MB_OK | MB_ICONERROR);
    }
    return 0;
}
