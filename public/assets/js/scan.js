/* ============================================================
   scan.js Ã¢â‚¬â€ Logique page scan (prÃƒÂ©sence, QR, countdown)
   SMARTPRESENCE CI
   ============================================================ */

let currentSessionId = null;
let countdownInterval = null;
let qrCodeInstance = null;
let qrExpireAt = 0;

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// HARDWARE FINGERPRINTING
// Lit les caractÃƒÂ©ristiques PHYSIQUES du tÃƒÂ©lÃƒÂ©phone.
// Ces donnÃƒÂ©es NE CHANGENT PAS : ni en incognito, ni en vidant
// le cache, ni en changeant de navigateur (Chrome/Firefox/Safari).
// On les combine pour crÃƒÂ©er une "empreinte matÃƒÂ©rielle" unique.
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬

/**
 * CrÃƒÂ©e un hash numÃƒÂ©rique simple (djb2) ÃƒÂ  partir d'une chaÃƒÂ®ne.
 * Retourne une chaÃƒÂ®ne hexadÃƒÂ©cimale de 8 caractÃƒÂ¨res.
 */
function hashString(str) {
  let hash = 5381;
  for (let i = 0; i < str.length; i++) {
    hash = ((hash << 5) + hash) ^ str.charCodeAt(i);
    hash = hash & hash; // Conversion en entier 32-bit signÃƒÂ©
  }
  // Convertir en hexa non-signÃƒÂ© sur 8 caractÃƒÂ¨res
  return (hash >>> 0).toString(16).padStart(8, '0').toUpperCase();
}

/**
 * GÃƒÂ©nÃƒÂ¨re une empreinte canvas.
 * Le rendu du texte varie lÃƒÂ©gÃƒÂ¨rement selon le GPU et les polices
 * installÃƒÂ©es sur l'appareil.
 */
function getCanvasFingerprint() {
  try {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    canvas.width = 200;
    canvas.height = 50;
    ctx.textBaseline = 'top';
    ctx.font = '14px Arial';
    ctx.fillStyle = '#f60';
    ctx.fillRect(125, 1, 62, 20);
    ctx.fillStyle = '#069';
    ctx.fillText('SmartPresenceÃ°Å¸Å½â€œIUAÃ¢Å“â€œ', 2, 15);
    ctx.fillStyle = 'rgba(102,204,0,0.7)';
    ctx.fillText('Empreinte2024', 4, 25);
    return hashString(canvas.toDataURL());
  } catch (e) {
    return 'CNVS_ERR';
  }
}

/**
 * Construit l'empreinte matÃƒÂ©rielle complÃƒÂ¨te du tÃƒÂ©lÃƒÂ©phone.
 * Combine : rÃƒÂ©solution ÃƒÂ©cran, CPU cores, RAM, timezone, langue,
 *           profondeur de couleur, plateforme et canvas fingerprint.
 *
 * RÃƒâ€°SULTAT : une chaÃƒÂ®ne hexadÃƒÂ©cimale STABLE peu importe le navigateur.
 */
