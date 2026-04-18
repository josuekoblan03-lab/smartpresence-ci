// ============================================================
// sha256.h — Implémentation SHA-256 + HMAC-SHA256 (header-only)
// Utilisé pour la signature des tokens JWT
// ============================================================
#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace sha256_impl {

// Constantes SHA-256
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
inline uint32_t maj(uint32_t a, uint32_t b, uint32_t c) { return (a & b) ^ (a & c) ^ (b & c); }
inline uint32_t sig0(uint32_t x) { return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
inline uint32_t sig1(uint32_t x) { return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
inline uint32_t gam0(uint32_t x) { return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
inline uint32_t gam1(uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

// Calcul SHA-256 brut — retourne 32 octets
inline void compute(const uint8_t* data, size_t len, uint8_t digest[32]) {
    // Valeurs initiales
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };

    // Padding
    uint64_t bitLen = (uint64_t)len * 8;
    size_t totalLen = len + 1;
    while (totalLen % 64 != 56) {
        totalLen++;
    }
    totalLen += 8; // Multiple exact de 64
    
    // On travaille sur un buffer padé (initialisé à 0)
    uint8_t* buf = new uint8_t[totalLen]();
    memcpy(buf, data, len);
    buf[len] = 0x80;
    // Longueur en big-endian sur les 8 derniers octets
    for (int i = 0; i < 8; i++)
        buf[totalLen - 1 - i] = (uint8_t)((bitLen >> (8 * i)) & 0xff);

    // Traitement des blocs de 64 octets
    for (size_t blk = 0; blk < totalLen; blk += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)buf[blk+i*4+0] << 24) |
                   ((uint32_t)buf[blk+i*4+1] << 16) |
                   ((uint32_t)buf[blk+i*4+2] << 8)  |
                   ((uint32_t)buf[blk+i*4+3]);
        }
        for (int i = 16; i < 64; i++)
            w[i] = gam1(w[i-2]) + w[i-7] + gam0(w[i-15]) + w[i-16];

        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + sig1(e) + ch(e,f,g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + maj(a,b,c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    delete[] buf;

    // Sérialisation big-endian
    for (int i = 0; i < 8; i++) {
        digest[i*4+0] = (h[i] >> 24) & 0xff;
        digest[i*4+1] = (h[i] >> 16) & 0xff;
        digest[i*4+2] = (h[i] >> 8)  & 0xff;
        digest[i*4+3] = (h[i])        & 0xff;
    }
}

} // namespace sha256_impl

// ============================================================
// API publique
// ============================================================

// SHA-256 d'une chaîne — retourne hex string 64 caractères
inline std::string sha256(const std::string& input) {
    uint8_t digest[32];
    sha256_impl::compute(
        reinterpret_cast<const uint8_t*>(input.data()),
        input.size(), digest
    );
    std::ostringstream oss;
    for (int i = 0; i < 32; i++)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return oss.str();
}

// SHA-256 — retourne bytes bruts
inline void sha256_bytes(const std::string& input, uint8_t out[32]) {
    sha256_impl::compute(
        reinterpret_cast<const uint8_t*>(input.data()),
        input.size(), out
    );
}

// HMAC-SHA256 — retourne bytes bruts (32 octets)
inline void hmac_sha256(const std::string& key, const std::string& msg, uint8_t out[32]) {
    const size_t BLOCK = 64;
    uint8_t kpad[BLOCK] = {};

    // Clé > 64 octets : on la hashe
    if (key.size() > BLOCK) {
        sha256_impl::compute(
            reinterpret_cast<const uint8_t*>(key.data()), key.size(), kpad
        );
    } else {
        memcpy(kpad, key.data(), key.size());
    }

    // ipad et opad
    uint8_t ikey[BLOCK], okey[BLOCK];
    for (size_t i = 0; i < BLOCK; i++) {
        ikey[i] = kpad[i] ^ 0x36;
        okey[i] = kpad[i] ^ 0x5c;
    }

    // H(ikey || msg)
    std::string inner(reinterpret_cast<char*>(ikey), BLOCK);
    inner += msg;
    uint8_t inner_hash[32];
    sha256_impl::compute(
        reinterpret_cast<const uint8_t*>(inner.data()), inner.size(), inner_hash
    );

    // H(okey || inner_hash)
    std::string outer(reinterpret_cast<char*>(okey), BLOCK);
    outer += std::string(reinterpret_cast<char*>(inner_hash), 32);
    sha256_impl::compute(
        reinterpret_cast<const uint8_t*>(outer.data()), outer.size(), out
    );
}

// HMAC-SHA256 — retourne hex string
inline std::string hmac_sha256_hex(const std::string& key, const std::string& msg) {
    uint8_t mac[32];
    hmac_sha256(key, msg, mac);
    std::ostringstream oss;
    for (int i = 0; i < 32; i++)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
    return oss.str();
}
