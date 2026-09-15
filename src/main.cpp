#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <random>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include <iomanip>

#include "miniaudio.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace fs = std::filesystem;

using namespace ftxui;

// ============================================================
// CONSTANTS
// ============================================================

constexpr double PI = 3.14159265358979323846;
constexpr int LEFT_PANEL_WIDTH = 35;

// ============================================================
// SONG
// ============================================================

struct Song
{
    std::string path;
    std::string filename;
};

// ============================================================
// AUDIO FILE CHECK
// ============================================================

bool isAudioFile(const fs::path& path)
{
    if (!path.has_extension())
        return false;

    std::string ext = path.extension().string();

    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return ext == ".mp3" ||
           ext == ".wav" ||
           ext == ".flac" ||
           ext == ".ogg" ||
           ext == ".m4a";
}

// ============================================================
// SCAN MUSIC FOLDER
// ============================================================

std::vector<Song> scanMusicFolder(const std::string& folder)
{
    std::vector<Song> songs;

    try
    {
        for (const auto& entry :
             fs::recursive_directory_iterator(
                 folder,
                 fs::directory_options::skip_permission_denied))
        {
            if (entry.is_regular_file() &&
                isAudioFile(entry.path()))
            {
                songs.push_back(
                    {
                        entry.path().string(),
                        entry.path().filename().string()
                    });
            }
        }
    }
    catch (...)
    {
        // Ignore inaccessible folders/files.
    }

    std::sort(
        songs.begin(),
        songs.end(),
        [](const Song& a, const Song& b)
        {
            std::string A = a.filename;
            std::string B = b.filename;

            std::transform(
                A.begin(),
                A.end(),
                A.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

            std::transform(
                B.begin(),
                B.end(),
                B.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

            return A < B;
        });

    return songs;
}

// ============================================================
// CONFIG
// ============================================================

const std::string CONFIG_FILE = "config.txt";

std::string loadSavedFolder()
{
    std::ifstream file(CONFIG_FILE);

    if (!file.is_open())
        return "";

    std::string folder;
    std::getline(file, folder);

    return folder;
}

void saveFolder(const std::string& folder)
{
    std::ofstream file(CONFIG_FILE);

    if (file.is_open())
        file << folder;
}

// ============================================================
// FORMAT TIME
// ============================================================

std::string formatTime(double seconds)
{
    if (seconds < 0)
        seconds = 0;

    int total =
        static_cast<int>(seconds);

    int minutes = total / 60;
    int secs = total % 60;

    std::ostringstream out;

    out << std::setfill('0')
        << std::setw(2)
        << minutes
        << ":"
        << std::setw(2)
        << secs;

    return out.str();
}

// ============================================================
// MUSIC PLAYER
// ============================================================

class MusicPlayer
{
private:

    ma_engine engine{};
    ma_sound sound{};

    bool engineReady = false;
    bool soundReady = false;

    std::vector<Song> songs;

    int currentIndex = -1;

    bool paused = false;
    bool shuffle = true;
    bool repeat = false;

    float volume = 1.0f;

    std::vector<int> shuffleOrder;
    int shufflePosition = 0;

    mutable std::mutex mutex;

    // ========================================================
    // SHUFFLE ORDER
    // ========================================================

    void buildShuffleOrderUnlocked()
    {
        shuffleOrder.clear();

        for (int i = 0;
             i < static_cast<int>(songs.size());
             ++i)
        {
            shuffleOrder.push_back(i);
        }

        if (shuffleOrder.empty())
        {
            shufflePosition = 0;
            return;
        }

        std::random_device rd;
        std::mt19937 generator(rd());

        std::shuffle(
            shuffleOrder.begin(),
            shuffleOrder.end(),
            generator);

        // Keep current song at the beginning.
        if (currentIndex >= 0)
        {
            auto it =
                std::find(
                    shuffleOrder.begin(),
                    shuffleOrder.end(),
                    currentIndex);

            if (it != shuffleOrder.end())
            {
                std::iter_swap(
                    shuffleOrder.begin(),
                    it);
            }
        }

        shufflePosition = 0;
    }

    // ========================================================
    // UNLOAD CURRENT SOUND
    // ========================================================

    void unloadSoundUnlocked()
    {
        if (soundReady)
        {
            ma_sound_uninit(&sound);
            soundReady = false;
        }

        paused = false;
    }

    // ========================================================
    // LOAD AND PLAY
    // ========================================================

    bool loadAndPlayUnlocked(int index)
    {
        if (!engineReady)
            return false;

        if (index < 0 ||
            index >= static_cast<int>(songs.size()))
        {
            return false;
        }

        unloadSoundUnlocked();

        ma_result result =
            ma_sound_init_from_file(
                &engine,
                songs[index].path.c_str(),
                MA_SOUND_FLAG_STREAM,
                nullptr,
                nullptr,
                &sound);

        if (result != MA_SUCCESS)
            return false;

        soundReady = true;

        ma_sound_set_volume(
            &sound,
            volume);

        result =
            ma_sound_start(&sound);

        if (result != MA_SUCCESS)
        {
            ma_sound_uninit(&sound);
            soundReady = false;

            return false;
        }

        currentIndex = index;
        paused = false;

        if (shuffle)
        {
            auto it =
                std::find(
                    shuffleOrder.begin(),
                    shuffleOrder.end(),
                    currentIndex);

            if (it != shuffleOrder.end())
            {
                shufflePosition =
                    static_cast<int>(
                        std::distance(
                            shuffleOrder.begin(),
                            it));
            }
        }

        return true;
    }

public:

    // ========================================================
    // INITIALIZE
    // ========================================================

    bool initialize()
    {
        ma_result result =
            ma_engine_init(
                nullptr,
                &engine);

        if (result != MA_SUCCESS)
            return false;

        engineReady = true;

        return true;
    }

    // ========================================================
    // SHUTDOWN
    // ========================================================

    void shutdown()
    {
        std::lock_guard<std::mutex> lock(mutex);

        unloadSoundUnlocked();

        if (engineReady)
        {
            ma_engine_uninit(&engine);
            engineReady = false;
        }
    }

    // ========================================================
    // SET SONGS
    // ========================================================

    void setSongs(
        const std::vector<Song>& newSongs)
    {
        std::lock_guard<std::mutex> lock(mutex);

        songs = newSongs;

        buildShuffleOrderUnlocked();
    }

    // ========================================================
    // PLAY
    // ========================================================

    bool play(int index)
    {
        std::lock_guard<std::mutex> lock(mutex);

        return loadAndPlayUnlocked(index);
    }

    // ========================================================
    // PLAY INITIAL
    // ========================================================

    bool playInitial()
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (songs.empty())
            return false;

        if (shuffleOrder.empty())
            buildShuffleOrderUnlocked();

        int initialIndex =
            shuffle
                ? shuffleOrder.front()
                : 0;

        return loadAndPlayUnlocked(initialIndex);
    }

    // ========================================================
    // PLAY / PAUSE
    // ========================================================

    void togglePause()
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!soundReady)
            return;

        if (paused)
        {
            ma_sound_start(&sound);
            paused = false;
        }
        else
        {
            ma_sound_stop(&sound);
            paused = true;
        }
    }

    // ========================================================
    // NEXT
    // ========================================================

    void next(bool continuous = false)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (songs.empty())
            return;

        int nextIndex = 0;

        if (shuffle)
        {
            if (shuffleOrder.empty())
                buildShuffleOrderUnlocked();

            if (shuffleOrder.empty())
                return;

            if (shufflePosition + 1 >=
                static_cast<int>(
                    shuffleOrder.size()))
            {
                if (!repeat && !continuous)
                    return;

                buildShuffleOrderUnlocked();

                if (shuffleOrder.empty())
                    return;

                shufflePosition = 0;
            }
            else
            {
                ++shufflePosition;
            }

            nextIndex =
                shuffleOrder[shufflePosition];
        }
        else
        {
            nextIndex =
                currentIndex + 1;

            if (nextIndex >=
                static_cast<int>(songs.size()))
            {
                if (!repeat && !continuous)
                    return;

                nextIndex = 0;
            }
        }

        loadAndPlayUnlocked(nextIndex);
    }

    // ========================================================
    // PREVIOUS
    // ========================================================

    void previous()
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (songs.empty())
            return;

        int previousIndex;

        // If more than 3 seconds into the song,
        // restart the current song.
        if (soundReady)
        {
            float position = 0.0f;

            ma_sound_get_cursor_in_seconds(
                &sound,
                &position);

            if (position > 3.0f)
            {
                ma_sound_seek_to_second(
                    &sound,
                    0.0f);

                return;
            }
        }

        if (shuffle &&
            !shuffleOrder.empty())
        {
            if (shufflePosition > 0)
            {
                --shufflePosition;

                previousIndex =
                    shuffleOrder[shufflePosition];
            }
            else
            {
                previousIndex = currentIndex;
            }
        }
        else
        {
            previousIndex =
                currentIndex - 1;

            if (previousIndex < 0)
            {
                previousIndex =
                    static_cast<int>(
                        songs.size()) - 1;
            }
        }

        loadAndPlayUnlocked(previousIndex);
    }

    // ========================================================
    // VOLUME
    // ========================================================

    void setVolume(float value)
    {
        std::lock_guard<std::mutex> lock(mutex);

        volume =
            std::clamp(
                value,
                0.0f,
                1.0f);

        if (soundReady)
        {
            ma_sound_set_volume(
                &sound,
                volume);
        }
    }

    // ========================================================
    // SHUFFLE
    // ========================================================

    void toggleShuffle()
    {
        std::lock_guard<std::mutex> lock(mutex);

        shuffle = !shuffle;

        if (shuffle)
        {
            buildShuffleOrderUnlocked();
        }
        else
        {
            shuffleOrder.clear();
            shufflePosition = 0;
        }
    }

    // ========================================================
    // REPEAT
    // ========================================================

    void toggleRepeat()
    {
        std::lock_guard<std::mutex> lock(mutex);

        repeat = !repeat;
    }

    // ========================================================
    // SEEK
    // ========================================================

    void seek(double seconds)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!soundReady)
            return;

        float current = 0.0f;
        float length = 0.0f;

        ma_sound_get_cursor_in_seconds(
            &sound,
            &current);

        ma_sound_get_length_in_seconds(
            &sound,
            &length);

        double target =
            static_cast<double>(current) +
            seconds;

        target =
            std::clamp(
                target,
                0.0,
                static_cast<double>(length));

        ma_sound_seek_to_second(
            &sound,
            static_cast<float>(target));
    }

    // ========================================================
    // IS PLAYING
    // ========================================================

    bool isPlaying() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return soundReady &&
               ma_sound_is_playing(&sound);
    }

    // ========================================================
    // HAS ENDED
    // ========================================================

    bool hasEnded() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return soundReady &&
               ma_sound_at_end(&sound);
    }

    // ========================================================
    // POSITION
    // ========================================================

    double position() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!soundReady)
            return 0.0;

        float position = 0.0f;

        ma_sound_get_cursor_in_seconds(
            &sound,
            &position);

        return static_cast<double>(position);
    }

    // ========================================================
    // DURATION
    // ========================================================

    double duration() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!soundReady)
            return 0.0;

        float duration = 0.0f;

        ma_sound_get_length_in_seconds(
            &sound,
            &duration);

        return static_cast<double>(duration);
    }

    // ========================================================
    // CURRENT SONG
    // ========================================================

    int current() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return currentIndex;
    }

    // ========================================================
    // PAUSED
    // ========================================================

    bool isPaused() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return paused;
    }

    // ========================================================
    // SHUFFLE STATE
    // ========================================================

    bool isShuffle() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return shuffle;
    }

    // ========================================================
    // REPEAT STATE
    // ========================================================

    bool isRepeat() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return repeat;
    }

    // ========================================================
    // VOLUME PERCENT
    // ========================================================

    int volumePercent() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return static_cast<int>(
            volume * 100.0f);
    }
};

