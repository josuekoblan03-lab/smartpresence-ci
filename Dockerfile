# ============================================================
# Dockerfile — SMARTPRESENCE CI pour Railway (Microservices AI)
# ============================================================

# 🛠️ Stage 1 : Compilation C++ 
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    gcc \
    make \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN gcc -c libs/sqlite3.c -o sqlite3.o -O2

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

# 🚀 Stage 2 : Serveur Mixte (C++ & Python InsightFace LÉGER)
FROM debian:bookworm-slim

WORKDIR /app

# Installer Python 3, pip, et dépendances OpenCV/ONNX
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    libgl1-mesa-glx \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Copier l'exécutable et le projet web
COPY --from=builder /app/smartpresence .
COPY public/ ./public/
COPY start.sh ./start.sh
RUN chmod +x start.sh

# Copier et installer le microservice AI Python
COPY ai_service/ ./ai_service/
RUN pip3 install --no-cache-dir -r ai_service/requirements.txt --break-system-packages

EXPOSE 8080 5000
CMD ["./start.sh"]
