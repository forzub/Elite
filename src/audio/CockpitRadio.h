#pragma once

// THE COCKPIT RADIO («магнитола», author 2026-11-23: «можно ли сделать что бы был бесплатный доступ к интернет радио?
// поставим магнитолу. может дать игроку возможность грузить свой плей-лист?»):
//   PLAYLIST - the player's own music: every .mp3 / .flac / .wav under a folder (default: the user's Music folder;
//              radio.json "music_dir"), shuffled, streamed from disk;
//   RADIO    - free internet radio: the open radio-browser.info catalogue (no key) - the most voted working MP3
//              stations + Russian ones, cached; the stream is read by WinHTTP and decoded on its own thread into a PCM
//              ring buffer the mixer plays from (the network never stalls the audio thread: a gap is silence).
// The mixer is AudioEngine's (miniaudio). Keys (SpaceState): Ctrl+B power, Ctrl+Shift+B playlist <-> radio,
// Ctrl+[ / Ctrl+] previous / next, Ctrl+Shift+[ / ] volume. Settings: <client preferences dir>/radio.json.
#include <memory>
#include <string>

namespace audio
{
class CockpitRadio
{
public:
    enum class Source
    {
        Playlist,
        Radio
    };

    static CockpitRadio& instance();
    ~CockpitRadio();

    void togglePower();
    void switchSource();
    void next();
    void previous();
    void volumeStep(int steps);   // +-1 = +-10 %

    // once a frame: a finished track -> the next one, the station's state
    void update();

    // THE RECEPTION (author 2026-11-23: «распределим радиостанции по зонам колоний и там где есть маяки. нет маяков -
    // шум ... чем дальше от маяка - тем сильнее шум»): the best transmitter the ship hears now - its polity (whose
    // stations play) and the signal quality 0..1 (the rest is noise). SpaceState feeds it from the hubs / beacons around
    // (radio_regions.json); no transmitter in the system = quality 0 = only noise.
    void setReception(const std::string& polity, double quality01);
    // the transmitters' table (radio_regions.json): a hub id -> its polity, beacon or not; and the reach by kind
    struct Transmitter
    {
        std::string polity;
        bool beacon = false;
    };
    const Transmitter* transmitter(const std::string& hubId) const;
    double rangeMeters(bool beacon) const;
    double noiseFactor() const;

    bool on() const;
    Source source() const;
    // the magnitola's display: "РАДИО · <station> · 128 кбит/с" / "ПЛЕЙЛИСТ 3/12 · <track>" / a state ("поиск станций")
    std::string displayLine() const;
    // a short notice of the last change (the HUD shows it for a few seconds), taken once
    std::string takeNotice();

    // what the magnitola widget shows: the source, the zone / the track counter, the title, the signal and the volume
    struct Display
    {
        bool on = false;
        Source source = Source::Playlist;
        std::string header;   // "Земля (Сол)" / "3 / 12" / "нет передатчиков"
        std::string title;    // the station / the track / a state ("поиск станций", "шум")
        int signal = -1;      // 0..100 for the radio, -1 = none
        int volume = 0;       // 0..100
        bool connecting = false;
    };
    Display display() const;

    struct Impl;

private:
    CockpitRadio();
    std::unique_ptr<Impl> m;
};
} // namespace audio
