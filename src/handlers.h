// ============================================================
// handlers.h — Déclarations de tous les handlers API
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include "server.h"
#include "database.h"
#include "auth.h"

// Namespace contenant tous les handlers API REST
namespace handlers {

// ── Authentification ──
HttpResponse login(const HttpRequest& req, Database& db);
HttpResponse logout(const HttpRequest& req);
HttpResponse me(const HttpRequest& req, Database& db);

// ── Dashboard ──
HttpResponse dashboard_stats(const HttpRequest& req, Database& db);

// ── Sessions ──
HttpResponse list_sessions(const HttpRequest& req, Database& db);
HttpResponse create_session(const HttpRequest& req, Database& db);
HttpResponse get_session(const HttpRequest& req, Database& db);
HttpResponse close_session(const HttpRequest& req, Database& db);

// ── QR Code ──
HttpResponse get_qrcode(const HttpRequest& req, Database& db);
HttpResponse get_qrcode_token(const HttpRequest& req, Database& db);

// ── Présences ──
HttpResponse mark_presence(const HttpRequest& req, Database& db);
HttpResponse list_presences(const HttpRequest& req, Database& db);

// ── Étudiants ──
HttpResponse list_students(const HttpRequest& req, Database& db);
HttpResponse get_student(const HttpRequest& req, Database& db);
HttpResponse create_student(const HttpRequest& req, Database& db);

// ── Filières ──
HttpResponse list_filieres(const HttpRequest& req, Database& db);

// ── Fraudes ──
HttpResponse list_frauds(const HttpRequest& req, Database& db);

// ── Rapports ──
HttpResponse report_session(const HttpRequest& req, Database& db);

// ── Events Polling (fallback pour Railway) ──
HttpResponse get_events_poll(const HttpRequest& req, Database& db);

} // namespace handlers
