
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <string>
#include <iomanip>
#include <algorithm>

using namespace std;

struct Process {
    string name;

    int arrival = 0;
    vector<int> bursts;

    int burstIndex = 0;
    int remainingCPU = 0;
    int remainingIO = 0;

    int level = 0;
    int quantumUsed = 0;
    int readyWait = 0;

    int firstResponse = -1;
    int completionTime = -1;

    bool arrived = false;
    bool blocked = false;
    bool finished = false;
};

struct Slice {
    string name;
    int level;
    int start;
    int end;
};

vector<Process> processes;

queue<int> Q0, Q1, Q2;

vector<Slice> gantt;

int currentTime = 0;
int completedProcesses = 0;

const int QUANTUM[2] = {4, 8};

// --------------------------------------------------
// Add process to its appropriate ready queue
// --------------------------------------------------

void addToReadyQueue(int index) {
    Process &p = processes[index];

    p.readyWait = 0;
    p.quantumUsed = 0;

    if (p.level == 0)
        Q0.push(index);
    else if (p.level == 1)
        Q1.push(index);
    else
        Q2.push(index);
}

// --------------------------------------------------
// Get the highest-priority ready process
// --------------------------------------------------

int getNextProcess() {
    if (!Q0.empty()) {
        int id = Q0.front();
        Q0.pop();
        return id;
    }

    if (!Q1.empty()) {
        int id = Q1.front();
        Q1.pop();
        return id;
    }

    if (!Q2.empty()) {
        int id = Q2.front();
        Q2.pop();
        return id;
    }

    return -1;
}

// --------------------------------------------------
// Check whether a higher-priority process is ready
// --------------------------------------------------

bool higherPriorityReady(int level) {
    if (level > 0 && !Q0.empty())
        return true;

    if (level > 1 && !Q1.empty())
        return true;

    return false;
}

// --------------------------------------------------
// Move processes that waited more than 36 units up
// one queue level
// --------------------------------------------------

void applyAging() {
    queue<int> temp;

    // Check Q1
    while (!Q1.empty()) {
        int id = Q1.front();
        Q1.pop();

        processes[id].readyWait++;

        if (processes[id].readyWait > 36) {
            processes[id].level = 0;
            processes[id].readyWait = 0;
            processes[id].quantumUsed = 0;
            Q0.push(id);
        } else {
            temp.push(id);
        }
    }

    Q1 = temp;
    temp = queue<int>();

    // Check Q2
    while (!Q2.empty()) {
        int id = Q2.front();
        Q2.pop();

        processes[id].readyWait++;

        if (processes[id].readyWait > 36) {
            processes[id].level = 1;
            processes[id].readyWait = 0;
            processes[id].quantumUsed = 0;
            Q1.push(id);
        } else {
            temp.push(id);
        }
    }

    Q2 = temp;

    // Q0 processes are not promoted, but their
    // waiting time is still tracked.
    temp = queue<int>();

    while (!Q0.empty()) {
        int id = Q0.front();
        Q0.pop();

        processes[id].readyWait++;
        temp.push(id);
    }

    Q0 = temp;
}

// --------------------------------------------------
// Add a Gantt chart slice.
// Consecutive slices of the same process and queue
// are combined.
// --------------------------------------------------

void addGanttSlice(int id, int start, int end) {
    string name = processes[id].name;
    int level = processes[id].level;

    if (!gantt.empty() &&
        gantt.back().name == name &&
        gantt.back().level == level &&
        gantt.back().end == start) {

        gantt.back().end = end;
    } else {
        gantt.push_back({name, level, start, end});
    }
}

// --------------------------------------------------
// Print horizontal Gantt chart
// --------------------------------------------------

