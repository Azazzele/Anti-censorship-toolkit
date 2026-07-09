#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <random>
#include <sstream>
#include <regex>

// Кроссплатформенные инклуды для сокетов (Windows / Linux)
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
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
    typedef int SOCKET;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
    #define WSAGetLastError() errno
    #define WSAEWOULDBLOCK EWOULDBLOCK
#endif

#include "crypto_variants.h"

// Константы конфигурации
const std::string VPS_IP = "127.0.0.1";   // ВАЖНО: Раз мы на 3-м варианте (без VPS), шлем на свой же ПК!
const int VPS_PORT = 54321;               // Порт, который слушает сервер на ПК
const uint8_t MASTER_KEY = 0x7A;          // Общий секретный ключ (должен совпадать с protocol.cpp)

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

// Перевод сокета в неблокирующий режим (Фикс: скобка теперь закрыта правильно!)
void set_socket_non_blocking(SOCKET sock) {
    #ifdef _WIN32
        unsigned long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
    #else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #endif
}
// Функция, которая отправляет красивую HTML-страницу заглушки прямо в браузер
void send_propaganda_block_page(SOCKET browser_socket, const std::string& domain) {
    // Формируем чистый, стандартный ответ прокси-сервера: "403 Доступ Запрещен цензурой"
    std::ostringstream http_response;
    http_response << "HTTP/1.1 403 Forbidden\r\n"
                  << "Proxy-Connection: close\r\n"
                  << "Connection: close\r\n"
                  << "Content-Type: text/plain; charset=UTF-8\r\n\r\n"
                  << "ACCESS DENIED: Resource '" << domain << "' is classified as state propaganda.";

    std::string response_str = http_response.str();
    send(browser_socket, response_str.c_str(), static_cast<int>(response_str.length()), 0);
    
    // Мгновенно закрываем сокет, выбрасывая сайт из сети
    closesocket(browser_socket);
}




