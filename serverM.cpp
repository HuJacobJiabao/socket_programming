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
#include <map>
#include <iomanip>
#include <algorithm>

using namespace std;

#define PORT_UDP 44812
#define PORT_TCP 45812
#define PORT_A 41812
#define PORT_P 42812
#define PORT_Q 43812
#define MAXBUFLEN 1024
#define BUFSIZE 8192
#define LOCALHOST "127.0.0.1"

// Function headers
/* 
 * Sets up a TCP socket for listening to client connections.
 * @param sockfd: reference to the TCP socket file descriptor.
 * @param addr: reference to a sockaddr_in struct to be configured.
 */
void setupTCPSocket(int& sockfd, struct sockaddr_in& addr);

/* 
 * Sets up a UDP socket for communicating with backend servers.
 * @param sockfd: reference to the UDP socket file descriptor.
 * @param addr: reference to a sockaddr_in struct to be configured.
 */
void setupUDPSocket(int& sockfd, struct sockaddr_in& addr);

/* 
 * Configures a sockaddr_in structure with localhost IP and the given port.
 * @param addr: sockaddr_in structure to populate.
 * @param port: port number to assign.
 */
void setupServer(struct sockaddr_in& addr, const int port);

/* 
 * Encrypts the input password using 3-offset for letters and digits.
 * @param password: plain-text password.
 * @return: encrypted password string.
 */
string encryptPassword(const string& password);

/*
 * Handles client login via Server A.
 * @param client_fd: TCP socket file descriptor connected to client.
 * @param udp_sockfd: UDP socket for backend communication.
 * @param serverAAddr: address structure for serverA.
 * @param logged_in: reference to a boolean that will be set true if login is successful.
 * @param username: reference to store logged-in user's username.
 * @return: false if client disconnected, true otherwise.
 */
bool handleLogin(int client_fd, int udp_sockfd,
                 struct sockaddr_in& serverAAddr,
                 bool& logged_in,
                 string& username);

/*
 * Determines the type of client command and routes it to the appropriate handler.
 * @param command: entire client command string.
 * @param client_fd: TCP socket connected to the client.
 * @param udp_sockfd: UDP socket for backend communication.
 * @param serverPAddr: sockaddr_in structure of Server P.
 * @param serverQAddr: sockaddr_in structure of Server Q.
 * @param username: current logged-in user.
 */
void dispatchClientCommand(const string& command,
                           int client_fd,
                           int udp_sockfd,
                           struct sockaddr_in& serverPAddr,
                           struct sockaddr_in& serverQAddr,
                           const string& username);

/*
 * Forwards a quote or quote <stock> command to serverQ and sends the response to the client.
 * @param command: quote command from client.
 * @param client_fd: TCP socket connected to the client.
 * @param udp_sockfd: UDP socket to communicate with serverQ.
 * @param serverQAddr: sockaddr_in of serverQ.
 * @param username: current user's username.
 */
void handleQuoteCommand(const string& command,
                        int client_fd,
                        int udp_sockfd,
                        struct sockaddr_in& serverQAddr,
                        const string& username);

/*
 * Handles the buy command from client: fetches quote, confirms with client, then sends to serverP.
 * @param iss: parsed istringstream of the command string.
 * @param client_fd: TCP socket connected to client.
 * @param udp_sockfd: UDP socket for backend.
 * @param serverPAddr: sockaddr_in of serverP.
 * @param serverQAddr: sockaddr_in of serverQ.
 * @param username: current logged-in user.
 */
void handleBuyCommand(istringstream& iss,
                      int client_fd,
                      int udp_sockfd,
                      struct sockaddr_in& serverPAddr,
                      struct sockaddr_in& serverQAddr,
                      const string& username);

/*
 * Handles the sell command from client: fetches quote,
 * verifies shares with serverP, confirms, and sends to serverP.
 * @param iss: parsed istringstream of the command string.
 * @param client_fd: TCP socket connected to client.
 * @param udp_sockfd: UDP socket for backend.
 * @param serverPAddr: sockaddr_in of serverP.
 * @param serverQAddr: sockaddr_in of serverQ.
 * @param username: current logged-in user.
 */
void handleSellCommand(istringstream& iss,
                      int client_fd,
                      int udp_sockfd,
                      struct sockaddr_in& serverPAddr,
                      struct sockaddr_in& serverQAddr,
                      const string& username);

/*
 * Sends a forward time instruction to serverQ to advance the stock price index.
 * @param stock: stock symbol to shift.
 * @param udp_sockfd: UDP socket for sending.
 * @param serverQAddr: sockaddr_in of serverQ.
 */
