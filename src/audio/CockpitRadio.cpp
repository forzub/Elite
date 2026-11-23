#include "src/audio/CockpitRadio.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "miniaudio.h"
#include "src/audio/AudioEngine.h"
#include "src/ui/platform/ClientPreferencesStore.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace audio
{
namespace
{
namespace fs = std::filesystem;

std::string toUtf8(const std::wstring& w)
{
#ifdef _WIN32
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
#else
    return std::string(w.begin(), w.end());
#endif
}

std::wstring toWide(const std::string& s)
{
#ifdef _WIN32
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
#else
    return std::wstring(s.begin(), s.end());
#endif
}

#ifdef _WIN32
// ---------------------------------------------------------------- a plain HTTP(S) GET over WinHTTP (no extra libraries)
// the client identifies itself generically - nothing about the player goes out
constexpr const wchar_t* UserAgent = L"EliteGameRadio/0.1";

struct HttpGet
{
    HINTERNET session = nullptr, connect = nullptr, request = nullptr;
    int status = 0;

    bool open(const std::string& url, int timeoutMs)
    {
        const std::wstring wurl = toWide(url);
        URL_COMPONENTS uc {};
        uc.dwStructSize = sizeof(uc);
        wchar_t host[256] = {}, path[2048] = {}, extra[2048] = {};
        uc.lpszHostName = host;
        uc.dwHostNameLength = 255;
        uc.lpszUrlPath = path;
        uc.dwUrlPathLength = 2047;
        uc.lpszExtraInfo = extra;
        uc.dwExtraInfoLength = 2047;
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
            return false;
        session = WinHttpOpen(UserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return false;
        WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
        connect = WinHttpConnect(session, host, uc.nPort, 0);
        if (!connect)
            return false;
        const std::wstring target = std::wstring(path) + extra;
        request = WinHttpOpenRequest(connect, L"GET", target.empty() ? L"/" : target.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if (!request)
            return false;
        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, nullptr))
            return false;
        DWORD code = 0, size = sizeof(code);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code,
                            &size, WINHTTP_NO_HEADER_INDEX);
        status = static_cast<int>(code);
        return status >= 200 && status < 300;
    }
    // blocking read of up to n bytes; 0 = the end / an error
    std::size_t read(void* buffer, std::size_t n)
    {
        DWORD got = 0;
        if (!request || !WinHttpReadData(request, buffer, static_cast<DWORD>(n), &got))
            return 0;
        return got;
    }
    ~HttpGet()
    {
        if (request)
            WinHttpCloseHandle(request);
        if (connect)
            WinHttpCloseHandle(connect);
        if (session)
            WinHttpCloseHandle(session);
    }
};

bool httpGetText(const std::string& url, std::string& out)
{
    HttpGet get;
    if (!get.open(url, 15000))
        return false;
    char buf[16384];
    for (std::size_t n; (n = get.read(buf, sizeof buf)) > 0;)
    {
        out.append(buf, n);
        if (out.size() > 8u * 1024u * 1024u)
            break;
    }
    return !out.empty();
}
#endif

// ---------------------------------------------------------------- the PCM ring buffer as a mixer data source
struct RingSource
{
    ma_data_source_base base {};
    ma_pcm_rb* rb = nullptr;
    ma_uint32 channels = 2, rate = 48000;
};

ma_result ringRead(ma_data_source* ds, void* out, ma_uint64 frameCount, ma_uint64* framesRead)
{
    auto* src = static_cast<RingSource*>(ds);
    float* dst = static_cast<float*>(out);
    ma_uint64 done = 0;
    while (done < frameCount)
    {
        ma_uint32 want = static_cast<ma_uint32>(std::min<ma_uint64>(frameCount - done, 4096));
        void* ptr = nullptr;
        if (ma_pcm_rb_acquire_read(src->rb, &want, &ptr) != MA_SUCCESS || want == 0)
            break;
        std::copy_n(static_cast<const float*>(ptr), static_cast<std::size_t>(want) * src->channels, dst + done * src->channels);
        ma_pcm_rb_commit_read(src->rb, want);
        done += want;
    }
    // an underrun (the network behind): silence, the sound keeps playing
    std::fill(dst + done * src->channels, dst + frameCount * src->channels, 0.0f);
    if (framesRead)
        *framesRead = frameCount;
    return MA_SUCCESS;
}
ma_result ringSeek(ma_data_source*, ma_uint64) { return MA_NOT_IMPLEMENTED; }
ma_result ringFormat(ma_data_source* ds, ma_format* format, ma_uint32* channels, ma_uint32* rate, ma_channel* map, size_t mapCap)
{
    auto* src = static_cast<RingSource*>(ds);
    if (format)
        *format = ma_format_f32;
    if (channels)
        *channels = src->channels;
    if (rate)
        *rate = src->rate;
    if (map)
        ma_channel_map_init_standard(ma_standard_channel_map_default, map, mapCap, src->channels);
    return MA_SUCCESS;
}
ma_result ringCursor(ma_data_source*, ma_uint64* cursor)
{
    if (cursor)
        *cursor = 0;
    return MA_NOT_IMPLEMENTED;
}
ma_result ringLength(ma_data_source*, ma_uint64* length)
{
    if (length)
        *length = 0;
    return MA_NOT_IMPLEMENTED;
}
ma_data_source_vtable g_ringVtable = {ringRead, ringSeek, ringFormat, ringCursor, ringLength, nullptr, 0};

struct Station
{
    std::string uuid, name, url, country;
    int bitrate = 0;
};

const char* kAudioExtensions[] = {".mp3", ".flac", ".wav"};
} // namespace

