#include "game/game.hpp"
#include "core/debug_log.hpp"

#include <string>
#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <locale.h>
#endif

#ifdef _WIN32
LONG WINAPI VectoredExceptionHandler(EXCEPTION_POINTERS* ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        fprintf(stderr, "\n=== ACCESS VIOLATION DETECTED ===\n");
        fprintf(stderr, "Address: 0x%llX\n", (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
        fprintf(stderr, "Creating minidump...\n");
        fflush(stderr);

        HANDLE hFile = CreateFileA(
            "crash-minidump.dmp",
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION info = {
                GetCurrentThreadId(),
                ep,
                FALSE
            };
            BOOL ok = MiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                hFile,
                MiniDumpWithFullMemory,
                &info,
                NULL,
                NULL
            );
            CloseHandle(hFile);
            if (ok) {
                fprintf(stderr, "Minidump created: crash-minidump.dmp\n");
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main()
{

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    AddVectoredExceptionHandler(1, VectoredExceptionHandler);
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