void sendTimeShift(const string& stock, int udp_sockfd,
                   struct sockaddr_in& serverQAddr);

/*
 * Handles the position command: gathers user's portfolio from serverP and quote info from serverQ.
 * Calculates profit/loss and returns summary to client.
 * @param client_fd: TCP socket connected to client.
 * @param udp_sockfd: UDP socket for backend communication.
 * @param serverPAddr: sockaddr_in of serverP.
 * @param serverQAddr: sockaddr_in of serverQ.
 * @param username: current logged-in user.
 */
void handlePositionCommand(int client_fd,
                      int udp_sockfd,
                      struct sockaddr_in& serverPAddr,
                      struct sockaddr_in& serverQAddr,
                      const string& username);

// Helper data structure
struct Credentials {
    char username[51];
    char password[51];
};

int main() {
    int tcp_sockfd, udp_sockfd, client_fd;
    struct sockaddr_in serverTCPAddr, 
                       serverUDPAddr, 
                       serverAAddr, 
                       serverQAddr,
                       serverPAddr,
                       clientAddr;
    socklen_t addr_size = sizeof(clientAddr);

    // Set up
    setupTCPSocket(tcp_sockfd, serverTCPAddr);
    setupUDPSocket(udp_sockfd, serverUDPAddr);
    setupServer(serverAAddr, PORT_A);
    setupServer(serverPAddr, PORT_P);
    setupServer(serverQAddr, PORT_Q);

    cout << "[Server M] Booting up using UDP on port " << PORT_UDP << ".\n";

    while (true) {
        client_fd = accept(tcp_sockfd, (struct sockaddr*)&clientAddr, &addr_size);
        if (client_fd < 0) {
            cerr << "Accept failed.\n";
            continue;
        }

        // Use child process to handle client command
        pid_t pid = fork();
        if (pid < 0) {
            // cerr << "Fork failed.\n";
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // Child process
            close(tcp_sockfd);
            // DEBUG
            // cout << "[Server M] New client process started (PID: " << getpid() << ").\n";

            bool logged_in = false;

            string username;
            while (true) {
                if (!logged_in) {
                    // Expect login credentials
                    bool active = handleLogin(client_fd, udp_sockfd, serverAAddr, logged_in, username);
                    if (!active) {
                        // DEBUG
                        // cout << "[Server M - PID " << getpid() << "] Client disconnected during login.\n";
                        break;
                    }
                    continue;
                } else {
                    // Expect command as string
                    char buffer[MAXBUFLEN] = {0};
                    int bytes_received = recv(client_fd, buffer, MAXBUFLEN - 1, 0);
                    if (bytes_received <= 0) {
                        // cout << "[Server M - PID " << getpid() << "] Client disconnected after login.\n";
                        break;
                    }

                    buffer[bytes_received] = '\0';
                    string command(buffer);

                    dispatchClientCommand(command, client_fd, udp_sockfd, serverPAddr, serverQAddr, username);
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
    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "TCP socket creation failed.\n";
        exit(1);
    }

    // Allow address reuse to avoid bind errors on restart
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "setsockopt(SO_REUSEADDR) failed.\n";
        exit(1);
    }

    // Configure address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_TCP);
    inet_pton(AF_INET, LOCALHOST, &addr.sin_addr);

    // Bind to the specified address and port
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "TCP bind failed.\n";
        exit(1);
    }

    // Start listening
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
                 bool& logged_in,
                 string& username) {
    Credentials cred;
    memset(&cred, 0, sizeof(cred));

    int bytes_received = recv(client_fd, &cred, sizeof(cred), 0);
    if (bytes_received <= 0) {
        return false;
    }

    username = string(cred.username);
    transform(username.begin(), username.end(), username.begin(), ::tolower);

    string password(cred.password);
    cout << "[Server M] Received username " << username << " and Password ****." << endl;

    string encrypted = encryptPassword(password);
    memset(cred.password, 0, sizeof(cred.password));
    strncpy(cred.password, encrypted.c_str(), sizeof(cred.password) - 1);

    sendto(udp_sockfd, &cred, sizeof(cred), 0,
           (struct sockaddr*)&serverAAddr, sizeof(serverAAddr));
    cout << "[Server M] Sent the authentication request to Server A." << endl;

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
void dispatchClientCommand(const string& command,
                           int client_fd,
                           int udp_sockfd,
                           struct sockaddr_in& serverPAddr,
                           struct sockaddr_in& serverQAddr,
                           const string& username) {
    istringstream iss(command);
    string cmd;
    iss >> cmd;

    if (cmd == "quote") {
        handleQuoteCommand(command, client_fd, udp_sockfd, serverQAddr, username);
    } else if (cmd == "buy") {
        handleBuyCommand(iss, client_fd, udp_sockfd, serverPAddr, serverQAddr, username);
    } else if (cmd == "sell") {
        handleSellCommand(iss, client_fd, udp_sockfd, serverPAddr, serverQAddr, username);
    } else if (cmd == "position") {
        handlePositionCommand(client_fd, udp_sockfd, serverPAddr, serverQAddr, username);
    }
    // } else {
    //     string msg = "[Server M] Invalid command.\n——Start a new request——";
    //     send(client_fd, msg.c_str(), msg.length(), 0);
    // }
}

void handleQuoteCommand(const string& command,
                        int client_fd,
                        int udp_sockfd,
                        struct sockaddr_in& serverQAddr,
                        const string& username) {
    istringstream iss(command);
    string cmd, stock;
    iss >> cmd >> stock;
    bool has_stock = !stock.empty();

    if (has_stock) {
        cout << "[Server M] Received a quote request from " << username
             << " for stock " << stock << ", using TCP over port " << PORT_TCP << "." << endl;
    } else {
        cout << "[Server M] Received a quote request from " << username
             << ", using TCP over port " << PORT_TCP << "." << endl;
    }

    sendto(udp_sockfd, command.c_str(), command.length(), 0,
           (struct sockaddr*)&serverQAddr, sizeof(serverQAddr));
    cout << "[Server M] Forwarded the quote request to server Q." << endl;

    vector<char> buffer(BUFSIZE);
    socklen_t addr_len = sizeof(serverQAddr);
    int bytes_received = recvfrom(udp_sockfd, buffer.data(), buffer.size() - 1, 0,
                                  (struct sockaddr*)&serverQAddr, &addr_len);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        string reply(buffer.data());

        if (has_stock) {
            cout << "[Server M] Received the quote response from server Q for stock "
                 << stock << " using UDP over "
                 << PORT_UDP << "." << endl;
        } else {
            cout << "[Server M] Received the quote response from server Q using UDP over "
                 << PORT_UDP << "." << endl;
        }
        
        cout << "[Server M] Forwarded the quote response to the client." << endl;

        send(client_fd, reply.c_str(), reply.length(), 0);
    }
}

void handleBuyCommand(istringstream& iss,
                      int client_fd,
                      int udp_sockfd,
                      struct sockaddr_in& serverPAddr,
                      struct sockaddr_in& serverQAddr,
                      const string& username) {
    cout << "[Server M] Received a buy request from member "
         << username << " using TCP over port "
         << PORT_TCP << "." << endl;

    string stock, shares;
    iss >> stock >> shares;
    if (stock.empty() || shares.empty()) return;

    string quote_cmd = "quote " + stock;
    sendto(udp_sockfd, quote_cmd.c_str(), quote_cmd.length(), 0,
           (struct sockaddr*)&serverQAddr, sizeof(serverQAddr));
    cout << "[Server M] Sent the quote request to server Q." << endl;

    vector<char> buffer(BUFSIZE);
    socklen_t qaddr_len = sizeof(serverQAddr);
    int bytes_received = recvfrom(udp_sockfd, buffer.data(), buffer.size() - 1, 0,
                                  (struct sockaddr*)&serverQAddr, &qaddr_len);
    cout << "[Server M] Received quote response from server Q." << endl;


    if (bytes_received <= 0) return;

    buffer[bytes_received] = '\0';
    string quote_reply(buffer.data());
    send(client_fd, quote_reply.c_str(), quote_reply.length(), 0);

    if (quote_reply.find("does not exist") != string::npos) {
        return;  // prevent hanging
    }
    cout << "[Server M] Sent the buy confirmation to the client." << endl;

    char confirm_buf[10] = {0};
    int confirm_bytes = recv(client_fd, confirm_buf, sizeof(confirm_buf) - 1, 0);
    if (confirm_bytes <= 0) return;

    string confirm(confirm_buf);
    if (confirm == "Y") {
        cout << "[Server M] Buy approved." << endl;

        istringstream quote_iss(quote_reply);
        string quoted_stock;
        double quoted_price;
        quote_iss >> quoted_stock >> quoted_price;

        string final_buy_cmd = "Y " + username + " buy " + stock + " " + shares + " " + to_string(quoted_price);
        sendto(udp_sockfd, final_buy_cmd.c_str(), final_buy_cmd.length(), 0,
               (struct sockaddr*)&serverPAddr, sizeof(serverPAddr));
        cout << "[Server M] Forwarded the buy confirmation response to Server P." << endl;

        sendTimeShift(stock, udp_sockfd, serverQAddr);

        socklen_t paddr_len = sizeof(serverPAddr);
        int final_bytes = recvfrom(udp_sockfd, buffer.data(), buffer.size() - 1, 0,
                                   (struct sockaddr*)&serverPAddr, &paddr_len);

        if (final_bytes > 0) {
            buffer[final_bytes] = '\0';
            string reply(buffer.data());
            send(client_fd, reply.c_str(), reply.length(), 0);
            cout << "[Server M] Forwarded the buy result to the client." << endl;
        }
    } else {
        cout << "[Server M] Buy denied." << endl;
        string msg = "N buy";
        sendto(udp_sockfd, msg.c_str(), msg.length(), 0,
               (struct sockaddr*)&serverPAddr, sizeof(serverPAddr));
        cout << "[Server M] Forwarded the buy confirmation response to Server P." << endl;

        sendTimeShift(stock, udp_sockfd, serverQAddr);

    }
}

void handleSellCommand(istringstream& iss,
                       int client_fd,
                       int udp_sockfd,
                       struct sockaddr_in& serverPAddr,
                       struct sockaddr_in& serverQAddr,
                       const string& username) {
    cout << "[Server M] Received a sell request from member "
         << username << " using TCP over port "
         << PORT_TCP << "." << endl;

    string stock, shares;
    iss >> stock >> shares;

    if (stock.empty() || shares.empty()) return;
    

    // Get quote from Server Q
    string quote_cmd = "quote " + stock;
    sendto(udp_sockfd, quote_cmd.c_str(), quote_cmd.length(), 0,
           (struct sockaddr*)&serverQAddr, sizeof(serverQAddr));
    cout << "[Server M] Sent the quote request to server Q." << endl;

    vector<char> buffer(BUFSIZE);
    socklen_t qaddr_len = sizeof(serverQAddr);
    int bytes_received = recvfrom(udp_sockfd, buffer.data(), buffer.size() - 1, 0,
                                  (struct sockaddr*)&serverQAddr, &qaddr_len);
    if (bytes_received <= 0) return;

    cout << "[Server M] Received quote response from server Q." << endl;

    buffer[bytes_received] = '\0';
    string quote_reply(buffer.data());

    if (quote_reply.find("does not exist") != string::npos) {
        send(client_fd, quote_reply.c_str(), quote_reply.length(), 0);
        return;
    }

    // Ask Server P to check ownership
    string check_cmd = "check " + username + " " + stock + " " + shares;
    sendto(udp_sockfd, check_cmd.c_str(), check_cmd.length(), 0,
           (struct sockaddr*)&serverPAddr, sizeof(serverPAddr));
    cout << "[Server M] Forwarded the sell request to server P." << endl;

    socklen_t paddr_len = sizeof(serverPAddr);
    int check_bytes = recvfrom(udp_sockfd, buffer.data(), buffer.size() - 1, 0,
                               (struct sockaddr*)&serverPAddr, &paddr_len);
    if (check_bytes <= 0) return;

    buffer[check_bytes] = '\0';
    string check_response(buffer.data());

    // If not enough shares, notify client and stop
    if (check_response == "Insufficient shares") {
        send(client_fd, check_response.c_str(), check_response.length(), 0);
        sendTimeShift(stock, udp_sockfd, serverQAddr);
        return;
    }

    // Forward current price for confirmation
    send(client_fd, quote_reply.c_str(), quote_reply.length(), 0);
    cout << "[Server M] Forwarded the sell confirmation to the client." << endl;
    
    // Receive confirmation from client
    char confirm_buf[10] = {0};
    int confirm_bytes = recv(client_fd, confirm_buf, sizeof(confirm_buf) - 1, 0);
    if (confirm_bytes <= 0) return;

    string confirm(confirm_buf);
    if (confirm == "Y") {
        // Parse current quote price
        istringstream quote_iss(quote_reply);
        string quoted_stock;
        double quoted_price;
        quote_iss >> quoted_stock >> quoted_price;

        // Send final sell command to Server P
        string sell_cmd = "Y " + username + " sell " + stock + " " + shares + " " + to_string(quoted_price);
        sendto(udp_sockfd, sell_cmd.c_str(), sell_cmd.length(), 0,
               (struct sockaddr*)&serverPAddr, sizeof(serverPAddr));
        cout << "[Server M] Forwarded the sell confirmation response to Server P." << endl;

        sendTimeShift(stock, udp_sockfd, serverQAddr);

        // Receive sell result
        int final_bytes = recvfrom(udp_sockfd, buffer.data(), buffer.size() - 1, 0,
                                   (struct sockaddr*)&serverPAddr, &paddr_len);
        if (final_bytes > 0) {
            buffer[final_bytes] = '\0';
            string reply(buffer.data());
            send(client_fd, reply.c_str(), reply.length(), 0);
            cout << "[Server M] Forwarded the sell result to the client." << endl;
        }

    } else {
        string msg = "N sell";
        sendto(udp_sockfd, msg.c_str(), msg.length(), 0,
               (struct sockaddr*)&serverPAddr, sizeof(serverPAddr));
        cout << "[Server M] Forwarded the sell confirmation response to Server P." << endl;

        sendTimeShift(stock, udp_sockfd, serverQAddr);

    }
}

void sendTimeShift(const string& stock, int udp_sockfd,
                   struct sockaddr_in& serverQAddr) {
    string msg = "forward " + stock;
    cout << "[Server M] Sent a time forward request for " << stock << ".\n";
    sendto(udp_sockfd, msg.c_str(), msg.length(), 0, 
            (struct sockaddr*)&serverQAddr, sizeof(serverQAddr));

}

void handlePositionCommand(int client_fd,
                           int udp_sockfd,
                           struct sockaddr_in& serverPAddr,
                           struct sockaddr_in& serverQAddr,
                           const string& username) {
    cout << "[Server M] Received a position request from Member to check "
         << username << "'s gain using TCP over port " << PORT_TCP << "." << endl;

    // Request all quotes from Server Q
    string quote_cmd = "quote";
    sendto(udp_sockfd, quote_cmd.c_str(), quote_cmd.length(), 0,
           (struct sockaddr*)&serverQAddr, sizeof(serverQAddr));
    cout << "[Server M] Sent quote request to server Q." << endl;

    vector<char> quote_buf(BUFSIZE);
    socklen_t qaddr_len = sizeof(serverQAddr);
    int quote_bytes = recvfrom(udp_sockfd, quote_buf.data(), quote_buf.size() - 1, 0,
                               (struct sockaddr*)&serverQAddr, &qaddr_len);
    if (quote_bytes <= 0) return;
    quote_buf[quote_bytes] = '\0';
    string quote_reply(quote_buf.data());
    cout << "[Server M] Received quote response from server Q." << endl;

    // Parse quotes into a map
    map<string, double> quote_prices;
    istringstream quote_stream(quote_reply);
    string stock;
    double price;
    while (quote_stream >> stock >> price) {
        quote_prices[stock] = price;
    }

    // Request portfolio from Server P
    string pos_cmd = "position " + username;
    sendto(udp_sockfd, pos_cmd.c_str(), pos_cmd.length(), 0,
           (struct sockaddr*)&serverPAddr, sizeof(serverPAddr));
    cout << "[Server M] Forwarded the position request to server P." << endl;

    vector<char> port_buf(BUFSIZE);
    socklen_t paddr_len = sizeof(serverPAddr);
    int port_bytes = recvfrom(udp_sockfd, port_buf.data(), port_buf.size() - 1, 0,
                              (struct sockaddr*)&serverPAddr, &paddr_len);
    if (port_bytes <= 0) return;
    port_buf[port_bytes] = '\0';
    string portfolio(port_buf.data());
    cout << "[Server M] Received user’s portfolio from server P using UDP over " << PORT_UDP << "." << endl;

    // Parse portfolio and compute profit
    istringstream port_stream(portfolio);
    string header;
    port_stream >> header; // skip "stock"
    port_stream >> header; // skip "shares"
    port_stream >> header; // skip "avg_buy_price"

    ostringstream reply;
    reply << fixed << setprecision(2);
    reply << "stock shares avg_buy_price\n";
    double total_profit = 0.0;
    while (port_stream >> stock) {
        int shares;
        double avg_price;
        if (!(port_stream >> shares >> avg_price)) break;

        reply << stock << " " << shares << " " << avg_price << "\n";

        if (quote_prices.find(stock) != quote_prices.end()) {
            double current_price = quote_prices[stock];
            total_profit += shares * (current_price - avg_price);
        }
    }

    reply << username << "’s current profit is " << total_profit << ".";
    string result = reply.str();

    // Send result to client
    send(client_fd, result.c_str(), result.length(), 0);
    cout << "[Server M] Forwarded the gain to the client." << endl;
}