// ==================================================================================================================
struct CockpitRadio::Impl
{
    // settings (radio.json)
    fs::path settingsPath;
    fs::path musicDir;
    float volume = 0.6f;
    Source source = Source::Playlist;
    std::string stationUuid;

    bool on = false;
    std::string notice;
    mutable std::mutex mutex;   // the station list / status shared with the worker threads

    // the playlist
    std::vector<fs::path> tracks;
    std::size_t track = 0;
    bool scanned = false;
    ma_sound fileSound {};
    bool fileSoundReady = false;

    // the radio: the region (the polity whose transmitter is heard best) and its stations
    struct PolityRadio
    {
        std::string name;
        std::vector<std::string> countries;
        std::string tag;
    };
    std::unordered_map<std::string, PolityRadio> polities;
    std::unordered_map<std::string, CockpitRadio::Transmitter> transmitters;
    double hubRange = 3.0e7, beaconRange = 3.0e8, noiseFactor = 1000.0;
    std::string region;                     // the polity on air now ("" = nothing heard)
    double quality = 0.0;                   // 0..1
    std::unordered_map<std::string, std::vector<Station>> catalogues;   // per polity
    std::unordered_map<std::string, std::size_t> stationOf;             // the station picked per polity
    std::vector<Station> stations;          // the region's list (a copy, under the mutex)
    std::size_t station = 0;
    std::atomic<int> catalogueState {0};   // 0 not asked, 1 fetching, 2 ready, 3 failed (for `fetching`)
    std::string fetching;                   // the polity being fetched
    std::thread catalogueThread;
    ma_noise noise {};
    ma_sound noiseSound {};
    bool noiseReady = false;
    std::thread streamThread;
    std::atomic<bool> streamStop {false};
    std::atomic<int> streamState {0};      // 0 off, 1 connecting, 2 playing, 3 no signal
    ma_pcm_rb rb {};
    bool rbReady = false;
    RingSource ring;
    ma_sound ringSound {};
    bool ringSoundReady = false;

    ma_engine* engine() const { return static_cast<ma_engine*>(AudioEngine::instance().nativeEngine()); }

