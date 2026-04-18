// ============================================================
// router.cpp — Implémentation du routeur URL
// SMARTPRESENCE CI
// ============================================================
#include "router.h"
#include "handlers.h"
#include "auth.h"
#include "utils.h"
#include <iostream>
#include <regex>

Router::Router(Database& db) : db_(db) {}

// ──────────────────────────────────────────────────────────────
// Convertir un chemin de route avec :param en regex
// Exemples :
//   /api/sessions/:id    → /api/sessions/([^/]+)
//   /api/qrcode/:session_id/token → /api/qrcode/([^/]+)/token
// ──────────────────────────────────────────────────────────────
std::pair<std::regex, std::vector<std::string>>
Router::path_to_regex(const std::string& path) {
    std::vector<std::string> param_names;
    std::string pattern = "^";

    size_t i = 0;
    while (i < path.size()) {
        if (path[i] == ':') {
            // Extraire le nom du paramètre
            size_t end = i + 1;
            while (end < path.size() && path[end] != '/') end++;
            param_names.push_back(path.substr(i + 1, end - i - 1));
            pattern += "([^/]+)";
            i = end;
        } else {
            // Échapper les caractères regex spéciaux
            char c = path[i];
            if (c == '.' || c == '*' || c == '+' || c == '?' ||
                c == '(' || c == ')' || c == '[' || c == ']' ||
                c == '{' || c == '}' || c == '^' || c == '$') {
                pattern += '\\';
            }
            pattern += c;
            i++;
        }
    }
    pattern += "$";

    return { std::regex(pattern), param_names };
}

// ──────────────────────────────────────────────────────────────
// Ajouter une route
// ──────────────────────────────────────────────────────────────
void Router::add_route(const std::string& method, const std::string& path,
                       Handler handler, bool auth_required, bool admin_only) {
    auto [pattern, param_names] = path_to_regex(path);
    Route r;
    r.method       = method;
    r.path         = path;
    r.pattern      = pattern;
    r.param_names  = param_names;
    r.handler      = handler;
    r.auth_required = auth_required;
    r.admin_only   = admin_only;
    routes_.push_back(r);
}

// ──────────────────────────────────────────────────────────────
// Vérifier l'authentification JWT
// ──────────────────────────────────────────────────────────────
JWTClaims Router::check_auth(const HttpRequest& req) {
    auto it = req.cookies.find("smartpresence_token");
    if (it != req.cookies.end()) {
        return auth::verify_token(it->second);
    }
    // Fallback : Authorization header
    auto auth_it = req.headers.find("authorization");
    if (auth_it != req.headers.end()) {
        std::string bearer = auth_it->second;
        if (bearer.size() > 7 && bearer.substr(0, 7) == "Bearer ") {
            return auth::verify_token(bearer.substr(7));
        }
    }
    return JWTClaims{};
}