void printGanttChart() {
    cout << "\n========== GANTT CHART ==========\n\n";

    if (gantt.empty()) {
        cout << "No CPU execution recorded.\n";
        return;
    }

    vector<int> widths;

    for (const Slice &s : gantt) {
        string label = s.name + "(Q" + to_string(s.level) + ")";

        int duration = s.end - s.start;

        int width = max({
            8,
            (int)label.length() + 2,
            duration * 2,
            (int)to_string(s.start).length() + 1,
            (int)to_string(s.end).length() + 1
        });

        widths.push_back(width);
    }

    // Top border
    cout << "+";
    for (int width : widths) {
        cout << string(width, '-') << "+";
    }
    cout << "\n";

    // Process names and queue levels
    cout << "|";

    for (int i = 0; i < (int)gantt.size(); i++) {
        string label = gantt[i].name +
                       "(Q" + to_string(gantt[i].level) + ")";

        int width = widths[i];
        int left = (width - (int)label.length()) / 2;
        int right = width - left - (int)label.length();

        cout << string(left, ' ')
             << label
             << string(right, ' ')
             << "|";
    }

    cout << "\n";

    // Bottom border
    cout << "+";
    for (int width : widths) {
        cout << string(width, '-') << "+";
    }
    cout << "\n";

    // Timeline
    cout << left;

    for (int i = 0; i < (int)gantt.size(); i++) {
        cout << setw(widths[i] + 1) << gantt[i].start;
    }

    cout << gantt.back().end << "\n\n";
}

// --------------------------------------------------
// Load processes from input file
// Format:
// name arrival cpu io cpu io ... cpu
// --------------------------------------------------

bool loadProcesses(const string &filename) {
    ifstream file(filename);

    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << "\n";
        return false;
    }

    string line;

    while (getline(file, line)) {
        if (line.empty())
            continue;

        if (line[0] == '#')
            continue;

        stringstream ss(line);

        Process p;

        if (!(ss >> p.name >> p.arrival)) {
            cerr << "Invalid input line: " << line << "\n";
            return false;
        }

        int value;

        while (ss >> value) {
            if (value <= 0) {
                cerr << "Burst times must be positive.\n";
                return false;
            }

            p.bursts.push_back(value);
        }

        // There must be an odd number of bursts:
        // CPU, IO, CPU, IO, CPU...
        if (p.bursts.empty() || p.bursts.size() % 2 == 0) {
            cerr << "Invalid CPU/I-O burst sequence for "
                 << p.name << "\n";
            return false;
        }

        if (p.arrival < 0) {
            cerr << "Arrival time cannot be negative.\n";
            return false;
        }

        p.remainingCPU = p.bursts[0];

        processes.push_back(p);
    }

    return !processes.empty();
}

// --------------------------------------------------
// Main MLFQ simulation
// --------------------------------------------------