    // ------------------------------------------------------------ settings
    void loadRegions()
    {
        std::ifstream in("assets/data/audio/radio_regions.json");
        if (!in)
            return;
        try
        {
            nlohmann::json j;
            in >> j;
            if (j.contains("ranges_m"))
            {
                hubRange = j["ranges_m"].value("hub", hubRange);
                beaconRange = j["ranges_m"].value("beacon", beaconRange);
            }
            noiseFactor = j.value("noise_factor", noiseFactor);
            const nlohmann::json politiesJson = j.value("polities", nlohmann::json::object());
            for (const auto& [id, p] : politiesJson.items())
            {
                PolityRadio r;
                r.name = p.value("name", id);
                r.countries = p.value("countries", std::vector<std::string>());
                r.tag = p.value("tag", std::string());
                polities[id] = std::move(r);
            }
            const nlohmann::json transmittersJson = j.value("transmitters", nlohmann::json::object());
            for (const auto& [id, t] : transmittersJson.items())
                transmitters[id] = {t.value("polity", std::string()), t.value("kind", std::string()) == "beacon"};
            std::cerr << "[Radio] regions: " << polities.size() << " polities, " << transmitters.size() << " transmitters" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Radio] radio_regions.json: " << e.what() << std::endl;
        }
    }

    void loadSettings()
    {
        settingsPath = ui::platform::ClientPreferencesStore::defaultPath().parent_path() / "radio.json";
#ifdef _WIN32
        if (const wchar_t* home = _wgetenv(L"USERPROFILE"))
            musicDir = fs::path(home) / L"Music";
#endif
        std::ifstream in(settingsPath);
        if (!in)
            return;
        try
        {
            nlohmann::json j;
            in >> j;
            if (j.contains("music_dir") && j["music_dir"].is_string())
                musicDir = fs::path(toWide(j["music_dir"].get<std::string>()));
            volume = std::clamp(j.value("volume", volume), 0.0f, 1.0f);
            source = j.value("source", std::string("playlist")) == "radio" ? Source::Radio : Source::Playlist;
            stationUuid = j.value("station", std::string());
        }
        catch (const std::exception&)
        {
        }
    }
    void saveSettings() const
    {
        nlohmann::json j;
        j["music_dir"] = toUtf8(musicDir.wstring());
        j["volume"] = volume;
        j["source"] = source == Source::Radio ? "radio" : "playlist";
        j["station"] = stationUuid;
        std::error_code ec;
        fs::create_directories(settingsPath.parent_path(), ec);
        std::ofstream out(settingsPath);
        if (out)
            out << j.dump(2) << "\n";
    }

    // ------------------------------------------------------------ the playlist
    void scanMusic()
    {
        scanned = true;
        tracks.clear();
        std::error_code ec;
        if (musicDir.empty() || !fs::is_directory(musicDir, ec))
            return;
        for (fs::recursive_directory_iterator it(musicDir, fs::directory_options::skip_permission_denied, ec), end; it != end;
             it.increment(ec))
        {
            if (ec)
                break;
            if (!it->is_regular_file(ec))
                continue;
            std::wstring ext = it->path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            for (const char* e : kAudioExtensions)
                if (ext == toWide(e))
                    tracks.push_back(it->path());
            if (tracks.size() >= 5000)
                break;
        }
        std::shuffle(tracks.begin(), tracks.end(), std::mt19937(static_cast<unsigned>(std::random_device {}())));
        std::cerr << "[Radio] playlist: " << tracks.size() << " track(s) in " << toUtf8(musicDir.wstring()) << std::endl;
    }
    void stopFile()
    {
        if (fileSoundReady)
        {
            ma_sound_uninit(&fileSound);
            fileSoundReady = false;
        }
    }
    void playTrack()
    {
        stopFile();
        ma_engine* e = engine();
        if (!e || tracks.empty())
            return;
        // a file that does not open: the next one (at most one pass round the list)
        for (std::size_t tries = 0; tries < tracks.size(); ++tries)
        {
            const fs::path& p = tracks[track % tracks.size()];
#ifdef _WIN32
            const ma_result r = ma_sound_init_from_file_w(e, p.wstring().c_str(),
                                                          MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
                                                          nullptr, &fileSound);
#else
            const ma_result r = ma_sound_init_from_file(e, p.string().c_str(),
                                                        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
                                                        nullptr, &fileSound);
#endif
            if (r == MA_SUCCESS)
            {
                fileSoundReady = true;
                ma_sound_set_volume(&fileSound, volume);
                ma_sound_start(&fileSound);
                notice = "МАГНИТОЛА: " + toUtf8(p.stem().wstring());
                return;
            }
            std::cerr << "[Radio] cannot play " << toUtf8(p.wstring()) << " (" << r << ")" << std::endl;
            track = (track + 1) % tracks.size();
        }
    }

    // ------------------------------------------------------------ the radio
    void fetchCatalogue(const std::string& polity)
    {
        if (catalogueState.load() == 1)
            return;   // one fetch at a time; update() asks again when it is done
        if (catalogueThread.joinable())
            catalogueThread.join();
        const auto pit = polities.find(polity);
        if (pit == polities.end() || (pit->second.countries.empty() && pit->second.tag.empty()))
        {
            catalogues[polity] = {};   // a polity with no radio (the Philomorphosis zone): only noise
            return;
        }
        catalogueState = 1;
        fetching = polity;
        const PolityRadio radio = pit->second;
        const fs::path cache = settingsPath.parent_path() / ("radio_stations_" + polity + ".json");
        catalogueThread = std::thread([this, polity, radio, cache] {
            std::vector<Station> list;
            const auto parse = [&](const std::string& body, std::size_t limit) {
                try
                {
                    const auto j = nlohmann::json::parse(body);
                    std::size_t added = 0;
                    for (const auto& s : j)
                    {
                        if (added >= limit)
                            break;
                        Station st;
                        st.uuid = s.value("stationuuid", std::string());
                        st.name = s.value("name", std::string());
                        st.url = s.value("url_resolved", std::string());
                        st.country = s.value("countrycode", std::string());
                        st.bitrate = s.value("bitrate", 0);
                        if (st.url.empty() || st.name.empty())
                            continue;
                        if (std::any_of(list.begin(), list.end(), [&](const Station& o) { return o.uuid == st.uuid; }))
                            continue;
                        list.push_back(std::move(st));
                        ++added;
                    }
                }
                catch (const std::exception&)
                {
                }
            };
#ifdef _WIN32
            // the open catalogue (radio-browser.info, no key): working MP3 streams by votes - about a dozen per region
            // (author: «не надо миллионы станций. но десяток, что бы был»), one or a few per country
            const std::string base =
                "https://all.api.radio-browser.info/json/stations/search?codec=MP3&hidebroken=true&order=votes&reverse=true";
            constexpr std::size_t Want = 14;
            if (!radio.tag.empty())
            {
                std::string body;
                if (httpGetText(base + "&tag=" + radio.tag + "&limit=" + std::to_string(Want + 6), body))
                    parse(body, Want);
            }
            const std::size_t perCountry = radio.countries.empty() ? 0 : std::max<std::size_t>(1, (Want + radio.countries.size() - 1) / radio.countries.size());
            for (const auto& c : radio.countries)
            {
                if (list.size() >= Want)
                    break;
                std::string body;
                if (httpGetText(base + "&countrycode=" + c + "&limit=" + std::to_string(perCountry + 3), body))
                    parse(body, perCountry);
            }
#endif
            if (!list.empty())
            {
                nlohmann::json j = nlohmann::json::array();
                for (const auto& s : list)
                    j.push_back({{"stationuuid", s.uuid}, {"name", s.name}, {"url_resolved", s.url}, {"countrycode", s.country},
                                 {"bitrate", s.bitrate}});
                std::ofstream(cache) << j.dump(1) << "\n";
            }
            else if (std::ifstream in(cache); in)   // offline: the last list of this region
            {
                const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                parse(body, 64);
            }
            std::cerr << "[Radio] catalogue " << polity << ": " << list.size() << " station(s)" << std::endl;
            {
                std::lock_guard<std::mutex> lock(mutex);
                catalogues[polity] = std::move(list);
            }
            catalogueState = 2;
        });
    }

    // the region's list in place (under the mutex), the polity's last station picked
    bool selectRegion()
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto it = catalogues.find(region);
        if (it == catalogues.end())
            return false;
        stations = it->second;
        station = stationOf.count(region) ? stationOf[region] : 0;
        for (std::size_t i = 0; i < stations.size(); ++i)
            if (stations[i].uuid == stationUuid)
                station = i;
        return true;
    }

    // the noise of the ether: pink noise, louder the worse the signal
    void ensureNoise()
    {
        ma_engine* e = engine();
        if (noiseReady || !e)
            return;
        ma_noise_config nc = ma_noise_config_init(ma_format_f32, 2, ma_noise_type_pink, 0, 0.35);
        if (ma_noise_init(&nc, nullptr, &noise) != MA_SUCCESS)
            return;
        if (ma_sound_init_from_data_source(e, &noise, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &noiseSound) != MA_SUCCESS)
        {
            ma_noise_uninit(&noise, nullptr);
            return;
        }
        ma_sound_set_looping(&noiseSound, MA_TRUE);
        noiseReady = true;
    }
    void stopNoise()
    {
        if (noiseReady)
        {
            ma_sound_uninit(&noiseSound);
            ma_noise_uninit(&noise, nullptr);
            noiseReady = false;
        }
    }
    void applyMix()
    {
        const bool radioOn = on && source == Source::Radio;
        const double q = radioOn ? quality : 0.0;
        if (ringSoundReady)
            ma_sound_set_volume(&ringSound, static_cast<float>(volume * q));
        if (radioOn)
        {
            ensureNoise();
            if (noiseReady)
            {
                ma_sound_set_volume(&noiseSound, static_cast<float>(volume * 0.55 * (1.0 - q)));
                if (!ma_sound_is_playing(&noiseSound))
                    ma_sound_start(&noiseSound);
            }
        }
        else if (noiseReady && ma_sound_is_playing(&noiseSound))
            ma_sound_stop(&noiseSound);
    }

    void stopStream()
    {
        streamStop = true;
        if (streamThread.joinable())
            streamThread.join();
        streamStop = false;
        if (ringSoundReady)
        {
            ma_sound_uninit(&ringSound);
            ringSoundReady = false;
        }
        if (rbReady)
        {
            ma_pcm_rb_uninit(&rb);
            rbReady = false;
        }
        streamState = 0;
    }

    void playStation()
    {
        stopStream();
        ma_engine* e = engine();
        Station st;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!e || stations.empty())
                return;
            st = stations[station % stations.size()];
            stationOf[region] = station % stations.size();
        }
        stationUuid = st.uuid;
        const ma_uint32 rate = ma_engine_get_sample_rate(e);
        // 4 s of buffer: the network's hiccups ride it out
        if (ma_pcm_rb_init(ma_format_f32, 2, rate * 4, nullptr, nullptr, &rb) != MA_SUCCESS)
            return;
        rbReady = true;
        ring.rb = &rb;
        ring.channels = 2;
        ring.rate = rate;
        ma_data_source_config dsc = ma_data_source_config_init();
        dsc.vtable = &g_ringVtable;
        if (ma_data_source_init(&dsc, &ring.base) != MA_SUCCESS)
            return;
        if (ma_sound_init_from_data_source(e, &ring, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &ringSound) != MA_SUCCESS)
            return;
        ringSoundReady = true;
        ma_sound_start(&ringSound);
        applyMix();
        streamState = 1;
        notice = "РАДИО: " + st.name + " - подключение";
        std::cerr << "[Radio] station " << st.name << " (" << st.country << ", " << st.bitrate << " kbit/s) " << st.url << std::endl;
