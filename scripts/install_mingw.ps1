# ============================================================
# install_mingw.ps1 — Installation de MinGW-w64 via winget
# SMARTPRESENCE CI
# ============================================================
# Executer en tant qu'administrateur :
# powershell -ExecutionPolicy Bypass -File .\scripts\install_mingw.ps1

Write-Host "Installation de MinGW-w64 (compilateur C++)..." -ForegroundColor Cyan

# Methode 1 : via winget (Windows 11 / Windows 10 recent)
try {
    winget install --id MSYS2.MSYS2 --silent --accept-source-agreements --accept-package-agreements
    Write-Host "MSYS2 installe. Lancez MSYS2 et executez : pacman -S mingw-w64-x86_64-gcc" -ForegroundColor Green
    exit 0
} catch {}

# Methode 2 : Telecharger MinGW directement
Write-Host "Telechargement de MinGW-w64..." -ForegroundColor Yellow
$url  = "https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/x86_64-13.2.0-release-win32-seh-msvcrt-rt_v11-rev1.7z"
$out  = "$env:TEMP\mingw64.7z"
Write-Host "URL: $url"
Write-Host "Cette methode necessite 7-Zip. Utilisez plutot :"
Write-Host ""
Write-Host "OPTION RECOMMANDEE :" -ForegroundColor Green
Write-Host "1. Telechargez WinLibs : https://winlibs.com/" -ForegroundColor Yellow
Write-Host "   -> Choisissez GCC 13 Win64 (UCRT runtime) ZIP"
Write-Host "2. Extrayez dans C:\mingw64"
Write-Host "3. Ajoutez C:\mingw64\bin au PATH Windows"
Write-Host "4. Relancez le terminal"
Write-Host ""
Write-Host "COMPILATION (apres installation g++) :" -ForegroundColor Cyan
Write-Host 'g++ main.cpp src/*.cpp libs/sqlite3.c -o smartpresence.exe -lpthread -lws2_32 -std=c++17'
