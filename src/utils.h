// ============================================================
// utils.h — Utilitaires : JSON, base64, UUID, URL-decode, hash mot de passe
// ============================================================
#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <random>
#include <chrono>

namespace utils {

// ─────────────────────────────────────────
// BASE64 URL-safe (pour JWT)
// ─────────────────────────────────────────
static const std::string B64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline std::string base64_encode(const std::string& input) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(B64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(B64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

inline std::string base64_url_encode(const std::string& input) {
    std::string s = base64_encode(input);
    for (auto& c : s) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    // Supprimer le padding '='
    while (!s.empty() && s.back() == '=') s.pop_back();
    return s;
}

inline std::string base64_decode(std::string input) {
    // Ajouter padding si nécessaire
    while (input.size() % 4) input += '=';
    for (auto& c : input) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(uint8_t)B64_CHARS[i]] = i;
    int val = 0, valb = -8;
    for (uint8_t c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((char)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// ─────────────────────────────────────────
// URL DECODE (%20 → espace, etc.)
// ─────────────────────────────────────────
inline std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int val = std::stoi(s.substr(i + 1, 2), nullptr, 16);
            out += (char)val;
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

// ─────────────────────────────────────────
// PARSE FORM DATA (application/x-www-form-urlencoded)
// ─────────────────────────────────────────
inline std::map<std::string, std::string> parse_form(const std::string& body) {
    std::map<std::string, std::string> params;
    std::istringstream ss(body);
    std::string token;
    while (std::getline(ss, token, '&')) {
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            std::string key   = url_decode(token.substr(0, eq));
            std::string value = url_decode(token.substr(eq + 1));
            params[key] = value;
        }
    }
    return params;
}

// ─────────────────────────────────────────
// JSON SIMPLE — builder sans dépendances
// ─────────────────────────────────────────
inline std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

inline std::string json_str(const std::string& key, const std::string& val) {
    return "\"" + json_escape(key) + "\":\"" + json_escape(val) + "\"";
}

inline std::string json_num(const std::string& key, long long val) {
    return "\"" + json_escape(key) + "\":" + std::to_string(val);
}

inline std::string json_bool(const std::string& key, bool val) {
    return "\"" + json_escape(key) + "\":" + (val ? "true" : "false");
}

// Retourne { "success": true, "message": "..." }
inline std::string json_ok(const std::string& msg = "") {
    std::string r = "{\"success\":true";
    if (!msg.empty()) r += ",\"message\":\"" + json_escape(msg) + "\"";
    r += "}";
    return r;
}

// Retourne { "success": false, "error": "..." }
inline std::string json_err(const std::string& err) {
    return "{\"success\":false,\"error\":\"" + json_escape(err) + "\"}";
}

// Parser JSON minimaliste (extraire un champ string)
inline std::string json_get_str(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    while (end != std::string::npos && json[end - 1] == '\\') end = json.find('"', end + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

// Parser JSON — extraire un champ entier
inline long long json_get_int(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return 0;
    // Chercher le premier chiffre
    while (pos < json.size() && (json[pos] < '0' || json[pos] > '9') && json[pos] != '-') pos++;
    if (pos >= json.size()) return 0;
    return std::stoll(json.substr(pos));
}

// ─────────────────────────────────────────
// UUID v4 (pseudo-aléatoire)
// ─────────────────────────────────────────
inline std::string generate_uuid() {
    static std::mt19937_64 rng(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    std::uniform_int_distribution<uint32_t> dist(0, 0xffffffff);
    
    auto r = [&]() -> uint32_t { return dist(rng); };
    
    uint32_t a = r(), b = r(), c = r(), d = r();
    // Version 4 + variante 1
    b = (b & 0xffff0fff) | 0x00004000;
    c = (c & 0x3fffffff) | 0x80000000;
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << a << "-"
        << std::setw(4) << (b >> 16) << "-"
        << std::setw(4) << (b & 0xFFFF) << "-"
        << std::setw(4) << (c >> 16) << "-"
        << std::setw(4) << (c & 0xFFFF)
        << std::setw(8) << d;
    return oss.str();
}

// ─────────────────────────────────────────
// TIMESTAMP UNIX
// ─────────────────────────────────────────
inline long long now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Formater un timestamp Unix → "YYYY-MM-DD HH:MM:SS"
inline std::string format_datetime(long long ts) {
    std::time_t t = (std::time_t)ts;
    char buf[32];
    struct tm* tm_info;
#ifdef _WIN32
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    tm_info = &tm_buf;
#else
    tm_info = localtime(&t);
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf);
}

// ─────────────────────────────────────────
// HASH MOT DE PASSE (SHA-256 avec sel)
// ─────────────────────────────────────────
// Déclaré ici, défini dans utils.cpp car dépendance sha256.h
std::string hash_password(const std::string& password, const std::string& salt = "");
bool verify_password(const std::string& password, const std::string& hash_stored);

// ─────────────────────────────────────────
// COOKIES — parser les cookies d'une requête
// ─────────────────────────────────────────
inline std::map<std::string, std::string> parse_cookies(const std::string& cookie_header) {
    std::map<std::string, std::string> cookies;
    std::istringstream ss(cookie_header);
    std::string pair;
    while (std::getline(ss, pair, ';')) {
        // Trim espaces
        auto start = pair.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        pair = pair.substr(start);
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);
            cookies[key] = val;
        }
    }
    return cookies;
}

// ─────────────────────────────────────────
// TRIM string
// ─────────────────────────────────────────
inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

// Générer un code aléatoire à N chiffres
inline std::string random_code(int digits = 6) {
    static std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 9);
    std::string code;
    for (int i = 0; i < digits; i++) code += ('0' + dist(rng));
    return code;
}

// Générer matricule étudiant (ex: IUAxxxxxxxx)
inline std::string random_matricule(int index) {
    std::ostringstream oss;
    oss << "IUA" << std::setw(8) << std::setfill('0') << (2024000 + index);
    return oss.str();
}

// ─────────────────────────────────────────
// Cacluls GPS et Biométrie
// ─────────────────────────────────────────

// Calculer la distance en mètres entre deux points GPS
double haversine_distance(double lat1, double lon1, double lat2, double lon2);

// Calculer la distance euclidienne entre deux visages (tableaux JSON de floats)
double face_distance(const std::string& desc1, const std::string& desc2);

} // namespace utils
