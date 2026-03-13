#include <NovadeskAPI/novadesk_addon.h>
#include "MediaController.h"
#include <memory>

const NovadeskHostAPI* g_Host = nullptr;
std::unique_ptr<MediaController> g_Controller;

namespace
{
    MediaController& GetController()
    {
        if (!g_Controller)
        {
            g_Controller = std::make_unique<MediaController>();
        }
        return *g_Controller;
    }

    int JsNowPlayingStats(novadesk_context ctx)
    {
        const MediaStats stats = GetController().GetStats();
        g_Host->PushObject(ctx);
        g_Host->RegisterBool(ctx, "available", stats.available ? 1 : 0);
        g_Host->RegisterString(ctx, "player", stats.player.c_str());
        g_Host->RegisterString(ctx, "artist", stats.artist.c_str());
        g_Host->RegisterString(ctx, "album", stats.album.c_str());
        g_Host->RegisterString(ctx, "title", stats.title.c_str());
        g_Host->RegisterString(ctx, "thumbnail", stats.thumbnail.c_str());
        g_Host->RegisterNumber(ctx, "duration", stats.duration);
        g_Host->RegisterNumber(ctx, "position", stats.position);
        g_Host->RegisterNumber(ctx, "progress", stats.progress);
        g_Host->RegisterNumber(ctx, "state", stats.state);
        g_Host->RegisterNumber(ctx, "status", stats.status);
        g_Host->RegisterBool(ctx, "shuffle", stats.shuffle ? 1 : 0);
        g_Host->RegisterBool(ctx, "repeat", stats.repeat ? 1 : 0);
        g_Host->RegisterString(ctx, "genres", stats.genres.c_str());
        return 1;
    }

    int JsNowPlayingBackend(novadesk_context ctx)
    {
        g_Host->PushString(ctx, "smtc_v2"); // Updated backend name to reflect rewrite
        return 1;
    }

    int JsNowPlayingPlay(novadesk_context ctx) { GetController().QueueAction(MediaAction::Play); g_Host->PushBool(ctx, 1); return 1; }
    int JsNowPlayingPause(novadesk_context ctx) { GetController().QueueAction(MediaAction::Pause); g_Host->PushBool(ctx, 1); return 1; }
    int JsNowPlayingPlayPause(novadesk_context ctx) { GetController().QueueAction(MediaAction::PlayPause); g_Host->PushBool(ctx, 1); return 1; }
    int JsNowPlayingStop(novadesk_context ctx) { GetController().QueueAction(MediaAction::Stop); g_Host->PushBool(ctx, 1); return 1; }
    int JsNowPlayingNext(novadesk_context ctx) { GetController().QueueAction(MediaAction::Next); g_Host->PushBool(ctx, 1); return 1; }
    int JsNowPlayingPrevious(novadesk_context ctx) { GetController().QueueAction(MediaAction::Previous); g_Host->PushBool(ctx, 1); return 1; }

    int JsNowPlayingSetPosition(novadesk_context ctx)
    {
        if (g_Host->GetTop(ctx) < 1 || !g_Host->IsNumber(ctx, 0))
        {
            g_Host->ThrowError(ctx, "nowPlaying.setPosition(value[, isPercent])");
            return 0;
        }
        int value = static_cast<int>(g_Host->GetNumber(ctx, 0));
        bool isPercent = false;
        if (g_Host->GetTop(ctx) > 1 && !g_Host->IsNull(ctx, 1))
        {
            isPercent = g_Host->GetBool(ctx, 1) != 0;
        }
        GetController().QueueAction(MediaAction::SetPosition, value, isPercent);
        g_Host->PushBool(ctx, 1);
        return 1;
    }

    int JsNowPlayingSetShuffle(novadesk_context ctx)
    {
        if (g_Host->GetTop(ctx) < 1)
        {
            g_Host->ThrowError(ctx, "nowPlaying.setShuffle(enabled)");
            return 0;
        }
        bool enabled = g_Host->GetBool(ctx, 0) != 0;
        GetController().QueueAction(MediaAction::SetShuffle, 0, enabled);
        g_Host->PushBool(ctx, 1);
        return 1;
    }

    int JsNowPlayingSetRepeat(novadesk_context ctx)
    {
        if (g_Host->GetTop(ctx) < 1 || !g_Host->IsNumber(ctx, 0))
        {
            g_Host->ThrowError(ctx, "nowPlaying.setRepeat(mode)");
            return 0;
        }
        int mode = static_cast<int>(g_Host->GetNumber(ctx, 0));
        GetController().QueueAction(MediaAction::SetRepeat, mode);
        g_Host->PushBool(ctx, 1);
        return 1;
    }
}

NOVADESK_ADDON_INIT(ctx, hMsgWnd, host)
{
    (void)hMsgWnd;
    g_Host = host;

    novadesk::Addon addon(ctx, host);
    addon.RegisterString("name", "NowPlaying");
    addon.RegisterString("version", "2.0.0"); // Upped version
    addon.RegisterObject("nowPlaying", [](novadesk::Addon& obj) {
        obj.RegisterFunction("stats", JsNowPlayingStats, 0);
        obj.RegisterFunction("backend", JsNowPlayingBackend, 0);
        obj.RegisterFunction("play", JsNowPlayingPlay, 0);
        obj.RegisterFunction("pause", JsNowPlayingPause, 0);
        obj.RegisterFunction("playPause", JsNowPlayingPlayPause, 0);
        obj.RegisterFunction("stop", JsNowPlayingStop, 0);
        obj.RegisterFunction("next", JsNowPlayingNext, 0);
        obj.RegisterFunction("previous", JsNowPlayingPrevious, 0);
        obj.RegisterFunction("setPosition", JsNowPlayingSetPosition, 2);
        obj.RegisterFunction("setShuffle", JsNowPlayingSetShuffle, 1);
        obj.RegisterFunction("setRepeat", JsNowPlayingSetRepeat, 1);
    });
}

NOVADESK_ADDON_UNLOAD()
{
    g_Controller.reset();
}
