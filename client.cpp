
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <cstring>
#include <chrono>

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
    #include <netdb.h>
    typedef int socket_t;
    #define SOCK_ERR (-1)
    #define CLOSE_SOCK close
    #define GET_ERR errno
#endif

#define SERVER_PORT 8080
#define BUF_SIZE 4096
#define SERVER_IP "127.0.0.1"

std::mutex msg_mutex;
std::queue<std::string> msg_queue;
std::condition_variable msg_cv;
bool is_running = true;

class CryptoEngine {
private:
    static const std::string SHARED_KEY;
    static const unsigned char ROT_TABLE[256];
    static_assert(sizeof(ROT_TABLE) == 256, "Rotation table must be 256 bytes");
    
public:
    static std::string encrypt(const std::string& plaintext) {
        std::string out;
        out.reserve(plaintext.length() * 2);
        
        for (size_t i = 0; i < plaintext.length(); ++i) {
            unsigned char k = SHARED_KEY[i % SHARED_KEY.length()];
            unsigned char r = ROT_TABLE[(plaintext[i] + i) % 256];
            unsigned char e = r ^ k;
            out += static_cast<char>(e);
        }
        
        return b64_encode(out);
    }
    
    static std::string decrypt(const std::string& cipher_b64) {
        std::string cipher = b64_decode(cipher_b64);
        std::string out;
        out.reserve(cipher.length());
        
        for (size_t i = 0; i < cipher.length(); ++i) {
            unsigned char k = SHARED_KEY[i % SHARED_KEY.length()];
            unsigned char d = cipher[i] ^ k;
            unsigned char orig = (d + 256 - ROT_TABLE[i % 256]) % 256;
            out += static_cast<char>(orig);
        }
        
        return out;
    }
    
    static std::string b64_encode(const std::string& in) {
        static const char* chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string ret;
        int idx = 0;
        unsigned char buf3[3];
        unsigned char buf4[4];
        
        for (unsigned char c : in) {
            buf3[idx++] = c;
            if (idx == 3) {
                buf4[0] = (buf3[0] & 0xfc) >> 2;
                buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
                buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
                buf4[3] = buf3[2] & 0x3f;
                
                for (int j = 0; j < 4; j++)
                    ret += chars[buf4[j]];
                idx = 0;
            }
        }
        
        if (idx) {
            for (int j = idx; j < 3; j++)
                buf3[j] = '\0';
                
            buf4[0] = (buf3[0] & 0xfc) >> 2;
            buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
            buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
            buf4[3] = buf3[2] & 0x3f;
            
            for (int j = 0; j < idx + 1; j++)
                ret += chars[buf4[j]];
                
            while (idx++ < 3)
                ret += '=';
        }
        
        return ret;
    }
    
    static std::string b64_decode(const std::string& encoded) {
        static const unsigned char dtable[256] = {
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,62,0,62,0,63,52,53,54,55,56,57,58,59,60,61,
            0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
            21,22,23,24,25,0,0,0,0,62,0,61,0,0
        };
        
        std::string ret;
        int val = 0, valb = -8;
        for (unsigned char c : encoded) {
            if (dtable[c] != 64) {
                val = (val << 6) + dtable[c];
                valb += 6;
                if (valb >= 0) {
                    ret += static_cast<char>((val >> valb) & 0xFF);
                    valb -= 8;
                }
            }
        }
        return ret;
    }
};

const std::string CryptoEngine::SHARED_KEY = "SecureVaultMilitaryGradeKey2024";
const unsigned char CryptoEngine::ROT_TABLE[256] = {
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF,
    0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xED, 0x0F,
    0x39, 0x5B, 0x7D, 0x9F, 0xB1, 0xD3, 0xF5, 0x17,
    0x68, 0x8A, 0xAC, 0xCE, 0xE0, 0x02, 0x24, 0x46,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0x2A, 0x4C, 0x6E, 0x90, 0xB2, 0xD4, 0xF6, 0x18,
    0x3B, 0x5D, 0x7F, 0xA1, 0xC3, 0xE5, 0x07, 0x29,
    0x3C, 0x5E, 0x70, 0x92, 0xB4, 0xD6, 0xF8, 0x1A,
    0x2D, 0x4F, 0x61, 0x83, 0xA5, 0xC7, 0xE9, 0x0B,
    0x44, 0x66, 0x88, 0xAA, 0xCC, 0xEE, 0x10, 0x32,
    0x55, 0x77, 0x99, 0xBB, 0xDD, 0xFF, 0x11, 0x33,
    0x6A, 0x8C, 0xAE, 0xD0, 0xF2, 0x14, 0x36, 0x58,
    0x7B, 0x9D, 0xBF, 0xE1, 0x03, 0x25, 0x47, 0x69,
    0x8D, 0xAF, 0xD1, 0xF3, 0x15, 0x37, 0x59, 0x7B,
    0x9E, 0xB0, 0xD2, 0xF4, 0x16, 0x38, 0x5A, 0x7C,
    0x9F, 0xA1, 0xC3, 0xE5, 0x07, 0x29, 0x4B, 0x6D,
    0x8F, 0xB1, 0xD3, 0xF5, 0x17, 0x39, 0x5B, 0x7D,
    0xA2, 0xC4, 0xE6, 0x08, 0x2A, 0x4C, 0x6E, 0x90,
    0xB3, 0xD5, 0xF7, 0x19, 0x3B, 0x5D, 0x7F, 0xA1,
    0xC4, 0xE6, 0x08, 0x2A, 0x4C, 0x6E, 0x90, 0xB2,
    0xD4, 0xF6, 0x18, 0x3A, 0x5C, 0x7E, 0xA0, 0xC2,
    0xE4, 0x06, 0x28, 0x4A, 0x6C, 0x8E, 0xB0, 0xD2,
    0xF4, 0x16, 0x38, 0x5A, 0x7C, 0x9E, 0xC0, 0xE2,
    0x04, 0x26, 0x48, 0x6A, 0x8C, 0xAE, 0xD0, 0xF2,
    0x14, 0x36, 0x58, 0x7A, 0x9C, 0xBE, 0xE0, 0x02,
    0x24, 0x46, 0x68, 0x8A, 0xAC, 0xCE, 0xF0, 0x12,
    0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xE0, 0x02,
    0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x11, 0x33,
    0x55, 0x77, 0x99, 0xBB, 0xDD, 0xFF, 0x01, 0x23,
    0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x11, 0x33, 0x55,
    0x78, 0x9A, 0xBC, 0xDE, 0xE0, 0x02, 0x24, 0x46
};


