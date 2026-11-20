
#include<cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <array>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>

using namespace std;

const bool ALICE_SIDE = false; 

const string FIFO_A2B = "/tmp/123_a2b";
const string FIFO_B2A = "/tmp/123_b2a";

atomic<bool> running(true);

const string banned[] = {
    "badword", "idiot", "stupid", "hate", "abuse"
};

bool containsBannedWord(string message) {
    for (char &c : message)
        c = tolower(static_cast<unsigned char>(c));

    for (string word : banned) {
        size_t pos = message.find(word);

        while (pos != string::npos) {
            bool leftBoundary =
                (pos == 0 || !isalpha(
                    static_cast<unsigned char>(message[pos - 1])));

            size_t end = pos + word.length();

            bool rightBoundary =
                (end == message.length() || !isalpha(
                    static_cast<unsigned char>(message[end])));

            if (leftBoundary && rightBoundary)
                return true;

            pos = message.find(word, pos + 1);
        }
    }

    return false;
}

void receiveMessages(int fd) {
    string message;
    char buffer[256];

    while (running) {
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int result = poll(&pfd, 1, 200);

        if (result <= 0)
            continue;

        int n = read(fd, buffer, sizeof(buffer));

        if (n <= 0)
            continue;

        for (int i = 0; i < n; i++) {
            if (buffer[i] == '\n') {
                if (message == "quit") {
                    cout << "\nOther user left the chat.\n";
                    running = false;
                    return;
                }

                if (containsBannedWord(message))
                    cout << "\nOther: [censored]\n";
                else
                    cout << "\nOther: " << message << "\n";

                message.clear();
            }
            else {
                message += buffer[i];
            }
        }
    }
}

int main() {
    mkfifo(FIFO_A2B.c_str(), 0666);
    if (errno != 0 && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    mkfifo(FIFO_B2A.c_str(), 0666);
    if (errno != 0 && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    int readFD, writeFD;

    // Both programs open FIFO A2B first, then FIFO B2A.
    if (ALICE_SIDE) {
        writeFD = open(FIFO_A2B.c_str(), O_WRONLY);
        readFD = open(FIFO_B2A.c_str(), O_RDONLY);
    }
    else {
        readFD = open(FIFO_A2B.c_str(), O_RDONLY);
        writeFD = open(FIFO_B2A.c_str(), O_WRONLY);
    }

    if (readFD == -1 || writeFD == -1) {
        perror("open");
        return 1;
    }

    string name = ALICE_SIDE ? "Alice" : "Bob";

    cout << name << " connected. Type quit to exit.\n";

    thread receiver(receiveMessages, readFD);

    while (running) {
        pollfd input;
        input.fd = STDIN_FILENO;
        input.events = POLLIN;
        input.revents = 0;

        int result = poll(&input, 1, 200);

        if (!running)
            break;

        if (result <= 0)
            continue;

        string message;
        if (!getline(cin, message))
            break;

        message += '\n';

        if (write(writeFD, message.c_str(), message.size()) == -1) {
            perror("write");
            break;
        }

        if (message == "quit\n") {
            running = false;
            break;
        }
    }

    running = false;

    receiver.join();

    close(readFD);
    close(writeFD);

    unlink(FIFO_A2B.c_str());
    unlink(FIFO_B2A.c_str());

    cout << "Chat closed.\n";

    return 0;
}
