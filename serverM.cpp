#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/wait.h>

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace std;

#define PORT_UDP 44812
#define PORT_TCP 45812
#define PORT_A 41812
#define PORT_P 42812
#define PORT_Q 43812
#define MAXBUFLEN 1024
#define LOCALHOST "127.0.0.1"

void setupTCPSocket(int& sockfd, struct sockaddr_in& addr);
void setupUDPSocket(int& sockfd, struct sockaddr_in& addr);
void setupServer(struct sockaddr_in& addr, const int port);
string encryptPassword(const string& password);
bool handleLogin(int client_fd, int udp_sockfd,
                 struct sockaddr_in& serverAAddr,
                 bool& logged_in);

struct Credentials {
    char username[51];
    char password[51];
};

int main() {
    int tcp_sockfd, udp_sockfd, client_fd;
    struct sockaddr_in serverTCPAddr, serverUDPAddr, serverAAddr, clientAddr;
    socklen_t addr_size = sizeof(clientAddr);

    setupTCPSocket(tcp_sockfd, serverTCPAddr);
    setupUDPSocket(udp_sockfd, serverUDPAddr);
    setupServer(serverAAddr, PORT_A);

    cout << "[Server M] Booting up using UDP on port " << PORT_UDP << ".\n";

    while (true) {
        client_fd = accept(tcp_sockfd, (struct sockaddr*)&clientAddr, &addr_size);
        if (client_fd < 0) {
            cerr << "Accept failed.\n";
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            cerr << "Fork failed.\n";
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // Child process
            close(tcp_sockfd);
            cout << "[Server M] New client process started (PID: " << getpid() << ").\n";

            bool logged_in = false;

            while (true) {
                if (!logged_in) {
                    // Expect login credentials
                    bool active = handleLogin(client_fd, udp_sockfd, serverAAddr, logged_in);
                    if (!active) {
                        cout << "[Server M - PID " << getpid() << "] Client disconnected during login.\n";
                        break;
                    }
                    continue;
                } else {
                    // Expect command as string
                    char buffer[MAXBUFLEN] = {0};
                    int bytes_received = recv(client_fd, buffer, MAXBUFLEN - 1, 0);
                    if (bytes_received <= 0) {
                        cout << "[Server M - PID " << getpid() << "] Client disconnected after login.\n";
                        break;
                    }

                    buffer[bytes_received] = '\0';
                    string command(buffer);

                    if (command == "exit") {
                        cout << "[Server M - PID " << getpid() << "] Client requested exit.\n";
                        break;
                    }

                    // TODO: Forward to serverP/serverQ based on command
                    string reply = "[Server M] Command received: " + command;
                    send(client_fd, reply.c_str(), reply.length(), 0);
                }
            }

            close(client_fd);
            close(udp_sockfd);
            exit(0);
        }

        // Parent process
        close(client_fd);
    }

    close(tcp_sockfd);
    close(udp_sockfd);
    return 0;
}

void setupTCPSocket(int& sockfd, struct sockaddr_in& addr) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "TCP socket creation failed.\n";
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_TCP);
    inet_pton(AF_INET, LOCALHOST, &addr.sin_addr);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "TCP bind failed.\n";
        exit(1);
    }

    if (listen(sockfd, 5) < 0) {
        cerr << "Listen failed.\n";
        exit(1);
    }
}

void setupUDPSocket(int& sockfd, struct sockaddr_in& addr) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cerr << "UDP socket creation failed.\n";
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_UDP);
    addr.sin_addr.s_addr = inet_addr(LOCALHOST);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "UDP bind failed." << endl;
        exit(1);
    }
}

void setupServer(struct sockaddr_in& addr, const int port) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, LOCALHOST, &addr.sin_addr);
}

string encryptPassword(const string& password) {
    string encrypted;
    for (char c : password) {
        if (isdigit(c)) {
            encrypted += (c - '0' + 3) % 10 + '0';
        } else if (isupper(c)) {
            encrypted += (c - 'A' + 3) % 26 + 'A';
        } else if (islower(c)) {
            encrypted += (c - 'a' + 3) % 26 + 'a';
        } else {
            encrypted += c;  // leave special characters
        }
    }
    return encrypted;
}

bool handleLogin(int client_fd, int udp_sockfd,
                 struct sockaddr_in& serverAAddr,
                 bool& logged_in) {
    Credentials cred;
    memset(&cred, 0, sizeof(cred));

    int bytes_received = recv(client_fd, &cred, sizeof(cred), 0);
    if (bytes_received <= 0) {
        return false;
    }

    string username(cred.username);
    string password(cred.password);
    cout << "[Server M] Received username " << username << " and Password ****." << endl;

    string encrypted = encryptPassword(password);
    memset(cred.password, 0, sizeof(cred.password));
    strncpy(cred.password, encrypted.c_str(), sizeof(cred.password) - 1);

    cout << "[Server M] Sent the authentication request to Server A." << endl;
    sendto(udp_sockfd, &cred, sizeof(cred), 0,
           (struct sockaddr*)&serverAAddr, sizeof(serverAAddr));

    char reply[MAXBUFLEN] = {0};
    socklen_t len = sizeof(serverAAddr);
    recvfrom(udp_sockfd, reply, MAXBUFLEN - 1, 0,
             (struct sockaddr*)&serverAAddr, &len);

    cout << "[Server M] Received the response from server A using UDP over " << PORT_UDP << "." << endl;
    cout << "[Server M] Sent the response from server A to the client using TCP over port " << PORT_TCP << "." << endl;
    send(client_fd, reply, strlen(reply), 0);
    logged_in = string(reply) == "Login successful";
    return true;
}