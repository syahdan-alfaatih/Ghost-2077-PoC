#include "core/ghost_core.h"
#include <windows.h>
#include <stdexcept> 

HANDLE g_hShutdownEvent = NULL;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    ShowWindow(GetConsoleWindow(), SW_HIDE);

    g_hShutdownEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_hShutdownEvent == NULL) {
        return -1; 
    }

    try {
        ghost::loader::Activate();
    } 
    catch (...) {
        if (g_hShutdownEvent) CloseHandle(g_hShutdownEvent);
        return -1; 
    }

    if (g_hShutdownEvent) {
        CloseHandle(g_hShutdownEvent);
    }

    return 0;
}