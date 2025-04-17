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
#define MAXBUFLEN 1024

struct StockInfo {
    vector<double> prices;
    int currentIndex = 0;
};

string processQuoteCommand(const string& command, map<string, StockInfo>& quotes);

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
    struct sockaddr_in serverAddr, serverMAddr;
    socklen_t addr_len = sizeof(serverMAddr);

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

    char buffer[MAXBUFLEN];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recvfrom(udp_sockfd, buffer, MAXBUFLEN - 1, 0,
                                    (struct sockaddr*)&serverMAddr, &addr_len);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            string command(buffer);

            istringstream iss(command);
            string cmd;
            iss >> cmd;

            if (cmd == "quote") {
                string stock;
                iss >> stock;
                if (!stock.empty()) {
                    cout << "[Server Q] Received a quote request from the main server for stock " 
                         << stock << ".\n";
                } else {
                    cout << "[Server Q] Received a quote request from the main server." << endl;
                }
                string reply = processQuoteCommand(command, quotes);

                sendto(udp_sockfd, reply.c_str(), reply.length(), 0,
                    (struct sockaddr*)&serverMAddr, addr_len);

                if (!stock.empty()) {
                    cout << "[Server Q] Returned the stock quote of " 
                         << stock << ".\n";
                } else {
                    cout << "[Server Q] Returned all stock quotes." << endl;
                }

            } else if (cmd == "forward")  {
                string stock;
                iss >> stock;

                auto it = quotes.find(stock);
                cout << "[Server Q] Received a time forward request for " << stock
                     << ", the current price of that stock is " 
                     << it->second.prices[(it->second.currentIndex) % 10]
                     << " at time " << it->second.currentIndex << ".\n";
                it->second.currentIndex++;
                         
            } else{
                string reply = "[Server Q] Invalid command. Only 'quote' supported.\n——Start a new request——";
                sendto(udp_sockfd, reply.c_str(), reply.length(), 0,
                    (struct sockaddr*)&serverMAddr, addr_len);

                cout << "[Server Q] Rejected unsupported command: " << cmd << endl;
            }
        }
    }

    close(udp_sockfd);
    return 0;
}

string processQuoteCommand(const string& command, map<string, StockInfo>& quotes) {
    istringstream iss(command);
    string cmd, stock;
    iss >> cmd >> stock;

    ostringstream response;

    if (stock.empty()) {
        // General quote request
        for (auto& pair : quotes) {
            const string& stockName = pair.first;
            StockInfo& info = pair.second;

            if (!info.prices.empty()) {
                double price = info.prices[info.currentIndex % 10];
                response << stockName << " " << price << "\n";
            }
        }
    } else {
        // Specific quote request
        auto it = quotes.find(stock);
        if (it == quotes.end()) {
            response << stock << " does not exist. Please try again.\n";
        } else {
            StockInfo& info = it->second;
            if (info.prices.empty()) {
                response << stock << " does not exist. Please try again.\n";
            } else {
                double price = info.prices[info.currentIndex % 10];
                response << stock << " " << price << "\n";
            }
        }
    }

    return response.str();
}