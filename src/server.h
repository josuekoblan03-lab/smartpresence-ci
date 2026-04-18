// ============================================================
// server.h — Serveur HTTP raw socket (Winsock2 / POSIX)
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include <string>
#include <map>
#include <functional>
#include <vector>

// ─────────────────────────────────────────
// Requête HTTP parsée
// ─────────────────────────────────────────
struct HttpRequest {
    std::string method;                          // GET, POST, PUT, DELETE
    std::string path;                            // /api/sessions
    std::string query_string;                    // key=val&key2=val2
    std::map<std::string, std::string> headers;  // headers HTTP
    std::map<std::string, std::string> cookies;  // cookies parsés
    std::map<std::string, std::string> params;   // paramètres d'URL (ex: :id)
    std::map<std::string, std::string> query;    // query string parsé
    std::string body;                            // corps de la requête
    std::string client_ip;                       // IP du client
    bool        is_sse = false;                  // connexion SSE ?
};

// ─────────────────────────────────────────
// Réponse HTTP
// ─────────────────────────────────────────
struct HttpResponse {
    int         status_code    = 200;
    std::string content_type   = "application/json";
    std::string body;
    std::map<std::string, std::string> extra_headers;

    // Helpers de construction rapide
    static HttpResponse ok(const std::string& body,
                           const std::string& ct = "application/json");
    static HttpResponse created(const std::string& body);
    static HttpResponse error(int code, const std::string& msg);
    static HttpResponse redirect(const std::string& location);
    static HttpResponse html(const std::string& body);
    static HttpResponse sse_start();  // Démarre un stream SSE

    // Sérialiser en chaîne HTTP complète
    std::string to_string() const;
};

// Type handler : prend une requête, retourne une réponse
using Handler = std::function<HttpResponse(const HttpRequest&)>;

// ─────────────────────────────────────────
// Serveur HTTP
// ─────────────────────────────────────────
class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // Démarrer l'écoute sur le port donné (bloquant)
    bool listen(int port);

    // Enregistrer un handler (appelé depuis Router)
    void set_handler(Handler handler);

    // Stopper le serveur
    void stop();

private:
    int    server_fd_ = -1;
    bool   running_   = false;
    Handler handler_;

    // Parser une requête HTTP brute
    static HttpRequest parse_request(const std::string& raw, const std::string& client_ip);

    // Lire une requête complète depuis le socket
    static std::string read_request(int client_fd);

    // Envoyer une réponse
    static void send_response(int client_fd, const HttpResponse& resp);

    // Servir un fichier statique
    static HttpResponse serve_static(const std::string& path);

    // Traiter une connexion client dans un thread
    void handle_client(int client_fd, const std::string& client_ip);
};
