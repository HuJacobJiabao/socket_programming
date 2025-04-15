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
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

#define PORT_UDP 43812
#define LOCALHOST "127.0.0.1"

struct StockInfo {
    vector<double> prices;
    int currentIndex = 0;
};

map<string, StockInfo> loadQuoteDatabase(const string& filename) {
    map<string, StockInfo> quoteMap;
    ifstream infile(filename);
    string line;

    // Read every line from input file
    while (getline(infile, line)) {
        istringstream iss(line);
        string stock;
        iss >> stock;

        double price;
        StockInfo info;
        while (iss >> price) {
            info.prices.push_back(price);
        }

        quoteMap[stock] = info;
    }

    return quoteMap;
}

int main() {
    int udp_sockfd;
    struct sockaddr_in serverAddr;

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        cerr << "creating UDP socket for server Q failed" << endl;
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(LOCALHOST);
    serverAddr.sin_port = htons(PORT_UDP);

    if (bind(udp_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "bind for udp failed" << endl;
        return 1;
    }

    auto quotes = loadQuoteDatabase("quotes.txt");

    // Debug print
    // for (const auto& pair : quotes) {
    //     cout << pair.first << ": ";
    //     for (double p : pair.second.prices) cout << p << " ";
    //     cout << endl;
    // }

    cout << "[Server Q] Booting up using UDP on port "
         << PORT_UDP << "." << endl;

    while (true) {
        pause();
    }

    close(udp_sockfd);
    return 0;
}