// ============================================================
// utils.cpp — Implémentation des fonctions hash mot de passe
// ============================================================
#include "utils.h"
#include "../libs/sha256.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

namespace utils {

// Hash mot de passe : SHA256(sel + ":" + password)
// Le résultat stocké en base : "sel:hash"
std::string hash_password(const std::string& password, const std::string& salt) {
    // Générer un sel aléatoire si non fourni
    std::string s = salt;
    if (s.empty()) {
        static std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<uint32_t> dist(0, 0xffffffff);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(8) << dist(rng)
            << std::setw(8) << dist(rng);
        s = oss.str();
    }
    std::string raw = s + ":" + password;
    std::string hash = sha256(raw);
    return s + ":" + hash;
}

// Vérifier : extraire sel, recalculer, comparer
bool verify_password(const std::string& password, const std::string& hash_stored) {
    auto colon = hash_stored.find(':');
    if (colon == std::string::npos) return false;
    std::string salt = hash_stored.substr(0, colon);
    std::string raw  = salt + ":" + password;
    std::string computed = sha256(raw);
    std::string expected = hash_stored.substr(colon + 1);
    return computed == expected;
}

} // namespace utils
