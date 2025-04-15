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

using namespace std;

#define PORT_UDP 42812
#define LOCALHOST "127.0.0.1"

struct OwnedStockInfo {
    int shares;
    double avg_price;
    double total;
};

struct UserInfo {
    map<string, OwnedStockInfo> portfolio;
};

// Return all users with their portfolios
map<string, UserInfo> loadPortfolioDatabase(const string& filename) {
    map<string, UserInfo> userMap;
    ifstream infile(filename);
    string line;
    string current_user;

    // Read every line from input file
    while (getline(infile, line)) {
        istringstream iss(line);
        string first_token;
        iss >> first_token;

        if (first_token.empty()) continue; // skip empty lines

        string second_token;
        if (!(iss >> second_token)) {
            // Only one token → username line
            current_user = first_token;
            userMap[current_user] = UserInfo();
            // cout << "[DEBUG] User: " << current_user << endl;
        } else {
            // More than one token → stock info line
            string stock = first_token;
            int shares = stoi(second_token);
            double avg_price;

            if (iss >> avg_price) {
                userMap[current_user].portfolio[stock] = {shares, avg_price, shares * avg_price};
                // cout << "[DEBUG]   " << stock << ": " << shares << " shares @ $" << avg_price << endl;
            }
        }
    }

    return userMap;
}

int main() {
    int udp_sockfd;
    struct sockaddr_in serverAddr;

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        cerr << "creating UDP socket for server P failed" << endl;
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

    // Load all users' portfolios
    auto portfolios = loadPortfolioDatabase("portfolios.txt");


    // Loop for debugging
    // for (map<string, UserInfo>::const_iterator it = portfolios.begin(); it != portfolios.end(); ++it) {
    //     const string& username = it->first;
    //     const UserInfo& userInfo = it->second;

    //     cout << username << "'s portfolio:\n";
    //     for (map<string, OwnedStockInfo>::const_iterator stock_it = userInfo.portfolio.begin(); stock_it != userInfo.portfolio.end(); ++stock_it) {
    //         const string& stockName = stock_it->first;
    //         const OwnedStockInfo& OwnedStockInfo = stock_it->second;

    //         cout << "  " << stockName
    //              << " - " << OwnedStockInfo.shares
    //              << " shares @ $" << OwnedStockInfo.avg_price << "\n";
    //     }
    //     cout << endl;
    // }

    cout << "[Server P] Booting up using UDP on port "
         << PORT_UDP << "." << endl;

    while (true) {
        pause();
    }

    close(udp_sockfd);
    return 0;
}