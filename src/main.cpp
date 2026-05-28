#include "game/game.hpp"
#include "core/debug_log.hpp"

#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <locale.h>
#endif

int main()
{

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, ".UTF-8");
#endif

    try {
        core::debugLog("A", "main.cpp:main", "main_enter");
        game::Game game;
        core::debugLog("A", "main.cpp:main", "game_ctor_done");
        game.init();
        core::debugLog("A", "main.cpp:main", "game_init_done");
        game.run();
        core::debugLog("A", "main.cpp:main", "game_run_done");
        game.destroy();
        core::debugLog("A", "main.cpp:main", "game_destroy_done");
    } catch (const std::exception &e) {
        core::debugLog("E", "main.cpp:main", "std_exception", std::string("{\"what\":\"") + e.what() + "\"}");
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}