// ============================================================
// ANIMATED VINYL RECORD
// ============================================================

std::string makeVinyl(
    int frame,
    bool playing)
{
    const int width = 25;
    const int height = 11;

    const double centerX =
        (width - 1) / 2.0;

    const double centerY =
        (height - 1) / 2.0;

    const double radius = 4.8;

    // Rotation only changes while playing.
    double rotation =
        playing
            ? frame * 0.06
            : 0.0;

    std::vector<std::string> rows;

    for (int y = 0; y < height; ++y)
    {
        std::string row;

        for (int x = 0; x < width; ++x)
        {
            // Terminal characters are taller than wide.
            // Compress X to make a circular disc.

            double dx =
                (x - centerX) * 0.55;

            double dy =
                (y - centerY);

            double distance =
                std::sqrt(
                    dx * dx +
                    dy * dy);

            // OUTSIDE RECORD
            if (distance > radius)
            {
                row += " ";
                continue;
            }

            // CENTER HOLE
            if (distance < 1.0)
            {
                row += "●";
                continue;
            }

            // CENTER RING
            if (distance < 1.8)
            {
                row += "◉";
                continue;
            }

            // ANGLE
            double angle =
                std::atan2(dy, dx);

            if (angle < 0)
                angle += 2.0 * PI;

            // ROTATING ANGLE
            double relative =
                std::fmod(
                    angle -
                        rotation +
                        10.0 * PI,
                    2.0 * PI);

            // GROOVES
            int groove =
                static_cast<int>(
                    distance * 2.0);

            bool grooveLine =
                (groove % 3 == 0);

            // MAIN HIGHLIGHT
            bool highlight =
                relative < 0.34;

            // SECONDARY HIGHLIGHT
            bool secondary =
                std::abs(
                    std::sin(
                        relative * 3.0)) >
                0.94;

            // DARK RADIAL CUT
            bool darkCut =
                std::abs(
                    std::sin(
                        relative * 2.0)) <
                0.035;

            std::string symbol = " ";

            if (highlight)
            {
                symbol = "█";
            }
            else if (secondary)
            {
                symbol = "▓";
            }
            else if (darkCut)
            {
                symbol = " ";
            }
            else if (grooveLine)
            {
                symbol = "░";
            }
            else
            {
                symbol = "▒";
            }

            // INNER LABEL RING
            if (distance >= 2.0 &&
                distance < 2.7)
            {
                if (highlight)
                    symbol = "█";
                else
                    symbol = "▓";
            }

            row += symbol;
        }

        rows.push_back(row);
    }

    std::string result;

    for (const auto& row : rows)
    {
        result += row;
        result += "\n";
    }

    return result;
}

