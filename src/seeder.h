// ============================================================
// seeder.h — Insertion des données de test
// SMARTPRESENCE CI
// ============================================================
#pragma once
#include "database.h"

namespace seeder {

// Insérer toutes les données de test si les tables sont vides
// - 1 université, 3 filières, 2 enseignants, 50 étudiants
// - 5 sessions passées avec présences simulées
// - Compte admin + compte enseignant
void seed(Database& db);

// Vérifier si le seeding a déjà été effectué
bool already_seeded(Database& db);

} // namespace seeder
