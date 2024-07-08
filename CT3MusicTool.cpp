#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <unordered_set>

#define ErrorExit(Text) { cout << "Error: " << Text << "\n"; cin.get(); return 1; }

using namespace std;

// West Coast:     m03.ogg, m04.ogg, m05.ogg
// Glitter Oasis:  m08.ogg, m09.ogg, m10.ogg
// Small Apple:    m13.ogg, m14.ogg, m15.ogg
static const vector<vector<int>> SongIDs = { { 3, 8, 13 }, { 4, 9, 14 }, { 5, 10, 15 } };

static const string MusicDir = "CustomMusic";


// Requires ffmpeg.exe! //

static void MakeFiles(const vector<string_view>& Files, const vector<int>& OutputIDs)
{

    // Get output file paths -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    vector<string> OutputFiles;
    transform(OutputIDs.begin(), OutputIDs.end(), back_inserter(OutputFiles), [](int ID) {
        return format("Media/Music/m{:02d}.ogg", ID);
    });


    // Parse input .ogg files =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    string InputFiles = accumulate(Files.begin(), Files.end(), string(),
        [](const string_view& a, const string_view& b) { return a.data() + MusicDir + "/" + b.data() + "|"; });
    InputFiles.pop_back();


    // Send ffmpeg command -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    system(format("ffmpeg -hide_banner -loglevel quiet -y -i \"concat:{}\" -c copy {}",
        InputFiles, OutputFiles[0]).c_str());


    // Copy ffmpeg output to remaining output files =-=-=-=-

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

int main()
{

    // Start CT3.exe -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    cout << "Starting Crazy Taxi 3...\n";
    if (!CreateProcess(L"CT3.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        ErrorExit("Failed to start CT3.exe.");


    // Get songs in the CustomMusic folder -=-=-=-=-=-=-=-=-=-

    unordered_set<string_view> Songs;

    for (const auto& entry : filesystem::directory_iterator(MusicDir))
        if (entry.is_regular_file() && entry.path().extension() == ".ogg")
            Songs.insert(entry.path().filename().string());

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

    DWORD exitCode;
    while (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
    {

        // Shuffle music upon reaching menu =-=-=-=-=-=-=-=-=-

        ReadProcessMemory(pi.hProcess, (LPVOID) 0x00697998, &Value, sizeof(Value), nullptr);
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
            
            vector<thread> Threads;
            for (int i = 0; i < PlaylistCt; i++)
                Threads.emplace_back(MakeFiles, Playlists[i], SongIDs[i]);
            for (thread& Thread : Threads)
                Thread.join();

            cout << "Shuffle complete.\n";
        }

        bShouldShuffle = (Value == 0);
        Sleep(200);
    }


    // Exit program when game process terminates -=-=-=-=-=-=-

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    cout << "CT3.exe has exited, closing command line.\n";
    return 0;

}