#ifdef _WIN32
        streamThread = std::thread([this, url = st.url, rate] {
            // the stream: WinHTTP -> the MP3 decoder (pulls the bytes itself) -> the ring buffer; reconnects after 5 s
            while (!streamStop)
            {
                HttpGet get;
                if (get.open(url, 10000))
                {
                    struct Pull
                    {
                        HttpGet* get;
                        std::atomic<bool>* stop;
                    } pull {&get, &streamStop};
                    const auto onRead = [](ma_decoder* d, void* buf, size_t n, size_t* got) -> ma_result {
                        auto* p = static_cast<Pull*>(d->pUserData);
                        if (p->stop->load())
                        {
                            *got = 0;
                            return MA_AT_END;
                        }
                        *got = p->get->read(buf, n);
                        return *got > 0 ? MA_SUCCESS : MA_AT_END;
                    };
                    const auto onSeek = [](ma_decoder*, ma_int64 offset, ma_seek_origin origin) -> ma_result {
                        return offset == 0 && origin == ma_seek_origin_current ? MA_SUCCESS : MA_NOT_IMPLEMENTED;
                    };
                    ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 2, rate);
                    dc.encodingFormat = ma_encoding_format_mp3;
                    ma_decoder decoder;
                    if (ma_decoder_init(onRead, onSeek, &pull, &dc, &decoder) == MA_SUCCESS)
                    {
                        streamState = 2;
                        std::cerr << "[Radio] stream decoding: " << decoder.outputSampleRate << " Hz" << std::endl;
                        float pcm[4096 * 2];
                        while (!streamStop)
                        {
                            ma_uint64 frames = 0;
                            if (ma_decoder_read_pcm_frames(&decoder, pcm, 4096, &frames) != MA_SUCCESS || frames == 0)
                                break;
                            ma_uint64 written = 0;
                            while (written < frames && !streamStop)
                            {
                                ma_uint32 room = static_cast<ma_uint32>(frames - written);
                                void* dst = nullptr;
                                if (ma_pcm_rb_acquire_write(&rb, &room, &dst) != MA_SUCCESS || room == 0)
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(20));   // the buffer is full
                                    continue;
                                }
                                std::copy_n(pcm + written * 2, static_cast<std::size_t>(room) * 2, static_cast<float*>(dst));
                                ma_pcm_rb_commit_write(&rb, room);
                                written += room;
                            }
                        }
                        ma_decoder_uninit(&decoder);
                    }
                }
                if (streamStop)
                    break;
                streamState = 3;   // no signal: try again
                for (int i = 0; i < 50 && !streamStop; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                streamState = 1;
            }
        });
