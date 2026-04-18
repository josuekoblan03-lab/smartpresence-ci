// ============================================================
// shields.cpp — Implémentation des 6 Boucliers Anti-Fraude
// SMARTPRESENCE CI
// ============================================================
#include "shields.h"
#include "utils.h"
#include "qrcode.h"
#include <iostream>

namespace shields {

// ──────────────────────────────────────────────────────────────
// Bouclier 1 : Token QR valide et non expiré
// Le token doit correspondre à la session et ne pas être expiré
// ──────────────────────────────────────────────────────────────
ShieldResult check_qr_token(const PresenceAttempt& attempt,
                             const SessionCours& session,
                             Database& db) {
    ShieldResult r;
    r.shield_name = "Bouclier 1 - Token QR";

    // Vérifier que le token correspond à la session courante
    if (attempt.qr_token != session.qr_token) {
        r.passed  = false;
        r.message = "Token QR invalide ou expiré (ne correspond pas à la session)";
        return r;
    }

    // Vérifier la validité temporelle du token
    if (!qrcode::verify_token(attempt.qr_token, attempt.session_id, session.qr_expire_at)) {
        r.passed  = false;
        r.message = "Token QR expiré - scannez le nouveau QR Code";
        return r;
    }

    r.message = "Token QR valide";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 2 : Fenêtre temporelle
// La présence doit être marquée dans les 2h après date_debut
// et la session doit être ouverte
// ──────────────────────────────────────────────────────────────
ShieldResult check_time_window(const PresenceAttempt& attempt,
                                const SessionCours& session) {
    ShieldResult r;
    r.shield_name = "Bouclier 2 - Fenêtre Temporelle";

    long long now = utils::now_unix();
    // Fenêtre : de 5 minutes avant jusqu'à 2h après le début
    long long window_start = session.date_debut - (5 * 60);
    long long window_end   = session.date_debut + (2 * 3600);

    if (now < window_start) {
        r.passed  = false;
        r.message = "La session n'a pas encore commencé";
        return r;
    }

    if (now > window_end) {
        r.passed  = false;
        r.message = "Délai de présence dépassé (plus de 2h après le début)";
        return r;
    }

    r.message = "Présence dans la fenêtre temporelle autorisée";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 3 : Code personnel correct
// Chaque étudiant a un code PIN unique à 6 chiffres
// ──────────────────────────────────────────────────────────────
ShieldResult check_code_personnel(const PresenceAttempt& attempt,
                                   const Etudiant& etudiant) {
    ShieldResult r;
    r.shield_name = "Bouclier 3 - Code Personnel";

    if (etudiant.id == 0) {
        r.passed  = false;
        r.message = "Étudiant introuvable";
        return r;
    }

    if (attempt.code_personnel != etudiant.code_personnel) {
        r.passed  = false;
        r.message = "Code personnel incorrect";
        return r;
    }

    r.message = "Code personnel vérifié";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 4 : Anti double-scan
// Un étudiant ne peut marquer sa présence qu'une seule fois par session
// ──────────────────────────────────────────────────────────────
ShieldResult check_no_duplicate(const PresenceAttempt& attempt,
                                 Database& db) {
    ShieldResult r;
    r.shield_name = "Bouclier 4 - Anti Double-Scan";

    if (db.has_presence(attempt.session_id, attempt.etudiant_id)) {
        r.passed  = false;
        r.message = "Présence déjà enregistrée pour cette session";
        return r;
    }

    r.message = "Aucun double-scan détecté";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 5 : Session ouverte
// On ne peut marquer une présence que sur une session ouverte
// ──────────────────────────────────────────────────────────────
ShieldResult check_session_open(const SessionCours& session) {
    ShieldResult r;
    r.shield_name = "Bouclier 5 - Session Ouverte";

    if (session.id == 0) {
        r.passed  = false;
        r.message = "Session introuvable";
        return r;
    }

    if (session.statut != "ouverte") {
        r.passed  = false;
        r.message = "La session est fermée - présence refusée";
        return r;
    }

    r.message = "Session ouverte et active";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 6 : Rate limiting
// Maximum 5 tentatives par IP par minute
// ──────────────────────────────────────────────────────────────
ShieldResult check_rate_limit(const PresenceAttempt& attempt,
                               Database& db) {
    ShieldResult r;
    r.shield_name = "Bouclier 6 - Rate Limiting";

    // Compter les tentatives de fraude dans la dernière minute
    long long since = utils::now_unix() - 60;
    int attempts = db.count_fraudes_by_ip(attempt.ip_client, since);

    if (attempts >= 5) {
        r.passed  = false;
        r.message = "Trop de tentatives depuis cette IP (" +
                    attempt.ip_client + ") - Accès bloqué temporairement";
        return r;
    }

    r.message = "Taux de tentatives OK (" + std::to_string(attempts) + "/5 par minute)";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 7 : Unicité de l'appareil (Empreinte numérique)
// Un appareil ne peut scripter une présence que pour UN seul étudiant par session
// ──────────────────────────────────────────────────────────────
ShieldResult check_device_uniqueness(const PresenceAttempt& attempt,
                                      Database& db) {
    ShieldResult r;
    r.shield_name = "Bouclier 7 - Unicité Matérielle";

    // Si on n'a pas de device_id (navigateur sans cache) on bloque immédiatement.
    // Cela force le téléphone à re-télécharger le nouveau script scan.js?v=2
    if (attempt.device_id.empty()) {
        r.passed  = false;
        r.message = "Fraude: Traceur de sécurité manquant. (Veuillez rafraîchir la page ou vider le cache)";
        return r;
    }

    // Vérifier si ce device_id a déjà été utilisé POUR UN AUTRE étudiant dans cette session.
    // has_device_voted() renvoie true SI et SEULEMENT SI le device a voté (peu importe l'étudiant).
    // Mais si l'étudiant courant EST celui qui a voté avec ce device, l'anti-double-scan (Bouclier 4)
    // s'en chargera. Donc on doit bloquer si l'empreinte existe ET n'appartient pas à la tentative actuelle ?
    // Simplification : Bouclier 4 bloque le double-scan de l'étudiant.
    // Si Bouclier 4 n'a pas déclenché, c'est que l'étudiant courant n'a pas de présence.
    // Si le device a une présence, c'est que le device a scanné pour qq d'autre !
    if (db.has_device_voted(attempt.session_id, attempt.device_id)) {
        r.passed  = false;
        r.message = "Fraude: Ce téléphone a déjà validé la présence d'un autre étudiant durant cette session. L'empreinte matérielle (HW_) de cet appareil est blacklistée.";
        return r;
    }

    r.message = "Empreinte numérique unique (Autorisée)";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 8 : Déplacement interdit pendant le scan (Anti-Triche par relais)
// ──────────────────────────────────────────────────────────────
ShieldResult check_gps_stationary(const PresenceAttempt& attempt) {
    ShieldResult r;
    r.shield_name = "Bouclier 8 - Stationnement GPS";

    // Si les GPS sont à zéro, c'est que le client a bloqué ou échoué à envoyer le GPS
    // ou n'a pas pu obtenir la haute précision.
    if (attempt.gps_start_lat == 0.0 && attempt.gps_start_lng == 0.0) {
        r.passed  = false;
        r.message = "Localisation GPS de départ introuvable. Activez votre GPS et autorisez le navigateur.";
        return r;
    }
    if (attempt.gps_end_lat == 0.0 && attempt.gps_end_lng == 0.0) {
        r.passed  = false;
        r.message = "Localisation GPS de fin introuvable. La vérification de déplacement a échoué.";
        return r;
    }

    double dist = utils::haversine_distance(
        attempt.gps_start_lat, attempt.gps_start_lng,
        attempt.gps_end_lat, attempt.gps_end_lng
    );

    // Marge de 10 mètres pour tolérer les erreurs GPS et le bruit atmosphérique
    if (dist > 10.0) {
        r.passed  = false;
        r.message = "Déplacement suspect détecté pendant la vérification (" + std::to_string(dist) + "m). Vous devez rester immobile.";
        return r;
    }

    r.message = "Aucun déplacement suspect détecté (" + std::to_string(dist) + "m)";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Bouclier 9 : Biométrie Faciale (Anti-Copie / Anti-Double visage)
// ──────────────────────────────────────────────────────────────
ShieldResult check_facial_biometrics(const PresenceAttempt& attempt,
                                     Etudiant& etudiant,
                                     Database& db) {
    ShieldResult r;
    r.shield_name = "Bouclier 9 - Biométrie Faciale";

    if (attempt.face_descriptor.empty() || attempt.face_descriptor == "CNVS_ERR") {
        r.passed  = false;
        r.message = "Empreinte faciale absente. Impossible de valider la présence.";
        return r;
    }

    // 1. Vérifier si ce visage a DÉJÀ été utilisé par un AUTRE étudiant dans cette session
    std::vector<Presence> presences = db.list_presences(attempt.session_id);
    for (const auto& p : presences) {
        if (p.etudiant_id != attempt.etudiant_id && !p.face_descriptor.empty()) {
            double dist = utils::face_distance(attempt.face_descriptor, p.face_descriptor);
            if (dist < 0.55) { // Seuil usuel ArcFace (Cosinus > 0.45)
                r.passed  = false;
                r.message = "Fraude absolue: Ce visage a DÉJÀ été validé pour un autre étudiant lors de cette session !";
                return r;
            }
        }
    }

    // 2. Vérifier l'identité de l'étudiant
    if (!etudiant.face_descriptor.empty() && etudiant.face_descriptor.size() > 10) {
        // Profil biométrique existant ! Comparer le visage soumis avec le profil enregistré.
        double dist = utils::face_distance(attempt.face_descriptor, etudiant.face_descriptor);
        if (dist > 0.55) {
            r.passed  = false;
            r.message = "Échec biométrique: Le visage ne correspond pas au profil enregistré pour ce matricule. (Distance: " + std::to_string(dist) + ")";
            return r;
        }
    } else {
        // C'est son tout premier scan réussi -> Son profil biométrique est vierge
        // La sauvegarde finale se fera lors de l'insertion en BD, on ne le fait pas ici.
        // Mais on l'autorise.
    }

    r.message = "Identité biométrique confirmée";
    return r;
}

// ──────────────────────────────────────────────────────────────
// Vérification globale : passe tous les boucliers dans l'ordre
// ──────────────────────────────────────────────────────────────
ShieldResult verify_all(const PresenceAttempt& attempt,
                         const SessionCours& session,
                         const Etudiant& etudiant,
                         Database& db) {

    std::cout << "[Shields] Vérification pour étudiant #"
              << attempt.etudiant_id << " session #" << attempt.session_id << std::endl;

    // Bouclier 6 en premier (rate limiting) pour économiser les ressources
    ShieldResult r6 = check_rate_limit(attempt, db);
    if (!r6.passed) { std::cout << "[Shields] ❌ " << r6.shield_name << std::endl; return r6; }

    // Bouclier 5 : session ouverte
    ShieldResult r5 = check_session_open(session);
    if (!r5.passed) { std::cout << "[Shields] ❌ " << r5.shield_name << std::endl; return r5; }

    // Bouclier 1 : token QR valide
    ShieldResult r1 = check_qr_token(attempt, session, db);
    if (!r1.passed) { std::cout << "[Shields] ❌ " << r1.shield_name << std::endl; return r1; }

    // Bouclier 2 : fenêtre temporelle
    ShieldResult r2 = check_time_window(attempt, session);
    if (!r2.passed) { std::cout << "[Shields] ❌ " << r2.shield_name << std::endl; return r2; }

    // Bouclier 3 : code personnel
    ShieldResult r3 = check_code_personnel(attempt, etudiant);
    if (!r3.passed) { std::cout << "[Shields] ❌ " << r3.shield_name << std::endl; return r3; }

    // Bouclier 4 : anti double-scan
    ShieldResult r4 = check_no_duplicate(attempt, db);
    if (!r4.passed) { std::cout << "[Shields] ❌ " << r4.shield_name << std::endl; return r4; }

    // Bouclier 7 : unicité de l'appareil
    ShieldResult r7 = check_device_uniqueness(attempt, db);
    if (!r7.passed) { std::cout << "[Shields] ❌ " << r7.shield_name << std::endl; return r7; }

    // Bouclier 8 : verrouillage stationnaire (GPS)
    ShieldResult r8 = check_gps_stationary(attempt);
    if (!r8.passed) { std::cout << "[Shields] ❌ " << r8.shield_name << std::endl; return r8; }

    // Bouclier 9 : biométrie faciale
    ShieldResult r9 = check_facial_biometrics(attempt, const_cast<Etudiant&>(etudiant), db);
    if (!r9.passed) { std::cout << "[Shields] ❌ " << r9.shield_name << std::endl; return r9; }

    std::cout << "[Shields] ✅ Tous les 9 boucliers validés" << std::endl;

    ShieldResult ok;
    ok.passed  = true;
    ok.message = "Tous les boucliers validés - présence autorisée";
    return ok;
}

} // namespace shields
