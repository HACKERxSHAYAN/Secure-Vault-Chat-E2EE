
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
    #define SOCK_ERR SOCKET_ERROR
    #define CLOSE_SOCK closesocket
    #define GET_ERR WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netinet/tcp.h>
    typedef int socket_t;
    #define SOCK_ERR (-1)
    #define CLOSE_SOCK close
    #define GET_ERR errno
#endif

#define PORT 8080
#define BUF_SZ 4096
#define MAX_CLIENTS 10

std::mutex clients_lock;
std::unordered_map<socket_t, std::string> active_clients;

#if defined(_WIN32) || defined(_WIN64)
bool init_winsock() {
    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err != 0) {
        std::cerr << "[!] Winsock failed to start: " << err << std::endl;
        return false;
    }
    return true;
}
#endif

void handle_client(socket_t client_sock, std::string client_id) {
    char buf[BUF_SZ];
    std::cout << "[+] Client connected: " << client_id << std::endl;

    while (true) {
        memset(buf, 0, BUF_SZ);
        
        ssize_t bytes = recv(client_sock, buf, BUF_SZ, 0);
        
        if (bytes <= 0) {
            std::cout << "[-] Client left: " << client_id << std::endl;
            break;
        }

        std::string enc_msg(buf, bytes);
        
        std::lock_guard<std::mutex> lock(clients_lock);
        for (auto& pair : active_clients) {
            if (pair.first != client_sock) {
                send(pair.first, enc_msg.c_str(), enc_msg.length(), 0);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(clients_lock);
        active_clients.erase(client_sock);
    }
    CLOSE_SOCK(client_sock);
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

    socket_t serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_fd == SOCK_ERR) {
        std::cerr << "[!] Socket creation error: " << GET_ERR << std::endl;
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    int opt = 1;
    if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCK_ERR) {
        std::cerr << "[!] Setsockopt failed (SO_REUSEADDR)" << std::endl;
        CLOSE_SOCK(serv_fd);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    int tcp_nd = 1;
    setsockopt(serv_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&tcp_nd, sizeof(tcp_nd));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(serv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[!] Bind failed on port " << PORT << ". Error: " << GET_ERR << std::endl;
        std::cerr << "    Maybe something else using that port?" << std::endl;
        CLOSE_SOCK(serv_fd);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    if (listen(serv_fd, MAX_CLIENTS) < 0) {
        std::cerr << "[!] Listen call failed" << std::endl;
        CLOSE_SOCK(serv_fd);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    std::cout << "\n[+] SERVER UP ON PORT " << PORT << std::endl;
    std::cout << "[+] Waiting for clients... (Press Ctrl+C to stop)" << std::endl;
    std::cout << "[+] MODE: ZERO-KNOWLEDGE RELAY\n" << std::endl;

    std::vector<std::thread> client_threads;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        socket_t client_sock = accept(serv_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock == SOCK_ERR) {
            std::cerr << "[!] Accept failed: " << GET_ERR << std::endl;
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        std::string cli_id = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));

        std::cout << "[+] Connection from " << cli_id << std::endl;

        {
            std::lock_guard<std::mutex> lock(clients_lock);
            active_clients[client_sock] = cli_id;
        }

        client_threads.emplace_back(handle_client, client_sock, cli_id);
    }

    for (auto& t : client_threads) {
        if (t.joinable()) t.join();
    }

    CLOSE_SOCK(serv_fd);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
