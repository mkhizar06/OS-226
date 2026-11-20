
#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <cmath>
using namespace std;

const int N = 435;

using Matrix = vector<vector<double>>;

Matrix A(N, vector<double>(N));
Matrix B(N, vector<double>(N));

Matrix createMatrix()
{
    return Matrix(N, vector<double>(N, 0.0));
}

void initialize()
{
    mt19937 gen(42);
    uniform_real_distribution<double> dist(0.0, 10.0);

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
        {
            A[i][j] = dist(gen);
            B[i][j] = dist(gen);
        }
}

// Version 1: Single-thread i-j-k
Matrix version1()
{
    Matrix C = createMatrix();

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j] += A[i][k] * B[k][j];

    return C;
}

// Function used by parallel version
void calculateRows(Matrix &C, int start, int end)
{
    for (int i = start; i < end; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j] += A[i][k] * B[k][j];
}

// Version 2: Parallel i-j-k
Matrix version2(int threadCount)
{
    Matrix C = createMatrix();
    vector<thread> threads;

    int rows = N / threadCount;

    for (int t = 0; t < threadCount; t++)
    {
        int start = t * rows;
        int end = (t == threadCount - 1)
                  ? N : start + rows;

        threads.emplace_back(
            calculateRows, ref(C), start, end
        );
    }

    for (auto &t : threads)
        t.join();

    return C;
}

// Version 3: Single-thread i-k-j
Matrix version3()
{
    Matrix C = createMatrix();

    for (int i = 0; i < N; i++)
        for (int k = 0; k < N; k++)
            for (int j = 0; j < N; j++)
                C[i][j] += A[i][k] * B[k][j];

    return C;
}

double measure(Matrix (*function)())
{
    auto start = chrono::high_resolution_clock::now();

    Matrix C = function();

    auto end = chrono::high_resolution_clock::now();

    return chrono::duration<double, milli>(
        end - start
    ).count();
}

int main()
{
    initialize();

    Matrix reference = version1();

    cout << "Version 1: "
         << measure(version1) << " ms\n";

    for (int t : {1, 2, 4, 8})
    {
        auto start = chrono::high_resolution_clock::now();

        Matrix C = version2(t);

        auto end = chrono::high_resolution_clock::now();

        double time = chrono::duration<double, milli>(
            end - start
        ).count();

        cout << "Version 2 (" << t << " threads): "
             << time << " ms\n";

        bool correct = true;

        for (int i = 0; i < N && correct; i++)
            for (int j = 0; j < N; j++)
                if (abs(C[i][j] - reference[i][j]) > 1e-6)
                {
                    correct = false;
                    break;
                }

        cout << "Correct: " << (correct ? "Yes" : "No") << "\n";
    }

    auto start = chrono::high_resolution_clock::now();

    Matrix C3 = version3();

    auto end = chrono::high_resolution_clock::now();

    double time3 = chrono::duration<double, milli>(
        end - start
    ).count();

    cout << "Version 3 (i-k-j): " << time3 << " ms\n";

    cout << "Version 3 correct: ";

    cout << (abs(C3[0][0] - reference[0][0]) < 1e-6
             ? "Yes" : "No") << endl;

    return 0;
}
