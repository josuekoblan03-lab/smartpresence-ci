# SMARTPRESENCE CI

> Système de Gestion Intelligente des Présences — **Institut Universitaire d'Abidjan** 🇨🇮

## 🚀 Déploiement Railway

Ce projet est déployé via **Docker** sur Railway.app.

### Variables d'environnement
| Variable | Valeur par défaut | Description |
|---|---|---|
| `PORT` | `8080` | Port d'écoute (injecté par Railway) |

### Identifiants par défaut
| Rôle | Email | Mot de passe |
|---|---|---|
| Admin | `admin@smartpresence-ci.com` | `Admin2024!` |
| Enseignant | `prof.kone@iua.ci` | `Prof2024!` |

### Stack technique
- **Backend** : C++17 (Raw Socket, multi-threadé)
- **Base de données** : SQLite3 embarquée
- **Temps réel** : Server-Sent Events (SSE)
- **Frontend** : HTML/CSS/JS vanilla (Dark Mode, Glassmorphism)
- **Conteneur** : Docker (Debian Slim)
