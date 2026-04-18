#!/bin/bash
echo "Démarrage du processus global..."
export PORT=${PORT:-8080}

# 1. Démarrer le microservice AI Python en arrière-plan
echo "Booting Python AI Service on port 5000..."
cd ai_service && uvicorn main:app --host 127.0.0.1 --port 5000 &
AI_PID=$!

# Attendre que le port s'ouvre (2 secs)
sleep 2

# 2. Démarrer le Backend C++ au premier plan (sur le port fourni par Railway)
echo "Booting C++ Main Backend on port $PORT..."
cd /app
./smartpresence

# Si le C++ meurt, on tue le script
kill $AI_PID
