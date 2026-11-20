
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <iomanip>

using namespace std;

const int N = 7;
const long QUANTUM_NS = 160000000;

// Get wall-clock time in seconds
double getTime()
{
    timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// Sleep for 160 milliseconds
void sleepQuantum()
{
    timespec req;

    req.tv_sec = 0;
    req.tv_nsec = QUANTUM_NS;

    while (nanosleep(&req, &req) == -1)
    {
        if (errno != EINTR)
        {
            perror("nanosleep");
            exit(1);
        }
    }
}

// Child process workload
void childWork()
{
    // Wait until parent stops and resumes us
    raise(SIGSTOP);

    timespec start, now;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    int counter = 0;

    while (counter < 20)
    {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);

        double cpuTime =
            (now.tv_sec - start.tv_sec) +
            (now.tv_nsec - start.tv_nsec) / 1000000000.0;

        // Print every 100 ms of CPU time
        if (cpuTime >= (counter + 1) * 0.1)
        {
            counter++;

            printf("Child PID: %d | Counter: %d\n",
                   getpid(), counter);

            fflush(stdout);
        }
    }

    _exit(0);
}

int main()
{
    vector<pid_t> children;

    vector<bool> finished(N, false);

    vector<double> completion(N, 0.0);

    // Prevent duplicated buffered output after fork
    fflush(NULL);

    // Create seven child processes
    for (int i = 0; i < N; i++)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            childWork();
        }
        else if (pid > 0)
        {
            children.push_back(pid);
        }
        else
        {
            perror("fork");
            return 1;
        }
    }

    // Ensure all children have stopped themselves
    for (int i = 0; i < N; i++)
    {
        int status;

        if (waitpid(children[i], &status, WUNTRACED) == -1)
        {
            perror("waitpid");
            return 1;
        }
    }

    double startTime = getTime();

    int remaining = N;

    cout << fixed << setprecision(3);

    cout << "\nROUND ROBIN SCHEDULER\n";
    cout << "Quantum: 160 ms\n\n";

    // Scheduling loop
    while (remaining > 0)
    {
        for (int i = 0; i < N; i++)
        {
            if (finished[i])
                continue;

            pid_t pid = children[i];

            // Resume selected child
            if (kill(pid, SIGCONT) == -1)
            {
                if (errno == ESRCH)
                    continue;

                perror("SIGCONT");
                continue;
            }

            double sliceStart = getTime();

            // Allow child to execute for one quantum
            sleepQuantum();

            // Stop child at end of quantum
            kill(pid, SIGSTOP);

            double sliceEnd = getTime();

            // Check whether child has finished
            int status;

            pid_t result = waitpid(pid, &status, WNOHANG);

            if (result == pid)
            {
                finished[i] = true;
                remaining--;

                completion[i] = getTime() - startTime;
            }

            cout << "[" << sliceStart - startTime
                 << " - " << sliceEnd - startTime
                 << " s] PID " << pid;

            if (finished[i])
                cout << " FINISHED";

            cout << endl;
        }
    }

    // Display completion times
    cout << "\nCHILD COMPLETION TIMES\n";

    for (int i = 0; i < N; i++)
    {
        cout << "PID " << children[i]
             << " : " << completion[i]
             << " seconds\n";
    }

    return 0;
}