// ============================================================
// AUDIO VISUALIZER
// ============================================================

std::string makeVisualizer(
    int frame,
    bool playing)
{
    constexpr int barCount = 5;
    constexpr int height = 8;

    constexpr const char* block = "█";

    std::vector<int> levels(
        barCount,
        1);

    if (playing)
    {
        for (int bar = 0;
             bar < barCount;
             ++bar)
        {
            double wave =
                std::sin(
                    frame * 0.18 +
                    bar * 1.15) *
                    0.5 +

                std::sin(
                    frame * 0.07 +
                    bar * 0.65) *
                    0.3 +

                0.5;

            levels[bar] =
                std::clamp(
                    static_cast<int>(
                        wave * height),
                    1,
                    height);
        }
    }

    std::string result;

    for (int row = height;
         row > 0;
         --row)
    {
        for (int bar = 0;
             bar < barCount;
             ++bar)
        {
            result +=
                levels[bar] >= row
                    ? block
                    : " ";

            result += " ";
        }

        result += "\n";
    }

    return result;
}

// ============================================================
// MOOD LIGHTING
// ============================================================

struct MoodTheme
{
    std::string name;

    Color foreground;
    Color accent;
    Color background;
};

const std::vector<MoodTheme> MOOD_THEMES =
{
    {
        "Dragon",
        Color::RGB(230, 230, 230),
        Color::RGB(255, 100, 45),
        Color::RGB(25, 8, 8)
    },

    {
        "Ocean",
        Color::RGB(210, 235, 255),
        Color::RGB(70, 190, 255),
        Color::RGB(5, 18, 32)
    },

    {
        "Forest",
        Color::RGB(220, 245, 220),
        Color::RGB(90, 220, 125),
        Color::RGB(6, 24, 14)
    },

    {
        "Sunset",
        Color::RGB(255, 230, 190),
        Color::RGB(255, 170, 55),
        Color::RGB(35, 12, 20)
    }
};

