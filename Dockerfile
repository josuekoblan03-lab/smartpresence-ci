# ============================================================
# Dockerfile — SMARTPRESENCE CI pour Railway (Linux)
# ============================================================

# ── Stage 1 : Compilation ──────────────────────────────────
FROM debian:bookworm-slim AS builder

# Installer les outils de compilation
RUN apt-get update && apt-get install -y \
    g++ \
    gcc \
    make \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copier tout le projet
WORKDIR /app
COPY . .

# Compiler sqlite3.c en objet séparé (en C)
RUN gcc -c libs/sqlite3.c -o sqlite3.o -O2

# Compiler le projet C++ (mode Linux, sans -lws2_32)
RUN g++ main.cpp \
    src/auth.cpp \
    src/database.cpp \
    src/handlers.cpp \
    src/qrcode.cpp \
    src/router.cpp \
    src/seeder.cpp \
    src/server.cpp \
    src/shields.cpp \
    src/sse.cpp \
    src/utils.cpp \
    sqlite3.o \
    -I libs/ \
    -o smartpresence \
    -lpthread \
    -std=c++17 \
    -O2

# ── Stage 2 : Image finale légère ──────────────────────────
FROM debian:bookworm-slim

WORKDIR /app

# Copier l'exécutable et les fichiers publics
COPY --from=builder /app/smartpresence .
COPY public/ ./public/

# Port exposé (Railway utilise la variable PORT)
EXPOSE 8080

# Démarrer le serveur
CMD ["./smartpresence"]
