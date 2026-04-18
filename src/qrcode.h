// ============================================================
// qrcode.h — Génération de tokens QR et rendu SVG
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include <string>

namespace qrcode {

// Générer un token unique pour un QR Code de session
// Format : session_id + timestamp + UUID + HMAC
std::string generate_token(int session_id);

// Générer le SVG d'un QR Code à partir d'un token
// Utilise une matrice QR simplifiée (format Micro QR / version 1)
// (le rendu final est fait par JS via la lib qrcodejs côté client)
// Cette fonction retourne un SVG placeholder avec le token encodé
std::string generate_svg(const std::string& token, int session_id);

// Vérifier qu'un token est valide (HMAC correct + non expiré)
bool verify_token(const std::string& token, int session_id, long long qr_expire_at);

// Durée de vie d'un QR Code en secondes
static const int QR_LIFETIME_SECONDS = 30;

// Clé secrète pour signer les tokens QR
static const std::string QR_SECRET = "SmartPresence_QR_2024!";

} // namespace qrcode
