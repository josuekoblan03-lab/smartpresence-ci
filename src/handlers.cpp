// ============================================================
// handlers.cpp — Implémentation de tous les handlers API REST
// SMARTPRESENCE CI
// ============================================================
#include "handlers.h"
#include "utils.h"
#include "auth.h"
#include "sse.h"
#include "qrcode.h"
#include "shields.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
  #include <winsock2.h>
  #define SEND_DATA(fd, buf, len) send(fd, buf, len, 0)
  #define CLOSE_FD(fd) closesocket(fd)
#else
  #include <sys/socket.h>
  #include <unistd.h>
  #define SEND_DATA(fd, buf, len) send(fd, buf, len, 0)
  #define CLOSE_FD(fd) close(fd)
#endif

namespace handlers {

// ──────────────────────────────────────────────────────────────
// Helper : extraire le JWT courant de la requête
// ──────────────────────────────────────────────────────────────
static JWTClaims get_claims(const HttpRequest& req) {
    auto it = req.cookies.find("smartpresence_token");
    if (it == req.cookies.end()) {
        // Chercher dans Authorization: Bearer <token>
        auto auth_it = req.headers.find("authorization");
        if (auth_it != req.headers.end()) {
            std::string bearer = auth_it->second;
            if (bearer.substr(0, 7) == "Bearer ") {
                return auth::verify_token(bearer.substr(7));
            }
        }
        return JWTClaims{};
    }
    return auth::verify_token(it->second);
}

// Helper : JSON d'un utilisateur
static std::string user_to_json(const Utilisateur& u) {
    return "{" +
        utils::json_num("id",    u.id)      + "," +
        utils::json_str("email", u.email)   + "," +
        utils::json_str("nom",   u.nom)     + "," +
        utils::json_str("prenom",u.prenom)  + "," +
        utils::json_str("role",  u.role)    +
        "}";
}

// Helper : JSON d'un étudiant
static std::string etudiant_to_json(const Etudiant& e) {
    return "{" +
        utils::json_num("id",             e.id)            + "," +
        utils::json_str("matricule",      e.matricule)     + "," +
        utils::json_str("nom",            e.nom)           + "," +
        utils::json_str("prenom",         e.prenom)        + "," +
        utils::json_str("email",          e.email)         + "," +
        utils::json_num("filiere_id",     e.filiere_id)    + "," +
        utils::json_str("filiere_nom",    e.filiere_nom)   +
        "}";
}

// Helper : JSON d'une session
static std::string session_to_json(const SessionCours& s) {
    return "{" +
        utils::json_num("id",              s.id)             + "," +
        utils::json_str("titre",           s.titre)          + "," +
        utils::json_str("matiere",         s.matiere)        + "," +
        utils::json_num("enseignant_id",   s.enseignant_id)  + "," +
        utils::json_str("enseignant_nom",  s.enseignant_nom) + "," +
        utils::json_num("filiere_id",      s.filiere_id)     + "," +
        utils::json_str("filiere_nom",     s.filiere_nom)    + "," +
        utils::json_num("date_debut",      s.date_debut)     + "," +
        utils::json_str("date_debut_fmt",  utils::format_datetime(s.date_debut)) + "," +
        utils::json_str("statut",          s.statut)         + "," +
        utils::json_str("qr_token",        s.qr_token)       + "," +
        utils::json_num("qr_expire_at",    s.qr_expire_at)   + "," +
        utils::json_num("presences_count", s.presences_count) +
        "}";
}

// ──────────────────────────────────────────────────────────────
// POST /api/auth/login
// ──────────────────────────────────────────────────────────────
HttpResponse login(const HttpRequest& req, Database& db) {
    // Parser le body (JSON)
    std::string email    = utils::json_get_str(req.body, "email");
    std::string password = utils::json_get_str(req.body, "password");

    // Fallback form-urlencoded
    if (email.empty()) {
        auto params = utils::parse_form(req.body);
        email    = params["email"];
        password = params["password"];
    }

    if (email.empty() || password.empty()) {
        return HttpResponse::error(400, "Email et mot de passe requis");
    }

    // Chercher l'utilisateur
    Utilisateur user = db.find_user_by_email(email);
    if (user.id == 0) {
        return HttpResponse::error(401, "Identifiants incorrects");
    }

    // Vérifier le mot de passe
    if (!utils::verify_password(password, user.password_hash)) {
        return HttpResponse::error(401, "Identifiants incorrects");
    }

    if (!user.actif) {
        return HttpResponse::error(403, "Compte désactivé");
    }

    // Générer le JWT
    std::string token = auth::create_token(user.id, user.role, user.email,
                                             user.nom + " " + user.prenom);

    // Réponse avec cookie
    HttpResponse resp;
    resp.status_code  = 200;
    resp.content_type = "application/json";
    resp.body = "{\"success\":true," +
                utils::json_str("token", token) + "," +
                "\"user\":" + user_to_json(user) + "}";
    resp.extra_headers["Set-Cookie"] =
        "smartpresence_token=" + token +
        "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400";

    std::cout << "[Auth] Login: " << email << " (" << user.role << ")" << std::endl;
    return resp;
}

// ──────────────────────────────────────────────────────────────
// POST /api/auth/logout
// ──────────────────────────────────────────────────────────────
HttpResponse logout(const HttpRequest& req) {
    HttpResponse resp;
    resp.status_code  = 200;
    resp.content_type = "application/json";
    resp.body         = utils::json_ok("Déconnexion réussie");
    resp.extra_headers["Set-Cookie"] =
        "smartpresence_token=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";
    return resp;
}

// ──────────────────────────────────────────────────────────────
// GET /api/auth/me
// ──────────────────────────────────────────────────────────────
HttpResponse me(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    Utilisateur user = db.find_user_by_id(claims.user_id);
    if (user.id == 0) return HttpResponse::error(404, "Utilisateur introuvable");

    return HttpResponse::ok("{\"success\":true,\"user\":" + user_to_json(user) + "}");
}

// ──────────────────────────────────────────────────────────────
// GET /api/dashboard/stats
// ──────────────────────────────────────────────────────────────
HttpResponse dashboard_stats(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    // Admin voit tout, enseignant voit ses propres stats
    int eid = (claims.role == "admin") ? 0 : claims.user_id;
    DashboardStats stats = db.get_dashboard_stats(eid);

    std::ostringstream json;
    json << "{"
         << "\"success\":true,"
         << "\"stats\":{"
         << utils::json_num("total_etudiants",   stats.total_etudiants)   << ","
         << utils::json_num("total_sessions",    stats.total_sessions)    << ","
         << utils::json_num("sessions_ouvertes", stats.sessions_ouvertes) << ","
         << utils::json_num("presences_today",   stats.presences_today)   << ","
         << utils::json_num("tentatives_fraude", stats.tentatives_fraude) << ","
         << utils::json_num("total_presences",   stats.total_presences)   << ","
         << "\"taux_presence\":" << (int)stats.taux_presence
         << "}}";

    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// GET /api/sessions
// ──────────────────────────────────────────────────────────────
HttpResponse list_sessions(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int eid = (claims.role == "admin") ? 0 : claims.user_id;
    auto sessions = db.list_sessions(eid, 50);

    std::ostringstream json;
    json << "{\"success\":true,\"sessions\":[";
    for (size_t i = 0; i < sessions.size(); i++) {
        if (i > 0) json << ",";
        json << session_to_json(sessions[i]);
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// POST /api/sessions — Créer une nouvelle session
// ──────────────────────────────────────────────────────────────
HttpResponse create_session(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");
    if (claims.role == "admin" == false && claims.role != "enseignant")
        return HttpResponse::error(403, "Réservé aux enseignants");

    std::string titre    = utils::json_get_str(req.body, "titre");
    std::string matiere  = utils::json_get_str(req.body, "matiere");
    int filiere_id       = (int)utils::json_get_int(req.body, "filiere_id");

    if (titre.empty() || matiere.empty()) {
        return HttpResponse::error(400, "Titre et matière requis");
    }

    SessionCours sc;
    sc.titre         = titre;
    sc.matiere       = matiere;
    sc.enseignant_id = claims.user_id;
    sc.filiere_id    = filiere_id;
    sc.date_debut    = utils::now_unix();
    sc.qr_token      = qrcode::generate_token(0); // ID temporaire
    sc.qr_expire_at  = utils::now_unix() + qrcode::QR_LIFETIME_SECONDS;

    int sid = db.create_session(sc);
    if (sid < 0) return HttpResponse::error(500, "Erreur création session");

    // Regénérer le token avec le vrai session_id
    std::string real_token = qrcode::generate_token(sid);
    long long   expire_at  = utils::now_unix() + qrcode::QR_LIFETIME_SECONDS;
    db.update_qr_token(sid, real_token, expire_at);

    SessionCours created = db.find_session_by_id(sid);
    std::cout << "[Handler] Session créée: #" << sid << " - " << titre << std::endl;

    // Démarrer le thread de renouvellement QR
    std::thread([sid, &db]() mutable {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(qrcode::QR_LIFETIME_SECONDS));
            // Vérifier que la session est toujours ouverte
            SessionCours s = db.find_session_by_id(sid);
            if (s.statut != "ouverte") break;
            // Renouveler le token
            std::string new_token = qrcode::generate_token(sid);
            long long   new_exp   = utils::now_unix() + qrcode::QR_LIFETIME_SECONDS;
            db.update_qr_token(sid, new_token, new_exp);
            // Notifier les clients SSE
            SSEManager::instance().broadcast("qr_refresh",
                sse_events::qr_refresh(sid, new_token));
            std::cout << "[QR] Token renouvelé pour session #" << sid << std::endl;
        }
    }).detach();

    return HttpResponse::created(
        "{\"success\":true," +
        utils::json_num("session_id", sid) + "," +
        "\"session\":" + session_to_json(created) +
        "}"
    );
}

// ──────────────────────────────────────────────────────────────
// GET /api/sessions/:id
// ──────────────────────────────────────────────────────────────
HttpResponse get_session(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int sid = 0;
    auto it = req.params.find("id");
    if (it != req.params.end()) sid = std::stoi(it->second);
    if (sid <= 0) return HttpResponse::error(400, "ID de session invalide");

    SessionCours s = db.find_session_by_id(sid);
    if (s.id == 0) return HttpResponse::error(404, "Session introuvable");

    return HttpResponse::ok("{\"success\":true,\"session\":" + session_to_json(s) + "}");
}

// ──────────────────────────────────────────────────────────────
// PUT /api/sessions/:id/close
// ──────────────────────────────────────────────────────────────
HttpResponse close_session(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int sid = 0;
    auto it = req.params.find("id");
    if (it != req.params.end()) sid = std::stoi(it->second);
    if (sid <= 0) return HttpResponse::error(400, "ID invalide");

    db.update_session_statut(sid, "fermee");
    std::cout << "[Handler] Session fermée: #" << sid << std::endl;

    return HttpResponse::ok(utils::json_ok("Session fermée"));
}

// ──────────────────────────────────────────────────────────────
// GET /api/qrcode/:session_id — SVG du QR Code
// ──────────────────────────────────────────────────────────────
HttpResponse get_qrcode(const HttpRequest& req, Database& db) {
    int sid = 0;
    auto it = req.params.find("session_id");
    if (it != req.params.end()) sid = std::stoi(it->second);
    if (sid <= 0) return HttpResponse::error(400, "ID session invalide");

    SessionCours s = db.find_session_by_id(sid);
    if (s.id == 0) return HttpResponse::error(404, "Session introuvable");

    // Vérifier si le token est expiré et le renouveler
    if (utils::now_unix() > s.qr_expire_at && s.statut == "ouverte") {
        std::string new_token = qrcode::generate_token(sid);
        long long   new_exp   = utils::now_unix() + qrcode::QR_LIFETIME_SECONDS;
        db.update_qr_token(sid, new_token, new_exp);
        s.qr_token    = new_token;
        s.qr_expire_at = new_exp;
    }

    std::string svg = qrcode::generate_svg(s.qr_token, sid);
    return HttpResponse::ok(svg, "image/svg+xml");
}

// ──────────────────────────────────────────────────────────────
// GET /api/qrcode/:session_id/token — Token JSON seulement
// ──────────────────────────────────────────────────────────────
HttpResponse get_qrcode_token(const HttpRequest& req, Database& db) {
    int sid = 0;
    auto it = req.params.find("session_id");
    if (it != req.params.end()) sid = std::stoi(it->second);
    if (sid <= 0) return HttpResponse::error(400, "ID session invalide");

    SessionCours s = db.find_session_by_id(sid);
    if (s.id == 0) return HttpResponse::error(404, "Session introuvable");

    // Renouveler si expiré
    if (utils::now_unix() > s.qr_expire_at && s.statut == "ouverte") {
        s.qr_token    = qrcode::generate_token(sid);
        s.qr_expire_at = utils::now_unix() + qrcode::QR_LIFETIME_SECONDS;
        db.update_qr_token(sid, s.qr_token, s.qr_expire_at);
    }

    long long expires_in = s.qr_expire_at - utils::now_unix();
    if (expires_in < 0) expires_in = 0;

    return HttpResponse::ok(
        "{\"success\":true," +
        utils::json_str("token",    s.qr_token)  + "," +
        utils::json_num("session_id", sid)        + "," +
        utils::json_num("expires_in", expires_in) + "," +
        utils::json_num("expire_at", s.qr_expire_at) +
        "}"
    );
}

// ──────────────────────────────────────────────────────────────
// POST /api/presence/mark — Marquer une présence
// ──────────────────────────────────────────────────────────────
HttpResponse mark_presence(const HttpRequest& req, Database& db) {
    // Extraire les données
    std::string matricule      = utils::json_get_str(req.body, "matricule");
    std::string code_personnel = utils::json_get_str(req.body, "code_personnel");
    std::string qr_token       = utils::json_get_str(req.body, "qr_token");
    int session_id             = (int)utils::json_get_int(req.body, "session_id");
    std::string device_id      = utils::json_get_str(req.body, "device_id");

    // Fallback form data
    if (matricule.empty()) {
        auto params    = utils::parse_form(req.body);
        matricule      = params["matricule"];
        code_personnel = params["code_personnel"];
        qr_token       = params["qr_token"];
        device_id      = params["device_id"];
        if (!params["session_id"].empty()) session_id = std::stoi(params["session_id"]);
    }

    if (matricule.empty() || code_personnel.empty() || qr_token.empty() || session_id <= 0) {
        return HttpResponse::error(400, "Matricule, code personnel, token QR et session_id requis");
    }

    // Charger étudiant et session
    Etudiant etudiant = db.find_etudiant_by_matricule(matricule);
    if (etudiant.id == 0) {
        return HttpResponse::error(404, "Matricule inconnu : " + matricule);
    }

    SessionCours session = db.find_session_by_id(session_id);

    // Construire la tentative
    PresenceAttempt attempt;
    attempt.session_id      = session_id;
    attempt.etudiant_id     = etudiant.id;
    attempt.qr_token        = qr_token;
    attempt.code_personnel  = code_personnel;
    attempt.ip_client       = req.client_ip;
    attempt.device_id       = device_id;
    attempt.timestamp       = utils::now_unix();

    // ── Vérification des 6 boucliers ──
    ShieldResult result = shields::verify_all(attempt, session, etudiant, db);

    if (!result.passed) {
        // Enregistrer la tentative de fraude
        FraudeLog fraude;
        fraude.session_id  = session_id;
        fraude.etudiant_id = etudiant.id;
        fraude.type_fraude = result.shield_name;
        fraude.description = result.message;
        fraude.ip_client   = req.client_ip;
        db.log_fraude(fraude);

        // Notifier SSE
        SSEManager::instance().broadcast("fraud_detected",
            sse_events::fraud_detected(
                result.shield_name,
                result.message,
                req.client_ip,
                session_id
            )
        );

        return HttpResponse::error(403, result.message);
    }

    // ── Enregistrer la présence ──
    Presence p;
    p.session_id  = session_id;
    p.etudiant_id = etudiant.id;
    p.horodatage  = utils::now_unix();
    p.ip_client   = req.client_ip;
    p.device_id   = device_id;
    p.valide      = true;

    if (!db.mark_presence(p)) {
        return HttpResponse::error(500, "Erreur enregistrement présence");
    }

    // Notifier SSE
    SSEManager::instance().broadcast("presence_marked",
        sse_events::presence_marked(
            etudiant.nom + " " + etudiant.prenom,
            etudiant.matricule,
            session_id,
            utils::format_datetime(p.horodatage)
        )
    );

    std::cout << "[Présence] ✅ " << etudiant.nom << " " << etudiant.prenom
              << " | Session #" << session_id << std::endl;

    return HttpResponse::ok(
        "{\"success\":true," +
        utils::json_str("message",       "Présence enregistrée avec succès") + "," +
        utils::json_str("etudiant_nom",  etudiant.nom + " " + etudiant.prenom) + "," +
        utils::json_str("matricule",     etudiant.matricule) + "," +
        utils::json_num("session_id",    session_id) + "," +
        utils::json_str("horodatage",    utils::format_datetime(p.horodatage)) +
        "}"
    );
}

// ──────────────────────────────────────────────────────────────
// GET /api/presences/:session_id — Liste des présences d'une session
// ──────────────────────────────────────────────────────────────
HttpResponse list_presences(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int sid = 0;
    auto it = req.params.find("session_id");
    if (it != req.params.end()) sid = std::stoi(it->second);

    auto presences = db.list_presences(sid);

    std::ostringstream json;
    json << "{\"success\":true,\"presences\":[";
    for (size_t i = 0; i < presences.size(); i++) {
        if (i > 0) json << ",";
        auto& p = presences[i];
        json << "{"
             << utils::json_num("id",           p.id)                          << ","
             << utils::json_str("etudiant_nom",  p.etudiant_nom)               << ","
             << utils::json_str("matricule",     p.etudiant_matricule)          << ","
             << utils::json_num("horodatage",    p.horodatage)                 << ","
             << utils::json_str("horodatage_fmt",utils::format_datetime(p.horodatage)) << ","
             << utils::json_str("ip_client",     p.ip_client)                  << ","
             << utils::json_bool("valide",       p.valide)
             << "}";
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// GET /api/students
// ──────────────────────────────────────────────────────────────
HttpResponse list_students(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int filiere_id = 0;
    auto it = req.query.find("filiere_id");
    if (it != req.query.end() && !it->second.empty())
        filiere_id = std::stoi(it->second);

    int page = 1;
    auto pg = req.query.find("page");
    if (pg != req.query.end() && !pg->second.empty())
        page = std::stoi(pg->second);

    auto students = db.list_etudiants(filiere_id, page, 50);
    int total     = db.count_etudiants(filiere_id);

    std::ostringstream json;
    json << "{\"success\":true,"
         << "\"total\":" << total << ","
         << "\"page\":" << page << ","
         << "\"students\":[";
    for (size_t i = 0; i < students.size(); i++) {
        if (i > 0) json << ",";
        json << etudiant_to_json(students[i]);
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// GET /api/students/:id
// ──────────────────────────────────────────────────────────────
HttpResponse get_student(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int eid = 0;
    auto it = req.params.find("id");
    if (it != req.params.end()) eid = std::stoi(it->second);

    Etudiant e = db.find_etudiant_by_id(eid);
    if (e.id == 0) return HttpResponse::error(404, "Étudiant introuvable");

    return HttpResponse::ok("{\"success\":true,\"student\":" + etudiant_to_json(e) + "}");
}

// ──────────────────────────────────────────────────────────────
// POST /api/students
// ──────────────────────────────────────────────────────────────
HttpResponse create_student(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");
    if (claims.role != "admin") return HttpResponse::error(403, "Admin requis");

    Etudiant e;
    e.nom            = utils::json_get_str(req.body, "nom");
    e.prenom         = utils::json_get_str(req.body, "prenom");
    e.email          = utils::json_get_str(req.body, "email");
    e.matricule      = utils::json_get_str(req.body, "matricule");
    e.code_personnel = utils::json_get_str(req.body, "code_personnel");
    e.filiere_id     = (int)utils::json_get_int(req.body, "filiere_id");
    e.universite_id  = 1;

    if (e.nom.empty() || e.prenom.empty() || e.matricule.empty()) {
        return HttpResponse::error(400, "Nom, prénom et matricule requis");
    }
    if (e.code_personnel.empty()) {
        e.code_personnel = utils::random_code(6);
    }

    if (!db.create_etudiant(e)) {
        return HttpResponse::error(409, "Matricule déjà existant");
    }

    return HttpResponse::created(
        "{\"success\":true," +
        utils::json_str("message",        "Étudiant créé") + "," +
        utils::json_str("code_personnel", e.code_personnel) +
        "}"
    );
}

// ──────────────────────────────────────────────────────────────
// GET /api/teachers
// ──────────────────────────────────────────────────────────────
HttpResponse list_enseignants(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");
    if (claims.role != "admin") return HttpResponse::error(403, "Admin requis");

    auto enseignants = db.list_enseignants(1); // 1 = IUA
    std::ostringstream json;
    json << "{\"success\":true,\"teachers\":[";
    for (size_t i = 0; i < enseignants.size(); i++) {
        if (i > 0) json << ",";
        auto& p = enseignants[i];
        json << "{"
             << utils::json_num("id",      p.id)            << ","
             << utils::json_str("nom",     p.nom)           << ","
             << utils::json_str("prenom",  p.prenom)        << ","
             << utils::json_str("email",   p.email)         << ","
             << utils::json_bool("actif",  p.actif)
             << "}";
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// POST /api/teachers
// ──────────────────────────────────────────────────────────────
HttpResponse create_enseignant(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");
    if (claims.role != "admin") return HttpResponse::error(403, "Admin requis");

    Utilisateur u;
    u.nom            = utils::json_get_str(req.body, "nom");
    u.prenom         = utils::json_get_str(req.body, "prenom");
    u.email          = utils::json_get_str(req.body, "email");
    std::string pass = utils::json_get_str(req.body, "password");
    u.role           = "enseignant";
    u.universite_id  = 1; // IUA

    if (u.nom.empty() || u.prenom.empty() || u.email.empty() || pass.empty()) {
        return HttpResponse::error(400, "Nom, prénom, email et mot de passe requis");
    }

    u.password_hash = utils::hash_password(pass);

    if (!db.create_utilisateur(u)) {
        return HttpResponse::error(409, "Cet email est déjà utilisé");
    }

    return HttpResponse::created(
        "{\"success\":true," +
        utils::json_str("message", "Professeur créé avec succès") +
        "}"
    );
}

// ──────────────────────────────────────────────────────────────
// GET /api/filieres
// ──────────────────────────────────────────────────────────────
HttpResponse list_filieres(const HttpRequest& req, Database& db) {
    auto filieres = db.list_filieres();
    std::ostringstream json;
    json << "{\"success\":true,\"filieres\":[";
    for (size_t i = 0; i < filieres.size(); i++) {
        if (i > 0) json << ",";
        auto& f = filieres[i];
        json << "{"
             << utils::json_num("id",      f.id)            << ","
             << utils::json_str("nom",     f.nom)            << ","
             << utils::json_str("niveau",  f.niveau)         << ","
             << utils::json_num("universite_id", f.universite_id)
             << "}";
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// GET /api/frauds
// ──────────────────────────────────────────────────────────────
HttpResponse list_frauds(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    auto frauds = db.list_fraudes(100);

    std::ostringstream json;
    json << "{\"success\":true,\"frauds\":[";
    for (size_t i = 0; i < frauds.size(); i++) {
        if (i > 0) json << ",";
        auto& f = frauds[i];
        json << "{"
             << utils::json_num("id",            f.id)           << ","
             << utils::json_num("session_id",    f.session_id)   << ","
             << utils::json_str("etudiant_nom",  f.etudiant_nom) << ","
             << utils::json_str("type_fraude",   f.type_fraude)  << ","
             << utils::json_str("description",   f.description)  << ","
             << utils::json_str("ip_client",     f.ip_client)    << ","
             << utils::json_num("horodatage",    f.horodatage)   << ","
             << utils::json_str("horodatage_fmt",utils::format_datetime(f.horodatage))
             << "}";
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// GET /api/reports/session/:id — Rapport d'une session
// ──────────────────────────────────────────────────────────────
HttpResponse report_session(const HttpRequest& req, Database& db) {
    JWTClaims claims = get_claims(req);
    if (!claims.valid) return HttpResponse::error(401, "Non authentifié");

    int sid = 0;
    auto it = req.params.find("id");
    if (it != req.params.end()) sid = std::stoi(it->second);

    SessionCours s = db.find_session_by_id(sid);
    if (s.id == 0) return HttpResponse::error(404, "Session introuvable");

    auto presences = db.list_presences(sid);
    int total_filiere = db.count_etudiants(s.filiere_id);

    std::ostringstream json;
    json << "{"
         << "\"success\":true,"
         << "\"session\":" << session_to_json(s) << ","
         << "\"total_etudiants_filiere\":" << total_filiere << ","
         << "\"total_presents\":" << presences.size() << ","
         << "\"taux_presence\":" << (total_filiere > 0 ? (presences.size() * 100 / total_filiere) : 0) << ","
         << "\"presences\":[";
    for (size_t i = 0; i < presences.size(); i++) {
        if (i > 0) json << ",";
        auto& p = presences[i];
        json << "{"
             << utils::json_str("etudiant_nom",  p.etudiant_nom)        << ","
             << utils::json_str("matricule",     p.etudiant_matricule)   << ","
             << utils::json_str("horodatage",    utils::format_datetime(p.horodatage)) << ","
             << utils::json_str("ip_client",     p.ip_client)
             << "}";
    }
    json << "]}";
    return HttpResponse::ok(json.str());
}

// ──────────────────────────────────────────────────────────────
// GET /api/events — Events polling (remplace SSE sur Railway)
// ──────────────────────────────────────────────────────────────
HttpResponse get_events_poll(const HttpRequest& req, Database& db) {
    int last_id = 0;
    auto it = req.query.find("last_id");
    if (it != req.query.end() && !it->second.empty()) {
        try {
            last_id = std::stoi(it->second);
        } catch(...) {}
    }

    auto events = SSEManager::instance().get_recent_events(last_id);

    std::ostringstream json;
    json << "{\"success\":true,\"events\":[";
    for (size_t i = 0; i < events.size(); i++) {
        if (i > 0) json << ",";
        json << "{"
             << "\"id\":" << events[i].id << ","
             << "\"event\":\"" << utils::trim(events[i].event) << "\","
             // data contient déjà du JSON, on l'incorpore tel quel
             << "\"data\":" << events[i].data 
             << "}";
    }
    json << "]}";

    return HttpResponse::ok(json.str());
}

} // namespace handlers
