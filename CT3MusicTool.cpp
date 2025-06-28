#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <unordered_set>

#define ErrorExit(Text) { cout << "Error: " << Text << "\n"; cin.get(); return 1; }

using namespace std;

// West Coast:     m03.ogg, m04.ogg, m05.ogg
// Glitter Oasis:  m08.ogg, m09.ogg, m10.ogg
// Small Apple:    m13.ogg, m14.ogg, m15.ogg
static const vector<vector<int>> SongIDs = { { 3, 8, 13 }, { 4, 9, 14 }, { 5, 10, 15 } };

static const string MusicDir = "CustomMusic";

static bool bNeededEncode = false;


// Requires ffmpeg.exe! //

static void MakeFiles(const vector<string_view>& Files, const vector<int>& OutputIDs)
{
    // Get output file paths -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    vector<string> OutputFiles;
    transform(OutputIDs.begin(), OutputIDs.end(), back_inserter(OutputFiles), [](int ID) {
        return format("Media/Music/m{:02d}.ogg", ID);
    });

    // Parse input .ogg files =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    const string InputList = format("{}/list{}.txt", MusicDir, OutputIDs[0]);

    ofstream out(InputList);
    if (!out.is_open())
    {
        cout << "Failed to open " << InputList << " for writing.\n";
        return;
    }

    for (const auto& file : Files)
        out << format("file '{}'\n", file);

    out.close();
    SetFileAttributesA(InputList.c_str(), FILE_ATTRIBUTE_HIDDEN);

    // Send ffmpeg command -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    const string Command = format("ffmpeg -hide_banner -loglevel quiet -y -f concat -safe 0 -i {} -map 0:a -c copy {}", InputList, OutputFiles[0]);
    system(Command.c_str());

    filesystem::remove(InputList);

    // Copy ffmpeg output to remaining output files =-=-=-=-=-

    ifstream Input(OutputFiles[0], ios::binary);
    vector<ofstream> Outputs;
    for (size_t i = 1; i < OutputFiles.size(); i++)
        Outputs.emplace_back(OutputFiles[i], ios::binary);

    const size_t BufferSize = 4096;
    char Buffer[BufferSize];
    while (Input.read(Buffer, BufferSize))
        for (ofstream& Output : Outputs)
            Output.write(Buffer, Input.gcount());
}

void ReencodeOgg(const string& FilePath)
{
    std::string ProbeCmd = "ffprobe -hide_banner -loglevel quiet -v error -select_streams a:0 "
        "-show_entries stream=codec_name,sample_rate,channels "
        "-of default=noprint_wrappers=1:nokey=1 \"" + FilePath + "\"";

    FILE* Pipe = _popen(ProbeCmd.c_str(), "r");
    if (!Pipe) return;

    char Buffer[256];
    std::vector<std::string> Lines;
    while (fgets(Buffer, sizeof(Buffer), Pipe))
        Lines.emplace_back(Buffer);
    _pclose(Pipe);

    if (Lines.size() < 3) return;

    std::string Codec = Lines[0];
    int SampleRate = std::stoi(Lines[1]);
    int Channels = std::stoi(Lines[2]);

    if (Codec.find("vorbis") != std::string::npos && SampleRate == 44100 && Channels == 2)
        return;

    if (!bNeededEncode)
    {
        bNeededEncode = true;
        cout << "Re-encoding music files...\n";
    }
   
    const string TempFile = FilePath + ".temp.ogg";

    system(format("ffmpeg -hide_banner -loglevel quiet -y -i \"{}\" -map_metadata -1 -c:a libvorbis -ar 44100 -ac 2 \"{}\"", FilePath, TempFile).c_str());

    filesystem::remove(FilePath.c_str());
    filesystem::rename(TempFile.c_str(), FilePath.c_str());
}

