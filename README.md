<div align="center">
  <h1>🎓 SMARTPRESENCE CI — Institut Universitaire d'Abidjan (IUA)</h1>
  <p><strong>L'Infrastructure Biométrique de Suivi des Présences la plus avancée et hermétique au monde.</strong></p>

  ![C++](https://img.shields.io/badge/Core_Server-C++17-00599C?logo=c%2B%2B&style=for-the-badge)
  ![Python](https://img.shields.io/badge/AI_Microservice-Python_FastAPI-009688?logo=fastapi&style=for-the-badge)
  ![InsightFace](https://img.shields.io/badge/Biométrie-InsightFace_ArcFace-FF4500?style=for-the-badge)
  ![Railway](https://img.shields.io/badge/Hébergement-Railway_Docker-131415?logo=railway&style=for-the-badge)
</div>

---

**SMARTPRESENCE CI** est une véritable forteresse logicielle conçue spécialement pour réguler, surveiller et orchestrer les flux de présences académiques au sein des grands amphithéâtres de l'**Institut Universitaire d'Abidjan (IUA)**.

Là où les systèmes classiques par "QR Code simple" échouent pitoyablement face à la triche étudiante (partage de liens WhatsApp, amis scannant à distance), ce système déploie une **Architecture Microservices Hautes-Performances** couplée à une intelligence artificielle de grade militaire.

---

## 🎯 Le Défi Académique (Les Problèmes Constatés)

La gestion des présences dans les grands amphithéâtres est historiquement un enfer logistique et pédagogique. Les méthodes traditionnelles et même les récentes solutions numériques font face à des vulnérabilités critiques :

1. **La Perte de Temps Pédagogique :** Un appel nominal ou le passage d'une liste papier dans un amphi de 300 places ampute parfois jusqu'à 30 minutes de temps de cours effectif.
2. **La Fraude de Camaraderie (Signer pour son ami) :** Les fiches de présence papier sont systématiquement falsifiées par les étudiants solidaires.
3. **Le Hacker du QR Code (Fraude à Distance) :** Certaines universités utilisent des QR Codes simples. L'astuce des étudiants consiste à **prendre le QR Code en photo** et à l'envoyer dans le groupe WhatsApp de la promotion. Résultat : les étudiants restés au lit valident leur présence.
4. **L'Usurpation Visuelle (Spoofing) :** Face aux premières solutions de reconnaissance faciale, la triche a évolué : il suffisait de présenter une photo imprimée ou une tablette montrant le visage de l'ami absent devant la caméra pour valider sa présence.

---

## ⚡ La Solution Apportée par SMARTPRESENCE CI

Face à ce cahier des charges exigeant, **SMARTPRESENCE CI** apporte une réponse technologique implacable. Ce produit résout 100% de ces problématiques en utilisant la pleine puissance de l'Infrastructure Matérielle (Capteurs) et de l'Intelligence Artificielle.

* **Digitalisation Instantanée (10 secondes) :** Le professeur projette un QR Code sur l'écran de l'amphithéâtre. 500 étudiants peuvent le scanner en même temps (scalabilité C++ garantie).
* **Éradication de la Fraude à Distance (WhatsApp) :** Le QR Code s'autodétruit silencieusement toutes les 15 secondes. L'ami lointain flashera un code expiré. De plus, le **Verrou GPS algorithmique** s'assure que le téléphone est physiquement situé dans les coordonnées de l'amphithéâtre.
* **Biométrie de Grade Bancaire Inviolable :** Fini les mots de passe. C'est le visage exclusif de l'étudiant qui fait foi avec l'intégration du monstre algorithmique de vision par ordinateur mondialement connu : **InsightFace ArcFace**.
* **Test de "Liveness" (Preuve de vie humaine) :** L'intelligence du système détruit la triche par photo papier en exigeant de l'étudiant une action cognitive humaine aléatoire avant de numériser son empreinte (ex: *Tourner la tête à gauche, puis à droite*). La caméra locale lit les mouvements 3D de la tête et rejette les surfaces planes.

---

## 🏛️ Architecture Multinucléaire (Microservices)

Le système a abandonné les serveurs PHP ou Node.js basiques au profit d'une communication entre deux noyaux ultra-rapides :

1. ⚙️ **Le Contrôleur Maître (C++ Natif)**
   - Développé en C++17 pur (sans framework lourd de type Apache), il gère les milliers de connexions parallèles des étudiants.
   - Il pilote les Sockets Temps-Réel (SSE), l'authentification JWT, et interroge la base de données intégrée SQLite3 avec une protection absolue contre les injections SQL (Prepared Statements).
   
2. 🧠 **Le Cerveau IA Biométrique (Python & FastAPI)**
   - Connecté discrètement en arrière-plan (Localhost API), ce serveur fait tourner **InsightFace (MobileFaceNet)** par-dessus **OpenCV**.
   - Lorsqu'une capture de visage arrive, il analyse sa topologie parfaite pour en ressortir un identifiant d'ADN Facial : un code array inviolable de **512 dimensions**.

---

## 🛡️ Les 9 Boucliers de Sécurité Anti-Fraude (Impiratables)

L'étudiant qui souhaite falsifier ou contourner sa présence se heurtera de plein fouet à ces calculs mathématiques :

| Bouclier | Description de la technologie de Protection |
| :--- | :--- |
| **🌐 Verrou GPS Stationnaire** | Le système capte directement la position initiale dès le scan du QR. Si l'étudiant se déplace physiquement (ex: en marchant, en voiture) avant de réussir le test de reconnaissance faciale, le mouvement est détecté et la validation est complètement bloquée. |
| **🤖 Preuve de vie (Liveness)** | Le serveur IA n'accepte *aucune* photo figée. En direct, l'étudiant doit effectuer des mouvements dynamiques (**Tourner la tête, Sourire...**) via son appareil. Une photo sur papier est rejetée avec un avertissement. |
| **👁️ Empreinte à 512-Dimensions** | Une fois le Liveness passé, la photo est transformée en Vecteur 512D. Le serveur C++ va comparer à **0.55 de Marge Cosinus (Cosine Distance)** cette image avec le matricule de l'étudiant enregistré le 1er jour pour s'assurer que c'est bien la même personne physique. |
| **🕵️‍♂️ Blocage "Bon Samaritain"** | Un étudiant possède et confirme son propre visage mais remplace son matricule par celui de son ami ? Le C++ fouille la Session : *"Ce visage a déjà marqué sa présence sous un autre nom aujourd'hui"*. **Triche bloquée instantanément.** |
| **⏳ Horloge Atomique QR** | Le code projeté par le professeur meurt toutes les 15 secondes. L'ami situé à son domicile sur WhatsApp recevra une photo du QR code illisible passée ce laps de temps. |
| **📱 Sigle Matériel Limité** | L'empreinte numérique du navigateur (User-Agent + LocalStorage) bloque tout tentative d'utiliser le même téléphone portable pour cocher plusieurs étudiants. |

---

## 🚀 Interface Utilisateur Premium
* Un Dashboard "Live" et interactif pour le Professeur (pas de rechargement manuel).
* **0 latence** : tout s'affiche instantanément grâce au C++ connecté de manière asynchrone HTTP (SSE Long-Polling).
* Scanner mobile épuré et optimisé. Les calculs lourds ont été externalisés de JavaScript vers le serveur Python, offrant une fluidité parfaite même sur les téléphones étudiants les plus bas de gamme.

---

## ⚙️ Exécution & Déploiement

Déployé automatiquement sur le Cloud **Railway** via une intégration continue (CI) Docker `debian:bookworm-slim`.

**Le script de lancement Docker initie les deux mondes parallèles :**
```bash
# Lancement de l'I.A. (Local Port 5000)
cd ai_service && uvicorn main:app --host 127.0.0.1 --port 5000 &

# Lancement du Contrôleur (Port public)
./smartpresence_server
```

> **Auteur** : Projet propulsé et structuré pour la révolution technologique à l'IUA.
> **Confidentialité** : Ce système a l'autorisation requise de la base de données étudiante pour opérer sa biométrie à un niveau universitaire restreint.
