// ============================================================
// router.h — Routeur URL → Handlers
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include "server.h"
#include "database.h"
#include "auth.h"
#include <vector>
#include <regex>

// ─────────────────────────────────────────
// Route enregistrée
// ─────────────────────────────────────────
struct Route {
    std::string method;       // GET, POST, PUT, DELETE
    std::string path;         // /api/sessions/:id
    std::regex  pattern;      // regex compilé
    std::vector<std::string> param_names; // noms des paramètres d'URL
    Handler     handler;      // fonction handler
    bool        auth_required = true;     // nécessite JWT ?
    bool        admin_only    = false;    // réservé admin ?
};

// ─────────────────────────────────────────
// Routeur
// ─────────────────────────────────────────
class Router {
public:
    Router(Database& db);

    // Enregistrer toutes les routes API
    void register_routes();

    // Dispatcher une requête → handler approprié
    HttpResponse dispatch(const HttpRequest& req);

private:
    Database&    db_;
    std::vector<Route> routes_;

    // Ajouter une route
    void add_route(const std::string& method,
                   const std::string& path,
                   Handler handler,
                   bool auth_required = true,
                   bool admin_only    = false);

    // Convertir un chemin de route en regex
    // Ex: /api/sessions/:id → /api/sessions/([^/]+)
    static std::pair<std::regex, std::vector<std::string>>
        path_to_regex(const std::string& path);

    // Vérifier l'authentification JWT
    JWTClaims check_auth(const HttpRequest& req);
};
