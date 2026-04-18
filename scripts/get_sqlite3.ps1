# ============================================================
# get_sqlite3.ps1 — Téléchargement de SQLite3 amalgamation
# SMARTPRESENCE CI
# ============================================================
# Exécuter : .\scripts\get_sqlite3.ps1

Write-Host "╔══════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  SMARTPRESENCE CI — Setup SQLite3         ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Créer le dossier libs si inexistant
$libsDir = Join-Path $PSScriptRoot "..\libs"
if (-not (Test-Path $libsDir)) {
    New-Item -ItemType Directory -Path $libsDir | Out-Null
}

# Vérifier si sqlite3.c existe déjà
$sqlite3c = Join-Path $libsDir "sqlite3.c"
$sqlite3h = Join-Path $libsDir "sqlite3.h"

if ((Test-Path $sqlite3c) -and (Test-Path $sqlite3h)) {
    Write-Host "✓ SQLite3 déjà présent dans libs/" -ForegroundColor Green
    exit 0
}

Write-Host "Téléchargement de SQLite3 amalgamation..." -ForegroundColor Yellow

# URL de l'amalgamation SQLite3 (version 3.45.1)
$zipUrl  = "https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip"
$zipPath = Join-Path $env:TEMP "sqlite3-amalgamation.zip"
$extractPath = Join-Path $env:TEMP "sqlite3-extract"

try {
    # Télécharger
    Write-Host "→ Téléchargement depuis sqlite.org..."
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing

    # Extraire
    Write-Host "→ Extraction..."
    if (Test-Path $extractPath) { Remove-Item $extractPath -Recurse -Force }
    Expand-Archive -Path $zipPath -DestinationPath $extractPath

    # Trouver les fichiers
    $sqlite3cSrc = Get-ChildItem -Path $extractPath -Recurse -Filter "sqlite3.c" | Select-Object -First 1
    $sqlite3hSrc = Get-ChildItem -Path $extractPath -Recurse -Filter "sqlite3.h" | Select-Object -First 1

    if ($sqlite3cSrc -and $sqlite3hSrc) {
        Copy-Item $sqlite3cSrc.FullName $sqlite3c
        Copy-Item $sqlite3hSrc.FullName $sqlite3h
        Write-Host "✓ sqlite3.c copié ($([Math]::Round((Get-Item $sqlite3c).Length / 1KB)) KB)" -ForegroundColor Green
        Write-Host "✓ sqlite3.h copié" -ForegroundColor Green
    } else {
        throw "Fichiers sqlite3.c ou sqlite3.h introuvables dans l'archive"
    }

    # Nettoyage
    Remove-Item $zipPath -Force
    Remove-Item $extractPath -Recurse -Force

} catch {
    Write-Host "✗ Erreur : $_" -ForegroundColor Red
    Write-Host "`nTéléchargement manuel requis :" -ForegroundColor Yellow
    Write-Host "1. Allez sur https://www.sqlite.org/download.html"
    Write-Host "2. Téléchargez 'sqlite-amalgamation-XXXXXXXX.zip'"
    Write-Host "3. Extrayez sqlite3.c et sqlite3.h dans le dossier libs/"
    exit 1
}

Write-Host "`n✅ SQLite3 prêt ! Vous pouvez maintenant compiler :" -ForegroundColor Green
Write-Host 'g++ main.cpp src/*.cpp libs/sqlite3.c -o smartpresence.exe -lpthread -lws2_32 -std=c++17' -ForegroundColor White
