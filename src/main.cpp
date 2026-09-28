#include "App.h"

int main() {
    App app;

    if (!app.Initialize())
        return -1;

    app.Run();

    app.Shutdown();

    return 0;
}