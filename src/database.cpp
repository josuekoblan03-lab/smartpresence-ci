// ============================================================
// database.cpp — Implémentation couche SQLite
// SMARTPRESENCE CI
// ============================================================
#include "database.h"
#include "utils.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

// ──────────────────────────────────────────────────────────────
// Constructeur / Destructeur
// ──────────────────────────────────────────────────────────────
Database::Database() : db_(nullptr) {}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────
// Ouverture + création du schéma
// ──────────────────────────────────────────────────────────────
bool Database::open(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] Erreur ouverture: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    // Activer les clés étrangères et WAL pour les performances
    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA journal_mode = WAL;");
    std::cout << "[DB] Base ouverte : " << path << std::endl;
    return true;
}

// Créer toutes les tables si elles n'existent pas
void Database::create_schema() {
    // ── Universités ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS universites (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            nom        TEXT NOT NULL,
            ville      TEXT,
            created_at INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    // ── Filières ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS filieres (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            nom           TEXT NOT NULL,
            niveau        TEXT NOT NULL,
            universite_id INTEGER REFERENCES universites(id),
            created_at    INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    // ── Utilisateurs (admin + enseignants) ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS utilisateurs (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            email         TEXT UNIQUE NOT NULL,
            nom           TEXT NOT NULL,
            prenom        TEXT NOT NULL,
            role          TEXT NOT NULL CHECK(role IN ('admin','enseignant')),
            password_hash TEXT NOT NULL,
            universite_id INTEGER REFERENCES universites(id),
            actif         INTEGER DEFAULT 1,
            created_at    INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    // ── Étudiants ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS etudiants (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            matricule      TEXT UNIQUE NOT NULL,
            nom            TEXT NOT NULL,
            prenom         TEXT NOT NULL,
            email          TEXT,
            code_personnel TEXT NOT NULL,
            filiere_id     INTEGER REFERENCES filieres(id),
            universite_id  INTEGER REFERENCES universites(id),
            created_at     INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    // ── Sessions de cours ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS sessions_cours (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            titre          TEXT NOT NULL,
            matiere        TEXT NOT NULL,
            enseignant_id  INTEGER REFERENCES utilisateurs(id),
            filiere_id     INTEGER REFERENCES filieres(id),
            date_debut     INTEGER NOT NULL,
            date_fin       INTEGER,
            statut         TEXT DEFAULT 'ouverte' CHECK(statut IN ('ouverte','fermee')),
            qr_token       TEXT,
            qr_expire_at   INTEGER DEFAULT 0,
            created_at     INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    // ── Présences ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS presences (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id  INTEGER REFERENCES sessions_cours(id),
            etudiant_id INTEGER REFERENCES etudiants(id),
            horodatage  INTEGER DEFAULT (strftime('%s','now')),
            ip_client   TEXT,
            valide      INTEGER DEFAULT 1,
            UNIQUE(session_id, etudiant_id)
        );
    )");

    // ── Logs de fraude ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS fraude_logs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id  INTEGER REFERENCES sessions_cours(id),
            etudiant_id INTEGER,
            type_fraude TEXT NOT NULL,
            description TEXT,
            ip_client   TEXT,
            horodatage  INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    // ── Rate limiting (tentatives par IP) ──
    exec(R"(
        CREATE TABLE IF NOT EXISTS rate_limits (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            ip         TEXT NOT NULL,
            endpoint   TEXT,
            horodatage INTEGER DEFAULT (strftime('%s','now'))
        );
    )");

    std::cout << "[DB] Schema créé avec succès." << std::endl;
}

// ──────────────────────────────────────────────────────────────
// Utilitaires privés
// ──────────────────────────────────────────────────────────────
bool Database::exec(const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] Erreur exec: " << (errmsg ? errmsg : "?") << " | SQL: " << sql.substr(0,80) << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

std::vector<Database::Row> Database::query(const std::string& sql) {
    std::vector<Row> results;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[DB] Erreur prepare: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        for (int i = 0; i < cols; i++) {
            const char* name = sqlite3_column_name(stmt, i);
            const char* val  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[name ? name : ""] = val ? val : "";
        }
        results.push_back(row);
    }
    sqlite3_finalize(stmt);
    return results;
}

sqlite3_stmt* Database::prepare(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    return stmt;
}

