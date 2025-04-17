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
#include <vector>

#define SERVERM_TCP_PORT 45812
#define LOCALHOST "127.0.0.1"
#define MAXBUFLEN 1024
#define BUFSIZE 8192

using namespace std;

struct Credentials {
    char username[51];
    char password[51];
};

void handleQuoteCommand(int sockfd, const string& input);
void handleBuyCommand(int sockfd, const string& input, const string& username);
void handleSellCommand(int sockfd, const string& input, const string& username);
void handlePositionCommand(int sockfd, const string& username);

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

    cout << "[Client] Please enter the command:\n"
         << "<quote>\n"
         << "<quote <stock name>>\n"
         << "<buy <stock name> <number of shares>>\n"
         << "<sell <stock name> <number of shares>>\n"
         << "<position>\n"
         << "<exit>" << endl;
    // === Command Loop ===
    while (true) {
        string input;
        getline(cin, input);

        if (input.empty()) continue;

        istringstream iss(input);
        string cmd;
        iss >> cmd;
        
        if (cmd == "quote") {
            handleQuoteCommand(sockfd, input);
        } else if (cmd == "buy") {
            handleBuyCommand(sockfd, input, username);
        } else if (cmd == "sell") {
            handleSellCommand(sockfd, input, username);
        } else if (cmd == "position") {
            // handlePositionCommand(sockfd, username);
        } else if (cmd == "exit"){
           break;
        } else {
            continue;
        }
    }

    close(sockfd);
    return 0;
}

void handleQuoteCommand(int sockfd, const string& input) {
    cout << "[Client] Sent a quote request to the main server." << endl;
    send(sockfd, input.c_str(), input.length(), 0);

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    getsockname(sockfd, (struct sockaddr*)&client_addr, &len);
    int client_port = ntohs(client_addr.sin_port);

    vector<char> buffer(BUFSIZE);
    int bytes_received = recv(sockfd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_received <= 0) {
        cerr << "[Client] Connection lost while receiving quote." << endl;
        return;
    }

    buffer[bytes_received] = '\0';
    string response(buffer.data());

    cout << "[Client] Received the response from the main server using TCP over port " 
         << client_port << "." << endl;
    cout << response << endl;

    cout << "——Start a new request——" << endl;
}

void handleBuyCommand(int sockfd, const string& input, const string& username) {
    istringstream iss(input);
    string cmd, stock, shares;
    iss >> cmd >> stock >> shares;

    // struct sockaddr_in client_addr;
    // socklen_t len = sizeof(client_addr);
    // getsockname(sockfd, (struct sockaddr*)&client_addr, &len);
    // int client_port = ntohs(client_addr.sin_port);

    if (!(stock.empty() || shares.empty())) {
        send(sockfd, input.c_str(), input.length(), 0);
    } else {
        cout << "[Client] Error: stock name/shares are required."
             << "Please specify a stock name to buy." << endl;
        return;
    }


    // Stock does not exist
    vector<char> buffer(BUFSIZE);
    int bytes_received = recv(sockfd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_received <= 0) return;
    
    
    buffer[bytes_received] = '\0';
    string response(buffer.data());

    if (response.find("does not exist") != string::npos) {
        cout << "[Client] Error: stock name does not exist. Please check again." << endl;
        return;
    }

    // Stock exists and have enough shares, prompt for confirmation
    istringstream quote_iss(response);
    string quoted_stock;
    double quoted_price;
    quote_iss >> quoted_stock >> quoted_price;

    while (true) {
        cout << "[Client] " << quoted_stock << "’s current price is "
             << quoted_price << ". Proceed to buy? (Y/N)" << endl;
        string confirm;
        cin >> confirm;
        if (confirm == "Y") {
            send(sockfd, confirm.c_str(), confirm.length(), 0);

            // === Receive confirmation of success ===
            int final_bytes = recv(sockfd, buffer.data(), buffer.size() - 1, 0);
            if (final_bytes > 0) {
                buffer[final_bytes] = '\0';
                string final_reply(buffer.data());

                // === Display confirmation and port info ===
                struct sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);
                getsockname(sockfd, (struct sockaddr*)&client_addr, &len);
                int client_port = ntohs(client_addr.sin_port);

                cout << "[Client] Received the response from the main server using TCP over port "
                    << client_port << "." << endl;
                cout << final_reply << endl;
                cout << "——Start a new request——" << endl;
            }
            break;
        } else if (confirm == "N") {
            send(sockfd, confirm.c_str(), confirm.length(), 0);
            break;
        }
    }
}

void handleSellCommand(int sockfd, const string& input, const string& username) {
    istringstream iss(input);
    string cmd, stock, shares;
    iss >> cmd >> stock >> shares;

    if (stock.empty() || shares.empty()) {
        cout << "[Client] Error: stock name/shares are required."
             << " Please specify a stock name to sell." << endl;
        return;
    }

    // Send sell command to server
    send(sockfd, input.c_str(), input.length(), 0);

    // Receive quote + ownership check
    vector<char> buffer(BUFSIZE);
    int bytes_received = recv(sockfd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_received <= 0) return;

    buffer[bytes_received] = '\0';
    string response(buffer.data());

    // Handle error if stock doesn't exist or insufficient shares
    if (response.find("does not exist") != string::npos) {
        cout << "[Client] Error: stock name does not exist. Please check again." << endl;
        return;
    }
    if (response.find("Insufficient shares") != string::npos) {
        cout << "[Client] Error: " << username << " does not have enough shares of " 
             << stock << " to sell. Please try again" << endl;
        cout << "—Start a new request—" << endl;
        return;
    }

    // Extract quote and prompt for confirmation
    istringstream quote_iss(response);
    string quoted_stock;
    double quoted_price;
    quote_iss >> quoted_stock >> quoted_price;

    while (true) {
        cout << "[Client] " << quoted_stock << "’s current price is "
             << quoted_price << ". Proceed to sell? (Y/N)" << endl;
        string confirm;
        cin >> confirm;
        if (confirm == "Y") {
            send(sockfd, confirm.c_str(), confirm.length(), 0);

            // Receive final confirmation
            int final_bytes = recv(sockfd, buffer.data(), buffer.size() - 1, 0);
            if (final_bytes > 0) {
                buffer[final_bytes] = '\0';
                string final_reply(buffer.data());

                cout << final_reply << endl;
                cout << "——Start a new request——" << endl;
            }
            break;
        } else if (confirm == "N") {
            send(sockfd, confirm.c_str(), confirm.length(), 0);
            break;
        }
    }
}



