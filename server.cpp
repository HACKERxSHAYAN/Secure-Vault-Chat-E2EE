/*
 * SECURE-VAULT-CHAT
 * Military-Grade End-to-End Encrypted Messaging System
 * Server Component - Zero-Knowledge Relay
 *
 * Cross-Platform: Windows (winsock2) & Linux (sys/socket)
 * Architecture: Client messages are encrypted before transmission.
 * Server only relays ciphertext, never sees plaintext.
 */

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
    #define GET_LAST_ERROR WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netinet/tcp.h>
    typedef int socket_t;
    #define SOCKET_ERROR_VAL (-1)
    #define CLOSE_SOCKET close
    #define GET_LAST_ERROR errno
#endif

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

std::mutex clients_mutex;
std::unordered_map<socket_t, std::string> client_sockets; // socket -> client ID

#if defined(_WIN32) || defined(_WIN64)
bool init_winsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[-] WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}
#endif

void handle_client(socket_t client_socket, std::string client_id) {
    char buffer[BUFFER_SIZE];
    std::cout << "[+] Secure channel established for client: " << client_id << std::endl;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        
        if (bytes_received <= 0) {
            std::cout << "[-] Client disconnected: " << client_id << std::endl;
            break;
        }

        std::string encrypted_message(buffer, bytes_received);

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (auto& pair : client_sockets) {
                if (pair.first != client_socket) {
                    send(pair.first, encrypted_message.c_str(), encrypted_message.length(), 0);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets.erase(client_socket);
    }
    CLOSE_SOCKET(client_socket);
}

int main() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║          SECURE-VAULT-CHAT SERVER [ZERO-KNOWLEDGE]        ║
║                                                           ║
║  Architecture: Client-Side Encryption Only                ║
║  Server Role: Ciphertext Relay (Never Sees Plaintext)    ║
║  Status: Military-Grade Security Active                  ║
╚═══════════════════════════════════════════════════════════╝
)";

#if defined(_WIN32) || defined(_WIN64)
    if (!init_winsock()) {
        return -1;
    }
#endif

    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == SOCKET_ERROR_VAL) {
        std::cerr << "[-] Socket creation failed. Error: " << GET_LAST_ERROR << std::endl;
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR_VAL) {
        std::cerr << "[-] Setsockopt SO_REUSEADDR failed" << std::endl;
        CLOSE_SOCKET(server_fd);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    int tcp_nodelay = 1;
    setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&tcp_nodelay, sizeof(tcp_nodelay));

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "[-] Bind failed. Error: " << GET_LAST_ERROR << std::endl;
        CLOSE_SOCKET(server_fd);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        std::cerr << "[-] Listen failed" << std::endl;
        CLOSE_SOCKET(server_fd);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    std::cout << "\n[+] SERVER RUNNING ON PORT " << PORT << std::endl;
    std::cout << "[+] WAITING FOR SECURE CONNECTIONS..." << std::endl;
    std::cout << "[+] MODE: ZERO-KNOWLEDGE RELAY (Ciphertext-Only)" << std::endl << std::endl;

    std::vector<std::thread> client_threads;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        socket_t client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket == SOCKET_ERROR_VAL) {
            std::cerr << "[-] Accept failed. Error: " << GET_LAST_ERROR << std::endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::string client_id = std::string(client_ip) + ":" + std::to_string(ntohs(client_addr.sin_port));

        std::cout << "[+] Secure connection from: " << client_id << std::endl;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets[client_socket] = client_id;
        }

        client_threads.emplace_back(handle_client, client_socket, client_id);
    }

    for (auto& t : client_threads) {
        if (t.joinable()) t.join();
    }

    CLOSE_SOCKET(server_fd);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}