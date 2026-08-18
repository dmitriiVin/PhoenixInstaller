#include "App.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    App app;

    if (!app.Initialize())
        return -1;

    app.Run();

    app.Shutdown();

    return 0;
}