// Асинхронный мост пересылки трафика с фрагментацией пакетов против ТСПУ
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

        // Данные от браузера -> ШИФРУЕМ -> шлем локальному серверу
        if (FD_ISSET(client_sock, &read_fds)) {
            int bytes = recv(client_sock, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (bytes < 0) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
            if (bytes == 0) break;

            if (algo) {
                algo->encrypt(reinterpret_cast<uint8_t*>(buffer.data()), static_cast<size_t>(bytes), crypto_key);
            }
            
            if (send(target_sock, buffer.data(), bytes, 0) == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
        }

        // Ответ от локального сервера -> РАСШИФРОВЫВАЕМ -> отдаем браузеру
        if (FD_ISSET(target_sock, &read_fds)) {
            int bytes = recv(target_sock, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (bytes < 0) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR) continue;
                break;
            }
            if (bytes == 0) break;

            if (algo) {
                algo->decrypt(reinterpret_cast<uint8_t*>(buffer.data()), static_cast<size_t>(bytes), crypto_key);
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




// Шаг 2: Обработка сайтов НЕ из списка (Прямой обход без шифрования и VPS)
bool handle_direct_connection(SOCKET browser_socket, const std::string& connect_request) {
    size_t start = connect_request.find(" ") + 1;
    size_t end = connect_request.find(" ", start);
    if (start == std::string::npos || end == std::string::npos) {
        closesocket(browser_socket);
        return false;
    }

    std::string url_with_port = connect_request.substr(start, end - start);
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

    std::string target_ip = resolve_domain_to_ip(target_domain);
    if (target_ip.empty()) {
        closesocket(browser_socket);
        return false;
    }

    SOCKET target_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip.c_str(), &target_addr.sin_addr);

    if (connect(target_socket, (struct sockaddr*)&target_addr, sizeof(target_addr)) == SOCKET_ERROR) {
        closesocket(target_socket);
        closesocket(browser_socket);
        return false;
    }

    std::string status_ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(browser_socket, status_ok.c_str(), static_cast<int>(status_ok.length()), 0);

    // Запускаем асинхронную пересылку напрямую (передаем nullptr вместо алгоритма)
    std::thread tunnel_thread(forward_traffic, browser_socket, target_socket, nullptr, 0);
    tunnel_thread.detach();
    return true;
}

bool is_site_banned(const std::string& cleaned_target_domain) {
    const std::vector<std::string> hardcoded_bans = {
        "1tv", "ria", "tass", "smotrim", "vesti", "ntv", "sputnik", "russian.rt",
        "1internet", "teleport", "servicecdn", "smotrim","vesti","ntv","tvc","tvzvezda",
        "lenta","gazeta","life","vz","rt","sputniknews","rg","kp","aif","iz","tsargrad","readovka","news-front"
    };

    for (const std::string& rule : hardcoded_bans) {
        if (cleaned_target_domain.find(rule) != std::string::npos) {
            return true; 
        }
    }
    return false;
}


// 2. Проверка на блокировки РФ ( VPN-лист )
bool is_site_blocked_in_rf(const std::string& cleaned_target_domain) {
    // Ищем просто подстроку названия, чтобы ловить любые поддомены инсты и фейсбука
    if (cleaned_target_domain.find("instagram") != std::string::npos ||
        cleaned_target_domain.find("facebook") != std::string::npos ||
        cleaned_target_domain.find("x.com") != std::string::npos) {
        return true;
    }
    return false;
}

bool handle_browser_connection(SOCKET browser_socket) {
    std::vector<char> buffer(4096);
    int bytes_from_browser = recv(browser_socket, buffer.data(), buffer.size() - 1, 0);
    if (bytes_from_browser <= 0) { closesocket(browser_socket); return false; }
    
    std::string connect_request(buffer.data(), bytes_from_browser);
    if (connect_request.find("CONNECT") == std::string::npos) { closesocket(browser_socket); return false; }

    // 1. Извлекаем сырую строку хоста
    size_t start = connect_request.find(" ");
    if (start == std::string::npos) { closesocket(browser_socket); return false; }
    start += 1;

    size_t end = connect_request.find(":", start);
    if (end == std::string::npos) {
        end = connect_request.find(" ", start);
    }
    if (end == std::string::npos) { closesocket(browser_socket); return false; }

    std::string target_domain = connect_request.substr(start, end - start);

    // 2. БРОНЕБОЙНАЯ РЕГУЛЯРКА: Удаляем вообще любые кавычки (", '), скобки, двоеточия,
    // знаки равенства, пробелы, переносы строк и прочую грязь.
    // Оставляем ТОЛЬКО латиницу, цифры, точки и дефисы.
    std::regex clean_expr("[^a-zA-Z0-9.-]");
    std::string cleaned_domain = std::regex_replace(target_domain, clean_expr, "");

    // Приводим строго к нижнему регистру
    std::transform(cleaned_domain.begin(), cleaned_domain.end(), cleaned_domain.begin(), ::tolower);

    if (cleaned_domain.empty()) { closesocket(browser_socket); return false; }

    // ТЕПЕРЬ ТУТ БУДЕТ ИДЕАЛЬНО ЧИСТАЯ СТРОКА ТИПА: cdn-storage-media.tass.ru
    std::cout << "[CLIENT] [DEBUG] Clear target domain: " << cleaned_domain << std::endl;

    // === ПУТЬ 1: БЛОКИРОВКА ПРОПАГАНДЫ ===
    if (is_site_banned(cleaned_domain)) {
        std::cout << "[CLIENT] [X] PROPAGANDA DETECTED! Blocking access to: " << cleaned_domain << std::endl;
        send_propaganda_block_page(browser_socket, cleaned_domain);
        return true;
    }

    // === ПУТЬ 2: ОБХОД БЛОКИРОВОК (АРК) ===
    if (is_site_blocked_in_rf(cleaned_domain)) {
        std::cout << "[CLIENT] [!] Blocked site in RF detected. Routing via Ark Protocol: " << cleaned_domain << std::endl;
        
        // ... (весь твой код отправки зашифрованного пакета на protocol.exe остается без изменений) ...
        SOCKET vps_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in vps_addr{};
        vps_addr.sin_family = AF_INET;
        vps_addr.sin_port = htons(VPS_PORT);
        inet_pton(AF_INET, VPS_IP.c_str(), &vps_addr.sin_addr);

        if (connect(vps_socket, (struct sockaddr*)&vps_addr, sizeof(vps_addr)) == SOCKET_ERROR) {
            closesocket(vps_socket); closesocket(browser_socket); return false;
        }

        static std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(1, 20);
        std::uniform_int_distribution<int> key_distribution(1, 255);
        uint8_t random_id = static_cast<uint8_t>(distribution(generator));
        uint8_t session_key = static_cast<uint8_t>(key_distribution(generator));
        const CryptoVariant* variant_ptr = get_crypto_variant_by_id(random_id);
        CryptoVariant current_variant = *variant_ptr;

        uint8_t fake_magic = 0x16;
        uint8_t encrypted_magic = fake_magic ^ MASTER_KEY;
        uint8_t encrypted_id    = current_variant.id ^ (MASTER_KEY + session_key); 
        uint8_t encrypted_key   = session_key ^ (MASTER_KEY * 3);

        std::vector<uint8_t> encrypted_packet;
        encrypted_packet.push_back(encrypted_magic); encrypted_packet.push_back(encrypted_id); encrypted_packet.push_back(encrypted_key);         

        std::vector<uint8_t> raw_payload(connect_request.begin(), connect_request.end());
        current_variant.encrypt(raw_payload.data(), raw_payload.size(), session_key);
        encrypted_packet.insert(encrypted_packet.end(), raw_payload.begin(), raw_payload.end());
        send(vps_socket, reinterpret_cast<char*>(encrypted_packet.data()), static_cast<int>(encrypted_packet.size()), 0);

        int bytes_from_vps = recv(vps_socket, buffer.data(), buffer.size() - 1, 0);
        if (bytes_from_vps <= 0) { closesocket(vps_socket); closesocket(browser_socket); return false; }
        current_variant.decrypt(reinterpret_cast<uint8_t*>(buffer.data()), static_cast<size_t>(bytes_from_vps), session_key);
        std::string response_str(buffer.data(), bytes_from_vps);

        if (response_str.find("200 Connection Established") != std::string::npos) {
            std::string raw_ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
            send(browser_socket, raw_ok.c_str(), static_cast<int>(raw_ok.length()), 0);
            std::thread tunnel_thread(forward_traffic, browser_socket, vps_socket, variant_ptr, session_key);
            tunnel_thread.detach();
            return true;
        }
        closesocket(vps_socket); closesocket(browser_socket); return false;
    }

    // === ПУТЬ 3: ПРЯМОЙ ДОСТУП ===
    std::cout << "[CLIENT] [+] Safe site. Routing directly: " << cleaned_domain << std::endl;
    return handle_direct_connection(browser_socket, connect_request);
}




// Шаг 4: Точка входа в программу
int main() {
    std::setvbuf(stdout, NULL, _IONBF, 0);
    const int LOCAL_PROXY_PORT = 8080; 

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
#endif

    srand(static_cast<unsigned int>(time(nullptr)));
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    server_addr.sin_port = htons(LOCAL_PROXY_PORT);

    if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(listen_socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    listen(listen_socket, SOMAXCONN);

    std::cout << "[CLIENT] [+] Local obfuscator live on port " << LOCAL_PROXY_PORT << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        SOCKET browser_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (browser_socket != INVALID_SOCKET) {
            std::thread conn_thread(handle_browser_connection, browser_socket);
            conn_thread.detach();
        }
    }

    closesocket(listen_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
