#include <NovadeskAPI/novadesk_addon.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <roapi.h>
#include <Windows.h>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <filesystem>
#include <fstream>
#include <vector>
#include <memory>

#pragma comment(lib, "runtimeobject.lib")

const NovadeskHostAPI *g_Host = nullptr;

using namespace winrt::Windows::Media;
using namespace winrt::Windows::Media::Control;

namespace
{
    struct NowPlayingStats
    {
        bool available = false;
        std::string player;
        std::string artist;
        std::string album;
        std::string title;
        std::string thumbnail;
        int duration = 0;
        int position = 0;
        int progress = 0;
        int state = 0;
        int status = 0;
        bool shuffle = false;
        bool repeat = false;
    };

    enum class NowPlayingAction
    {
        Play,
        Pause,
        PlayPause,
        Stop,
        Next,
        Previous,
        SetPosition,
        SetShuffle,
        ToggleShuffle,
        SetRepeat
    };

    struct NowPlayingActionItem
    {
        NowPlayingAction action{};
        int value = 0;
        bool flag = false;
    };

    class NowPlayingController
    {
    public:
        NowPlayingController()
        {
            m_worker = std::thread(&NowPlayingController::WorkerMain, this);
        }
        ~NowPlayingController()
        {
            {
                std::lock_guard<std::mutex> lock(m_actionMutex);
                m_stop = true;
            }
            m_actionSignal.notify_all();
            if (m_worker.joinable())
                m_worker.join();
        }

        NowPlayingStats ReadStats()
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            return m_stats;
        }

        bool Queue(NowPlayingAction action, int value = 0, bool flag = false)
        {
            std::lock_guard<std::mutex> lock(m_actionMutex);
            if (m_stop)
                return false;
            m_actions.push(NowPlayingActionItem{action, value, flag});
            m_actionSignal.notify_one();
            return true;
        }

    private:
        std::mutex m_statsMutex;
        NowPlayingStats m_stats{};

        std::mutex m_actionMutex;
        std::queue<NowPlayingActionItem> m_actions;
        std::condition_variable m_actionSignal;
        bool m_stop = false;
        std::thread m_worker;

        GlobalSystemMediaTransportControlsSessionManager m_manager{nullptr};
        std::wstring m_coverPath;
        std::wstring m_coverTrackKey;
        std::chrono::steady_clock::time_point m_localPosTime{};
        double m_localPosSec = 0.0;
        bool m_hasLocalPos = false;
        NowPlayingStats m_lastStats{};