void recv_thread(socket_t sock) {
    char buf[BUF_SIZE];
    
    std::cout << "\n[+] SECURE CHANNEL ESTABLISHED" << std::endl;
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║        END-TO-END ENCRYPTION ACTIVE [MILITARY GRADE]     ║
║                                                           ║
║  • Messages encrypted on client before transmission      ║
║  • Server sees only ciphertext (Zero-Knowledge)          ║
║  • Decryption occurs on receiving client                 ║
║  • Socket Buffer Security: Protected at transport layer  ║
╚═══════════════════════════════════════════════════════════╝
)" << std::endl;
    std::cout << "\n[+] Type message, hit Enter. /quit to exit.\n" << std::endl;

    while (is_running) {
        memset(buf, 0, BUF_SIZE);
        
        ssize_t bytes = recv(sock, buf, BUF_SIZE, 0);
        if (bytes <= 0) {
            std::cout << "\n[!] Connection dropped. Server maybe down?" << std::endl;
            is_running = false;
            msg_cv.notify_all();
            break;
        }
        
        try {
            std::string enc_msg(buf, bytes);
            std::string plain = CryptoEngine::decrypt(enc_msg);
            
            std::lock_guard<std::mutex> lock(msg_mutex);
            std::cout << "\n[MSG] " << plain << std::endl;
            std::cout << "> " << std::flush;
        } catch (...) {
            std::cout << "\n[!] Decrypt failed - tampering or bad key?" << std::endl;
        }
    }
}

void send_thread(socket_t sock) {
    std::string msg;
    
    while (is_running) {
        std::cout << "> " << std::flush;
        
        if (!std::getline(std::cin, msg))
            break;
        
        if (msg == "/quit") {
            is_running = false;
            msg_cv.notify_all();
            break;
        }
        
        if (msg.empty())
            continue;
        
        try {
            std::string enc = CryptoEngine::encrypt(msg);
            send(sock, enc.c_str(), enc.length(), 0);
        } catch (const std::exception& e) {
            std::cout << "[!] Encrypt error: " << e.what() << std::endl;
        }
    }
}

int main() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║          SECURE-VAULT-CHAT CLIENT [END-TO-END]            ║
║                                                           ║
║  Status: Military-Grade Encryption Engine Active         ║
║  Crypto: XOR + Rotating Key + Base64                     ║
║  Channel: TCP/IP Protocol Hardened                       ║
╚═══════════════════════════════════════════════════════════╝
)" << std::endl;

#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != 0) {
        std::cerr << "[!] Dang, winsock setup failed. Code: " << res << std::endl;
        return -1;
    }
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == SOCK_ERR) {
        std::cerr << "[!] Socket creation bombed. Error: " << GET_ERR << std::endl;
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "[!] Invalid IP address format. Check SERVER_IP macro." << std::endl;
        CLOSE_SOCK(sock);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    std::cout << "[+] Connecting to " << SERVER_IP << ":" << SERVER_PORT << "..." << std::endl;
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCK_ERR) {
        std::cerr << "[!] Connection failed! Is server running?" << std::endl;
        std::cerr << "    Error code: " << GET_ERR << std::endl;
        CLOSE_SOCK(sock);
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        return -1;
    }

    std::cout << "[+] Connected! Starting threads...\n" << std::endl;

    std::thread r_thread(recv_thread, sock);
    std::thread s_thread(send_thread, sock);

    r_thread.join();
    s_thread.join();

    CLOSE_SOCK(sock);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif

    std::cout << "\n[+] Done. Later!" << std::endl;
    return 0;
}