// ──────────────────────────────────────────────────────────────
// Enregistrer toutes les routes
// ──────────────────────────────────────────────────────────────
void Router::register_routes() {
    // ── Auth ──
    add_route("POST", "/api/auth/login",   [this](const HttpRequest& req) {
        return handlers::login(req, db_);
    }, false);

    add_route("POST", "/api/auth/logout",  [this](const HttpRequest& req) {
        return handlers::logout(req);
    }, true);

    add_route("GET",  "/api/auth/me",      [this](const HttpRequest& req) {
        return handlers::me(req, db_);
    }, true);

    // ── Dashboard ──
    add_route("GET",  "/api/dashboard/stats", [this](const HttpRequest& req) {
        return handlers::dashboard_stats(req, db_);
    }, true);

    // ── Sessions ──
    add_route("GET",  "/api/sessions",         [this](const HttpRequest& req) {
        return handlers::list_sessions(req, db_);
    }, true);

    add_route("POST", "/api/sessions",         [this](const HttpRequest& req) {
        return handlers::create_session(req, db_);
    }, true);

    add_route("GET",  "/api/sessions/:id",     [this](const HttpRequest& req) {
        return handlers::get_session(req, db_);
    }, true);

    add_route("PUT",  "/api/sessions/:id/close", [this](const HttpRequest& req) {
        return handlers::close_session(req, db_);
    }, true);

    // ── QR Code ──
    add_route("GET",  "/api/qrcode/:session_id",       [this](const HttpRequest& req) {
        return handlers::get_qrcode(req, db_);
    }, false);  // Public (page de projection)

    add_route("GET",  "/api/qrcode/:session_id/token", [this](const HttpRequest& req) {
        return handlers::get_qrcode_token(req, db_);
    }, false);

    // ── Présences ──
    add_route("POST", "/api/presence/mark",    [this](const HttpRequest& req) {
        return handlers::mark_presence(req, db_);
    }, false);  // Public (accessible depuis scan.html)

    add_route("GET",  "/api/presences/:session_id", [this](const HttpRequest& req) {
        return handlers::list_presences(req, db_);
    }, true);

    // ── Étudiants ──
    add_route("GET",  "/api/students",         [this](const HttpRequest& req) {
        return handlers::list_students(req, db_);
    }, true);

    add_route("POST", "/api/students",         [this](const HttpRequest& req) {
        return handlers::create_student(req, db_);
    }, true, true);  // Admin seulement

    add_route("GET",  "/api/students/:id",     [this](const HttpRequest& req) {
        return handlers::get_student(req, db_);
    }, true);

    // ── Enseignants ──
    add_route("GET",  "/api/teachers",         [this](const HttpRequest& req) {
        return handlers::list_enseignants(req, db_);
    }, true, true);  // Admin seulement

    add_route("POST", "/api/teachers",         [this](const HttpRequest& req) {
        return handlers::create_enseignant(req, db_);
    }, true, true);  // Admin seulement

    // ── Filières ──
    add_route("GET",  "/api/filieres",         [this](const HttpRequest& req) {
        return handlers::list_filieres(req, db_);
    }, false);

    // ── Fraudes ──
    add_route("GET",  "/api/frauds",           [this](const HttpRequest& req) {
        return handlers::list_frauds(req, db_);
    }, true);

    // ── Rapports ──
    add_route("GET",  "/api/reports/session/:id", [this](const HttpRequest& req) {
        return handlers::report_session(req, db_);
    }, true);

    // ── Events polling (remplace SSE pour Railway) ──
    add_route("GET",  "/api/sse/events",       [this](const HttpRequest& req) {
        return handlers::get_events_poll(req, db_);
    }, false);

    add_route("GET",  "/api/events",           [this](const HttpRequest& req) {
        return handlers::get_events_poll(req, db_);
    }, false);

    std::cout << "[Router] " << routes_.size() << " routes enregistrées" << std::endl;
}

// ──────────────────────────────────────────────────────────────
// Dispatcher une requête → handler approprié
// ──────────────────────────────────────────────────────────────
HttpResponse Router::dispatch(const HttpRequest& req) {
    std::smatch match;

    for (auto& route : routes_) {
        // Vérifier la méthode HTTP
        if (route.method != req.method) continue;

        // Tester le pattern
        if (!std::regex_match(req.path, match, route.pattern)) continue;

        // Extraire les paramètres d'URL
        HttpRequest mutable_req = req;
        for (size_t i = 0; i < route.param_names.size() && i + 1 < match.size(); i++) {
            mutable_req.params[route.param_names[i]] = match[i + 1].str();
        }

        // Vérifier l'authentification si requise
        if (route.auth_required) {
            JWTClaims claims = check_auth(req);
            if (!claims.valid) {
                return HttpResponse::error(401, "Authentification requise");
            }
            if (route.admin_only && claims.role != "admin") {
                return HttpResponse::error(403, "Accès réservé aux administrateurs");
            }
        }

        std::cout << "[Router] " << req.method << " " << req.path << std::endl;

        // Appeler le handler
        return route.handler(mutable_req);
    }

    // Aucune route trouvée
    return HttpResponse::error(404, "Route introuvable : " + req.method + " " + req.path);
}