        static int ToSeconds(winrt::Windows::Foundation::TimeSpan span)
        {
            return static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(span).count());
        }

        static std::wstring ToWide(const std::string &value)
        {
            return winrt::to_hstring(value).c_str();
        }

        GlobalSystemMediaTransportControlsSession Session()
        {
            if (m_manager == nullptr)
                return nullptr;
            try
            {
                return m_manager.GetCurrentSession();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        void EnsureManager()
        {
            if (m_manager != nullptr)
                return;
            try
            {
                m_manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            }
            catch (...)
            {
                m_manager = nullptr;
            }
        }

        void ProcessActions(std::queue<NowPlayingActionItem> &pending)
        {
            while (!pending.empty())
            {
                const auto action = pending.front();
                pending.pop();
                auto session = Session();
                if (session == nullptr)
                    continue;
                try
                {
                    switch (action.action)
                    {
                    case NowPlayingAction::Play:
                        session.TryPlayAsync();
                        break;
                    case NowPlayingAction::Pause:
                        session.TryPauseAsync();
                        break;
                    case NowPlayingAction::PlayPause:
                        session.TryTogglePlayPauseAsync();
                        break;
                    case NowPlayingAction::Stop:
                        session.TryStopAsync();
                        break;
                    case NowPlayingAction::Next:
                        session.TrySkipNextAsync();
                        break;
                    case NowPlayingAction::Previous:
                        session.TrySkipPreviousAsync();
                        break;
                    case NowPlayingAction::SetPosition:
                    {
                        auto tl = session.GetTimelineProperties();
                        int duration = 0;
                        if (tl != nullptr)
                            duration = (std::max)(0, ToSeconds(tl.EndTime()) - ToSeconds(tl.StartTime()));
                        int position = action.value;
                        if (action.flag && duration > 0)
                        {
                            const int pct = (std::min)(100, (std::max)(0, action.value));
                            position = (duration * pct + 50) / 100;
                        }
                        const int64_t ticks = static_cast<int64_t>((std::max)(0, position)) * 10000000LL;
                        session.TryChangePlaybackPositionAsync(ticks);
                        break;
                    }
                    case NowPlayingAction::SetShuffle:
                        session.TryChangeShuffleActiveAsync(action.flag);
                        break;
                    case NowPlayingAction::ToggleShuffle:
                    {
                        auto pb = session.GetPlaybackInfo();
                        bool enabled = true;
                        if (pb != nullptr)
                        {
                            auto sh = pb.IsShuffleActive();
                            if (sh != nullptr)
                                enabled = !sh.Value();
                        }
                        session.TryChangeShuffleActiveAsync(enabled);
                        break;
                    }
                    case NowPlayingAction::SetRepeat:
                    {
                        MediaPlaybackAutoRepeatMode repeatMode = MediaPlaybackAutoRepeatMode::None;
                        if (action.value == 1)
                            repeatMode = MediaPlaybackAutoRepeatMode::Track;
                        else if (action.value == 2)
                            repeatMode = MediaPlaybackAutoRepeatMode::List;
                        session.TryChangeAutoRepeatModeAsync(repeatMode);
                        break;
                    }
                    }
                }
                catch (...)
                {
                }
            }
        }

        void RefreshStats()
        {
            NowPlayingStats stats{};
            auto session = Session();
            if (session != nullptr)
            {
                stats.available = true;
                stats.status = 1;
                stats.player = winrt::to_string(session.SourceAppUserModelId());
                const auto nowSteady = std::chrono::steady_clock::now();

                try
                {
                    auto props = session.TryGetMediaPropertiesAsync().get();
                    if (props != nullptr)
                    {
                        stats.artist = winrt::to_string(props.Artist());
                        stats.album = winrt::to_string(props.AlbumTitle());
                        stats.title = winrt::to_string(props.Title());
                        const std::wstring trackKey =
                            ToWide(stats.player) + L"|" +
                            ToWide(stats.artist) + L"|" +
                            ToWide(stats.album) + L"|" +
                            ToWide(stats.title);
                        auto thumb = props.Thumbnail();
                        if (thumb != nullptr)
                        {
                            if (trackKey != m_coverTrackKey || m_coverPath.empty())
                            {
                                if (SaveThumbnail(thumb))
                                    m_coverTrackKey = trackKey;
                            }
                            if (!m_coverPath.empty())
                                stats.thumbnail = winrt::to_string(m_coverPath);
                        }
                        else
                        {
                            m_coverTrackKey.clear();
                            m_coverPath.clear();
                        }
                    }
                }
                catch (...)
                {
                }

                try
                {
                    auto tl = session.GetTimelineProperties();
                    if (tl != nullptr)
                    {
                        stats.duration = (std::max)(0, ToSeconds(tl.EndTime()) - ToSeconds(tl.StartTime()));
                        stats.position = (std::max)(0, ToSeconds(tl.Position()));
                        if (stats.duration > 0)
                        {
                            stats.position = (std::min)(stats.position, stats.duration);
                            stats.progress = (std::min)(100, (stats.position * 100) / stats.duration);
                        }
                    }
                }
                catch (...)
                {
                }

                try
                {
                    auto pb = session.GetPlaybackInfo();
                    if (pb != nullptr)
                    {
                        auto ps = pb.PlaybackStatus();
                        if (ps == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
                            stats.state = 1;
                        else if (ps == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused)
                            stats.state = 2;
                        else
                            stats.state = 0;
                        auto sh = pb.IsShuffleActive();
                        if (sh != nullptr)
                            stats.shuffle = sh.Value();
                        auto rp = pb.AutoRepeatMode();
                        if (rp != nullptr)
                            stats.repeat = rp.Value() != MediaPlaybackAutoRepeatMode::None;
                    }
                }
                catch (...)
                {
                }

                if (stats.state == 1 && stats.duration > 0)
                {
                    try
                    {
                        auto tl = session.GetTimelineProperties();
                        if (tl != nullptr)
                        {
                            auto last = tl.LastUpdatedTime().time_since_epoch();
                            auto now = winrt::clock::now().time_since_epoch();
                            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - last).count());
                            if (elapsed > 0 && elapsed <= 3)
                                stats.position += elapsed;
                        }
                    }
                    catch (...)
                    {
                    }

                    const bool sameTrack =
                        !m_lastStats.title.empty() &&
                        m_lastStats.player == stats.player &&
                        m_lastStats.artist == stats.artist &&
                        m_lastStats.album == stats.album &&
                        m_lastStats.title == stats.title &&
                        m_lastStats.duration == stats.duration;
                    if (!sameTrack || !m_hasLocalPos)
                    {
                        m_localPosSec = static_cast<double>(stats.position);
                        m_localPosTime = nowSteady;
                        m_hasLocalPos = true;
                    }
                    else
                    {
                        const double elapsedLocal = std::chrono::duration<double>(nowSteady - m_localPosTime).count();
                        if (elapsedLocal > 0.0 && elapsedLocal < 5.0)
                            m_localPosSec += elapsedLocal;
                        m_localPosTime = nowSteady;
                        int predicted = static_cast<int>(m_localPosSec);
                        if (stats.duration > 0)
                            predicted = (std::min)(predicted, stats.duration);
                        if (stats.position < predicted)
                        {
                            stats.position = predicted;
                        }
                        else if (stats.position > predicted + 1)
                        {
                            m_localPosSec = static_cast<double>(stats.position);
                            m_localPosTime = nowSteady;
                        }
                    }
                }
                else
                {
                    m_hasLocalPos = false;
                }
                if (stats.duration > 0)
                {
                    stats.position = (std::min)(stats.position, stats.duration);
                    stats.progress = (std::min)(100, (stats.position * 100) / stats.duration);
                }
            }

            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats = stats;
            m_lastStats = stats;
        }

        void WorkerMain()
        {
            const HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
            const bool uninit = SUCCEEDED(hr);
            auto nextRefresh = std::chrono::steady_clock::now();
            while (true)
            {
                std::queue<NowPlayingActionItem> pending;
                {
                    std::unique_lock<std::mutex> lock(m_actionMutex);
                    m_actionSignal.wait_until(lock, nextRefresh, [&]
                                              { return m_stop || !m_actions.empty(); });
                    if (m_stop)
                        break;
                    std::swap(pending, m_actions);
                }
                EnsureManager();
                ProcessActions(pending);
                auto now = std::chrono::steady_clock::now();
                if (now >= nextRefresh)
                {
                    RefreshStats();
                    nextRefresh = now + std::chrono::milliseconds(500);
                }
            }
            if (uninit)
                RoUninitialize();
        }

        std::wstring EnsureCoverPath()
        {
            if (!m_coverPath.empty())
                return m_coverPath;
            wchar_t tempPath[MAX_PATH] = {};
            const DWORD len = GetTempPathW(MAX_PATH, tempPath);
            if (len == 0 || len >= MAX_PATH)
                return L"";
            std::wstring dir = std::wstring(tempPath) + L"Novadesk\\NowPlaying\\";
            std::filesystem::create_directories(std::filesystem::path(dir));
            m_coverPath = dir + L"cover.jpg";
            return m_coverPath;
        }

        bool SaveThumbnail(winrt::Windows::Storage::Streams::IRandomAccessStreamReference const &thumb)
        {
            try
            {
                const std::wstring path = EnsureCoverPath();
                if (path.empty())
                    return false;
                auto stream = thumb.OpenReadAsync().get();
                uint64_t size = stream.Size();
                if (size == 0 || size > (16ULL * 1024ULL * 1024ULL))
                    return false;

                winrt::Windows::Storage::Streams::Buffer buffer(static_cast<uint32_t>(size));
                auto rb = stream.ReadAsync(buffer, static_cast<uint32_t>(size), winrt::Windows::Storage::Streams::InputStreamOptions::None).get();
                uint32_t len = rb.Length();
                if (len == 0)
                    return false;
                auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(rb);
                std::vector<uint8_t> bytes(len);
                reader.ReadBytes(bytes);
                std::ofstream out(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
                if (!out.is_open())
                    return false;
                out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    };

    std::mutex g_controllerMutex;
    std::unique_ptr<NowPlayingController> g_controller;

    NowPlayingController &GetController()
    {
        std::lock_guard<std::mutex> lock(g_controllerMutex);
        if (!g_controller)
        {
            g_controller = std::make_unique<NowPlayingController>();
        }
        return *g_controller;
    }

    void ShutdownController()
    {
        std::lock_guard<std::mutex> lock(g_controllerMutex);
        g_controller.reset();
    }

    BOOL WINAPI NowPlayingCtrlHandler(DWORD ctrlType)
    {
        switch (ctrlType)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            ShutdownController();
            return FALSE;
        default:
            return FALSE;
        }
    }

    NowPlayingStats ReadStats()
    {
        return GetController().ReadStats();
    }

    bool TryPlay()
    {
        return GetController().Queue(NowPlayingAction::Play);
    }

    bool TryPause()
    {
        return GetController().Queue(NowPlayingAction::Pause);
    }

    bool TryPlayPause()
    {
        return GetController().Queue(NowPlayingAction::PlayPause);
    }

    bool TryStop()
    {
        return GetController().Queue(NowPlayingAction::Stop);
    }

    bool TryNext()
    {
        return GetController().Queue(NowPlayingAction::Next);
    }

    bool TryPrevious()
    {
        return GetController().Queue(NowPlayingAction::Previous);
    }

    bool TrySetPosition(int value, bool isPercent)
    {
        return GetController().Queue(NowPlayingAction::SetPosition, value, isPercent);
    }

    bool TrySetShuffle(bool enabled)
    {
        return GetController().Queue(NowPlayingAction::SetShuffle, 0, enabled);
    }

    bool TryToggleShuffle()
    {
        return GetController().Queue(NowPlayingAction::ToggleShuffle);
    }

    bool TrySetRepeat(int mode)
    {
        return GetController().Queue(NowPlayingAction::SetRepeat, mode, false);
    }

    int JsNowPlayingStats(novadesk_context ctx)
    {
        const NowPlayingStats stats = ReadStats();
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
        return 1;
    }

    int JsNowPlayingBackend(novadesk_context ctx)
    {
        g_Host->PushString(ctx, "winrt");
        return 1;
    }

    int JsNowPlayingPlay(novadesk_context ctx) { g_Host->PushBool(ctx, TryPlay() ? 1 : 0); return 1; }
    int JsNowPlayingPause(novadesk_context ctx) { g_Host->PushBool(ctx, TryPause() ? 1 : 0); return 1; }
    int JsNowPlayingPlayPause(novadesk_context ctx) { g_Host->PushBool(ctx, TryPlayPause() ? 1 : 0); return 1; }
    int JsNowPlayingStop(novadesk_context ctx) { g_Host->PushBool(ctx, TryStop() ? 1 : 0); return 1; }
    int JsNowPlayingNext(novadesk_context ctx) { g_Host->PushBool(ctx, TryNext() ? 1 : 0); return 1; }
    int JsNowPlayingPrevious(novadesk_context ctx) { g_Host->PushBool(ctx, TryPrevious() ? 1 : 0); return 1; }

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
        g_Host->PushBool(ctx, TrySetPosition(value, isPercent) ? 1 : 0);
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
        g_Host->PushBool(ctx, TrySetShuffle(enabled) ? 1 : 0);
        return 1;
    }

    int JsNowPlayingToggleShuffle(novadesk_context ctx)
    {
        g_Host->PushBool(ctx, TryToggleShuffle() ? 1 : 0);
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
        g_Host->PushBool(ctx, TrySetRepeat(mode) ? 1 : 0);
        return 1;
    }
}

NOVADESK_ADDON_INIT(ctx, hMsgWnd, host)
{
    (void)hMsgWnd;
    g_Host = host;
    SetConsoleCtrlHandler(NowPlayingCtrlHandler, TRUE);

    novadesk::Addon addon(ctx, host);
    addon.RegisterString("name", "NowPlaying");
    addon.RegisterString("version", "1.0.0");
    addon.RegisterObject("nowPlaying", [](novadesk::Addon &obj) {
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
        obj.RegisterFunction("toggleShuffle", JsNowPlayingToggleShuffle, 0);
        obj.RegisterFunction("setRepeat", JsNowPlayingSetRepeat, 1);
    });
}

NOVADESK_ADDON_UNLOAD()
{
    SetConsoleCtrlHandler(NowPlayingCtrlHandler, FALSE);
    ShutdownController();
}
