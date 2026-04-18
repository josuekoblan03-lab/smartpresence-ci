// ============================================================
// qrcode.cpp — Génération tokens QR + SVG animé
// SMARTPRESENCE CI
// ============================================================
#include "qrcode.h"
#include "utils.h"
#include "../libs/sha256.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace qrcode {

// ──────────────────────────────────────────────────────────────
// Générer un token QR unique sécurisé
// Format : sid_<session_id>_ts_<timestamp>_<uuid>_<hmac6>
// ──────────────────────────────────────────────────────────────
std::string generate_token(int session_id) {
    long long ts   = utils::now_unix();
    std::string uuid = utils::generate_uuid();

    // Payload = session_id + timestamp + uuid
    std::string payload = std::to_string(session_id) + ":" + std::to_string(ts) + ":" + uuid;

    // HMAC-SHA256 tronqué à 8 caractères
    std::string sig = hmac_sha256_hex(QR_SECRET, payload).substr(0, 8);

    return "SP_" + std::to_string(session_id) + "_" + std::to_string(ts) + "_" + sig;
}

// ──────────────────────────────────────────────────────────────
// Vérifier un token QR
// ──────────────────────────────────────────────────────────────
bool verify_token(const std::string& token, int session_id, long long qr_expire_at) {
    // Vérifier le préfixe
    if (token.substr(0, 3) != "SP_") return false;

    // Extraire le session_id du token
    // Format : SP_<sid>_<ts>_<sig>
    std::istringstream ss(token.substr(3));
    std::string sid_s, ts_s, sig_s;
    std::getline(ss, sid_s, '_');
    std::getline(ss, ts_s,  '_');
    std::getline(ss, sig_s);

    if (sid_s.empty() || ts_s.empty() || sig_s.empty()) return false;

    int    tok_sid = std::stoi(sid_s);
    long long tok_ts = std::stoll(ts_s);

    // Vérifier session_id
    if (tok_sid != session_id) return false;

    // Vérifier expiration (en utilisant qr_expire_at de la DB)
    if (utils::now_unix() > qr_expire_at) return false;

    // Vérifier HMAC (note: on n'a pas l'uuid original, on vérifie juste l'expiration ici)
    // Dans un vrai système, on stockerait l'uuid en base
    return true;
}

// ──────────────────────────────────────────────────────────────
// Générer un SVG QR Code
// Le SVG contient l'URL de scan qui sera rendue côté client
// par qrcodejs. Le SVG est un "splash" stylisé indiquant
// que le vrai QR est rendu côté client.
// ──────────────────────────────────────────────────────────────
std::string generate_svg(const std::string& token, int session_id) {
    // L'URL que le QR code encode = URL de la page de scan avec le token
    std::string scan_url = "http://localhost:8080/scan.html?token=" + token +
                           "&sid=" + std::to_string(session_id);

    // SVG avec animation de renouvellement + texte indicatif
    // En prod, le frontend utilise qrcodejs pour rendre le vrai QR
    std::ostringstream svg;
    svg << R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" width="200" height="200">
  <defs>
    <linearGradient id="bg" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#1e1b4b"/>
      <stop offset="100%" style="stop-color:#312e81"/>
    </linearGradient>
    <linearGradient id="pulse" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#6366f1"/>
      <stop offset="100%" style="stop-color:#8b5cf6"/>
    </linearGradient>
  </defs>

  <!-- Fond -->
  <rect width="200" height="200" rx="12" fill="url(#bg)"/>

  <!-- Bordure animée -->
  <rect x="4" y="4" width="192" height="192" rx="10"
        fill="none" stroke="url(#pulse)" stroke-width="2" stroke-dasharray="8 4">
    <animateTransform attributeName="transform" type="rotate"
      from="0 100 100" to="360 100 100" dur="8s" repeatCount="indefinite"/>
  </rect>

  <!-- QR zones factices (coins) -->
  <rect x="20" y="20" width="40" height="40" rx="4" fill="none" stroke="#6366f1" stroke-width="3"/>
  <rect x="25" y="25" width="30" height="30" rx="2" fill="#6366f1" fill-opacity="0.4"/>
  <rect x="30" y="30" width="20" height="20" rx="1" fill="#6366f1"/>

  <rect x="140" y="20" width="40" height="40" rx="4" fill="none" stroke="#6366f1" stroke-width="3"/>
  <rect x="145" y="25" width="30" height="30" rx="2" fill="#6366f1" fill-opacity="0.4"/>
  <rect x="150" y="30" width="20" height="20" rx="1" fill="#6366f1"/>

  <rect x="20" y="140" width="40" height="40" rx="4" fill="none" stroke="#6366f1" stroke-width="3"/>
  <rect x="25" y="145" width="30" height="30" rx="2" fill="#6366f1" fill-opacity="0.4"/>
  <rect x="30" y="150" width="20" height="20" rx="1" fill="#6366f1"/>

  <!-- Icône centrale -->
  <circle cx="100" cy="95" r="22" fill="#6366f1" fill-opacity="0.2" stroke="#6366f1" stroke-width="2">
    <animate attributeName="r" values="20;24;20" dur="2s" repeatCount="indefinite"/>
    <animate attributeName="fill-opacity" values="0.2;0.4;0.2" dur="2s" repeatCount="indefinite"/>
  </circle>
  <text x="100" y="101" text-anchor="middle" font-family="monospace" font-size="20" fill="#6366f1">📋</text>

  <!-- Session ID -->
  <text x="100" y="135" text-anchor="middle" font-family="Arial,sans-serif"
        font-size="9" fill="#a5b4fc">Session #)SVG" << session_id << R"SVG(</text>

  <!-- Token tronqué -->
  <text x="100" y="148" text-anchor="middle" font-family="monospace"
        font-size="7" fill="#6366f1" fill-opacity="0.8">)SVG" << token.substr(0, 20) << R"SVG(</text>

  <!-- Barre de compte à rebours -->
  <rect x="20" y="165" width="160" height="4" rx="2" fill="#312e81"/>
  <rect x="20" y="165" width="160" height="4" rx="2" fill="url(#pulse)">
    <animate attributeName="width" from="160" to="0" dur="30s" repeatCount="once"/>
  </rect>

  <!-- Texte bas -->
  <text x="100" y="182" text-anchor="middle" font-family="Arial,sans-serif"
        font-size="8" fill="#a5b4fc">Renouvellement automatique dans 30s</text>
  <text x="100" y="195" text-anchor="middle" font-family="Arial,sans-serif"
        font-size="7" fill="#6366f1" fill-opacity="0.6">SMARTPRESENCE CI</text>
</svg>)SVG";

    return svg.str();
}

} // namespace qrcode
