#include <iostream>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <random>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define inet_pton(af, src, dst) InetPtonA(af, src, dst)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>       
    #include <sys/select.h>
    #include <errno.h>
    #define closesocket close
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
    #define WSAGetLastError() errno
    #define WSAEWOULDBLOCK EWOULDBLOCK
#endif

#include "crypto_variants.h"

const int PORT = 54321; 
const uint8_t MASTER_KEY = 0x7A; 
const int WARP_PORT = 40000; // ЖЕСТКО СТАВИМ ТВОЙ ПОРТ WARP ИЗ SETTINGS!

struct ProxyAddress {
    std::string ip;
    int port;
};

// Функция преобразования домена в IP
std::string resolve_domain_to_ip(const std::string& domain) {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(domain.c_str(), nullptr, &hints, &res) != 0) return "";
    char ip_str[INET_ADDRSTRLEN];
    struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
    inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
    freeaddrinfo(res);
    return std::string(ip_str);
}

// Перевод сокета в неблокирующий режим
void set_socket_non_blocking(SOCKET sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

// Стандартный асинхронный транзитный мост (Чистый транзит для WARP)
void forward_traffic(SOCKET client_sock, SOCKET target_sock, const CryptoVariant* algo, uint8_t crypto_key) {
    set_socket_non_blocking(client_sock);
    set_socket_non_blocking(target_sock);

    std::vector<char> buffer(65536);

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_sock, &read_fds);
        FD_SET(target_sock, &read_fds);

        SOCKET max_fd = (client_sock > target_sock) ? client_sock : target_sock;
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 5000; 

        int activity = select(static_cast<int>(max_fd + 1), &read_fds, nullptr, nullptr, &timeout);
        if (activity == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
            break;
        }

        // От клиента к WARP (Расшифровываем трафик Ark)
        if (FD_ISSET(client_sock, &read_fds)) {
            int bytes = recv(client_sock, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (bytes < 0) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
            if (bytes == 0) break;

            if (algo) {
                algo->decrypt(reinterpret_cast<uint8_t*>(buffer.data()), static_cast<size_t>(bytes), crypto_key);
            }
            
            if (send(target_sock, buffer.data(), bytes, 0) == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
        }

        // От WARP к клиенту (Шифруем трафик обратно в браузер)
        if (FD_ISSET(target_sock, &read_fds)) {
            int bytes = recv(target_sock, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (bytes < 0) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
            if (bytes == 0) break;

            if (algo) {
                algo->encrypt(reinterpret_cast<uint8_t*>(buffer.data()), static_cast<size_t>(bytes), crypto_key);
            }
            
            if (send(client_sock, buffer.data(), bytes, 0) == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
        }
    }
    closesocket(client_sock);
    closesocket(target_sock);
}

int main() {
    std::setvbuf(stdout, NULL, _IONBF, 0);

    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cout << "[-] Error: WSAStartup failed" << std::endl;
            return 1;
        }
    #endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cout << "[-] Error: bind failed on port " << PORT << std::endl;
        closesocket(server_fd);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }
    listen(server_fd, SOMAXCONN);
    std::cout << "[*] Ark Protocol Engine (WARP Core v11) live on port " << PORT << "..." << std::endl;

    while (true) {
        sockaddr_in client_address{};
        socklen_t addrlen = sizeof(client_address);
        SOCKET client_socket = accept(server_fd, (struct sockaddr*)&client_address, &addrlen);
        if (client_socket == INVALID_SOCKET) continue;

        std::vector<char> buffer(4096);
        int bytes_received = recv(client_socket, buffer.data(), buffer.size() - 1, 0);
        
        if (bytes_received > 3) {
            uint8_t encrypted_magic = static_cast<uint8_t>(buffer[0]);
            uint8_t encrypted_id    = static_cast<uint8_t>(buffer[1]);
            uint8_t encrypted_key   = static_cast<uint8_t>(buffer[2]);

            uint8_t magic_byte = encrypted_magic ^ MASTER_KEY;
            uint8_t crypto_key = encrypted_key ^ (MASTER_KEY * 3);
            uint8_t algo_id    = encrypted_id ^ (MASTER_KEY + crypto_key);

            if (magic_byte != 0x16) { closesocket(client_socket); continue; }
            const CryptoVariant* algo = get_crypto_variant_by_id(algo_id);
            if (!algo) { closesocket(client_socket); continue; }

            uint8_t* payload = reinterpret_cast<uint8_t*>(buffer.data()) + 3;
            size_t payload_size = static_cast<size_t>(bytes_received - 3);

            algo->decrypt(payload, payload_size, crypto_key);
            std::string request_str(reinterpret_cast<char*>(payload), payload_size);
            
            if (request_str.find("CONNECT") == std::string::npos) {
                closesocket(client_socket);
                continue;
            }

            size_t start = request_str.find(" ") + 1;
            size_t end = request_str.find(" ", start);
            
            if (start != std::string::npos && end != std::string::npos) {
                std::string url_with_port = request_str.substr(start, end - start);
                if (url_with_port.find("https://") != std::string::npos) url_with_port.erase(0, 8);
                
                std::string target_domain = url_with_port;
                int target_port = 443;
                size_t colon_pos = url_with_port.find(":");
                if (colon_pos != std::string::npos) {
                    target_domain = url_with_port.substr(0, colon_pos);
                    try {
                        target_port = std::stoi(url_with_port.substr(colon_pos + 1));
                    } catch (...) { target_port = 443; }
                }

                std::cout << "[SERVER] Connecting to local WARP SOCKS5 gateway on port " << WARP_PORT << "..." << std::endl;

                SOCKET proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in proxy_addr{};
                proxy_addr.sin_family = AF_INET;
                proxy_addr.sin_port = htons(WARP_PORT);
                proxy_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                if (connect(proxy_socket, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) != SOCKET_ERROR) {
                    
                    // Шаг А: SOCKS5 Авторизация без пароля
                    std::vector<char> socks_auth = { 0x05, 0x01, 0x00 };
                    send(proxy_socket, socks_auth.data(), 3, 0);
                    
                    std::vector<char> socks_auth_res(2, 0);
                    int auth_bytes = recv(proxy_socket, socks_auth_res.data(), 2, 0);
                    
                    if (auth_bytes > 0 && socks_auth_res[0] == 0x05 && socks_auth_res[1] == 0x00) {
                        std::cout << "[SERVER] SOCKS5 Auth Successful. Tunneling to: " << target_domain << ":" << target_port << std::endl;

                        // Шаг B: Команда SOCKS5 CONNECT (ATYP = 0x03)
                        std::vector<char> socks_req;
                        socks_req.push_back(0x05);
                        socks_req.push_back(0x01);
                        socks_req.push_back(0x00);
                        socks_req.push_back(0x03); // Domain name
                        socks_req.push_back(static_cast<char>(target_domain.length()));
                        socks_req.insert(socks_req.end(), target_domain.begin(), target_domain.end());
                        
                        // Кодируем порт (2 байта Big Endian)
                        uint16_t net_port = htons(target_port);
                        uint8_t* port_bytes = reinterpret_cast<uint8_t*>(&net_port);
                        socks_req.push_back(static_cast<char>(port_bytes[0]));
                        socks_req.push_back(static_cast<char>(port_bytes[1]));
                        
                        // Отправляем запрос CONNECT к SOCKS5 шлюзу WARP
                        send(proxy_socket, socks_req.data(), static_cast<int>(socks_req.size()), 0);
                        
                        std::vector<char> socks_res(512, 0);
                        int res_bytes = recv(proxy_socket, socks_res.data(), static_cast<int>(socks_res.size()), 0);
                        
                        // В протоколе SOCKS5 статус ответа лежит во втором байте: socks_res[1] == 0x00 (Успех)
                        if (res_bytes > 1 && socks_res[1] == 0x00) {
                            std::cout << "[SERVER] [++] WARP Tunnel Established successfully!" << std::endl;
                            
                            std::string status_ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
                            std::vector<uint8_t> status_bytes(status_ok.begin(), status_ok.end());
                            
                            // Зашифровываем ответ 200 OK перед отправкой клиенту
                            algo->encrypt(status_bytes.data(), status_bytes.size(), crypto_key);
                            send(client_socket, reinterpret_cast<char*>(status_bytes.data()), static_cast<int>(status_bytes.size()), 0);
                            
                            // Передаем сокеты асинхронному мосту пересылки трафика
                            forward_traffic(client_socket, proxy_socket, algo, crypto_key);
                        } 
                        else {
                            std::cout << "[SERVER] [-] SOCKS5 Connect Rejected by WARP. Code: " << (res_bytes > 1 ? int(socks_res[1]) : -1) << std::endl;
                        }
                    } 
                    else {
                        std::cout << "[SERVER] [-] SOCKS5 Auth Failed with local gateway." << std::endl;
                    }
                } 
                else {
                    std::cout << "[SERVER] [-] CRITICAL: Cannot connect to WARP port 40000! Check if WARP is running." << std::endl;
                }
                closesocket(proxy_socket);
            }
        }
        closesocket(client_socket);
    }
    
    closesocket(server_fd);
    
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

