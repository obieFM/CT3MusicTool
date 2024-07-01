#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_set>

#define cout std::cout

#define ErrorExit(Text) { cout << "Error: " << Text << "\n"; cin.get(); return 1; }

using namespace std;

static unordered_set<string> Songs;

// West Coast:     m03.ogg, m04.ogg, m05.ogg
// Glitter Oasis:  m08.ogg, m09.ogg, m10.ogg
// Small Apple:    m13.ogg, m14.ogg, m15.ogg
static const vector<vector<int>> SongIDs = { { 3, 8, 13 }, { 4, 9, 14 }, { 5, 10, 15 } };


// Requires ffmpeg.exe! //

static void MakeFiles(const vector<string>& Files, const vector<int>& OutputIDs)
{

    // Get output file paths -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    vector<string> OutputFiles;
    for (int id : OutputIDs) {
        stringstream ss;
        ss << "Media/Music/m" << setw(2) << setfill('0') << id << ".ogg";
        OutputFiles.push_back(ss.str());
    }


    // Parse input .ogg files =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    stringstream ss;
    ss << "ffmpeg -hide_banner -loglevel quiet -y -i \"concat:";
    for (const string& File : Files)
    {
        ss << "CustomMusic/" << File;
        if (File != Files.back())
            ss << "|";
    }


    // Send ffmpeg command -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

    ss << "\" -c copy " << OutputFiles[0];
    system(ss.str().c_str());


    // Copy ffmpeg output to remaining 2 output files =-=-=-=-

    ifstream input(OutputFiles[0], ios::binary);
    ofstream output1(OutputFiles[1], ios::binary);
    ofstream output2(OutputFiles[2], ios::binary);

    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)))
    {
        output1.write(buffer, input.gcount());
        output2.write(buffer, input.gcount());
    }

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

    const filesystem::directory_iterator end{};
    for (filesystem::directory_iterator iter{"CustomMusic"}; iter != end; ++iter)
        if (filesystem::is_regular_file(*iter) && iter->path().extension() == ".ogg")
            Songs.emplace(iter->path().filename().string());

    if (Songs.size() < 4)
        ErrorExit("Not enough songs found in music directory (need at least 4 .ogg files).");

    cout << Songs.size() << " songs found in music directory.\n";
    for (int i = 0; i < 17; i++) cout << "-="; cout << "-\n";


    // Lambdas for shuffling playlists -=-=-=-=-=-=-=-=-=-=-=-

    random_device rd;
    mt19937 Generator(rd());
    auto Shuffle = [&Generator](vector<string>& Playlist)
    {
        shuffle(Playlist.begin(), Playlist.end(), Generator);
    };
    auto SetupShuffle = [&Shuffle](vector<string>& Playlist)
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

        ReadProcessMemory(pi.hProcess, (void*) 0x00697998, &Value, sizeof(Value), 0);
        if (!bShuffled || ((Value == 0) && !bShouldShuffle))
        {
            bShuffled = true;
            cout << "Shuffling music...\n";

            // Make 3 playlists containing all the songs.

            vector<vector<string>> Playlists(SongIDs.size());
            for (vector<string>& Playlist : Playlists)
                SetupShuffle(Playlist);

            // Where each playlist meets, make sure the same song doesn't play back-to-back.
            // Also check that each playlist starts with a unique song (for Crazy X).

            while (true)
            {
                if (Playlists[0].back() == Playlists[1].front())
                    Shuffle(Playlists[0]);
                else if (Playlists[1].back() == Playlists[2].front() || Playlists[1].front() == Playlists[0].front())
                    Shuffle(Playlists[1]);
                else if (Playlists[2].back() == Playlists[0].front() || Playlists[2].front() == Playlists[1].front() || Playlists[2].front() == Playlists[0].front())
                    Shuffle(Playlists[2]);
                else break;
            }
            
            vector<thread> ShuffleThreads;
            for (int i = 0; i < Playlists.size(); i++)
                ShuffleThreads.emplace_back(MakeFiles, Playlists[i], SongIDs[i]);
            for (thread& ShuffleThread : ShuffleThreads)
                ShuffleThread.join();

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
