// ============================================================
// database.h — Interface couche base de données SQLite
// SMARTPRESENCE CI — Gestion de présences universitaires
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../libs/sqlite3.h"

// ─────────────────────────────────────────
// Structures de données
// ─────────────────────────────────────────

struct Utilisateur {
    int    id        = 0;
    std::string email;
    std::string nom;
    std::string prenom;
    std::string role;          // "admin", "enseignant", "etudiant"
    std::string password_hash;
    int    universite_id = 0;
    bool   actif = true;
    long long created_at = 0;
};

struct Etudiant {
    int    id         = 0;
    std::string matricule;
    std::string nom;
    std::string prenom;
    std::string email;
    std::string code_personnel; // Code PIN 6 chiffres
    int    filiere_id = 0;
    std::string filiere_nom;
    int    universite_id = 0;
    long long created_at = 0;
};

struct Filiere {
    int    id = 0;
    std::string nom;
    std::string niveau;
    int    universite_id = 0;
    std::string universite_nom;
};

struct SessionCours {
    int    id         = 0;
    std::string titre;
    std::string matiere;
    int    enseignant_id = 0;
    std::string enseignant_nom;
    int    filiere_id = 0;
    std::string filiere_nom;
    long long date_debut  = 0;
    long long date_fin    = 0;
    std::string statut;        // "ouverte", "fermee"
    std::string qr_token;
    long long qr_expire_at = 0;
    int    presences_count = 0;
};

struct Presence {
    int    id           = 0;
    int    session_id   = 0;
    int    etudiant_id  = 0;
    std::string etudiant_nom;
    std::string etudiant_matricule;
    long long horodatage = 0;
    std::string ip_client;
    bool   valide = true;
};

struct FraudeLog {
    int    id          = 0;
    int    session_id  = 0;
    int    etudiant_id = 0;
    std::string etudiant_nom;
    std::string type_fraude;    // nom du bouclier déclenché
    std::string description;
    std::string ip_client;
    long long horodatage = 0;
};

struct DashboardStats {
    int total_etudiants    = 0;
    int total_sessions     = 0;
    int sessions_ouvertes  = 0;
    int presences_today    = 0;
    int tentatives_fraude  = 0;
    int total_presences    = 0;
    double taux_presence   = 0.0;
};

// ─────────────────────────────────────────
// Classe Database
// ─────────────────────────────────────────
class Database {
public:
    Database();
    ~Database();

    // Initialisation
    bool open(const std::string& path);
    void create_schema();
    bool is_open() const { return db_ != nullptr; }

    bool exec(const std::string& sql);

    // ── Utilisateurs ──
    bool          create_utilisateur(const Utilisateur& u);
    Utilisateur   find_user_by_email(const std::string& email);
    Utilisateur   find_user_by_id(int id);
    std::vector<Utilisateur> list_enseignants(int universite_id = 0);

    // ── Étudiants ──
    bool         create_etudiant(const Etudiant& e);
    Etudiant     find_etudiant_by_id(int id);
    Etudiant     find_etudiant_by_matricule(const std::string& matricule);
    std::vector<Etudiant> list_etudiants(int filiere_id = 0, int page = 1, int per_page = 50);
    int          count_etudiants(int filiere_id = 0);

    // ── Filières ──
    bool         create_filiere(const Filiere& f);
    std::vector<Filiere> list_filieres(int universite_id = 0);

    // ── Sessions ──
    int           create_session(const SessionCours& s);
    SessionCours  find_session_by_id(int id);
    bool          update_session_statut(int id, const std::string& statut);
    bool          update_qr_token(int session_id, const std::string& token, long long expire_at);
    std::vector<SessionCours> list_sessions(int enseignant_id = 0, int limit = 20);

    // ── Présences ──
    bool         mark_presence(const Presence& p);
    bool         has_presence(int session_id, int etudiant_id);
    std::vector<Presence> list_presences(int session_id);
    int          count_presences_today();
    int          count_presences_session(int session_id);

    // ── Fraudes ──
    bool         log_fraude(const FraudeLog& f);
    std::vector<FraudeLog> list_fraudes(int limit = 50);
    int          count_fraudes_by_ip(const std::string& ip, long long since_ts);

    // ── Stats Dashboard ──
    DashboardStats get_dashboard_stats(int enseignant_id = 0);

    // ── QR Token ──
    std::string   get_current_qr_token(int session_id);

private:
    sqlite3* db_ = nullptr;

    // Exécuter requête sans résultat

    // Callback pour queries avec résultat
    using Row = std::map<std::string, std::string>;
    std::vector<Row> query(const std::string& sql);

    // Préparer une requête avec binding
    sqlite3_stmt* prepare(const std::string& sql);
};