#endif
    }

    // ------------------------------------------------------------ the whole set
    void start()
    {
        if (source == Source::Playlist)
        {
            if (!scanned)
                scanMusic();
            if (tracks.empty())
                notice = "МАГНИТОЛА: нет музыки в " + toUtf8(musicDir.wstring()) + " (mp3 / flac / wav; папка - radio.json)";
            else
                playTrack();
        }
        else
        {
            applyMix();   // the noise of the ether at once; the region's station follows (update)
            if (region.empty())
                notice = "РАДИО: нет передатчиков в зоне приёма - только шум";
            else if (!catalogues.count(region))
            {
                fetchCatalogue(region);
                notice = "РАДИО: " + polities[region].name + " - поиск станций...";
            }
            else if (selectRegion())
                playStation();
        }
    }
    void stop()
    {
        stopFile();
        stopStream();
        stopNoise();
    }
};

CockpitRadio& CockpitRadio::instance()
{
    static CockpitRadio radio;
    return radio;
}

CockpitRadio::CockpitRadio() : m(std::make_unique<Impl>())
{
    m->loadSettings();
    m->loadRegions();
}

CockpitRadio::~CockpitRadio()
{
    m->stop();
    if (m->catalogueThread.joinable())
        m->catalogueThread.join();
}

void CockpitRadio::togglePower()
{
    m->on = !m->on;
    if (m->on)
        m->start();
    else
    {
        m->stop();
        m->notice = "МАГНИТОЛА: выкл";
    }
}

