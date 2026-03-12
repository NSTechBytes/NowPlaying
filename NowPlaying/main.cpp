#include <NovadeskAPI/novadesk_addon.h>

const NovadeskHostAPI* g_Host = nullptr;

// Called once when the addon is loaded by Novadesk.
NOVADESK_ADDON_INIT(ctx, hMsgWnd, host) {
    (void)hMsgWnd;
    g_Host = host;

    novadesk::Addon addon(ctx, host);
    addon.RegisterString("name", "MyPlugin");
    addon.RegisterString("version", "1.0.0");

    addon.RegisterFunction("hello", [](novadesk_context ctx) -> int {
        g_Host->PushString(ctx, "Hello from MyPlugin");
        return 1;
    });
}

// Optional cleanup when addon unloads.
NOVADESK_ADDON_UNLOAD() {
}
