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
#include <algorithm>

using namespace std;

#define PORT_UDP 42812
#define LOCALHOST "127.0.0.1"
#define MAXBUFLEN 1024

struct OwnedStockInfo {
    int shares;
    double avg_price;
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
            transform(current_user.begin(), current_user.end(), current_user.begin(), ::tolower);
            userMap[current_user] = UserInfo();
            // cout << "[DEBUG] User: " << current_user << endl;
        } else {
            // More than one token → stock info line
            string stock = first_token;
            int shares = stoi(second_token);
            double avg_price;

            if (iss >> avg_price) {
                userMap[current_user].portfolio[stock] = {shares, avg_price};
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

    cout << "[Server P] Booting up using UDP on port "
         << PORT_UDP << "." << endl;
    
    while (true) {
        char buffer[MAXBUFLEN] = {0};
        struct sockaddr_in mAddr;
        socklen_t mAddrLen = sizeof(mAddr);

        int bytes_received = recvfrom(udp_sockfd, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr*)&mAddr, &mAddrLen);
        if (bytes_received <= 0) continue;

        buffer[bytes_received] = '\0';
        string request(buffer);
        istringstream iss(request);

        string confirm;
        iss >> confirm;

        if (confirm == "Y") {
            string username, cmd, stock;
            int shares;
            double price;
            iss >> username >> cmd >> stock >> shares >> price;
            string lower_username = username;
            transform(lower_username.begin(), lower_username.end(), lower_username.begin(), ::tolower);
            if (cmd == "buy") {
                cout << "[Server P] Received a buy request from the client." << endl;

                auto& user_stock = portfolios[lower_username].portfolio[stock];
                double total_cost = shares * price;

                user_stock.avg_price = 
                    (user_stock.avg_price * user_stock.shares + total_cost) / 
                    (user_stock.shares + shares);
                user_stock.shares += shares;

                cout << "[Server P] Successfully bought " << shares
                    << " shares of " << stock
                    << " and updated " << username << "’s portfolio." << endl;

                ostringstream response;
                response << username << " successfully bought " << shares
                        << " shares of " << stock << ".\n";

                string reply = response.str();
                sendto(udp_sockfd, reply.c_str(), reply.length(), 0,
                    (struct sockaddr*)&mAddr, mAddrLen);
            }

            else if (cmd == "sell") {
                cout << "[Server P] User approves selling the stock." << endl;
                auto& user_portfolio = portfolios[lower_username].portfolio;


                user_portfolio[stock].shares -= shares;
                if (user_portfolio[stock].shares == 0) {
                    user_portfolio.erase(stock);
                }

                cout << "[Server P] Successfully sold " << shares
                    << " shares of " << stock
                    << " and updated " << username << "’s portfolio." << endl;

                ostringstream response;
                response << username << " successfully sold " << shares
                        << " shares of " << stock << ".\n";

                string reply = response.str();
                sendto(udp_sockfd, reply.c_str(), reply.length(), 0,
                    (struct sockaddr*)&mAddr, mAddrLen);
            }
        }else if (confirm == "check") {
            cout << "[Server P] Received a sell request from the main server." << endl;

            string username, stock;
            int shares;
            iss >> username >> stock >> shares;

            string lower_username = username;
            transform(lower_username.begin(), lower_username.end(), lower_username.begin(), ::tolower);

            auto user_it = portfolios.find(lower_username);
            // Username not found
            if (user_it == portfolios.end()) {
                string err = "Insufficient shares";
                cout << "[Server P] Stock "
                    << stock << " does not have enough shares in " 
                    << username << "’s portfolio. Unable to sell "
                    << shares << " shares of " << stock << "." << endl;
                sendto(udp_sockfd, err.c_str(), err.length(), 0,
                    (struct sockaddr*)&mAddr, mAddrLen);
            }

            const auto& user_portfolio = user_it->second.portfolio;
            if (user_portfolio.find(stock) != user_portfolio.end() &&
                user_portfolio.at(stock).shares >= shares) {
                string ok = "OK";
                cout << "[Server P] Stock "
                    << stock << " has sufficient shares in " 
                    << username << "’s portfolio. Requesting users’ confirmation for selling stock."
                    << endl;
                sendto(udp_sockfd, ok.c_str(), ok.length(), 0,
                    (struct sockaddr*)&mAddr, mAddrLen);
            } else {
                string err = "Insufficient shares";
                cout << "[Server P] Stock "
                    << stock << " does not have enough shares in " 
                    << username << "’s portfolio. Unable to sell "
                    << shares << " shares of " << stock << "." << endl;
                sendto(udp_sockfd, err.c_str(), err.length(), 0,
                    (struct sockaddr*)&mAddr, mAddrLen);
            }
        } else if (confirm == "N") {
            string cmd;
            iss >> cmd;
            if (cmd == "sell") cout << "[Server P] Sell denied." << endl;
        } else if (confirm == "position") {
            string username;
            iss >> username;

            string lower_username = username;
            transform(lower_username.begin(), lower_username.end(), lower_username.begin(), ::tolower);

            cout << "[Server P] Received a position request from the main server for Member: " << username << endl;

            auto it = portfolios.find(lower_username);
            // TODO username not found
            if (it == portfolios.end()) {
                ostringstream response;
                response << "stock shares avg_buy_price\n";
                string reply = response.str();
                sendto(udp_sockfd, reply.c_str(), reply.length(), 0,
                    (struct sockaddr*)&mAddr, mAddrLen);
                continue;
            }

            const auto& portfolio = it->second.portfolio;
            ostringstream response;
            response << "stock shares avg_buy_price\n";

            for (const auto& entry : portfolio) {
                const string& stock = entry.first;
                const OwnedStockInfo& info = entry.second;

                if (info.shares == 0) continue;

                response << stock << " " << info.shares << " " << info.avg_price << "\n";
            }
            
            string reply = response.str();
            sendto(udp_sockfd, reply.c_str(), reply.length(), 0,
                (struct sockaddr*)&mAddr, mAddrLen);

            cout << "[Server P] Finished sending the gain and portfolio of " 
                 << username << " to the main server." << endl;
        }

        // Debug print portfolios
        // for (const auto& user : portfolios) {
        //     cout << user.first << "'s portfolio:\n";
        //     for (const auto& entry : user.second.portfolio) {
        //         cout << "  " << entry.first
        //             << " - " << entry.second.shares
        //             << " shares @ $" << entry.second.avg_price << "\n";
        //     }
        //     cout << endl;
        // }
    }


    close(udp_sockfd);
    return 0;
}