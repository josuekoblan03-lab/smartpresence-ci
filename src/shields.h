// ============================================================
// shields.h — 6 Boucliers Anti-Fraude SMARTPRESENCE CI
// ============================================================
#pragma once
#include <string>
#include "database.h"

// Résultat d'une vérification anti-fraude
struct ShieldResult {
    bool        passed      = true;    // true = OK, false = fraude détectée
    std::string shield_name;           // Nom du bouclier déclenché
    std::string message;               // Message explicatif
};

// ─────────────────────────────────────────
// Données de la tentative de marquage
// ─────────────────────────────────────────
struct PresenceAttempt {
    int         session_id;
    int         etudiant_id;
    std::string qr_token;          // Token scanné
    std::string code_personnel;    // Code PIN de l'étudiant
    std::string ip_client;
    long long   timestamp;
};

namespace shields {

// ─────────────────────────────────────────
// 6 Boucliers individuels
// ─────────────────────────────────────────

// Bouclier 1 : Token QR valide et non expiré
ShieldResult check_qr_token(const PresenceAttempt& attempt,
                             const SessionCours& session,
                             Database& db);

// Bouclier 2 : Fenêtre temporelle (±5 min de date_debut)
ShieldResult check_time_window(const PresenceAttempt& attempt,
                                const SessionCours& session);

// Bouclier 3 : Code personnel correct
ShieldResult check_code_personnel(const PresenceAttempt& attempt,
                                   const Etudiant& etudiant);

// Bouclier 4 : Pas de double-scan
ShieldResult check_no_duplicate(const PresenceAttempt& attempt,
                                 Database& db);

// Bouclier 5 : Session ouverte
ShieldResult check_session_open(const SessionCours& session);

// Bouclier 6 : Rate limiting (max 5 tentatives/minute par IP)
ShieldResult check_rate_limit(const PresenceAttempt& attempt,
                               Database& db);

// ─────────────────────────────────────────
// Vérification globale : passe tous les boucliers
// ─────────────────────────────────────────
ShieldResult verify_all(const PresenceAttempt& attempt,
                         const SessionCours& session,
                         const Etudiant& etudiant,
                         Database& db);

} // namespace shields
