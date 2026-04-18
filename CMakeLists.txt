cmake_minimum_required(VERSION 3.10)
project(SmartPresenceCI C CXX)

# Utiliser C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Inclure le dossier source et librairies
include_directories(src)
include_directories(libs)

# Trouver tous les fichiers source
file(GLOB SOURCES "src/*.cpp")
list(APPEND SOURCES "main.cpp" "libs/sqlite3.c")

# Créer l'exécutable
add_executable(SmartPresenceCI ${SOURCES})

# Lier les bibliothèques réseau Windows si exécuté sous Windows
if(WIN32)
    target_link_libraries(SmartPresenceCI Ws2_32 Crypt32)
else()
    # Pour Linux (via Railway ou WSL)
    target_link_libraries(SmartPresenceCI pthread)
endif()
