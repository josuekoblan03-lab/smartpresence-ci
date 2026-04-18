// ============================================================
// sse.cpp — Implémentation SSE Manager
// SMARTPRESENCE CI
// ============================================================
#include "sse.h"
#include "utils.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #define SEND_FN(fd, data, len) send(fd, data, len, 0)
#else
    #include <sys/socket.h>
    #define SEND_FN(fd, data, len) send(fd, data, len, 0)
#endif

// ─────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────
SSEManager& SSEManager::instance() {
    static SSEManager inst;
    return inst;
}

// ─────────────────────────────────────────
// Formater événement SSE
// ─────────────────────────────────────────
std::string SSEManager::format_event(const std::string& event, const std::string& data) {
    return "event: " + event + "\ndata: " + data + "\n\n";
}

// ─────────────────────────────────────────
// Ajouter un client
// ─────────────────────────────────────────
void SSEManager::add_client(int fd, SSEWriter writer) {
    std::lock_guard<std::mutex> lock(mutex_);
    SSEClient client;
    client.fd    = fd;
    client.write = writer;
    client.alive = true;
    clients_.push_back(client);
    std::cout << "[SSE] Client connecté fd=" << fd
              << " | total=" << clients_.size() << std::endl;

    // Envoyer un message de bienvenue
    std::string hello = format_event("connected",
        "{\"message\":\"SMARTPRESENCE CI — Flux temps réel connecté\","
        "\"clients\":" + std::to_string(clients_.size()) + "}");
    writer(hello);
}

// ─────────────────────────────────────────
// Supprimer un client
// ─────────────────────────────────────────
void SSEManager::remove_client(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [fd](const SSEClient& c) { return c.fd == fd; }),
        clients_.end()
    );
    std::cout << "[SSE] Client déconnecté fd=" << fd
              << " | restants=" << clients_.size() << std::endl;
}

// ─────────────────────────────────────────
// Diffuser un événement à tous les clients
// ─────────────────────────────────────────
void SSEManager::broadcast(const std::string& event, const std::string& json_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Garder en mémoire pour le polling
    SSEEvent ev;
    ev.id = next_event_id_++;
    ev.event = event;
    ev.data = json_data;
    recent_events_.push_back(ev);
    // Limiter la taille du buffer
    if (recent_events_.size() > 50) {
        recent_events_.erase(recent_events_.begin());
    }

    std::string msg = format_event(event, json_data);

    std::cout << "[SSE] Broadcast (" << ev.id << ") '" << event << "' → " << clients_.size() << " client(s)" << std::endl;

    // Marquer les clients morts
    for (auto& client : clients_) {
        if (!client.alive) continue;
        bool ok = client.write(msg);
        if (!ok) {
            client.alive = false;
        }
    }

    // Nettoyer les clients morts
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [](const SSEClient& c) { return !c.alive; }),
        clients_.end()
    );
}

int SSEManager::client_count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return (int)clients_.size();
}

std::vector<SSEEvent> SSEManager::get_recent_events(int since_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SSEEvent> result;
    for (const auto& ev : recent_events_) {
        if (ev.id > since_id) {
            result.push_back(ev);
        }
    }
    return result;
}

// ─────────────────────────────────────────
// Helpers événements
// ─────────────────────────────────────────
namespace sse_events {

std::string presence_marked(const std::string& etudiant_nom,
                             const std::string& matricule,
                             int session_id,
                             const std::string& timestamp) {
    return "{" +
        utils::json_str("etudiant_nom", etudiant_nom) + "," +
        utils::json_str("matricule",    matricule)     + "," +
        utils::json_num("session_id",   session_id)    + "," +
        utils::json_str("timestamp",    timestamp)     +
        "}";
}

std::string fraud_detected(const std::string& type_fraude,
                            const std::string& description,
                            const std::string& ip,
                            const std::string& matricule,
                            int session_id,
                            long long horodatage) {
    return "{" +
        utils::json_str("type_fraude", type_fraude)                        + "," +
        utils::json_str("description", description)                        + "," +
        utils::json_str("ip",          ip)                                 + "," +
        utils::json_str("matricule",   matricule)                           + "," +
        utils::json_str("heure",       utils::format_datetime(horodatage)) + "," +
        utils::json_num("horodatage",  horodatage)                         + "," +
        utils::json_num("session_id",  session_id)                         +
        "}";
}

std::string qr_refresh(int session_id, const std::string& new_token) {
    return "{" +
        utils::json_num("session_id", session_id)  + "," +
        utils::json_str("token",      new_token)   + "," +
        utils::json_num("expires_in", 30)          +
        "}";
}

std::string heartbeat() {
    return "{\"ts\":" + std::to_string(utils::now_unix()) + "}";
}

} // namespace sse_events
