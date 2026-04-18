// ============================================================
// seeder.cpp — Données de test SMARTPRESENCE CI
// Institut Universitaire d'Abidjan — Côte d'Ivoire
// ============================================================
#include "seeder.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <string>

namespace seeder {

bool already_seeded(Database& db) {
    // Vérifier s'il y a déjà des utilisateurs
    // (utilise la méthode find_user_by_email)
    auto admin = db.find_user_by_email("admin@smartpresence-ci.com");
    return admin.id > 0;
}

void seed(Database& db) {
    if (already_seeded(db)) {
        std::cout << "[Seeder] Données déjà présentes, seeding ignoré." << std::endl;
        return;
    }

    std::cout << "[Seeder] Insertion des données de test..." << std::endl;

    // ──────────────────────────────────────────────────────
    // 1. Université
    // ──────────────────────────────────────────────────────
    db.exec(
        "INSERT OR IGNORE INTO universites (id, nom, ville)"
        " VALUES (1, 'Institut Universitaire d''Abidjan', 'Abidjan')"
    );
    std::cout << "[Seeder] ✓ Université créée" << std::endl;

    // ──────────────────────────────────────────────────────
    // 2. Filières
    // ──────────────────────────────────────────────────────
    struct FiliereData { std::string nom; std::string niveau; };
    std::vector<FiliereData> filieres = {
        {"Informatique",   "L2"},
        {"Mathématiques",  "L3"},
        {"Gestion",        "M1"}
    };

    for (auto& f : filieres) {
        Filiere fil;
        fil.nom           = f.nom + " " + f.niveau;
        fil.niveau        = f.niveau;
        fil.universite_id = 1;
        db.create_filiere(fil);
    }
    std::cout << "[Seeder] ✓ 3 filières créées" << std::endl;

    // Récupérer les IDs filières
    auto fil_list = db.list_filieres(1);
    int fid_info = 1, fid_math = 2, fid_gest = 3;
    for (auto& f : fil_list) {
        if (f.nom.find("Informatique") != std::string::npos) fid_info = f.id;
        else if (f.nom.find("Math") != std::string::npos)   fid_math = f.id;
        else if (f.nom.find("Gestion") != std::string::npos) fid_gest = f.id;
    }

    // ──────────────────────────────────────────────────────
    // 3. Comptes admin + enseignants
    // ──────────────────────────────────────────────────────
    // Admin
    {
        Utilisateur admin;
        admin.email         = "admin@smartpresence-ci.com";
        admin.nom           = "KONAN";
        admin.prenom        = "Administrateur";
        admin.role          = "admin";
        admin.password_hash = utils::hash_password("Admin2024!");
        admin.universite_id = 1;
        db.create_utilisateur(admin);
    }

    // Enseignant 1 : Prof Koné
    {
        Utilisateur prof;
        prof.email         = "prof.kone@iua.ci";
        prof.nom           = "KONÉ";
        prof.prenom        = "Amadou";
        prof.role          = "enseignant";
        prof.password_hash = utils::hash_password("Prof2024!");
        prof.universite_id = 1;
        db.create_utilisateur(prof);
    }

    // Enseignant 2 : Dr. Diallo
    {
        Utilisateur prof2;
        prof2.email         = "dr.diallo@iua.ci";
        prof2.nom           = "DIALLO";
        prof2.prenom        = "Mariama";
        prof2.role          = "enseignant";
        prof2.password_hash = utils::hash_password("Prof2024!");
        prof2.universite_id = 1;
        db.create_utilisateur(prof2);
    }
    std::cout << "[Seeder] ✓ 3 comptes utilisateurs créés" << std::endl;

    // Récupérer les IDs des enseignants
    auto prof1 = db.find_user_by_email("prof.kone@iua.ci");
    auto prof2 = db.find_user_by_email("dr.diallo@iua.ci");
    int prof1_id = prof1.id > 0 ? prof1.id : 2;
    int prof2_id = prof2.id > 0 ? prof2.id : 3;

    // ──────────────────────────────────────────────────────
    // 4. 50 Étudiants avec noms africains réalistes
    // ──────────────────────────────────────────────────────
    struct StudentData {
        std::string nom;
        std::string prenom;
        int filiere_id;
    };

    std::vector<StudentData> students = {
        // Informatique L2 (20 étudiants)
        {"KOFFI",   "Ange-Désiré",   fid_info},
        {"KOUASSI", "Awa",            fid_info},
        {"BAMBA",   "Seydou",         fid_info},
        {"OUÉDRAOGO","Fatou",         fid_info},
        {"TRAORÉ",  "Ibrahima",       fid_info},
        {"COULIBALY","Mariam",        fid_info},
        {"N'GUESSAN","Kofi",          fid_info},
        {"DIABY",   "Issa",           fid_info},
        {"SORO",    "Aminata",        fid_info},
        {"TOURÉ",   "Mamadou",        fid_info},
        {"YAO",     "Evelyne",        fid_info},
        {"KONAN",   "Serge",          fid_info},
        {"ASSI",    "Patricia",       fid_info},
        {"BROU",    "Ghislain",       fid_info},
        {"DEMBÉLÉ", "Kadiatou",       fid_info},
        {"FOFANA",  "Souleymane",     fid_info},
        {"KEITA",   "Djénéba",        fid_info},
        {"SANOGO",  "Bassirou",       fid_info},
        {"DIARRA",  "Oumou",          fid_info},
        {"CAMARA",  "Tanguy",         fid_info},
        // Mathématiques L3 (15 étudiants)
        {"KONÉ",    "Adja",           fid_math},
        {"DIABATÉ", "François",       fid_math},
        {"CISSÉ",   "Rokia",          fid_math},
        {"BAKAYOKO","Cheick",         fid_math},
        {"COULIBALY","Tenin",         fid_math},
        {"BALDE",   "Alpha",          fid_math},
        {"DIALLO",  "Kadidiatou",     fid_math},
        {"KOUYATÉ", "Bafodé",         fid_math},
        {"SYLLA",   "Hadja",          fid_math},
        {"SOW",     "Boubacar",       fid_math},
        {"BARRY",   "Aminata",        fid_math},
        {"SALL",    "Ousmane",        fid_math},
        {"NDIAYE",  "Fatimata",       fid_math},
        {"MBAYE",   "Ibou",           fid_math},
        {"FALL",    "Rokhaya",        fid_math},
        // Gestion M1 (15 étudiants)
        {"ADJOUMANI","Christelle",    fid_gest},
        {"AKÉ",     "Patrick",        fid_gest},
        {"BONY",    "Roseline",       fid_gest},
        {"DAGOU",   "Emmanuel",       fid_gest},
        {"EBA",     "Sandrine",       fid_gest},
        {"GNAGA",   "Franck",         fid_gest},
        {"KASSI",   "Jocelyne",       fid_gest},
        {"KOUAKOU", "Evariste",       fid_gest},
        {"MÉITÉ",   "Hortense",       fid_gest},
        {"N'DA",    "Théophile",      fid_gest},
        {"OUATTARA","Euphrasie",      fid_gest},
        {"ZADI",    "Constance",      fid_gest},
        {"AHOUA",   "Maxime",         fid_gest},
        {"EHUI",    "Blandine",       fid_gest},
        {"KACOU",   "Romuald",        fid_gest},
    };

    int student_index = 1;
    for (auto& s : students) {
        Etudiant e;
        e.matricule      = utils::random_matricule(student_index++);
        e.nom            = s.nom;
        e.prenom         = s.prenom;
        e.email          = "";  // Optionnel
        e.code_personnel = utils::random_code(6);
        e.filiere_id     = s.filiere_id;
        e.universite_id  = 1;
        db.create_etudiant(e);
    }
    std::cout << "[Seeder] ✓ 50 étudiants créés" << std::endl;

    // ──────────────────────────────────────────────────────
    // 5. 5 Sessions passées avec présences simulées
    // ──────────────────────────────────────────────────────
    long long now = utils::now_unix();

    struct SessionSeed {
        std::string titre;
        std::string matiere;
        int enseignant_id;
        int filiere_id;
        long long offset_days;  // jours dans le passé
        int nb_presents;
    };

    std::vector<SessionSeed> sessions = {
        {"Algorithmique avancée - CM1",    "Algorithmique",     prof1_id, fid_info, 7,  17},
        {"Programmation C++ - TP3",        "Prog. Orientée Obj",prof1_id, fid_info, 5,  15},
        {"Analyse réelle - TD5",           "Analyse Mathématique",prof2_id,fid_math, 3,  12},
        {"Comptabilité générale - CM2",    "Comptabilité",      prof2_id, fid_gest, 2,  10},
        {"Structures de données - TP1",    "Informatique",      prof1_id, fid_info, 1,  18},
    };

    // Chercher les étudiants pour les présences
    auto etudiants_info = db.list_etudiants(fid_info, 1, 20);
    auto etudiants_math = db.list_etudiants(fid_math, 1, 15);
    auto etudiants_gest = db.list_etudiants(fid_gest, 1, 15);

    for (auto& s : sessions) {
        SessionCours sc;
        sc.titre        = s.titre;
        sc.matiere      = s.matiere;
        sc.enseignant_id= s.enseignant_id;
        sc.filiere_id   = s.filiere_id;
        sc.date_debut   = now - (s.offset_days * 86400);
        sc.statut       = "fermee";
        sc.qr_token     = "PAST_SESSION";
        sc.qr_expire_at = 0;

        int sid = db.create_session(sc);
        if (sid < 0) continue;

        // Fermer la session
        db.update_session_statut(sid, "fermee");

        // Ajouter des présences simulées
        std::vector<Etudiant>* etudiant_list = nullptr;
        if (s.filiere_id == fid_info) etudiant_list = &etudiants_info;
        else if (s.filiere_id == fid_math) etudiant_list = &etudiants_math;
        else etudiant_list = &etudiants_gest;

        int count = 0;
        for (auto& e : *etudiant_list) {
            if (count >= s.nb_presents) break;
            Presence p;
            p.session_id  = sid;
            p.etudiant_id = e.id;
            p.horodatage  = sc.date_debut + 5 + (count * 30);
            p.ip_client   = "192.168.1." + std::to_string(10 + count);
            p.valide      = true;
            db.mark_presence(p);
            count++;
        }
    }
    std::cout << "[Seeder] ✓ 5 sessions passées créées avec présences" << std::endl;

    std::cout << "[Seeder] ✅ Seeding terminé avec succès!" << std::endl;
    std::cout << "[Seeder] 👤 Admin    : admin@smartpresence-ci.com / Admin2024!" << std::endl;
    std::cout << "[Seeder] 👤 Enseignant : prof.kone@iua.ci / Prof2024!" << std::endl;
}

} // namespace seeder
