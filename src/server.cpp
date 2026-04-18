// ============================================================
// server.cpp — Implémentation serveur HTTP (Winsock2 / POSIX)
// SMARTPRESENCE CI
// ============================================================
#include "server.h"
#include "utils.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <filesystem>

// ─── Portabilité Windows / Linux ─────────────────────────────
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET(s) close(s)
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
#endif

// ─── MIME types ──────────────────────────────────────────────
static std::string get_mime(const std::string& ext) {
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".woff2")return "font/woff2";
    return "application/octet-stream";
}

// ─────────────────────────────────────────
// HttpResponse
// ─────────────────────────────────────────
HttpResponse HttpResponse::ok(const std::string& body, const std::string& ct) {
    HttpResponse r;
    r.status_code = 200;
    r.content_type = ct;
    r.body = body;
    return r;
}

HttpResponse HttpResponse::created(const std::string& body) {
    HttpResponse r;
    r.status_code = 201;
    r.content_type = "application/json";
    r.body = body;
    return r;
}

HttpResponse HttpResponse::error(int code, const std::string& msg) {
    HttpResponse r;
    r.status_code = code;
    r.content_type = "application/json";
    r.body = utils::json_err(msg);
    return r;
}

HttpResponse HttpResponse::redirect(const std::string& location) {
    HttpResponse r;
    r.status_code = 302;
    r.extra_headers["Location"] = location;
    r.body = "";
    return r;
}

HttpResponse HttpResponse::html(const std::string& body) {
    return ok(body, "text/html; charset=utf-8");
}

HttpResponse HttpResponse::sse_start() {
    HttpResponse r;
    r.status_code = 200;
    r.content_type = "text/event-stream";
    r.extra_headers["Cache-Control"] = "no-cache";
    r.extra_headers["Connection"] = "keep-alive";
    r.extra_headers["X-Accel-Buffering"] = "no";
    return r;
}

static std::string status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

std::string HttpResponse::to_string() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text(status_code) << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    for (auto& [k, v] : extra_headers) {
        oss << k << ": " << v << "\r\n";
    }
    if (content_type != "text/event-stream") {
        oss << "Content-Length: " << body.size() << "\r\n";
    }
    oss << "\r\n";
    if (content_type != "text/event-stream") {
        oss << body;
    }
    return oss.str();
}

// ─────────────────────────────────────────
// Parser HTTP
// ─────────────────────────────────────────
HttpRequest HttpServer::parse_request(const std::string& raw, const std::string& client_ip) {
    HttpRequest req;
    req.client_ip = client_ip;

    std::istringstream stream(raw);
    std::string line;

    // Première ligne : METHOD PATH HTTP/1.1
    if (!std::getline(stream, line)) return req;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream first_line(line);
    first_line >> req.method >> req.path;

    // Séparer chemin et query string
    auto qpos = req.path.find('?');
    if (qpos != std::string::npos) {
        req.query_string = req.path.substr(qpos + 1);
        req.path = req.path.substr(0, qpos);
    }

    // Parser query string
    if (!req.query_string.empty()) {
        req.query = utils::parse_form(req.query_string);
    }

    // Headers
    std::string raw_cookies;
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        if (line.back() == '\r') line.pop_back();
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = utils::trim(line.substr(0, colon));
            std::string val = utils::trim(line.substr(colon + 1));
            // Normaliser en minuscules
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = val;
            if (key == "cookie") raw_cookies = val;
        }
    }

    // Cookies
    if (!raw_cookies.empty()) {
        req.cookies = utils::parse_cookies(raw_cookies);
    }

    // Body : lire le reste
    std::string body_str;
    while (std::getline(stream, line)) {
        body_str += line + "\n";
    }
    if (!body_str.empty() && body_str.back() == '\n') body_str.pop_back();
    req.body = body_str;

    // Détecter SSE
    auto it = req.headers.find("accept");
    if (it != req.headers.end() && it->second.find("text/event-stream") != std::string::npos) {
        req.is_sse = true;
    }

    // Récupérer la VRAIE IP si derrière un Proxy (Railway / Nginx)
    auto fwd = req.headers.find("x-forwarded-for");
    if (fwd != req.headers.end() && !fwd->second.empty()) {
        std::string ip_list = fwd->second;
        // Prendre la première IP avant la virgule
        auto comma = ip_list.find(',');
        if (comma != std::string::npos) {
            req.client_ip = utils::trim(ip_list.substr(0, comma));
        } else {
            req.client_ip = utils::trim(ip_list);
        }
    }

    return req;
}

// ─────────────────────────────────────────
// Servir les fichiers statiques
// ─────────────────────────────────────────
HttpResponse HttpServer::serve_static(const std::string& url_path) {
    // Construire le chemin fichier relatif à ./public/
    std::string filepath = "public" + url_path;

    // Sécurité : empêcher les path traversal
    if (url_path.find("..") != std::string::npos) {
        return HttpResponse::error(403, "Accès refusé");
    }

    // Si c'est un répertoire, chercher index.html
    if (url_path == "/" || url_path.empty()) {
        filepath = "public/index.html";
    }

    // Vérifier existence
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        // Try index.html for SPA-like routing
        std::ifstream fallback("public/index.html");
        if (fallback.is_open()) {
            std::string content((std::istreambuf_iterator<char>(fallback)),
                                 std::istreambuf_iterator<char>());
            return HttpResponse::html(content);
        }
        return HttpResponse::error(404, "Fichier non trouvé : " + url_path);
    }

    // Lire le fichier
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Déterminer le MIME type depuis l'extension
    std::string ext;
    auto dot = filepath.rfind('.');
    if (dot != std::string::npos) ext = filepath.substr(dot);

    return HttpResponse::ok(content, get_mime(ext));
}