void CockpitRadio::switchSource()
{
    m->stop();
    m->source = m->source == Source::Playlist ? Source::Radio : Source::Playlist;
    m->saveSettings();
    if (!m->on)
        m->on = true;
    m->start();
}

void CockpitRadio::next()
{
    if (!m->on)
        return;
    if (m->source == Source::Playlist && !m->tracks.empty())
    {
        m->track = (m->track + 1) % m->tracks.size();
        m->playTrack();
    }
    else if (m->source == Source::Radio && !m->stations.empty())
    {
        {
            std::lock_guard<std::mutex> lock(m->mutex);
            m->station = (m->station + 1) % std::max<std::size_t>(1, m->stations.size());
        }
        m->playStation();
        m->saveSettings();
    }
}

void CockpitRadio::previous()
{
    if (!m->on)
        return;
    if (m->source == Source::Playlist && !m->tracks.empty())
    {
        m->track = (m->track + m->tracks.size() - 1) % m->tracks.size();
        m->playTrack();
    }
    else if (m->source == Source::Radio && !m->stations.empty())
    {
        {
            std::lock_guard<std::mutex> lock(m->mutex);
            const std::size_t n = std::max<std::size_t>(1, m->stations.size());
            m->station = (m->station + n - 1) % n;
        }
        m->playStation();
        m->saveSettings();
    }
}

void CockpitRadio::volumeStep(int steps)
{
    m->volume = std::clamp(m->volume + 0.1f * static_cast<float>(steps), 0.0f, 1.0f);
    if (m->fileSoundReady)
        ma_sound_set_volume(&m->fileSound, m->volume);
    if (m->ringSoundReady)
        ma_sound_set_volume(&m->ringSound, m->volume);
    m->notice = "МАГНИТОЛА: громкость " + std::to_string(static_cast<int>(m->volume * 100.0f + 0.5f)) + " %";
    m->saveSettings();
}