// ──────────────────────────────────────────────────────────────
// UTILISATEURS
// ──────────────────────────────────────────────────────────────
bool Database::create_utilisateur(const Utilisateur& u) {
    sqlite3_stmt* stmt = prepare(
        "INSERT OR IGNORE INTO utilisateurs (email, nom, prenom, role, password_hash, universite_id)"
        " VALUES (?, ?, ?, ?, ?, ?)"
    );
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, u.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, u.nom.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, u.prenom.c_str(),-1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, u.role.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, u.password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  6, u.universite_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

Utilisateur Database::find_user_by_email(const std::string& email) {
    Utilisateur u;
    sqlite3_stmt* stmt = prepare(
        "SELECT id, email, nom, prenom, role, password_hash, universite_id, actif"
        " FROM utilisateurs WHERE email = ? LIMIT 1"
    );
    if (!stmt) return u;
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u.id           = sqlite3_column_int(stmt, 0);
        const char* v;
        v = (const char*)sqlite3_column_text(stmt, 1); u.email = v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 2); u.nom   = v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 3); u.prenom= v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 4); u.role  = v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 5); u.password_hash = v ? v : "";
        u.universite_id = sqlite3_column_int(stmt, 6);
        u.actif         = sqlite3_column_int(stmt, 7) != 0;
    }
    sqlite3_finalize(stmt);
    return u;
}

Utilisateur Database::find_user_by_id(int id) {
    Utilisateur u;
    sqlite3_stmt* stmt = prepare(
        "SELECT id, email, nom, prenom, role, universite_id, actif"
        " FROM utilisateurs WHERE id = ? LIMIT 1"
    );
    if (!stmt) return u;
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u.id           = sqlite3_column_int(stmt, 0);
        const char* v;
        v = (const char*)sqlite3_column_text(stmt, 1); u.email  = v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 2); u.nom    = v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 3); u.prenom = v ? v : "";
        v = (const char*)sqlite3_column_text(stmt, 4); u.role   = v ? v : "";
        u.universite_id = sqlite3_column_int(stmt, 5);
        u.actif         = sqlite3_column_int(stmt, 6) != 0;
    }
    sqlite3_finalize(stmt);
    return u;
}

std::vector<Utilisateur> Database::list_enseignants(int universite_id) {
    std::vector<Utilisateur> liste;
    std::string sql = "SELECT id, email, nom, prenom, role, universite_id, actif, created_at "
                      "FROM utilisateurs WHERE role = 'enseignant'";
    if (universite_id > 0) {
        sql += " AND universite_id = " + std::to_string(universite_id);
    }
    sql += " ORDER BY nom ASC";

    auto rows = query(sql);
    for (auto& r : rows) {
        Utilisateur u;
        u.id = std::stoi(r["id"]);
        u.email = r["email"];
        u.nom = r["nom"];
        u.prenom = r["prenom"];
        u.role = r["role"];
        u.universite_id = std::stoi(r["universite_id"]);
        u.actif = std::stoi(r["actif"]) != 0;
        u.created_at = std::stoll(r["created_at"]);
        liste.push_back(u);
    }
    return liste;
}