// ─────────────────────────────────────────
// Lire une requête complète
// ─────────────────────────────────────────
std::string HttpServer::read_request(int client_fd) {
    std::string raw;
    char buf[4096];
    int received;

    // Lire jusqu'au double CRLF (fin des headers)
    while (true) {
#ifdef _WIN32
        received = recv(client_fd, buf, sizeof(buf) - 1, 0);
#else
        received = recv(client_fd, buf, sizeof(buf) - 1, 0);
#endif
        if (received <= 0) break;
        buf[received] = '\0';
        raw += buf;

        // Chercher fin des headers
        auto header_end = raw.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            // Vérifier Content-Length pour lire le body
            auto cl_pos = raw.find("Content-Length:");
            if (cl_pos == std::string::npos) cl_pos = raw.find("content-length:");
            if (cl_pos != std::string::npos) {
                auto cl_end = raw.find("\r\n", cl_pos);
                int content_length = std::stoi(
                    utils::trim(raw.substr(cl_pos + 15, cl_end - cl_pos - 15))
                );
                size_t body_start = header_end + 4;
                size_t body_received = raw.size() - body_start;

                // Lire le reste du body si nécessaire
                while ((int)body_received < content_length) {
                    received = recv(client_fd, buf, std::min((int)(sizeof(buf)-1), content_length - (int)body_received), 0);
                    if (received <= 0) break;
                    buf[received] = '\0';
                    raw += buf;
                    body_received += received;
                }
            }
            break;
        }
    }
    return raw;
}

// ─────────────────────────────────────────
// Envoyer une réponse
// ─────────────────────────────────────────
void HttpServer::send_response(int client_fd, const HttpResponse& resp) {
    std::string data = resp.to_string();
    size_t total = 0;
    while (total < data.size()) {
#ifdef _WIN32
        int sent = send(client_fd, data.c_str() + total, (int)(data.size() - total), 0);
#else
        ssize_t sent = send(client_fd, data.c_str() + total, data.size() - total, 0);
#endif
        if (sent <= 0) break;
        total += sent;
    }
}

// ─────────────────────────────────────────
// Traitement client en thread séparé
// ─────────────────────────────────────────
void HttpServer::handle_client(int client_fd, const std::string& client_ip) {
    // Lire la requête
    std::string raw = read_request(client_fd);
    if (raw.empty()) {
        CLOSE_SOCKET(client_fd);
        return;
    }

    // Parser
    HttpRequest req = parse_request(raw, client_ip);

    // OPTIONS preflight CORS
    if (req.method == "OPTIONS") {
        HttpResponse r;
        r.status_code = 204;
        r.body = "";
        r.extra_headers["Access-Control-Allow-Origin"] = "*";
        r.extra_headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        r.extra_headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        send_response(client_fd, r);
        CLOSE_SOCKET(client_fd);
        return;
    }

    // Fichiers statiques (chemin ne commençant pas par /api/)
    if (req.path.substr(0, 5) != "/api/" && req.path != "/api") {
        HttpResponse r = serve_static(req.path);
        send_response(client_fd, r);
        CLOSE_SOCKET(client_fd);
        return;
    }

    // Handler API
    HttpResponse resp;
    if (handler_) {
        resp = handler_(req);
    } else {
        resp = HttpResponse::error(500, "Aucun handler configuré");
    }

    // Si c'est une réponse SSE, le handler garde la connexion ouverte lui-même
    if (resp.content_type == "text/event-stream") {
        // Envoyer les headers SSE
        std::string headers = resp.to_string();
        send(client_fd, headers.c_str(), (int)headers.size(), 0);
        // Le handler SSE prend le contrôle du fd - on ne ferme pas ici
        // (le SSE manager gère la fermeture)
        return;
    }

    send_response(client_fd, resp);
    CLOSE_SOCKET(client_fd);
}

// ─────────────────────────────────────────
// Serveur principal
// ─────────────────────────────────────────
HttpServer::HttpServer() {
#ifdef _WIN32
    // Initialiser Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[Server] WSAStartup failed" << std::endl;
    }
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpServer::set_handler(Handler h) {
    handler_ = h;
}

void HttpServer::stop() {
    running_ = false;
    if (server_fd_ != -1) {
        CLOSE_SOCKET(server_fd_);
        server_fd_ = -1;
    }
}

bool HttpServer::listen(int port) {
    // Créer le socket
    server_fd_ = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == INVALID_SOCKET) {
        std::cerr << "[Server] Impossible de créer le socket" << std::endl;
        return false;
    }

    // SO_REUSEADDR
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Bind
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Server] Bind échoué sur le port " << port << std::endl;
        CLOSE_SOCKET(server_fd_);
        return false;
    }

    // Listen
    if (::listen(server_fd_, 128) == SOCKET_ERROR) {
        std::cerr << "[Server] Listen échoué" << std::endl;
        CLOSE_SOCKET(server_fd_);
        return false;
    }

    running_ = true;
    std::cout << "\n╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║  SMARTPRESENCE CI — Serveur démarré       ║" << std::endl;
    std::cout << "║  http://localhost:" << port << "                   ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝\n" << std::endl;

    // Boucle principale accept
    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = (int)accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == INVALID_SOCKET) {
            if (!running_) break;
            continue;
        }

        // IP client
        char ip_buf[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
        std::string client_ip(ip_buf);

        // Thread dédié par connexion
        std::thread([this, client_fd, client_ip]() {
            this->handle_client(client_fd, client_ip);
        }).detach();
    }

    return true;
}