function getHardwareFingerprint() {
  // CaractÃƒÂ©ristiques physiques du tÃƒÂ©lÃƒÂ©phone
  const ecranLargeur   = screen.width;          // Ex: 1080
  const ecranHauteur   = screen.height;          // Ex: 2400
  const ecranProfondeur= screen.colorDepth;      // Ex: 24 bits
  const nbCPU          = navigator.hardwareConcurrency || 0;  // Ex: 8 processeurs
  const ram            = navigator.deviceMemory  || 0;        // Ex: 6 GB (Chrome)
  const fuseau         = Intl.DateTimeFormat().resolvedOptions().timeZone; // Ex: Africa/Abidjan
  const langue         = navigator.language || 'fr';          // Ex: fr-CI
  const plateforme     = navigator.platform || '';            // Ex: Linux armv8l

  // Empreinte du rendu graphique (GPU)
  const empreinteCanvas = getCanvasFingerprint();

  // Assemblage en une chaÃƒÂ®ne de donnÃƒÂ©es combinÃƒÂ©es
  const donnees = [
    `ECRAN:${ecranLargeur}x${ecranHauteur}x${ecranProfondeur}`,
    `CPU:${nbCPU}`,
    `RAM:${ram}`,
    `TZ:${fuseau}`,
    `LANG:${langue}`,
    `PLAT:${plateforme}`,
    `CANVAS:${empreinteCanvas}`
  ].join('|');

  // Hash final = "Adresse MAC Virtuelle" du tÃƒÂ©lÃƒÂ©phone
  const empreinte = 'HW_' + hashString(donnees) + '_' + hashString(empreinteCanvas);

  console.log('[SmartPresence] Empreinte matÃƒÂ©rielle:', empreinte, '\nÃ¢â€ â€™ DonnÃƒÂ©es:', donnees);
  return empreinte;
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Initialisation de la page
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
document.addEventListener('DOMContentLoaded', async () => {
  loadFaceModels();
  // Lire les paramÃƒÂ¨tres d'URL
  const params = new URLSearchParams(window.location.search);
  const tokenFromUrl = params.get('token');
  const sidFromUrl   = params.get('sid');

  // PrÃƒÂ©remplir token
  if (tokenFromUrl) {
    document.getElementById('qr-token-input').value = tokenFromUrl;
  }

  // PrÃƒÂ©remplir session_id
  if (sidFromUrl) {
    currentSessionId = parseInt(sidFromUrl);
    document.getElementById('manual-session-id').value = sidFromUrl;
    await loadSessionInfo(currentSessionId);
  }

  // Formulaire prÃƒÂ©sence
  document.getElementById('presence-form').addEventListener('submit', submitPresence);

  // Majuscules automatiques sur le matricule
  const savedMatricule = localStorage.getItem('sp_matricule');
  if (savedMatricule) { document.getElementById('matricule').value = savedMatricule; }

  document.getElementById('matricule').addEventListener('input', function() {
    this.value = this.value.toUpperCase();
  });

  // Chiffres seulement pour le PIN
  document.getElementById('code-personnel').addEventListener('input', function() {
    this.value = this.value.replace(/[^0-9]/g, '').slice(0, 6);
  });
});

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Charger info de session
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
async function loadSessionInfo(sid) {
  sid = sid || parseInt(document.getElementById('manual-session-id').value);
  if (!sid || sid <= 0) {
    showToast('warning', 'Entrez un numÃƒÂ©ro de session valide');
    return;
  }

  currentSessionId = sid;

  try {
    // RÃƒÂ©cupÃƒÂ©rer le token QR de la session
    const res  = await fetch(`/api/qrcode/${sid}/token`);
    const data = await res.json();

    if (!data.success) {
      showToast('error', 'Session introuvable ou fermÃƒÂ©e');
      return;
    }

    // Afficher infos session
    const bannerEl = document.getElementById('session-banner');
    bannerEl.classList.remove('hidden');

    const sessionRes  = await fetch(`/api/sessions/${sid}`);
    const sessionData = await sessionRes.json();

    if (sessionData.success) {
      const s = sessionData.session;
      document.getElementById('session-name').textContent = s.titre;
      document.getElementById('session-info').textContent =
        s.filiere_nom + ' Ã‚Â· ' + s.enseignant_nom;

      if (s.statut !== 'ouverte') {
        document.getElementById('session-info').textContent += ' Ã¢â‚¬â€ Ã¢â€ºâ€ Session fermÃƒÂ©e';
        showToast('warning', 'Cette session est fermÃƒÂ©e.');
        return;
      }
    }

    // Afficher le token et le QR Code
    displayToken(data.token, data.expires_in, data.expire_at);
    qrExpireAt = data.expire_at;

    // PrÃƒÂ©remplir le champ token
    document.getElementById('qr-token-input').value = data.token;

    // DÃƒÂ©marrer le compte ÃƒÂ  rebours
    startCountdown(data.expires_in, sid);

    // Souscrire aux SSE pour renouvellement automatique
    listenQRRefresh(sid);

  } catch(e) {
    console.error('[Scan] Erreur chargement session:', e);
    showToast('error', 'Erreur de chargement de la session');
  }
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Afficher token + QR Code JS
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function displayToken(token, expiresIn, expireAt) {
  const tokenZone = document.getElementById('token-zone');
  tokenZone.classList.remove('hidden');

  const tokenDisplay = document.getElementById('token-display');
  tokenDisplay.textContent = token;

  // GÃƒÂ©nÃƒÂ©rer le QR Code avec qrcodejs
  const canvas = document.getElementById('qr-code-canvas');
  canvas.innerHTML = '';

  const scanUrl = `${window.location.origin}/scan.html?token=${encodeURIComponent(token)}&sid=${currentSessionId}`;

  if (typeof QRCode !== 'undefined') {
    qrCodeInstance = new QRCode(canvas, {
      text: scanUrl,
      width: 200,
      height: 200,
      colorDark: '#000000',
      colorLight: '#ffffff',
      correctLevel: QRCode.CorrectLevel.H
    });
  } else {
    canvas.innerHTML = `<img src="/api/qrcode/${currentSessionId}" width="200" height="200" style="border-radius:8px;">`;
  }

  document.getElementById('qr-token-input').value = token;
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Compte ÃƒÂ  rebours renouvellement QR
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function startCountdown(expiresIn, sessionId) {
  if (countdownInterval) clearInterval(countdownInterval);

  const countdownEl  = document.getElementById('countdown');
  const countdownBar = document.getElementById('countdown-bar');
  const totalSeconds = 30;
  let remaining = expiresIn > 0 ? expiresIn : totalSeconds;

  countdownBar.style.width = (remaining / totalSeconds * 100) + '%';

  countdownInterval = setInterval(async () => {
    remaining--;
    if (countdownEl) countdownEl.textContent = remaining + 's';

    const pct = Math.max(0, remaining / totalSeconds * 100);
    if (countdownBar) {
      countdownBar.style.width = pct + '%';
      countdownBar.style.background = remaining < 10 ?
        'linear-gradient(90deg,#ef4444,#dc2626)' : 'var(--gradient-primary)';
    }

    if (remaining <= 0) {
      clearInterval(countdownInterval);
      await refreshQRToken(sessionId);
    }
  }, 1000);
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// RafraÃƒÂ®chir le token QR
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
async function refreshQRToken(sessionId) {
  try {
    const res  = await fetch(`/api/qrcode/${sessionId}/token`);
    const data = await res.json();
    if (data.success) {
      displayToken(data.token, data.expires_in, data.expire_at);
      startCountdown(data.expires_in, sessionId);
      showToast('info', 'Ã°Å¸â€â€ž QR Code renouvelÃƒÂ© automatiquement');
    }
  } catch(e) {
    console.error('[QR] Erreur refresh:', e);
  }
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// SSE pour renouvellement QR
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function listenQRRefresh(sessionId) {
  const es = new EventSource('/api/sse/events');

  es.addEventListener('qr_refresh', (e) => {
    const data = JSON.parse(e.data);
    if (data.session_id === sessionId) {
      displayToken(data.token, data.expires_in, Date.now()/1000 + data.expires_in);
      startCountdown(data.expires_in, sessionId);
    }
  });

  es.onerror = () => es.close();
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// SOUMETTRE LA PRÃƒâ€°SENCE
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
async function submitPresence(e) {
  e.preventDefault();

  const matricule      = document.getElementById('matricule').value.trim();
  const codePersonnel  = document.getElementById('code-personnel').value.trim();
  const qrToken        = document.getElementById('qr-token-input').value.trim();
  const sessionId      = currentSessionId || parseInt(document.getElementById('manual-session-id').value) || 0;

  if (!matricule || !codePersonnel || !qrToken || !sessionId) {
    showError('Veuillez remplir correctement tous les champs.');
    return;
  }

  localStorage.setItem('sp_matricule', matricule);

    const payload = {
    matricule: matricule,
    code_personnel: codePersonnel,
    qr_token: qrToken,
    session_id: sessionId,
    device_id: getHardwareFingerprint()
  };

  document.getElementById('presence-form').classList.add('hidden');
  document.getElementById('biometric-scanner').classList.remove('hidden');
  document.querySelector('.scan-form-title').innerText = "VÃ©rification de sÃ©curitÃ©...";

  const overlay = document.getElementById('liveness-overlay');
  const overlayText = document.getElementById('bio-status');
  
  try {
    overlay.style.display = 'flex';
    overlayText.textContent = "Acquisition GPS de dÃ©part...";
    
    let gps1;
    try {
      gps1 = await getExactGPSPosition();
      document.getElementById('gps-status').classList.remove('hidden');
      document.getElementById('gps-status').innerText = '?? Position de dÃ©part capturÃ©e';
    } catch(err) {
      throw new Error("Localisation refusÃ©e. Activez le GPS.");
    }
    
    if (!faceModelsLoaded) {
      overlayText.textContent = "Chargement de l'intelligence artificielle...";
      while(!faceModelsLoaded) await new Promise(r => setTimeout(r, 500));
    }
    
    overlay.style.background = 'transparent'; 
    let faceDescriptorArray;
    try {
      faceDescriptorArray = await startBiometricScan();
    } catch(err) {
      throw new Error(err);
    }
    
    overlay.style.background = 'rgba(0,0,0,0.8)';
    overlayText.textContent = "Validation anti-dÃ©placement (GPS final)...";
    
    let gps2;
    try {
      gps2 = await getExactGPSPosition();
      document.getElementById('gps-status').innerText = '?? Position finale vÃ©rifiÃ©e !';
    } catch(err) {
      throw new Error("Impossible de vÃ©rifier la postion finale.");
    }
    
    overlayText.textContent = "Envoi sÃ©curisÃ© au serveur...";

    payload.gps_start_lat = gps1.lat.toString();
    payload.gps_start_lng = gps1.lng.toString();
    payload.gps_end_lat = gps2.lat.toString();
    payload.gps_end_lng = gps2.lng.toString();
    payload.face_descriptor = JSON.stringify(faceDescriptorArray);

    const response = await fetch('/api/presence/mark', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    const data = await response.json();
    document.getElementById('biometric-scanner').classList.add('hidden');

    if (data.success) {
      showSuccess(data.etudiant_nom, data.horodatage);
    } else {
      if (data.error && (data.error.includes('HW_') || data.error.includes('HW_') || data.error.includes('Empreinte') || data.error.includes('BiomÃ©trique'))) {
         showFraudBlocked(data.error);
      } else {
         showError(data.error || 'Erreur inconnue');
         document.getElementById('presence-form').classList.remove('hidden');
      }
    }

  } catch(err) {
    document.getElementById('biometric-scanner').classList.add('hidden');
    document.getElementById('presence-form').classList.remove('hidden');
    showError(err.message || 'Erreur inconnue');
  }
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// âœ… SUCCÃˆS
function showSuccess(nom, horodatage) {
  document.getElementById('presence-form-container').style.display = 'none';

  const result = document.getElementById('scan-result');
  result.className = 'scan-result';
  result.classList.remove('hidden');

  result.innerHTML = `
    <div id="confetti-zone" style="
      text-align:center;
      padding:40px 20px;
      background:linear-gradient(135deg,#0d2e1a,#0a3d21);
      border-radius:16px;
      border:2px solid #22c55e;
      animation:fadeIn 0.4s ease;
    ">
      <div style="font-size:4rem;animation:successBounce 0.6s ease;">Ã¢Å“â€¦</div>
      <h2 style="color:#22c55e;margin:12px 0 6px;font-size:1.5rem;">PrÃƒÂ©sence enregistrÃƒÂ©e !</h2>
      <p style="color:#86efac;font-size:1rem;">
        <strong>${escHtml(nom)}</strong><br>
        <span style="font-size:0.85rem;color:#4ade80;">&#128336; ${escHtml(horodatage || new Date().toLocaleString('fr-FR'))}</span>
      </p>
      <div style="margin-top:16px;padding:12px;background:rgba(34,197,94,0.1);border-radius:8px;border:1px solid #22c55e;">
        <p style="color:#86efac;font-size:0.85rem;margin:0;">&#10003; Les 7 boucliers de sÃƒÂ©curitÃƒÂ© validÃƒÂ©s</p>
        <p style="color:#4ade80;font-size:0.8rem;margin:4px 0 0;font-weight:700;">Vous pouvez fermer cette page.</p>
      </div>
      <div id="lock-countdown" style="margin-top:14px;color:#86efac;font-size:0.8rem;">Page verrouillÃƒÂ©e dans 3s...</div>
    </div>
  `;

  lancerConfetti();
  showToast('success', 'Ã¢Å“â€¦ PrÃƒÂ©sence validÃƒÂ©e !', 5000);

  // Verrouillage automatique aprÃƒÂ¨s 3 secondes
  let t = 3;
  const cd = setInterval(() => {
    t--;
    const el = document.getElementById('lock-countdown');
    if (el) el.textContent = t > 0 ? `Page verrouillÃƒÂ©e dans ${t}s...` : 'Page verrouillÃƒÂ©e.';
    if (t <= 0) {
      clearInterval(cd);
      verrouiller('success');
    }
  }, 1000);
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Ã°Å¸â€Â´ FRAUDE APPAREIL : Fond rouge pulsant + cadenas animÃƒÂ©
// L'empreinte matÃƒÂ©rielle du tÃƒÂ©lÃƒÂ©phone correspond ÃƒÂ  un autre ÃƒÂ©tudiant
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function showFraudBlocked(message) {
  document.getElementById('presence-form-container').style.display = 'none';

  const result = document.getElementById('scan-result');
  result.className = 'scan-result';
  result.classList.remove('hidden');

  result.innerHTML = `
    <div style="
      text-align:center;
      padding:40px 20px;
      background:linear-gradient(135deg,#2d0a0a,#450a0a);
      border-radius:16px;
      border:2px solid #ef4444;
      animation:pulseFraud 1.5s ease infinite;
    ">
      <div style="font-size:4rem;animation:lockShake 0.5s ease;">&#128274;</div>
      <h2 style="color:#ef4444;margin:12px 0 6px;font-size:1.4rem;">Ã°Å¸â€Â´ Tentative de fraude dÃƒÂ©tectÃƒÂ©e</h2>
      <p style="color:#fca5a5;font-size:0.9rem;line-height:1.6;">
        Cet appareil a dÃƒÂ©jÃƒÂ  ÃƒÂ©tÃƒÂ© utilisÃƒÂ© pour valider<br>
        une prÃƒÂ©sence dans cette session.<br><br>
        <strong style="color:#f87171;">Tentative signalÃƒÂ©e ÃƒÂ  l'enseignant</strong>
      </p>
      <div style="margin-top:16px;padding:12px;background:rgba(239,68,68,0.15);border-radius:8px;border:1px solid #ef4444;">
        <p style="color:#fca5a5;font-size:0.82rem;margin:0;">&#x26D4; Appareil physique blacklistÃƒÂ© automatiquement</p>
        <p style="color:#f87171;font-size:0.8rem;margin:6px 0 0;">Changer de navigateur ou vider le cache<br>ne changera rien Ã¢â‚¬â€ votre matÃƒÂ©riel est identifiÃƒÂ©.</p>
      </div>
    </div>
  `;

  if ('vibrate' in navigator) navigator.vibrate([200, 100, 200, 100, 400]);
  showToast('error', 'Ã°Å¸â€Â´ Fraude dÃƒÂ©tectÃƒÂ©e ! Enseignant notifiÃƒÂ©.', 8000);
  verrouiller('fraud');
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Afficher une erreur simple (mauvais code, QR expirÃƒÂ©...)
// Le formulaire reste accessible pour une nouvelle tentative
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function showError(message) {
  const errDiv  = document.getElementById('form-error');
  const errText = document.getElementById('error-text');
  errText.textContent = message;
  errDiv.classList.remove('hidden');
  if ('vibrate' in navigator) navigator.vibrate([100, 50, 100]);
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Verrouiller complÃƒÂ¨tement la page (aucune interaction possible)
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function verrouiller(type) {
  document.querySelectorAll('button, input, select, textarea').forEach(el => {
    el.disabled = true;
  });
  document.body.style.pointerEvents = 'none';

  const overlay = document.createElement('div');
  overlay.style.cssText = `
    position:fixed;top:0;left:0;width:100%;height:100%;z-index:9999;
    ${ type === 'fraud'
      ? 'background:rgba(239,68,68,0.08);'
      : 'background:rgba(0,0,0,0.35);'}
    pointer-events:all;
  `;
  document.body.appendChild(overlay);
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Lancer des confetti (animation CSS dynamique)
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function lancerConfetti() {
  const couleurs = ['#22c55e','#4ade80','#86efac','#fbbf24','#34d399','#a3e635'];
  for (let i = 0; i < 40; i++) {
    const el = document.createElement('div');
    el.style.cssText = `
      position:fixed;
      top:-10px;
      left:${Math.random()*100}vw;
      width:${Math.random()*10+5}px;
      height:${Math.random()*10+5}px;
      background:${couleurs[Math.floor(Math.random()*couleurs.length)]};
      border-radius:${Math.random()>0.5?'50%':'2px'};
      z-index:9998;
      animation:confettiFall ${Math.random()*2+1.5}s ease forwards;
      opacity:0.85;
    `;
    document.body.appendChild(el);
    setTimeout(() => el.remove(), 4000);
  }
}

// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
// Reset formulaire (usage interne)
// Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
function resetForm() {
  document.getElementById('presence-form-container').style.display = '';
  document.getElementById('scan-result').classList.add('hidden');
  document.getElementById('presence-form').reset();
  document.getElementById('form-error').classList.add('hidden');
  document.getElementById('submit-btn').disabled = false;
  document.getElementById('submit-text').textContent = 'Ã¢Å“â€œ Marquer ma prÃƒÂ©sence';
  document.getElementById('submit-spinner').classList.add('hidden');

  if (currentSessionId) {
    refreshQRToken(currentSessionId);
  }
}

// Helper escHtml (au cas oÃƒÂ¹ app.js pas chargÃƒÂ©)
function escHtml(str) {
  if (!str) return '';
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

// -------------------------------------------------------------
// BIOMÃ‰TRIE & GPS : Preuve de Vie et Anti-Relais
// -------------------------------------------------------------
let faceModelsLoaded = false;

async function loadFaceModels() {
  try {
    const MODEL_URL = 'https://cdn.jsdelivr.net/npm/@vladmandic/face-api/model/';
    await faceapi.nets.tinyFaceDetector.loadFromUri(MODEL_URL);
    await faceapi.nets.faceLandmark68Net.loadFromUri(MODEL_URL);
    await faceapi.nets.faceRecognitionNet.loadFromUri(MODEL_URL);
    faceModelsLoaded = true;
    console.log('[Biometrie] ModÃ¨les chargÃ©s');
  } catch(e) {
    console.error('[Biometrie] Erreur chargement modÃ¨les:', e);
  }
}

function getExactGPSPosition() {
  return new Promise((resolve, reject) => {
    if (!navigator.geolocation) {
      reject('GPS non supportÃ© par ce navigateur');
      return;
    }
    
    const successCb = (pos) => resolve({lat: pos.coords.latitude, lng: pos.coords.longitude});
    
    const errorCb = (err) => {
      let msg = 'Erreur GPS inconnue';
      if (err.code === 1) msg = 'Localisation refusÃ©e. Sur iPhone, allez dans RÃ©glages > Safari > Position et autorisez le site.';
      if (err.code === 2) msg = 'Position indisponible (signal GPS faible).';
      if (err.code === 3) msg = 'DÃ©lai dttente dÃ©passÃ© pour la localisation.';
      
      // Fallback si haute prÃ©cision Ã©choue (sauf si refusÃ©)
      if (err.code !== 1) {
        navigator.geolocation.getCurrentPosition(successCb, () => reject(msg), { enableHighAccuracy: false, timeout: 10000, maximumAge: 0 });
      } else {
        reject(msg);
      }
    };

    navigator.geolocation.getCurrentPosition(
      successCb,
      errorCb,
      { enableHighAccuracy: true, timeout: 6000, maximumAge: 0 }
    );
  });
}
    navigator.geolocation.getCurrentPosition(
      (pos) => resolve({lat: pos.coords.latitude, lng: pos.coords.longitude}),
      (err) => reject(err),
      { enableHighAccuracy: true, timeout: 10000, maximumAge: 0 }
    );
  });
}

function computeEAR(eye) {
  const dist = (p1, p2) => Math.sqrt(Math.pow(p1.x-p2.x, 2) + Math.pow(p1.y-p2.y, 2));
  const v1 = dist(eye[1], eye[5]);
  const v2 = dist(eye[2], eye[4]);
  const h  = dist(eye[0], eye[3]);
  return (v1 + v2) / (2.0 * h);
}

async function startBiometricScan() {
  return new Promise(async (resolve, reject) => {
    const video = document.getElementById('videoElement');
    const canvas = document.getElementById('faceCanvas');
    const bioStatus = document.getElementById('bio-status');
    const bioSpinner = document.getElementById('bio-spinner');
    
    let stream;
    try {
      stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'user' } });
      video.srcObject = stream;
    } catch(e) {
      reject('Erreur camÃ©ra: Veuillez autoriser la webcam.');
      return;
    }

    bioStatus.textContent = "CamÃ©ra prÃªte. Regardez l'Ã©cran.";
    
    video.addEventListener('play', async () => {
      canvas.width = video.videoWidth;
      canvas.height = video.videoHeight;
      const displaySize = { width: video.videoWidth, height: video.videoHeight };
      faceapi.matchDimensions(canvas, displaySize);
      
      let blinked = false;
      let frameCount = 0;
      
      bioStatus.textContent = "Veuillez CLIGNER DES YEUX";
      bioSpinner.style.display = 'none';

      const scanInterval = setInterval(async () => {
        const detections = await faceapi.detectAllFaces(video, new faceapi.TinyFaceDetectorOptions({ inputSize: 224, scoreThreshold: 0.3 }))
                                        .withFaceLandmarks()
                                        .withFaceDescriptors();
        
        if (!stream.active) return; // Prevent async overlaps if stopped
        
        const resized = faceapi.resizeResults(detections, displaySize);
        canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
        faceapi.draw.drawFaceLandmarks(canvas, resized);

        if (detections.length === 0) {
           bioStatus.textContent = "Aucun visage dÃ©tectÃ©";
           return;
        }
        if (detections.length > 1) {
           bioStatus.textContent = "?? Un seul visage autorisÃ© !";
           return;
        }

        const face = detections[0];
        const landmarks = face.landmarks;
        const leftEye = landmarks.getLeftEye();
        const rightEye = landmarks.getRightEye();

        const leftEAR = computeEAR(leftEye);
        const rightEAR= computeEAR(rightEye);
        const ear = (leftEAR + rightEAR) / 2.0;

        if (ear < 0.26) { // Yeux fermÃ©s
            blinked = true;
            bioStatus.textContent = "Clignement dÃ©tectÃ©... Ne bougez plus !";
        } else if (blinked && ear > 0.27) { // Yeux rouverts
            // SuccÃ¨s absolu Liveness
            clearInterval(scanInterval);
            
            // Attendre 300ms que le visage soit bien stable
            setTimeout(async () => {
              const finalDet = await faceapi.detectSingleFace(video, new faceapi.TinyFaceDetectorOptions({ inputSize: 224, scoreThreshold: 0.3 }))
                                         .withFaceLandmarks().withFaceDescriptor();
              
              stream.getTracks().forEach(t => t.stop());
              if (!finalDet) reject("Impossible d'extraire le visage final");
              else resolve(Array.from(finalDet.descriptor));
            }, 300);
        } else {
             if (!blinked) bioStatus.textContent = "Veuillez CLIGNER DES YEUX pour prouver que vous Ãªtes humain";
        }
      }, 100);
    });
  });
}
