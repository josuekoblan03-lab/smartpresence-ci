Write-Host 'Téléchargement de GCC/MinGW (WinLibs) en cours...' -ForegroundColor Cyan
$zipUrl = 'https://github.com/brechtsanders/winlibs_mingw/releases/download/13.2.0-16.0.6-11.0.1-ucrt-r2/winlibs-x86_64-posix-seh-gcc-13.2.0-llvm-16.0.6-mingw-w64ucrt-11.0.1-r2.zip'
$zipPath = "$env:TEMP\mingw64.zip"

Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing

Write-Host "Extraction en cours vers C:\mingw64 (Patientez...)..." -ForegroundColor Yellow
Expand-Archive -Path $zipPath -DestinationPath 'C:\' -Force

Write-Host "Nettoyage..." -ForegroundColor Gray
Remove-Item $zipPath -Force

Write-Host 'Ajout aux variables locales (PATH)...' -ForegroundColor Yellow
$userPath = [Environment]::GetEnvironmentVariable('PATH', 'User')
if ($userPath -notmatch 'mingw64') { 
    [Environment]::SetEnvironmentVariable('PATH', $userPath + ';C:\mingw64\bin', 'User') 
}

Write-Host 'Installation réussie !' -ForegroundColor Green
