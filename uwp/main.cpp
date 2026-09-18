#include <Windows.h>
#include <SDL2/SDL.h>

#include <cstdio>
#include <string>

extern "C" __declspec(dllimport) void* uwp_GetWindowReference();

namespace {

using PaperBoatMain = int (*)(int, char**);

void WriteStartupMarker(const char* message, DWORD error = ERROR_SUCCESS) {
    char* prefPath = SDL_GetPrefPath(nullptr, "PaperBoat");
    if (prefPath == nullptr) {
        return;
    }

    const std::string logPath = std::string(prefPath) + "wrapper.log";
    SDL_free(prefPath);
    if (SDL_RWops* log = SDL_RWFromFile(logPath.c_str(), "ab")) {
        char line[512]{};
        const int length = std::snprintf(line, sizeof(line), "%s (GetLastError=%lu)\r\n",
                                         message, static_cast<unsigned long>(error));
        if (length > 0) {
            SDL_RWwrite(log, line, 1, static_cast<size_t>(length));
        }
        SDL_RWclose(log);
    }
}

int Bootstrap(int argc, char** argv) {
    WriteStartupMarker("Bootstrap entered");
    uwp_GetWindowReference();
    WriteStartupMarker("libuwp CoreWindow bridge initialized");

    HMODULE game = LoadPackagedLibrary(L"PaperBoat.dll", 0);
    if (game == nullptr) {
        const DWORD error = GetLastError();
        WriteStartupMarker("LoadPackagedLibrary(PaperBoat.dll) failed", error);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Paper Boat startup failed",
                                 "PaperBoat.dll could not be loaded. See wrapper.log.", nullptr);
        return static_cast<int>(error);
    }

    const auto paperBoatMain = reinterpret_cast<PaperBoatMain>(GetProcAddress(game, "SDL_main"));
    if (paperBoatMain == nullptr) {
        const DWORD error = GetLastError();
        WriteStartupMarker("GetProcAddress(SDL_main) failed", error);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Paper Boat startup failed",
                                 "PaperBoat.dll does not export SDL_main. See wrapper.log.", nullptr);
        FreeLibrary(game);
        return static_cast<int>(error);
    }

    WriteStartupMarker("PaperBoat.dll loaded; calling SDL_main");
    const int result = paperBoatMain(argc, argv);
    WriteStartupMarker("SDL_main returned", static_cast<DWORD>(result));
    FreeLibrary(game);
    return result;
}

} // namespace

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return SDL_WinRTRunApp(Bootstrap, nullptr);
}