// ──────────────────────────────────────────────────────────────
// FILIÈRES
// ──────────────────────────────────────────────────────────────
bool Database::create_filiere(const Filiere& f) {
    sqlite3_stmt* stmt = prepare(
        "INSERT OR IGNORE INTO filieres (nom, niveau, universite_id) VALUES (?, ?, ?)"
    );
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, f.nom.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, f.niveau.c_str(),-1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  3, f.universite_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Filiere> Database::list_filieres(int universite_id) {
    std::vector<Filiere> list;
    std::string sql =
        "SELECT f.id, f.nom, f.niveau, f.universite_id, u.nom as univ_nom"
        " FROM filieres f LEFT JOIN universites u ON u.id = f.universite_id";
    if (universite_id > 0)
        sql += " WHERE f.universite_id = " + std::to_string(universite_id);
    auto rows = query(sql);
    for (auto& r : rows) {
        Filiere f;
        f.id            = std::stoi(r["id"]);
        f.nom           = r["nom"];
        f.niveau        = r["niveau"];
        f.universite_id = std::stoi(r["universite_id"]);
        f.universite_nom= r["univ_nom"];
        list.push_back(f);
    }
    return list;
}

// ──────────────────────────────────────────────────────────────
// ÉTUDIANTS
// ──────────────────────────────────────────────────────────────
bool Database::create_etudiant(const Etudiant& e) {
    sqlite3_stmt* stmt = prepare(
        "INSERT OR IGNORE INTO etudiants"
        " (matricule, nom, prenom, email, code_personnel, filiere_id, universite_id)"
        " VALUES (?, ?, ?, ?, ?, ?, ?)"
    );
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, e.matricule.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, e.nom.c_str(),             -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, e.prenom.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, e.email.c_str(),           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, e.code_personnel.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  6, e.filiere_id);
    sqlite3_bind_int(stmt,  7, e.universite_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

Etudiant Database::find_etudiant_by_id(int id) {
    Etudiant e;
    auto rows = query(
        "SELECT e.id, e.matricule, e.nom, e.prenom, e.email, e.code_personnel,"
        "       e.filiere_id, e.universite_id, f.nom as filiere_nom"
        " FROM etudiants e LEFT JOIN filieres f ON f.id = e.filiere_id"
        " WHERE e.id = " + std::to_string(id) + " LIMIT 1"
    );
    if (!rows.empty()) {
        auto& r = rows[0];
        e.id             = std::stoi(r["id"]);
        e.matricule      = r["matricule"];
        e.nom            = r["nom"];
        e.prenom         = r["prenom"];
        e.email          = r["email"];
        e.code_personnel = r["code_personnel"];
        e.filiere_id     = std::stoi(r["filiere_id"]);
        e.universite_id  = std::stoi(r["universite_id"]);
        e.filiere_nom    = r["filiere_nom"];
    }
    return e;
}

Etudiant Database::find_etudiant_by_matricule(const std::string& matricule) {
    Etudiant e;
    sqlite3_stmt* stmt = prepare(
        "SELECT e.id, e.matricule, e.nom, e.prenom, e.email, e.code_personnel,"
        "       e.filiere_id, e.universite_id, f.nom as filiere_nom"
        " FROM etudiants e LEFT JOIN filieres f ON f.id = e.filiere_id"
        " WHERE e.matricule = ? LIMIT 1"
    );
    if (!stmt) return e;
    sqlite3_bind_text(stmt, 1, matricule.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto col = [&](int i) -> std::string {
            const char* v = (const char*)sqlite3_column_text(stmt, i);
            return v ? v : "";
        };
        e.id             = sqlite3_column_int(stmt, 0);
        e.matricule      = col(1);
        e.nom            = col(2);
        e.prenom         = col(3);
        e.email          = col(4);
        e.code_personnel = col(5);
        e.filiere_id     = sqlite3_column_int(stmt, 6);
        e.universite_id  = sqlite3_column_int(stmt, 7);
        e.filiere_nom    = col(8);
    }
    sqlite3_finalize(stmt);
    return e;
}

std::vector<Etudiant> Database::list_etudiants(int filiere_id, int page, int per_page) {
    std::vector<Etudiant> list;
    std::string sql =
        "SELECT e.id, e.matricule, e.nom, e.prenom, e.email,"
        "       e.code_personnel, e.filiere_id, e.universite_id, f.nom as filiere_nom"
        " FROM etudiants e LEFT JOIN filieres f ON f.id = e.filiere_id";
    if (filiere_id > 0)
        sql += " WHERE e.filiere_id = " + std::to_string(filiere_id);
    sql += " ORDER BY e.nom, e.prenom";
    sql += " LIMIT " + std::to_string(per_page) +
           " OFFSET " + std::to_string((page - 1) * per_page);
    auto rows = query(sql);
    for (auto& r : rows) {
        Etudiant e;
        e.id             = std::stoi(r["id"]);
        e.matricule      = r["matricule"];
        e.nom            = r["nom"];
        e.prenom         = r["prenom"];
        e.email          = r["email"];
        e.code_personnel = r["code_personnel"];
        e.filiere_id     = !r["filiere_id"].empty() ? std::stoi(r["filiere_id"]) : 0;
        e.universite_id  = !r["universite_id"].empty() ? std::stoi(r["universite_id"]) : 0;
        e.filiere_nom    = r["filiere_nom"];
        list.push_back(e);
    }
    return list;
}

int Database::count_etudiants(int filiere_id) {
    std::string sql = "SELECT COUNT(*) as c FROM etudiants";
    if (filiere_id > 0) sql += " WHERE filiere_id = " + std::to_string(filiere_id);
    auto rows = query(sql);
    if (!rows.empty() && !rows[0]["c"].empty()) return std::stoi(rows[0]["c"]);
    return 0;
}

// ──────────────────────────────────────────────────────────────
// SESSIONS
// ──────────────────────────────────────────────────────────────
int Database::create_session(const SessionCours& s) {
    sqlite3_stmt* stmt = prepare(
        "INSERT INTO sessions_cours"
        " (titre, matiere, enseignant_id, filiere_id, date_debut, statut, qr_token, qr_expire_at)"
        " VALUES (?, ?, ?, ?, ?, 'ouverte', ?, ?)"
    );
    if (!stmt) return -1;
    sqlite3_bind_text(stmt, 1, s.titre.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.matiere.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  3, s.enseignant_id);
    sqlite3_bind_int(stmt,  4, s.filiere_id);
    sqlite3_bind_int64(stmt,5, s.date_debut);
    sqlite3_bind_text(stmt, 6, s.qr_token.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,7, s.qr_expire_at);
    int new_id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) new_id = (int)sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return new_id;
}

SessionCours Database::find_session_by_id(int id) {
    SessionCours s;
    auto rows = query(
        "SELECT sc.id, sc.titre, sc.matiere, sc.enseignant_id, sc.filiere_id,"
        "       sc.date_debut, sc.date_fin, sc.statut, sc.qr_token, sc.qr_expire_at,"
        "       u.nom || ' ' || u.prenom as enseignant_nom,"
        "       f.nom as filiere_nom,"
        "       (SELECT COUNT(*) FROM presences WHERE session_id = sc.id AND valide = 1) as presences_count"
        " FROM sessions_cours sc"
        " LEFT JOIN utilisateurs u ON u.id = sc.enseignant_id"
        " LEFT JOIN filieres f ON f.id = sc.filiere_id"
        " WHERE sc.id = " + std::to_string(id) + " LIMIT 1"
    );
    if (!rows.empty()) {
        auto& r = rows[0];
        s.id            = std::stoi(r["id"]);
        s.titre         = r["titre"];
        s.matiere       = r["matiere"];
        s.enseignant_id = !r["enseignant_id"].empty() ? std::stoi(r["enseignant_id"]) : 0;
        s.filiere_id    = !r["filiere_id"].empty() ? std::stoi(r["filiere_id"]) : 0;
        s.date_debut    = !r["date_debut"].empty() ? std::stoll(r["date_debut"]) : 0;
        s.date_fin      = !r["date_fin"].empty() ? std::stoll(r["date_fin"]) : 0;
        s.statut        = r["statut"];
        s.qr_token      = r["qr_token"];
        s.qr_expire_at  = !r["qr_expire_at"].empty() ? std::stoll(r["qr_expire_at"]) : 0;
        s.enseignant_nom= r["enseignant_nom"];
        s.filiere_nom   = r["filiere_nom"];
        s.presences_count = !r["presences_count"].empty() ? std::stoi(r["presences_count"]) : 0;
    }
    return s;
}

bool Database::update_session_statut(int id, const std::string& statut) {
    sqlite3_stmt* stmt = prepare(
        "UPDATE sessions_cours SET statut = ?, date_fin = ? WHERE id = ?"
    );
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, statut.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, utils::now_unix());
    sqlite3_bind_int(stmt,  3, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::update_qr_token(int session_id, const std::string& token, long long expire_at) {
    sqlite3_stmt* stmt = prepare(
        "UPDATE sessions_cours SET qr_token = ?, qr_expire_at = ? WHERE id = ?"
    );
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, expire_at);
    sqlite3_bind_int(stmt,  3, session_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<SessionCours> Database::list_sessions(int enseignant_id, int limit) {
    std::vector<SessionCours> list;
    std::string sql =
        "SELECT sc.id, sc.titre, sc.matiere, sc.enseignant_id, sc.filiere_id,"
        "       sc.date_debut, sc.date_fin, sc.statut, sc.qr_token, sc.qr_expire_at,"
        "       u.nom || ' ' || u.prenom as enseignant_nom,"
        "       f.nom as filiere_nom,"
        "       (SELECT COUNT(*) FROM presences WHERE session_id=sc.id AND valide=1) as presences_count"
        " FROM sessions_cours sc"
        " LEFT JOIN utilisateurs u ON u.id = sc.enseignant_id"
        " LEFT JOIN filieres f ON f.id = sc.filiere_id";
    if (enseignant_id > 0)
        sql += " WHERE sc.enseignant_id = " + std::to_string(enseignant_id);
    sql += " ORDER BY sc.date_debut DESC LIMIT " + std::to_string(limit);

    auto rows = query(sql);
    for (auto& r : rows) {
        SessionCours s;
        s.id            = std::stoi(r["id"]);
        s.titre         = r["titre"];
        s.matiere       = r["matiere"];
        s.enseignant_id = !r["enseignant_id"].empty() ? std::stoi(r["enseignant_id"]) : 0;
        s.filiere_id    = !r["filiere_id"].empty() ? std::stoi(r["filiere_id"]) : 0;
        s.date_debut    = !r["date_debut"].empty() ? std::stoll(r["date_debut"]) : 0;
        s.date_fin      = !r["date_fin"].empty() ? std::stoll(r["date_fin"]) : 0;
        s.statut        = r["statut"];
        s.qr_token      = r["qr_token"];
        s.qr_expire_at  = !r["qr_expire_at"].empty() ? std::stoll(r["qr_expire_at"]) : 0;
        s.enseignant_nom= r["enseignant_nom"];
        s.filiere_nom   = r["filiere_nom"];
        s.presences_count = !r["presences_count"].empty() ? std::stoi(r["presences_count"]) : 0;
        list.push_back(s);
    }
    return list;
}

// ──────────────────────────────────────────────────────────────
// PRÉSENCES
// ──────────────────────────────────────────────────────────────
bool Database::mark_presence(const Presence& p) {
    sqlite3_stmt* stmt = prepare(
        "INSERT OR IGNORE INTO presences (session_id, etudiant_id, horodatage, ip_client, valide)"
        " VALUES (?, ?, ?, ?, ?)"
    );
    if (!stmt) return false;
    sqlite3_bind_int(stmt,  1, p.session_id);
    sqlite3_bind_int(stmt,  2, p.etudiant_id);
    sqlite3_bind_int64(stmt,3, p.horodatage > 0 ? p.horodatage : utils::now_unix());
    sqlite3_bind_text(stmt, 4, p.ip_client.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  5, p.valide ? 1 : 0);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db_) > 0;
}

bool Database::has_presence(int session_id, int etudiant_id) {
    auto rows = query(
        "SELECT 1 FROM presences"
        " WHERE session_id = " + std::to_string(session_id) +
        " AND etudiant_id = " + std::to_string(etudiant_id) +
        " AND valide = 1 LIMIT 1"
    );
    return !rows.empty();
}

std::vector<Presence> Database::list_presences(int session_id) {
    std::vector<Presence> list;
    auto rows = query(
        "SELECT p.id, p.session_id, p.etudiant_id, p.horodatage, p.ip_client, p.valide,"
        "       e.nom || ' ' || e.prenom as etudiant_nom, e.matricule"
        " FROM presences p LEFT JOIN etudiants e ON e.id = p.etudiant_id"
        " WHERE p.session_id = " + std::to_string(session_id) +
        " ORDER BY p.horodatage ASC"
    );
    for (auto& r : rows) {
        Presence p;
        p.id              = std::stoi(r["id"]);
        p.session_id      = std::stoi(r["session_id"]);
        p.etudiant_id     = std::stoi(r["etudiant_id"]);
        p.horodatage      = std::stoll(r["horodatage"]);
        p.ip_client       = r["ip_client"];
        p.valide          = r["valide"] == "1";
        p.etudiant_nom    = r["etudiant_nom"];
        p.etudiant_matricule = r["matricule"];
        list.push_back(p);
    }
    return list;
}

int Database::count_presences_today() {
    long long start_of_day = utils::now_unix() - (utils::now_unix() % 86400);
    auto rows = query(
        "SELECT COUNT(*) as c FROM presences"
        " WHERE horodatage >= " + std::to_string(start_of_day) +
        " AND valide = 1"
    );
    if (!rows.empty() && !rows[0]["c"].empty()) return std::stoi(rows[0]["c"]);
    return 0;
}

int Database::count_presences_session(int session_id) {
    auto rows = query(
        "SELECT COUNT(*) as c FROM presences"
        " WHERE session_id = " + std::to_string(session_id) + " AND valide = 1"
    );
    if (!rows.empty() && !rows[0]["c"].empty()) return std::stoi(rows[0]["c"]);
    return 0;
}

// ──────────────────────────────────────────────────────────────
// FRAUDES
// ──────────────────────────────────────────────────────────────
bool Database::log_fraude(const FraudeLog& f) {
    sqlite3_stmt* stmt = prepare(
        "INSERT INTO fraude_logs (session_id, etudiant_id, type_fraude, description, ip_client)"
        " VALUES (?, ?, ?, ?, ?)"
    );
    if (!stmt) return false;
    sqlite3_bind_int(stmt,  1, f.session_id);
    sqlite3_bind_int(stmt,  2, f.etudiant_id);
    sqlite3_bind_text(stmt, 3, f.type_fraude.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, f.description.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, f.ip_client.c_str(),    -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<FraudeLog> Database::list_fraudes(int limit) {
    std::vector<FraudeLog> list;
    auto rows = query(
        "SELECT fl.id, fl.session_id, fl.etudiant_id, fl.type_fraude,"
        "       fl.description, fl.ip_client, fl.horodatage,"
        "       e.nom || ' ' || e.prenom as etudiant_nom"
        " FROM fraude_logs fl"
        " LEFT JOIN etudiants e ON e.id = fl.etudiant_id"
        " ORDER BY fl.horodatage DESC LIMIT " + std::to_string(limit)
    );
    for (auto& r : rows) {
        FraudeLog f;
        f.id           = std::stoi(r["id"]);
        f.session_id   = !r["session_id"].empty() ? std::stoi(r["session_id"]) : 0;
        f.etudiant_id  = !r["etudiant_id"].empty() ? std::stoi(r["etudiant_id"]) : 0;
        f.type_fraude  = r["type_fraude"];
        f.description  = r["description"];
        f.ip_client    = r["ip_client"];
        f.horodatage   = !r["horodatage"].empty() ? std::stoll(r["horodatage"]) : 0;
        f.etudiant_nom = r["etudiant_nom"];
        list.push_back(f);
    }
    return list;
}

int Database::count_fraudes_by_ip(const std::string& ip, long long since_ts) {
    auto rows = query(
        "SELECT COUNT(*) as c FROM fraude_logs"
        " WHERE ip_client = '" + ip + "'"
        " AND horodatage >= " + std::to_string(since_ts)
    );
    if (!rows.empty() && !rows[0]["c"].empty()) return std::stoi(rows[0]["c"]);
    return 0;
}

// ──────────────────────────────────────────────────────────────
// DASHBOARD STATS
// ──────────────────────────────────────────────────────────────
DashboardStats Database::get_dashboard_stats(int enseignant_id) {
    DashboardStats stats;
    stats.total_etudiants  = count_etudiants();
    stats.presences_today  = count_presences_today();

    std::string where = enseignant_id > 0
        ? " WHERE enseignant_id = " + std::to_string(enseignant_id) : "";

    auto r1 = query("SELECT COUNT(*) as c FROM sessions_cours" + where);
    if (!r1.empty() && !r1[0]["c"].empty()) stats.total_sessions = std::stoi(r1[0]["c"]);

    auto r2 = query("SELECT COUNT(*) as c FROM sessions_cours WHERE statut='ouverte'" +
                    (enseignant_id > 0 ? " AND enseignant_id=" + std::to_string(enseignant_id) : ""));
    if (!r2.empty() && !r2[0]["c"].empty()) stats.sessions_ouvertes = std::stoi(r2[0]["c"]);

    auto r3 = query("SELECT COUNT(*) as c FROM fraude_logs");
    if (!r3.empty() && !r3[0]["c"].empty()) stats.tentatives_fraude = std::stoi(r3[0]["c"]);

    auto r4 = query("SELECT COUNT(*) as c FROM presences WHERE valide=1");
    if (!r4.empty() && !r4[0]["c"].empty()) stats.total_presences = std::stoi(r4[0]["c"]);

    // Taux de présence moyen
    if (stats.total_sessions > 0 && stats.total_etudiants > 0) {
        auto r5 = query(
            "SELECT AVG(cnt) as avg FROM ("
            "  SELECT COUNT(*) as cnt FROM presences WHERE valide=1 GROUP BY session_id"
            ")"
        );
        if (!r5.empty() && !r5[0]["avg"].empty()) {
            double avg = std::stod(r5[0]["avg"]);
            stats.taux_presence = (avg / stats.total_etudiants) * 100.0;
        }
    }
    return stats;
}

// ──────────────────────────────────────────────────────────────
// QR TOKEN
// ──────────────────────────────────────────────────────────────
std::string Database::get_current_qr_token(int session_id) {
    auto rows = query(
        "SELECT qr_token FROM sessions_cours WHERE id = " + std::to_string(session_id) + " LIMIT 1"
    );
    if (!rows.empty()) return rows[0]["qr_token"];
    return "";
}
