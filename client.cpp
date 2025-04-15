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
#include <string>

#define SERVERM_TCP_PORT 45812
#define LOCALHOST "127.0.0.1"
#define MAXBUFLEN 1024

using namespace std;

struct Credentials {
    char username[51];
    char password[51];
};

int main() {
    int sockfd;
    struct sockaddr_in serverMaddr;

    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Failed to create TCP socket." << endl;
        return 1;
    }

    memset(&serverMaddr, 0, sizeof(serverMaddr));
    serverMaddr.sin_family = AF_INET;
    serverMaddr.sin_port = htons(SERVERM_TCP_PORT);
    inet_pton(AF_INET, LOCALHOST, &serverMaddr.sin_addr);

    if (connect(sockfd, (struct sockaddr*) &serverMaddr, sizeof(serverMaddr)) < 0) {
        cerr << "Connection to serverM failed." << endl;
        return 1;
    }

    cout << "[Client] Booting up." << endl;
    cout << "[Client] Logging in." << endl;
    // === Login Phase ===
    string username, password;
    while (true) {
        cout << "Please enter the username: ";
        cin >> username;
        cout << "Please enter the password: ";
        cin >> password;
        cin.ignore(); // flush leftover newline from cin

        Credentials cred;
        memset(&cred, 0, sizeof(cred));

        strncpy(cred.username, username.c_str(), sizeof(cred.username) - 1);
        strncpy(cred.password, password.c_str(), sizeof(cred.password) - 1);

        send(sockfd, &cred, sizeof(cred), 0);

        char response[MAXBUFLEN] = {0};
        int bytes_received = recv(sockfd, response, MAXBUFLEN - 1, 0);
        if (bytes_received > 0) {
            response[bytes_received] = '\0';
            string reply = response;

            if (reply == "Login successful") {
                cout << "[Client] You have been granted access." << endl;
                break;
            } else {
                cout << "[Client] The credentials are incorrect. Please try agian." << endl;
            }
        } else {
            cerr << "Connection lost during login." << endl;
            close(sockfd);
            return 1;
        }


    }


    // === Command Loop ===
    while (true) {
        cout << "[Client] Please enter the command:\n"
            << "<quote>\n"
            << "<quote <stock name>>\n"
            << "<buy <stock name> <number of shares>>\n"
            << "<sell <stock name> <number of shares>>\n"
            << "<position>\n"
            << "<exit>" << endl;
        string input;
        getline(cin, input);

        if (input.empty()) continue;

        send(sockfd, input.c_str(), input.length(), 0);

        if (input == "exit") {
            break;
        }

        char buffer[MAXBUFLEN] = {0};
        int bytes_received = recv(sockfd, buffer, MAXBUFLEN - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // ensure null termination
            cout << buffer << endl;
        } else {
            cerr << "Connection lost." << endl;
            break;
        }
    }

    close(sockfd);
    return 0;
}