void simulateMLFQ() {
    int n = processes.size();

    int running = -1;

    while (completedProcesses < n) {

        // ------------------------------------------
        // 1. Add newly arrived processes to Q0
        // ------------------------------------------

        for (int i = 0; i < n; i++) {
            if (!processes[i].arrived &&
                processes[i].arrival <= currentTime) {

                processes[i].arrived = true;
                processes[i].level = 0;

                addToReadyQueue(i);
            }
        }

        // ------------------------------------------
        // 2. Preempt if a higher-priority queue has
        //    a ready process
        // ------------------------------------------

        if (running != -1 &&
            higherPriorityReady(processes[running].level)) {

            int id = running;

            processes[id].quantumUsed = 0;

            addToReadyQueue(id);

            running = -1;
        }

        // ------------------------------------------
        // 3. Select next process if CPU is free
        // ------------------------------------------

        if (running == -1) {
            running = getNextProcess();
        }

        // ------------------------------------------
        // 4. If no process is ready, CPU is idle
        // ------------------------------------------

        if (running == -1) {
            int start = currentTime;

            currentTime++;

            // Age processes waiting in ready queues
            applyAging();

            // Reduce I/O time for blocked processes
            for (int i = 0; i < n; i++) {
                if (processes[i].blocked) {
                    processes[i].remainingIO--;

                    if (processes[i].remainingIO == 0) {
                        processes[i].blocked = false;
                        addToReadyQueue(i);
                    }
                }
            }

            // Add idle interval to Gantt chart
            if (!gantt.empty() &&
                gantt.back().name == "IDLE" &&
                gantt.back().end == start) {

                gantt.back().end = currentTime;
            } else {
                gantt.push_back({
                    "IDLE", -1, start, currentTime
                });
            }

            continue;
        }

        // ------------------------------------------
        // 5. Record first response time
        // ------------------------------------------

        Process &p = processes[running];

        if (p.firstResponse == -1) {
            p.firstResponse = currentTime - p.arrival;
        }

        // ------------------------------------------
        // 6. Execute process for one time unit
        // ------------------------------------------

        int start = currentTime;

        p.remainingCPU--;
        p.quantumUsed++;

        currentTime++;

        addGanttSlice(running, start, currentTime);

        // ------------------------------------------
        // 7. Age processes that were ready during
        //    this time unit
        // ------------------------------------------

        applyAging();

        // ------------------------------------------
        // 8. Progress all existing I/O operations
        // ------------------------------------------

        for (int i = 0; i < n; i++) {
            if (processes[i].blocked) {
                processes[i].remainingIO--;

                if (processes[i].remainingIO == 0) {
                    processes[i].blocked = false;
                    addToReadyQueue(i);
                }
            }
        }

        // ------------------------------------------
        // 9. Check whether current CPU burst finished
        // ------------------------------------------

        if (p.remainingCPU == 0) {

            p.burstIndex++;

            // All bursts completed
            if (p.burstIndex >= (int)p.bursts.size()) {
                p.finished = true;
                p.completionTime = currentTime;

                completedProcesses++;
                running = -1;
            }

            // CPU burst finished, process needs I/O
            else {
                p.remainingIO = p.bursts[p.burstIndex];
                p.burstIndex++;

                p.blocked = true;

                p.remainingCPU = p.bursts[p.burstIndex];

                p.quantumUsed = 0;

                // Process stays at its current queue level
                running = -1;
            }
        }

        // ------------------------------------------
        // 10. Check quantum expiration
        // ------------------------------------------

        else if (p.level < 2 &&
                 p.quantumUsed == QUANTUM[p.level]) {

            // Full quantum used without finishing
            // the current CPU burst: demote process
            p.level++;

            p.quantumUsed = 0;

            addToReadyQueue(running);

            running = -1;
        }
    }
}

// --------------------------------------------------
// Print process response times and average
// --------------------------------------------------

void printResults() {
    double totalResponse = 0;

    cout << "\n========== PROCESS RESULTS ==========\n\n";

    cout << left
         << setw(15) << "Process"
         << setw(18) << "Response Time"
         << setw(18) << "Completion Time"
         << "\n";

    cout << string(51, '-') << "\n";

    for (const Process &p : processes) {
        cout << left
             << setw(15) << p.name
             << setw(18) << p.firstResponse
             << setw(18) << p.completionTime
             << "\n";

        totalResponse += p.firstResponse;
    }

    cout << "\nAverage Response Time = "
         << fixed << setprecision(2)
         << totalResponse / processes.size()
         << " time units\n";
}

// --------------------------------------------------
// Main function
// --------------------------------------------------

int main(int argc, char *argv[]) {

    string filename = "processes.txt";

    if (argc > 1) {
        filename = argv[1];
    }

    if (!loadProcesses(filename)) {
        cerr << "No valid processes loaded.\n";
        return 1;
    }

    cout << "MLFQ Scheduling Simulation\n";
    cout << "Q0: Round Robin, Quantum = 4\n";
    cout << "Q1: Round Robin, Quantum = 8\n";
    cout << "Q2: FCFS\n";
    cout << "Aging: More than 36 consecutive waiting units\n";

    simulateMLFQ();

    printGanttChart();

    printResults();

    return 0;
}
