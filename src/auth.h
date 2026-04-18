// ============================================================
// auth.h — JWT : génération & validation des tokens
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include <string>

// Clé secrète HMAC (à changer en production !)
static const std::string JWT_SECRET = "SmartPresence_IUA_Secret_2024!";
static const long long   JWT_EXPIRY_SECONDS = 86400; // 24 heures

struct JWTClaims {
    int         user_id  = 0;
    std::string role;
    std::string email;
    std::string nom;
    long long   exp      = 0;
    bool        valid    = false;
};

namespace auth {
    // Créer un JWT signé
    std::string create_token(int user_id, const std::string& role,
                             const std::string& email, const std::string& nom);

    // Valider et décoder un JWT
    JWTClaims   verify_token(const std::string& token);

    // Construire l'en-tête Set-Cookie avec le JWT
    std::string make_cookie(const std::string& token, bool clear = false);
}
