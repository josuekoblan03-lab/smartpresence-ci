// ============================================================
// sse.h — Server-Sent Events : diffusion temps réel
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <functional>

// ─────────────────────────────────────────
// Événement SSE
// ─────────────────────────────────────────
struct SSEEvent {
    std::string event;  // nom de l'événement (ex: "presence_marked")
    std::string data;   // payload JSON
};

// Fonction d'écriture vers un socket client
using SSEWriter = std::function<bool(const std::string&)>;

// ─────────────────────────────────────────
// Client SSE connecté
// ─────────────────────────────────────────
struct SSEClient {
    int         fd;
    SSEWriter   write;
    bool        alive = true;
};

// ─────────────────────────────────────────
// Gestionnaire SSE global (singleton)
// ─────────────────────────────────────────
class SSEManager {
public:
    static SSEManager& instance();

    // Enregistrer un nouveau client SSE
    void add_client(int fd, SSEWriter writer);

    // Supprimer un client
    void remove_client(int fd);

    // Diffuser un événement à tous les clients
    void broadcast(const std::string& event, const std::string& json_data);

    // Nombre de clients connectés
    int client_count();

private:
    SSEManager() = default;
    std::vector<SSEClient> clients_;
    std::mutex             mutex_;

    // Formater un message SSE
    static std::string format_event(const std::string& event, const std::string& data);
};

// ─────────────────────────────────────────
// Helpers — construire les payloads JSON des événements
// ─────────────────────────────────────────
namespace sse_events {

// Présence marquée
std::string presence_marked(const std::string& etudiant_nom,
                             const std::string& matricule,
                             int session_id,
                             const std::string& timestamp);

// Tentative de fraude détectée
std::string fraud_detected(const std::string& type_fraude,
                            const std::string& description,
                            const std::string& ip,
                            int session_id);

// Renouvellement QR Code
std::string qr_refresh(int session_id, const std::string& new_token);

// Heartbeat (keeps connection alive)
std::string heartbeat();

} // namespace sse_events
