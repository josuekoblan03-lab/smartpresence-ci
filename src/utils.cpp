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

#include <cmath>
#include <sstream>

namespace utils {

// Convertir degr�s en radians
static double to_rad(double degree) {
    return degree * 3.14159265358979323846 / 180.0;
}

double haversine_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // Rayon de la Terre en m�tres
    double dLat = to_rad(lat2 - lat1);
    double dLon = to_rad(lon2 - lon1);
    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(to_rad(lat1)) * std::cos(to_rad(lat2)) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return R * c;
}

static std::vector<float> parse_descriptor(const std::string& desc) {
    std::vector<float> result;
    if (desc.empty() || desc.front() != '[') return result;
    std::string s = desc.substr(1);
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            result.push_back(std::stof(item));
        } catch(...) {}
    }
    return result;
}

double face_distance(const std::string& desc1, const std::string& desc2) {
    auto v1 = parse_descriptor(desc1);
    auto v2 = parse_descriptor(desc2);
    if (v1.size() != 128 || v2.size() != 128) return 999.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < 128; i++) {
        double diff = v1[i] - v2[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

} // namespace utils
