// ============================================================
// main.cpp — Point d'entrée SMARTPRESENCE CI
// Institut Universitaire d'Abidjan — Côte d'Ivoire
// ============================================================
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include "src/server.h"
#include "src/router.h"
#include "src/database.h"
#include "src/seeder.h"
#include "src/sse.h"
#include "src/handlers.h"

#ifdef _WIN32
  #include <winsock2.h>
#endif

// Serveur global pour arrêt propre
static HttpServer* g_server = nullptr;

// Gestionnaire de signal CTRL+C
void signal_handler(int sig) {
    std::cout << "\n[Main] Arrêt du serveur (signal " << sig << ")..." << std::endl;
    if (g_server) g_server->stop();
}

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════╗
║           SMARTPRESENCE CI — v1.0                        ║
║   Système de Gestion de Présences Universitaires         ║
║   Institut Universitaire d'Abidjan — Abidjan             ║
╚══════════════════════════════════════════════════════════╝
)" << std::endl;

    // ── Gestion des signaux ──
    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignorer les pipes cassés (SSE)
#endif

    // ── Initialisation de la base de données ──
    Database db;
    if (!db.open("smartpresence.db")) {
        std::cerr << "[Main] ❌ Impossible d'ouvrir la base de données" << std::endl;
        return 1;
    }

    // Créer le schéma des tables
    db.create_schema();

    // Insérer les données de test (si pas déjà fait)
    seeder::seed(db);

    // ── Initialisation du routeur ──
    Router router(db);
    router.register_routes();

    // ── Démarrage du serveur ──
    HttpServer server;
    g_server = &server;

    // Handler principal : déléguer au routeur
    server.set_handler([&router](const HttpRequest& req) -> HttpResponse {
        return router.dispatch(req);
    });

    // ── Lire le port depuis l'environnement (Railway injecte $PORT) ──
    int port = 8080;
    const char* env_port = std::getenv("PORT");
    if (env_port) port = std::atoi(env_port);

    // Lancer sur le port
    if (!server.listen(port)) {
        std::cerr << "[Main] ❌ Impossible de démarrer le serveur" << std::endl;
        return 1;
    }

    std::cout << "[Main] Serveur arrêté proprement." << std::endl;
    return 0;
}