// ============================================================
// MAIN
// ============================================================

int main()
{
    std::cout
        << "Starting DRAGON MUSIC PLAYER...\n";

    // ========================================================
    // LOAD SAVED FOLDER
    // ========================================================

    std::string folder =
        loadSavedFolder();

    if (folder.empty() ||
        !fs::exists(folder) ||
        !fs::is_directory(folder))
    {
        std::cout
            << "\nFirst-time setup.\n";

        std::cout
            << "Enter your music folder:\n";

        std::cout
            << "> ";

        std::getline(
            std::cin,
            folder);

        if (!fs::exists(folder) ||
            !fs::is_directory(folder))
        {
            std::cout
                << "Invalid folder.\n";

            return 1;
        }

        saveFolder(folder);
    }

    // ========================================================
    // SCAN MUSIC
    // ========================================================

    std::cout
        << "\nScanning music...\n";

    auto songs =
        scanMusicFolder(folder);

    if (songs.empty())
    {
        std::cout
            << "No music files found.\n";

        return 0;
    }

    std::cout
        << "Found "
        << songs.size()
        << " songs.\n";

    // ========================================================
    // INITIALIZE PLAYER
    // ========================================================

    MusicPlayer player;

    if (!player.initialize())
    {
        std::cerr
            << "Failed to initialize audio.\n";

        return 1;
    }

    player.setSongs(songs);

    // ========================================================
    // START FIRST SONG
    // ========================================================

    if (!player.playInitial())
    {
        std::cerr
            << "Failed to play first song.\n";

        player.shutdown();

        return 1;
    }

    // ========================================================
    // FTXUI SCREEN
    // ========================================================

    auto screen =
        ScreenInteractive::Fullscreen();

    // ========================================================
    // VINYL ANIMATION FRAME
    // ========================================================

    std::atomic<int> vinylFrame{0};

    // ========================================================
    // MOOD
    // ========================================================

    int moodIndex = 0;

    // ========================================================
    // PLAYLIST SELECTION
    //
    // IMPORTANT:
    // This is now completely controlled by DRAGON.
    //
    // ↑ = previous playlist item
    // ↓ = next playlist item
    //
    // It does NOT automatically change playback.
    // Press ENTER to play the selected song.
    // ========================================================

    int selectedSong =
        player.current();

    if (selectedSong < 0)
        selectedSong = 0;

    // ========================================================
    // PLAYLIST SCROLL POSITION
    // ========================================================

    int playlistScroll = 0;

    // Number of playlist rows visible at once.
    constexpr int VISIBLE_SONGS = 18;

    // ========================================================
    // RENDERER
    // ========================================================

    auto renderer =
        Renderer(
            [&]
            {
                int current =
                    player.current();

                double pos =
                    player.position();

                double len =
                    player.duration();

                bool playing =
                    player.isPlaying();

                bool paused =
                    player.isPaused();

                const MoodTheme& mood =
                    MOOD_THEMES[moodIndex];

                // ====================================================
                // KEEP SELECTION INSIDE VALID RANGE
                // ====================================================

                if (!songs.empty())
                {
                    selectedSong =
                        std::clamp(
                            selectedSong,
                            0,
                            static_cast<int>(
                                songs.size()) - 1);
                }

                // ====================================================
                // AUTOMATIC PLAYLIST SCROLL
                // ====================================================

                if (selectedSong < playlistScroll)
                {
                    playlistScroll =
                        selectedSong;
                }

                if (selectedSong >=
                    playlistScroll +
                    VISIBLE_SONGS)
                {
                    playlistScroll =
                        selectedSong -
                        VISIBLE_SONGS +
                        1;
                }

                int maxScroll =
                    std::max(
                        0,
                        static_cast<int>(
                            songs.size()) -
                        VISIBLE_SONGS);

                playlistScroll =
                    std::clamp(
                        playlistScroll,
                        0,
                        maxScroll);

                // ====================================================
                // PROGRESS
                // ====================================================

                float progress =
                    len > 0.0
                        ? static_cast<float>(
                              pos / len)
                        : 0.0f;

                progress =
                    std::clamp(
                        progress,
                        0.0f,
                        1.0f);

                // ====================================================
                // CURRENT SONG
                // ====================================================

                std::string currentSong =
                    current >= 0 &&
                    current <
                        static_cast<int>(
                            songs.size())
                        ? songs[current].filename
                        : "Nothing playing";

                // ====================================================
                // PLAYBACK STATE
                // ====================================================

                std::string state;

                if (paused)
                {
                    state = "PAUSED";
                }
                else if (playing)
                {
                    state = "PLAYING";
                }
                else
                {
                    state = "STOPPED";
                }

                // ====================================================
                // VINYL
                // ====================================================

                std::string vinyl =
                    makeVinyl(
                        vinylFrame.load(),
                        playing);

                // ====================================================
                // VISUALIZER
                // ====================================================

                std::string visualizer =
                    makeVisualizer(
                        vinylFrame.load(),
                        playing);

                // ====================================================
                // BUILD PLAYLIST
                // ====================================================

                Elements playlistElements;

                int playlistEnd =
                    std::min(
                        playlistScroll +
                            VISIBLE_SONGS,
                        static_cast<int>(
                            songs.size()));

                for (int i = playlistScroll;
                     i < playlistEnd;
                     ++i)
                {
                    std::string prefix =
                        (i == selectedSong)
                            ? " > "
                            : "   ";

                    std::string entry =
                        prefix +
                        std::to_string(i + 1) +
                        ". " +
                        songs[i].filename;

                    auto item =
                        text(entry);

                    if (i == selectedSong)
                    {
                        item =
                            item
                            | bold
                            | color(mood.accent);
                    }

                    if (i == current)
                    {
                        item =
                            item
                            | bold;
                    }

                    playlistElements.push_back(
                        item);
                }

                // ====================================================
                // PLAYLIST SCROLL INDICATOR
                // ====================================================

                std::string scrollInfo;

                if (songs.size() >
                    static_cast<size_t>(
                        VISIBLE_SONGS))
                {
                    scrollInfo =
                        "  ↑/↓  " +
                        std::to_string(
                            selectedSong + 1) +
                        "/" +
                        std::to_string(
                            songs.size());
                }

                // ====================================================
                // MAIN UI
                // ====================================================

                return vbox({

                    // =================================================
                    // TITLE
                    // =================================================

                    text(
                        " DRAGON MUSIC PLAYER ")
                        | bold
                        | center,

                    separator(),

                    // =================================================
                    // MAIN CONTENT
                    // =================================================

                    hbox({

                        // =============================================
                        // LEFT SONG LIST
                        // =============================================

                        vbox({

                            hbox({
                                text(
                                    " LOCAL MUSIC ")
                                    | bold,

                                filler(),

                                text(scrollInfo)
                                    | dim
                            }),

                            separator(),

                            vbox(
                                playlistElements)
                                | flex

                        })
                        | border
                        | size(
                            WIDTH,
                            GREATER_THAN,
                            LEFT_PANEL_WIDTH),

                        separator(),

                        // =============================================
                        // RIGHT NOW PLAYING
                        // =============================================

                        vbox({

                            text(
                                " NOW PLAYING ")
                                | bold
                                | center,

                            separator(),

                            text(currentSong)
                                | bold
                                | center,

                            text(state)
                                | center,

                            separator(),

                            // =========================================
                            // VINYL
                            // =========================================

                            hbox({

                                text(vinyl),

                                text(visualizer)
                                    | color(
                                        mood.accent)

                            })
                            | center,

                            separator(),

                            // =========================================
                            // TIME
                            // =========================================

                            text(
                                formatTime(pos) +
                                " ━━━━━━━━━━━━━━━ " +
                                formatTime(len))
                                | center,

                            // =========================================
                            // VOLUME / SHUFFLE / REPEAT
                            // =========================================

                            hbox({

                                text(
                                    "Volume: " +
                                    std::to_string(
                                        player.volumePercent()) +
                                    "%"),

                                filler(),

                                text(
                                    "Shuffle: " +
                                    std::string(
                                        player.isShuffle()
                                            ? "ON"
                                            : "OFF")),

                                text(
                                    "  |  "),

                                text(
                                    "Repeat: " +
                                    std::string(
                                        player.isRepeat()
                                            ? "ON"
                                            : "OFF"))

                            }),

                            // =========================================
                            // PROGRESS BAR
                            // =========================================

                            gauge(progress)
                                | color(
                                    mood.accent)
                                | flex

                        })
                        | border
                        | flex

                    })
                    | flex,

                    separator(),

                    // =================================================
                    // CONTROLS
                    // =================================================

                    text(
                        " SPACE Play/Pause   "
                        "N Next   P Previous   "
                        "S Shuffle   R Repeat   "
                        "L Mood: " +
                        mood.name)
                        | center,

                    text(
                        " ↑/↓ Browse   "
                        "ENTER Play Selected   "
                        "←/→ Seek   "
                        "+/- Volume   "
                        "Q Quit")
                        | center

                })
                | color(mood.foreground)
                | bgcolor(mood.background);
            });

    // ============================================================
    // KEYBOARD INPUT
    //
    // MOUSE IS COMPLETELY DISABLED HERE.
    // ============================================================

    auto component =
        CatchEvent(
            renderer,
            [&](Event event)
            {
                // =================================================
                // DISABLE ALL MOUSE INPUT
                // =================================================

                if (event.is_mouse())
                {
                    return true;
                }

                // =================================================
                // QUIT
                // =================================================

                if (
                    event == Event::Character("q") ||
                    event == Event::Character("Q"))
                {
                    screen.Exit();

                    return true;
                }

                // =================================================
                // PLAY / PAUSE
                // =================================================

                if (
                    event == Event::Character(" "))
                {
                    player.togglePause();

                    return true;
                }

                // =================================================
                // NEXT
                // =================================================

                if (
                    event == Event::Character("n") ||
                    event == Event::Character("N"))
                {
                    player.next();

                    // Move playlist selection
                    // to currently playing song.
                    selectedSong =
                        player.current();

                    return true;
                }

                // =================================================
                // PREVIOUS
                // =================================================

                if (
                    event == Event::Character("p") ||
                    event == Event::Character("P"))
                {
                    player.previous();

                    // Move playlist selection
                    // to currently playing song.
                    selectedSong =
                        player.current();

                    return true;
                }

                // =================================================
                // SHUFFLE
                // =================================================

                if (
                    event == Event::Character("s") ||
                    event == Event::Character("S"))
                {
                    player.toggleShuffle();

                    return true;
                }

                // =================================================
                // REPEAT
                // =================================================

                if (
                    event == Event::Character("r") ||
                    event == Event::Character("R"))
                {
                    player.toggleRepeat();

                    return true;
                }

                // =================================================
                // PLAYLIST UP
                //
                // ONLY CHANGES SELECTION.
                // DOES NOT CHANGE MUSIC.
                // =================================================

                if (event == Event::ArrowUp)
                {
                    if (!songs.empty())
                    {
                        selectedSong =
                            std::max(
                                selectedSong - 1,
                                0);
                    }

                    return true;
                }

                // =================================================
                // PLAYLIST DOWN
                //
                // ONLY CHANGES SELECTION.
                // DOES NOT CHANGE MUSIC.
                // =================================================

                if (event == Event::ArrowDown)
                {
                    if (!songs.empty())
                    {
                        selectedSong =
                            std::min(
                                selectedSong + 1,
                                static_cast<int>(
                                    songs.size()) - 1);
                    }

                    return true;
                }

                // =================================================
                // ENTER = PLAY SELECTED SONG
                // =================================================

                if (event == Event::Return)
                {
                    if (
                        selectedSong >= 0 &&
                        selectedSong <
                            static_cast<int>(
                                songs.size()))
                    {
                        player.play(
                            selectedSong);
                    }

                    return true;
                }

                // =================================================
                // MOOD LIGHTING
                // =================================================

                if (
                    event == Event::Character("l") ||
                    event == Event::Character("L"))
                {
                    moodIndex =
                        (moodIndex + 1) %
                        static_cast<int>(
                            MOOD_THEMES.size());

                    return true;
                }

                // =================================================
                // SEEK BACK
                // =================================================

                if (event == Event::ArrowLeft)
                {
                    player.seek(-5.0);

                    return true;
                }

                // =================================================
                // SEEK FORWARD
                // =================================================

                if (event == Event::ArrowRight)
                {
                    player.seek(5.0);

                    return true;
                }

                // =================================================
                // VOLUME UP
                // =================================================

                if (
                    event == Event::Character("+") ||
                    event == Event::Character("="))
                {
                    float currentVolume =
                        player.volumePercent() /
                        100.0f;

                    player.setVolume(
                        currentVolume +
                        0.05f);

                    return true;
                }

                // =================================================
                // VOLUME DOWN
                // =================================================

                if (
                    event ==
                    Event::Character("-"))
                {
                    float currentVolume =
                        player.volumePercent() /
                        100.0f;

                    player.setVolume(
                        currentVolume -
                        0.05f);

                    return true;
                }

                // =================================================
                // NUMBER KEYS 1-9
                // =================================================

                if (
                    event.character().size() == 1 &&
                    std::isdigit(
                        static_cast<unsigned char>(
                            event.character()[0])))
                {
                    int number =
                        event.character()[0] -
                        '0';

                    if (
                        number >= 1 &&
                        number <= 9 &&
                        number <=
                            static_cast<int>(
                                songs.size()))
                    {
                        player.play(
                            number - 1);

                        selectedSong =
                            number - 1;
                    }

                    return true;
                }

                // =================================================
                // IGNORE EVERYTHING ELSE
                // =================================================

                return false;
            });

    // ============================================================
    // UPDATE THREAD
    // ============================================================

    std::atomic<bool> running{true};

    std::thread updater(
        [&]
        {
            while (running)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100));

                // =================================================
                // ROTATE VINYL ONLY WHILE PLAYING
                // =================================================

                if (player.isPlaying())
                {
                    vinylFrame.fetch_add(1);
                }

                // =================================================
                // AUTO NEXT
                // =================================================

                if (player.hasEnded())
                {
                    player.next(true);

                    // Keep selected playlist item
                    // synchronized with playback.
                    selectedSong =
                        player.current();
                }

                // =================================================
                // REFRESH UI
                // =================================================

                screen.PostEvent(
                    Event::Custom);
            }
        });

    // ============================================================
    // START UI
    // ============================================================

    screen.Loop(component);

    // ============================================================
    // STOP UPDATE THREAD
    // ============================================================

    running = false;

    if (updater.joinable())
        updater.join();

    // ============================================================
    // SHUTDOWN AUDIO
    // ============================================================

    player.shutdown();

    return 0;
}