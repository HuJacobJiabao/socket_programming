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
#include <map>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace std;

#define PORT_UDP 41812
#define LOCALHOST "127.0.0.1"
#define MAXBUFLEN 1024

struct Credentials {
    char username[51];
    char password[51];
};

map<string, string> loadMemberDatabase(const string& filename);
string toLower(const string& s);

int main() {
    int udp_sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_len = sizeof(clientAddr);

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        cerr << "Creating UDP socket for server A failed." << endl;
        return 1; 
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(LOCALHOST);
    serverAddr.sin_port = htons(PORT_UDP);

    if (bind(udp_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Bind for UDP failed." << endl;
        return 1;
    }

    auto users = loadMemberDatabase("members.txt");

    cout << "[Server A] Booting up using UDP on port " << PORT_UDP << "." << endl;

    while (true) {
        Credentials cred;
        memset(&cred, 0, sizeof(cred));

        int bytes_received = recvfrom(udp_sockfd, &cred, sizeof(cred), 0,
                                      (struct sockaddr*)&clientAddr, &addr_len);
        if (bytes_received <= 0) {
            cerr << "[Server A] Failed to receive credentials." << endl;
            continue;
        }

        string username(cred.username);
        string password(cred.password);

        cout << "[Server A] Received username " << username << " and password ******." << endl;

        string lower_username = toLower(username);
        string response;
        if (users.find(lower_username) != users.end() && users[lower_username] == password) {
            response = "Login successful";
            cout << "[Server A] Member " << username << " has been authenticated." << endl; 
        } else {
            response = "Login failed";
            cout << "[Server A] The username " << username << " or password ****** is incorrect."<< endl;
        }

        sendto(udp_sockfd, response.c_str(), response.length(), 0,
               (struct sockaddr*)&clientAddr, addr_len);

    }

    close(udp_sockfd);
    return 0;
}

map<string, string> loadMemberDatabase(const string& filename) {
    map<string, string> userMap;
    ifstream infile(filename);
    string line;

    while (getline(infile, line)) {
        istringstream iss(line);
        string username, password;
        if (!(iss >> username >> password)) continue;

        userMap[toLower(username)] = password;
        // cout << "[Server A] Loaded user: " << username << ", Encrypted password: " << password << endl;
    }

    return userMap;
}

string toLower(const string& s) {
    string lower = s;
    for (char& c : lower) {
        c = tolower(c);
    }
    return lower;
}