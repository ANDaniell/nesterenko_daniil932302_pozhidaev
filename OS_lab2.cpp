#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <signal.h>
#include <sys/select.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

volatile sig_atomic_t g_hup = 0;

void hupHandler(int)
{
    g_hup = 1;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <port>\n";
        return EXIT_FAILURE;
    }

    char* endptr = nullptr;
    long portLong = strtol(argv[1], &endptr, 10);
    if (*argv[1] == '\0' || *endptr != '\0' || portLong <= 0 || portLong > 65535) {
        cout << "Invalid port\n";
        return EXIT_FAILURE;
    }
    int port = (int)portLong;

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listenFd);
        return EXIT_FAILURE;
    }

    if (listen(listenFd, SOMAXCONN) == -1) {
        perror("listen");
        close(listenFd);
        return EXIT_FAILURE;
    }

    cout << "Server on port " << port << endl;

    sigset_t blockMask, origMask;
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGHUP);

    if (sigprocmask(SIG_BLOCK, &blockMask, &origMask) == -1) {
        perror("sigprocmask");
        close(listenFd);
        return EXIT_FAILURE;
    }

    struct sigaction sa {};
    sa.sa_handler = hupHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, nullptr) == -1) {
        perror("sigaction");
        close(listenFd);
        return EXIT_FAILURE;
    }

    int clientFd = -1;

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenFd, &rfds);
        int maxFd = listenFd;

        if (clientFd != -1) {
            FD_SET(clientFd, &rfds);
            maxFd = max(maxFd, clientFd);
        }

        int ready = pselect(maxFd + 1, &rfds, nullptr, nullptr, nullptr, &origMask);

        if (ready == -1) {
            if (errno == EINTR) {
                if (g_hup) {
                    cout << "Received SIGHUP\n";
                    g_hup = 0;
                }
                continue;
            }
            perror("pselect");
            break;
        }

        if (FD_ISSET(listenFd, &rfds)) {
            sockaddr_in caddr{};
            socklen_t clen = sizeof(caddr);
            int newFd = accept(listenFd, (sockaddr*)&caddr, &clen);
            if (newFd == -1) {
                perror("accept");
            }
            else {
                cout << "Incoming connection\n";
                if (clientFd == -1) {
                    clientFd = newFd;
                    cout << "Accepted\n";
                }
                else {
                    cout << "Extra connection closed\n";
                    close(newFd);
                }
            }
        }

        if (clientFd != -1 && FD_ISSET(clientFd, &rfds)) {
            char buf[4096];
            ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
            if (n > 0) {
                cout << "Received " << n << " bytes\n";
            }
            else if (n == 0) {
                cout << "Client disconnected\n";
                close(clientFd);
                clientFd = -1;
            }
            else {
                perror("recv");
                close(clientFd);
                clientFd = -1;
            }
        }

        if (g_hup) {
            cout << "Received SIGHUP\n";
            g_hup = 0;
        }
    }

    if (clientFd != -1) close(clientFd);
    close(listenFd);
    return EXIT_FAILURE;
}