// #define SkipEXE
int main()
{
#ifndef SkipEXE
    // Start CT3.exe -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    cout << "Starting Crazy Taxi 3...\n";
    if (!CreateProcess(L"CT3.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        ErrorExit("Failed to start CT3.exe.");
#endif

    // Get songs in the CustomMusic folder -=-=-=-=-=-=-=-=-=-

    unordered_set<string> Songs;

    for (const auto& entry : filesystem::directory_iterator(MusicDir))
        if (entry.is_regular_file() && entry.path().extension() == ".ogg")
        {
            const string song(entry.path().filename().string());
            Songs.insert(song);
        }

    vector<thread> Reencoders;
    for (const string& Song : Songs)
    {
        Reencoders.emplace_back(ReencodeOgg, MusicDir + "/" + Song);
    }
    for (thread& Thread : Reencoders)
        Thread.join();

    if (bNeededEncode)
    {
        cout << "Re-encoding finished.\n";
    }
            
    const size_t PlaylistCt = SongIDs.size();

    if (Songs.size() < PlaylistCt)
        ErrorExit("Not enough songs found in music directory (need at least " << PlaylistCt << " .ogg files).");

    cout << Songs.size() << " songs found in music directory.\n";
    for (int i = 0; i < 17; i++) cout << "-="; cout << "-\n";

    // Lambdas for shuffling playlists -=-=-=-=-=-=-=-=-=-=-=-

    random_device rd;
    mt19937 Generator(rd());
    auto Shuffle = [&Generator](vector<string_view>& Playlist)
    {
        shuffle(Playlist.begin(), Playlist.end(), Generator);
    };
    auto SetupShuffle = [&Shuffle, &Songs](vector<string_view>& Playlist)
    {
        Playlist.assign(Songs.begin(), Songs.end());
        Shuffle(Playlist);
    };

    // Begin checking process memory -=-=-=-=-=-=-=-=-=-=-=-=-

    int Value{}; bool bShouldShuffle{}, bShuffled{};

#ifndef SkipEXE
    DWORD exitCode;
    while (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
#endif
    {
        // Shuffle music upon reaching menu =-=-=-=-=-=-=-=-=-

#ifndef SkipEXE
        ReadProcessMemory(pi.hProcess, (LPVOID)0x00697998, &Value, sizeof(Value), nullptr);
#endif
        if (!bShuffled || ((Value == 0) && !bShouldShuffle))
        {
            bShuffled = true;
            cout << "Shuffling music...\n";

            // Make playlists containing all the songs.

            vector<vector<string_view>> Playlists(PlaylistCt);
            for (vector<string_view>& Playlist : Playlists)
                SetupShuffle(Playlist);
            
            // Where each playlist meets, make sure the same song doesn't play back-to-back.
            // Also check that each playlist starts with a unique song.

            int ShufflesFinished{};
            do
            {
                for (int i = 0; i < PlaylistCt; i++)
                {
                    if (Playlists[i].back() == Playlists[(i < PlaylistCt - 1) ? (i + 1) : 0].front())
                        Shuffle(Playlists[i]);
                    else
                    {
                        bool MatchesFront{};
                        const string_view& MyFront = Playlists[i].front();
                        for (int j = 0; j < i; j++)
                        {
                            if (MyFront == Playlists[j].front())
                            {
                                MatchesFront = true;
                                break;
                            }
                        }
                        if (!MatchesFront)
                            ShufflesFinished++;
                    }
                    if (ShufflesFinished == 0)
                        break;
                }
            }
            while (ShufflesFinished != PlaylistCt);

            // Render the audio files.
            
            auto Start = chrono::high_resolution_clock::now();

            vector<thread> Threads;
            for (int i = 0; i < PlaylistCt; i++)
                Threads.emplace_back(MakeFiles, Playlists[i], SongIDs[i]);
            for (thread& Thread : Threads)
                Thread.join();

            auto End = chrono::high_resolution_clock::now();
            auto Duration = duration_cast<chrono::milliseconds>(End - Start).count();

            cout << "Shuffle complete in " << Duration << " ms.\n";
        }

        bShouldShuffle = (Value == 0);
        Sleep(200);
    }

    // Exit program when game process terminates -=-=-=-=-=-=-

#ifndef SkipEXE
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    cout << "CT3.exe has exited, closing command line.\n";
#endif

    return 0;

}
