
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>

using namespace std;

const int THREAD_COUNT = 5;
const int RUNS = 5;

// Check whether a character is a letter
bool isLetter(char ch) {
    return isalpha(static_cast<unsigned char>(ch));
}

// Count words in one section of the text
void countWords(
    const string& text,
    size_t start,
    size_t end,
    unordered_map<string, int>& wordMap
) {
    string word = "";

    for (size_t i = start; i < end; i++) {

        if (isLetter(text[i])) {
            word += static_cast<char>(
                tolower(static_cast<unsigned char>(text[i]))
            );
        }
        else {
            if (word != "") {
                wordMap[word]++;
                word = "";
            }
        }
    }

    // Count the last word if the section ends with a letter
    if (word != "") {
        wordMap[word]++;
    }
}

// Create boundaries for the chunks
vector<size_t> makeBoundaries(const string& text, int threadCount) {

    vector<size_t> boundaries(threadCount + 1);

    boundaries[0] = 0;
    boundaries[threadCount] = text.size();

    for (int i = 1; i < threadCount; i++) {

        size_t boundary = text.size() * i / threadCount;

        // Move forward if the boundary is inside a word
        while (boundary < text.size() && isLetter(text[boundary])) {
            boundary++;
        }

        boundaries[i] = boundary;
    }

    return boundaries;
}

// Count words using the requested number of threads
double runCounting(
    const string& text,
    int threadCount,
    unordered_map<string, int>& finalMap
) {
    vector<unordered_map<string, int>> localMaps(threadCount);
    vector<thread> workers;

    auto startTime = chrono::steady_clock::now();

    if (threadCount == 1) {

        workers.emplace_back(
            countWords,
            cref(text),
            0,
            text.size(),
            ref(localMaps[0])
        );
    }
    else {

        vector<size_t> boundaries =
            makeBoundaries(text, threadCount);

        for (int i = 0; i < threadCount; i++) {

            workers.emplace_back(
                countWords,
                cref(text),
                boundaries[i],
                boundaries[i + 1],
                ref(localMaps[i])
            );
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < threadCount; i++) {
        workers[i].join();
    }

    // Main thread merges all local maps
    finalMap.clear();

    for (int i = 0; i < threadCount; i++) {

        for (const auto& item : localMaps[i]) {
            finalMap[item.first] += item.second;
        }
    }

    auto endTime = chrono::steady_clock::now();

    chrono::duration<double, milli> elapsed =
        endTime - startTime;

    return elapsed.count();
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cout << "Usage: ./word_frequency filename.txt\n";
        return 1;
    }

    // Read the complete file
    ifstream file(argv[1], ios::binary);

    if (!file) {
        cout << "Could not open the file.\n";
        return 1;
    }

    string text(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>()
    );

    file.close();

    cout << "File size: " << text.size() << " bytes\n";

    if (text.size() < 20 * 1024 * 1024) {
        cout << "Error: File must be at least 20 MB.\n";
        return 1;
    }

    double oneThreadTotal = 0;
    double fiveThreadTotal = 0;

    unordered_map<string, int> finalMap;

    // Run one-thread version 5 times
    cout << "\nRunning with 1 thread...\n";

    for (int i = 0; i < RUNS; i++) {

        double time = runCounting(text, 1, finalMap);

        oneThreadTotal += time;

        cout << "Run " << i + 1 << ": "
             << fixed << setprecision(3)
             << time << " ms\n";
    }

    double oneThreadAverage = oneThreadTotal / RUNS;

    // Run five-thread version 5 times
    cout << "\nRunning with 5 threads...\n";

    for (int i = 0; i < RUNS; i++) {

        double time = runCounting(
            text,
            THREAD_COUNT,
            finalMap
        );

        fiveThreadTotal += time;

        cout << "Run " << i + 1 << ": "
             << fixed << setprecision(3)
             << time << " ms\n";
    }

    double fiveThreadAverage = fiveThreadTotal / RUNS;

    // Display top 10 words
    vector<pair<string, int>> words(
        finalMap.begin(),
        finalMap.end()
    );

    sort(words.begin(), words.end(),
        [](const pair<string, int>& a,
           const pair<string, int>& b) {

            if (a.second == b.second) {
                return a.first < b.first;
            }

            return a.second > b.second;
        }
    );

    cout << "\n========== TOP 10 WORDS ==========\n";

    for (int i = 0; i < 10 && i < (int)words.size(); i++) {

        cout << i + 1 << ". "
             << words[i].first
             << " : "
             << words[i].second << "\n";
    }

    // Display average times
    cout << "\n========== PERFORMANCE ==========\n";

    cout << "1-thread average: "
         << oneThreadAverage << " ms\n";

    cout << "5-thread average: "
         << fiveThreadAverage << " ms\n";

    // Calculate speedup
    double speedup = oneThreadAverage / fiveThreadAverage;

    cout << "Speedup: "
         << speedup << "x\n";

    return 0;
}
