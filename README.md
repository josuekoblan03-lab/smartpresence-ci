# 🎓 SMARTPRESENCE CI — Institut Universitaire d'Abidjan (IUA)

**SMARTPRESENCE CI** est un système de gestion intelligente, temps-réel et anti-fraude des présences académiques, développé spécifiquement pour l'**Institut Universitaire d'Abidjan**.
Conçu pour les environnements de haute densité (grands amphithéâtres), ce projet abandonne les langages web classiques côté serveur au profit d'un noyau **C++ natif hyper-optimisé** afin de traiter des milliers de connexions simultanément, sans latence.

Ce document décrit en détail l'architecture, le fonctionnement interactif, ainsi que les stratégies de cybersécurité employées pour garantir l’intégrité des données universitaires.

---

## 📑 Sommaire
1. [Flux de Fonctionnement (Exemple pratique)](#1-flux-de-fonctionnement-exemple-pratique)
2. [Panneau d'Administration et Rôles](#2-panneau-dadministration-et-rôles)
3. [Mécanismes Anti-Fraude (Sécurité)](#3-mécanismes-anti-fraude-sécurité)
4. [Stack Technique & Choix Architecturaux](#4-stack-technique--choix-architecturaux)
5. [Déploiement Continue (CI/CD) sur Railway](#5-déploiement-continue-cicd-sur-railway)

---

## 1. Flux de Fonctionnement (Exemple pratique)

Le système est pensé pour être extrêmement rapide lors des prises de présence en amphi, évitant ainsi les interminables files d'attente ou la perte de temps au début d'un cours.

### 👨‍🏫 Étape 1 : Le Professeur lance une session
- Le Professeur (ex: *M. Koné*) se connecte avec son identifiant sécurisé (`prof.kone@iua.ci`).
- Depuis son tableau de bord, il clique sur **"Nouvelle Session"** et sélectionne la filière (ex: *Licence 3 Informatique*).
- **Le système génère un QR Code dynamique** qui est projeté sur l'écran de l'amphithéâtre.

### 📱 Étape 2 : L'Étudiant scanne (Scénario de succès)
- L'étudiant (ex: *Konan Ange*) utilise son smartphone pour scanner le QR Code projeté.
- Son navigateur s'ouvre sur une page web épurée : **il n'a pas besoin de télécharger d’application**.
- Il saisit son matricule (`IUA20240001`) et son code PIN personnel à 6 chiffres.
- Il clique sur **"Valider ma présence"**.

### ⚡ Étape 3 : Le Temps Réel (Polling API)
- Sur l'ordinateur du professeur, un indicateur **LIVE** clignote.
- Dès que l'étudiant valide, le nom de l'étudiant s'affiche **instantanément** sur l'écran du professeur avec une notification visuelle et sonore (statistiques mises à jour en direct).
- Si un étudiant a déjà pointé, le système le bloque immédiatement pour "Double tentative".

---

## 2. Panneau d'Administration et Rôles

Le système met en place une séparation stricte des privilèges :

- **L'Administrateur** (`admin@smartpresence-ci.com`) :
  - Il a **les pleins pouvoirs**.
  - Il peut créer, voir et rechercher des **Étudiants**.
  - Il peut gérer le corps enseignant (via l'onglet **👨‍🏫 Professeurs**). Il définit les adresses emails et les mots de passe manuellement pour chaque professeur afin d'éviter la création de comptes fantômes.
  - Il a accès aux logs complets de tentatives de fraude.
- **L'Enseignant** :
  - Son compte est infailliblement délivré par l'administration.
  - Il peut uniquement créer et gérer **ses propres sessions de cours**.
  - Il peut télécharger les listes de présence en fin de cours.
  - Il n'a pas le droit d'ajouter des étudiants ou d'autres professeurs dans la base de données.

---

## 3. Mécanismes Anti-Fraude (Sécurité)

La gestion des présences par QR Code souffre généralement d'un problème majeur : **un étudiant dans la salle pourrait prendre le QR code en photo et l'envoyer à un ami resté chez lui via WhatsApp**. 

Voici comment **SMARTPRESENCE CI** aborde ces vulnérabilités :

### Vulnérabilité 1 : "L'Ami Resté à la Maison" (Partage de QR Code)
**Le problème :** Scan depuis une photo WhatsApp envoyée par un élève présent.
* **La Solution (QR Dynamique Tournant) :** Le QR code projeté sur le tableau possède une **durée de vie (Token Expiry)** de quelques secondes (typiquement 10 à 30 secondes). 
* **Comment s'est géré :** En arrière-plan (sans recharger la page), l'écran du professeur actualise silencieusement le QR Code. Si l'ami resté chez lui scanne la photo reçue sur WhatsApp quelques minutes plus tard, le serveur C++ rejettera sa requête : `Token QR Expiré`.

### Vulnérabilité 2 : "Le Fraudeur Multiple" (Scanner pour 5 amis)
**Le problème :** Un étudiant dans la salle scanne le QR code, valide avec son matricule, puis rafraîchit la page et revalide avec les matricules de 4 autres amis absents.
* **La Solution (IP Filtering & Rate Limiting) :**
  1. Lors d'une validation de présence, le noyau C++ extrait l'**Adresse IP** du smartphone.
  2. Chaque étudiant possède un **Code PIN personnel** qu'un autre étudiant ignore potentiellement.
  3. Si le système détecte **plusieurs matricules différents validés depuis la même adresse IP** dans un laps de temps court, il lève une Alerte de Fraude. L'écran LIVE du professeur clignote en 🚨**ROUGE**🚨 avec le message : *"Fraude détectée: Tentatives multiples depuis l'IP 192.168.x.x"*.

### Vulnérabilité 3 : Injection SQL (Le Hacker)
**Le problème :** Saisir des commandes SQL dans le champ Matricule (ex: `' OR '1'='1`).
* **La Sécurité (Prepared Statements) :** Absolument **toutes** les requêtes SQL (SQLite3) interagissant avec ce système utilisent la fonction C++ `sqlite3_bind_...`. Il est mathématiquement impossible d'injecter du code SQL via les formulaires frontend.

### Vulnérabilité 4 : Sessions fantômes
**Le problème :** Un professeur se fait voler son compte d'accès.
* **La Sécurité (Authentification JWT Fort) :** Le système de connexion, conçu intégralement *"From Scratch"* utilise la norme **JSON Web Tokens (JWT)**.
Le cookie est marqué `HttpOnly` (inaccessible par un script malveillant type XSS côté client) et `SameSite=Strict`.

---

## 4. Stack Technique & Choix Architecturaux

Pourquoi du C++ pour un site Web et pas du PHP ou Node.js ?
Afin qu'un Raspberry Pi ou un serveur aux ressources microscopiques puisse orchestrer les connexions massives et le flux "Live" avec 0 ms de délai.

### 🔧 Architecture Technique
- **Noyau Réseau :** Sockets systémiques sous Linux/Windows (POSIX/Winsock2). Pas de serveur lourd type Apache/NGINX (sauf en tant que Reverse Proxy global fourni par Railway).
- **Backend Logique :** C++17 avec un pool de threads. Le serveur écoute les requêtes HTTP et redirige vers un Routeur interne compilé.
- **Le Flux Temps-Réel (Nouveau standard) :**
  Pour outrepasser les règles de blocage des Proxys (SSE bufferisé ou timeout), le projet a mis au point une mécanique de **Long Polling Périodique API**. Le Javascript lance un `fetch()` silencieux toutes les 3 secondes : `GET /api/events?last_id=...`. Ainsi, l'interface du prof reste vivante 24h/24 sans déconnexion.
- **Base de Données :** `SQLite3`. Intégré dans l'arbre de compilation. Un fichier unique `.db`. Incassable, facile à sauvegarder.
- **Design Interface (UI/UX) :**
  - Philosophie *Glassmorphism* & *Dark Mode* par défaut.
  - Formulaires animés, chargements doux (Spinners), système de fenêtres déportées (Modales natives HTML/CSS).
  - Couleurs thématiques de l'Instituts.

---

## 5. Déploiement Continue (CI/CD) sur Railway

Ce projet tourne dans les nuages (Cloud) et se met à jour **automatiquement**.

1. Mettez le code à jour sur votre ordinateur local.
2. Créez un commit : `git commit -m "Explication de la mise à jour"`
3. Poussez sur GitHub : `git push`

⚡ **Railway intercepte le push.**
Grâce au fichier `Dockerfile` minutieusement configuré à la racine, Railway va allouer un micro-ordinateur sous Linux Debian (`bookworm-slim`), puis lancer la commande suivante :
```bash
g++ -std=c++17 src/*.cpp libs/sqlite3.c main.cpp -pthread -o smartpresence_server
```
Une fois le composant compilé, il lance le fichier binaire `.exe` (ou binaire Unix) de l'application. Ce process requiert moins de `25 Mb` de RAM totale pour opérer l'ensemble de l'université.

### Variables d'environnement critiques
| Variable | Rôle |
|---|---|
| `PORT` | Fourni nativement par les routeurs Railway. En interne le C++ va lier son port d'écoute sur ce port dynamique (fallback 8080 en local). |

---

*Développé avec la plus haute optimisation possible pour l'IUA. Mêlant la puissance de calcul brut native C++ à l'élégance du Web moderne.*
