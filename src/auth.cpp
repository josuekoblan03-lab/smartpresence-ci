// ============================================================
// auth.cpp — JWT HMAC-SHA256 encode/decode
// SMARTPRESENCE CI
// ============================================================
#include "auth.h"
#include "utils.h"
#include "../libs/sha256.h"
#include <sstream>
#include <iostream>

namespace auth {

// ──────────────────────────────────────────────────────────────
// Créer un token JWT signé
// Format : base64url(header).base64url(payload).signature
// ──────────────────────────────────────────────────────────────
std::string create_token(int user_id, const std::string& role,
                         const std::string& email, const std::string& nom) {
    // --- Header ---
    std::string header_json = R"({"alg":"HS256","typ":"JWT"})";
    std::string header_b64  = utils::base64_url_encode(header_json);

    // --- Payload ---
    long long exp = utils::now_unix() + JWT_EXPIRY_SECONDS;
    std::string payload_json =
        "{" +
        utils::json_num("uid",  user_id) + "," +
        utils::json_str("role", role)    + "," +
        utils::json_str("email",email)   + "," +
        utils::json_str("nom",  nom)     + "," +
        utils::json_num("exp",  exp)     + "," +
        utils::json_num("iat",  utils::now_unix()) +
        "}";
    std::string payload_b64 = utils::base64_url_encode(payload_json);

    // --- Signature HMAC-SHA256 ---
    std::string signing_input = header_b64 + "." + payload_b64;
    std::string sig_hex = hmac_sha256_hex(JWT_SECRET, signing_input);
    // Convertir hex → bytes → base64url
    std::string sig_bytes;
    for (size_t i = 0; i + 1 < sig_hex.size(); i += 2) {
        uint8_t byte = (uint8_t)std::stoi(sig_hex.substr(i, 2), nullptr, 16);
        sig_bytes += (char)byte;
    }
    std::string sig_b64 = utils::base64_url_encode(sig_bytes);

    return signing_input + "." + sig_b64;
}

// ──────────────────────────────────────────────────────────────
// Valider et décoder un JWT
// ──────────────────────────────────────────────────────────────
JWTClaims verify_token(const std::string& token) {
    JWTClaims claims;

    // Diviser en 3 parties
    auto p1 = token.find('.');
    if (p1 == std::string::npos) return claims;
    auto p2 = token.find('.', p1 + 1);
    if (p2 == std::string::npos) return claims;

    std::string header_b64  = token.substr(0, p1);
    std::string payload_b64 = token.substr(p1 + 1, p2 - p1 - 1);
    std::string sig_b64     = token.substr(p2 + 1);

    // Vérifier la signature
    std::string signing_input = header_b64 + "." + payload_b64;
    std::string expected_sig_hex = hmac_sha256_hex(JWT_SECRET, signing_input);
    // Reconstruire base64url de la signature attendue
    std::string expected_bytes;
    for (size_t i = 0; i + 1 < expected_sig_hex.size(); i += 2) {
        uint8_t byte = (uint8_t)std::stoi(expected_sig_hex.substr(i, 2), nullptr, 16);
        expected_bytes += (char)byte;
    }
    std::string expected_b64 = utils::base64_url_encode(expected_bytes);

    if (sig_b64 != expected_b64) {
        std::cerr << "[Auth] Token invalide : signature incorrecte" << std::endl;
        return claims; // valid reste false
    }

    // Décoder le payload
    std::string payload_json = utils::base64_decode(payload_b64);

    // Extraire les champs
    claims.user_id = (int)utils::json_get_int(payload_json, "uid");
    claims.role    = utils::json_get_str(payload_json, "role");
    claims.email   = utils::json_get_str(payload_json, "email");
    claims.nom     = utils::json_get_str(payload_json, "nom");
    claims.exp     = utils::json_get_int(payload_json, "exp");

    // Vérifier expiration
    if (utils::now_unix() > claims.exp) {
        std::cerr << "[Auth] Token expiré" << std::endl;
        return claims; // valid reste false
    }

    claims.valid = true;
    return claims;
}

// ──────────────────────────────────────────────────────────────
// Construire l'en-tête Set-Cookie
// ──────────────────────────────────────────────────────────────
std::string make_cookie(const std::string& token, bool clear) {
    if (clear) {
        return "Set-Cookie: smartpresence_token=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0\r\n";
    }
    return "Set-Cookie: smartpresence_token=" + token +
           "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" +
           std::to_string(JWT_EXPIRY_SECONDS) + "\r\n";
}

} // namespace auth