void CockpitRadio::update()
{
    if (!m->on)
        return;
    if (m->source == Source::Playlist && m->fileSoundReady && ma_sound_at_end(&m->fileSound))
    {
        m->track = (m->track + 1) % std::max<std::size_t>(1, m->tracks.size());
        m->playTrack();
    }
    if (m->source != Source::Radio)
        return;
    // a fetched catalogue: free the worker for the next one
    if (m->catalogueState.load() == 2)
    {
        m->catalogueState = 0;
        if (m->catalogueThread.joinable())
            m->catalogueThread.join();
    }
    // the region's station: on as soon as its list is there and the signal is above the floor
    if (!m->region.empty() && !m->ringSoundReady && m->quality > 0.08)
    {
        if (!m->catalogues.count(m->region))
            m->fetchCatalogue(m->region);
        else if (m->selectRegion() && !m->stations.empty())
            m->playStation();
    }
    // no signal at all: the stream stops (the noise stays)
    if (m->ringSoundReady && m->quality < 0.03)
        m->stopStream();
    m->applyMix();
}

void CockpitRadio::setReception(const std::string& polity, double quality01)
{
    m->quality = std::clamp(quality01, 0.0, 1.0);
    if (polity == m->region)
        return;
    // another polity's transmitter is the best now: its radio (the old station off; the new one in update())
    const std::string was = m->region;
    m->region = polity;
    if (m->on && m->source == Source::Radio)
    {
        m->stopStream();
        {
            std::lock_guard<std::mutex> lock(m->mutex);
            m->stations.clear();
        }
        const auto it = m->polities.find(polity);
        m->notice = polity.empty() ? "РАДИО: вне зоны передатчиков - шум"
                                   : "РАДИО: в эфире - " + (it != m->polities.end() ? it->second.name : polity);
    }
    std::cerr << "[Radio] reception: " << (was.empty() ? "-" : was) << " -> " << (polity.empty() ? "-" : polity) << " ("
              << static_cast<int>(m->quality * 100.0) << " %)" << std::endl;
}

const CockpitRadio::Transmitter* CockpitRadio::transmitter(const std::string& hubId) const
{
    const auto it = m->transmitters.find(hubId);
    return it == m->transmitters.end() ? nullptr : &it->second;
}

double CockpitRadio::rangeMeters(bool beacon) const
{
    return beacon ? m->beaconRange : m->hubRange;
}

double CockpitRadio::noiseFactor() const
{
    return m->noiseFactor;
}

bool CockpitRadio::on() const
{
    return m->on;
}

CockpitRadio::Source CockpitRadio::source() const
{
    return m->source;
}

std::string CockpitRadio::displayLine() const
{
    if (!m->on)
        return {};
    if (m->source == Source::Playlist)
    {
        if (m->tracks.empty() || !m->fileSoundReady)
            return "МАГНИТОЛА · нет музыки";
        return "МАГНИТОЛА " + std::to_string(m->track % m->tracks.size() + 1) + "/" + std::to_string(m->tracks.size()) + " · " +
               toUtf8(m->tracks[m->track % m->tracks.size()].stem().wstring());
    }
    std::lock_guard<std::mutex> lock(m->mutex);
    const int signal = static_cast<int>(m->quality * 100.0 + 0.5);
    if (m->region.empty())
        return "РАДИО · шум · нет передатчиков";
    const auto pit = m->polities.find(m->region);
    const std::string zone = pit != m->polities.end() ? pit->second.name : m->region;
    if (m->stations.empty())
        return "РАДИО · " + zone + (m->catalogues.count(m->region) ? " · станций нет · шум" : " · поиск станций") + " · сигнал " +
               std::to_string(signal) + " %";
    const Station& st = m->stations[m->station % m->stations.size()];
    const int s = m->streamState.load();
    std::string line = "РАДИО · " + zone + " · " + st.name + " · сигнал " + std::to_string(signal) + " %";
    if (s == 1)
        line += " · подключение";
    else if (s == 3)
        line += " · нет сигнала";
    return line;
}

std::string CockpitRadio::takeNotice()
{
    std::string n;
    n.swap(m->notice);
    return n;
}
} // namespace audio
