/* ============================================================
   scan.js — Logique page scan (présence, QR, countdown)
   SMARTPRESENCE CI
   ============================================================ */

let currentSessionId = null;
let countdownInterval = null;
let qrRefreshInterval = null;
let qrCodeInstance = null;
let qrExpireAt = 0;

// ─────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────
document.addEventListener('DOMContentLoaded', async () => {
  // Lire les paramètres d'URL
  const params = new URLSearchParams(window.location.search);
  const tokenFromUrl = params.get('token');
  const sidFromUrl   = params.get('sid');

  // Préremplir token
  if (tokenFromUrl) {
    document.getElementById('qr-token-input').value = tokenFromUrl;
  }

  // Préremplir session_id
  if (sidFromUrl) {
    currentSessionId = parseInt(sidFromUrl);
    document.getElementById('manual-session-id').value = sidFromUrl;
    await loadSessionInfo(currentSessionId);
  }

  // Formulaire présence
  document.getElementById('presence-form').addEventListener('submit', submitPresence);

  // Majuscules automatiques sur le matricule
  document.getElementById('matricule').addEventListener('input', function() {
    this.value = this.value.toUpperCase();
  });

  // Chiffres seulement pour le PIN
  document.getElementById('code-personnel').addEventListener('input', function() {
    this.value = this.value.replace(/[^0-9]/g, '').slice(0, 6);
  });
});

// ─────────────────────────────────────────
// Charger info de session
// ─────────────────────────────────────────
async function loadSessionInfo(sid) {
  sid = sid || parseInt(document.getElementById('manual-session-id').value);
  if (!sid || sid <= 0) {
    showToast('warning', 'Entrez un numéro de session valide');
    return;
  }

  currentSessionId = sid;

  try {
    // Récupérer le token QR de la session
    const res  = await fetch(`/api/qrcode/${sid}/token`);
    const data = await res.json();

    if (!data.success) {
      showToast('error', 'Session introuvable ou fermée');
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
        s.filiere_nom + ' · ' + s.enseignant_nom;

      if (s.statut !== 'ouverte') {
        document.getElementById('session-info').textContent += ' — ⛔ Session fermée';
        showToast('warning', 'Cette session est fermée.');
        return;
      }
    }

    // Afficher le token et le QR Code
    displayToken(data.token, data.expires_in, data.expire_at);
    qrExpireAt = data.expire_at;

    // Préremplir le champ token
    document.getElementById('qr-token-input').value = data.token;

    // Démarrer le compte à rebours
    startCountdown(data.expires_in, sid);

    // Souscrire aux SSE pour renouvellement automatique
    listenQRRefresh(sid);

  } catch(e) {
    console.error('[Scan] Erreur chargement session:', e);
    showToast('error', 'Erreur de chargement de la session');
  }
}

// ─────────────────────────────────────────
// Afficher token + QR Code JS
// ─────────────────────────────────────────
function displayToken(token, expiresIn, expireAt) {
  // Afficher le token
  const tokenZone = document.getElementById('token-zone');
  tokenZone.classList.remove('hidden');

  const tokenDisplay = document.getElementById('token-display');
  tokenDisplay.textContent = token;

  // Générer le QR Code avec qrcodejs
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
    // Fallback : afficher le SVG du serveur
    canvas.innerHTML = `<img src="/api/qrcode/${currentSessionId}" width="200" height="200" style="border-radius:8px;">`;
  }

  // Préremplir le champ
  document.getElementById('qr-token-input').value = token;
}

// ─────────────────────────────────────────
// Compte à rebours renouvellement QR
// ─────────────────────────────────────────
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

    // Mettre à jour la barre
    const pct = Math.max(0, remaining / totalSeconds * 100);
    if (countdownBar) {
      countdownBar.style.width = pct + '%';
      // Rouge quand < 10s
      countdownBar.style.background = remaining < 10 ?
        'linear-gradient(90deg,#ef4444,#dc2626)' : 'var(--gradient-primary)';
    }

    if (remaining <= 0) {
      clearInterval(countdownInterval);
      // Recharger le token
      await refreshQRToken(sessionId);
    }
  }, 1000);
}

// ─────────────────────────────────────────
// Rafraîchir le token QR
// ─────────────────────────────────────────
async function refreshQRToken(sessionId) {
  try {
    const res  = await fetch(`/api/qrcode/${sessionId}/token`);
    const data = await res.json();
    if (data.success) {
      displayToken(data.token, data.expires_in, data.expire_at);
      startCountdown(data.expires_in, sessionId);
      showToast('info', '🔄 QR Code renouvelé automatiquement');
    }
  } catch(e) {
    console.error('[QR] Erreur refresh:', e);
  }
}

// ─────────────────────────────────────────
// SSE pour renouvellement QR
// ─────────────────────────────────────────
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

// ─────────────────────────────────────────
// Soumettre la présence
// ─────────────────────────────────────────
async function submitPresence(e) {
  e.preventDefault();

  const matricule      = document.getElementById('matricule').value.trim();
  const codePersonnel  = document.getElementById('code-personnel').value.trim();
  const qrToken        = document.getElementById('qr-token-input').value.trim();
  const sessionId      = currentSessionId ||
                          parseInt(document.getElementById('manual-session-id').value) || 0;

  const btn       = document.getElementById('submit-btn');
  const btnText   = document.getElementById('submit-text');
  const spinner   = document.getElementById('submit-spinner');
  const errDiv    = document.getElementById('form-error');
  const errText   = document.getElementById('error-text');

  // Validation basique
  if (!matricule || !codePersonnel || !qrToken) {
    errText.textContent = 'Remplissez tous les champs';
    errDiv.classList.remove('hidden');
    return;
  }

  if (codePersonnel.length !== 6 || !/^\d+$/.test(codePersonnel)) {
    errText.textContent = 'Le code personnel doit être exactement 6 chiffres';
    errDiv.classList.remove('hidden');
    return;
  }

  if (sessionId <= 0) {
    errText.textContent = 'Session non sélectionnée. Entrez un numéro de session.';
    errDiv.classList.remove('hidden');
    return;
  }

  // Fingerprint (Empreinte numérique du téléphone)
  let deviceId = localStorage.getItem('smartpresence_device_id');
  if (!deviceId) {
    deviceId = 'DEV_' + Math.random().toString(36).substring(2, 15) + Date.now().toString(36);
    localStorage.setItem('smartpresence_device_id', deviceId);
  }

  // Spinner
  btn.disabled = true;
  btnText.textContent = 'Vérification en cours...';
  spinner.classList.remove('hidden');
  errDiv.classList.add('hidden');

  try {
    const res = await fetch('/api/presence/mark', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        matricule,
        code_personnel: codePersonnel,
        qr_token:       qrToken,
        session_id:     sessionId,
        device_id:      deviceId
      })
    });

    const data = await res.json();

    if (data.success) {
      showSuccess(data.etudiant_nom, data.horodatage);
    } else {
      showError(data.error || 'Présence refusée');
      btn.disabled = false;
      btnText.textContent = '✓ Marquer ma présence';
      spinner.classList.add('hidden');
    }
  } catch(err) {
    showError('Erreur de connexion au serveur');
    btn.disabled = false;
    btnText.textContent = '✓ Marquer ma présence';
    spinner.classList.add('hidden');
  }
}

// ─────────────────────────────────────────
// Afficher le succès
// ─────────────────────────────────────────
function showSuccess(nom, horodatage) {
  document.getElementById('presence-form-container').style.display = 'none';

  const result = document.getElementById('scan-result');
  result.className = 'scan-result success';
  result.classList.remove('hidden');

  document.getElementById('result-icon').textContent = '✅';
  document.getElementById('result-title').textContent = 'Présence enregistrée !';
  document.getElementById('result-desc').innerHTML = `
    <strong style="color:var(--color-accent-light, #34d399);">${escHtml(nom)}</strong>
    <br><br>
    🕐 ${escHtml(horodatage || new Date().toLocaleString('fr-FR'))}
    <br><br>
    Votre présence a été marquée avec succès.<br>
    Les 7 boucliers de sécurité ont été validés.
  `;

  showToast('success', '✅ Présence enregistrée avec succès !', 5000);
}

// ─────────────────────────────────────────
// Afficher l'erreur
// ─────────────────────────────────────────
function showError(message) {
  const errDiv  = document.getElementById('form-error');
  const errText = document.getElementById('error-text');
  errText.textContent = message;
  errDiv.classList.remove('hidden');

  // Vibration si disponible
  if ('vibrate' in navigator) navigator.vibrate([100, 50, 100]);
}

// ─────────────────────────────────────────
// Reset formulaire
// ─────────────────────────────────────────
function resetForm() {
  document.getElementById('presence-form-container').style.display = '';
  document.getElementById('scan-result').classList.add('hidden');
  document.getElementById('presence-form').reset();
  document.getElementById('form-error').classList.add('hidden');
  document.getElementById('submit-btn').disabled = false;
  document.getElementById('submit-text').textContent = '✓ Marquer ma présence';
  document.getElementById('submit-spinner').classList.add('hidden');

  // Re-charger le token si session sélectionnée
  if (currentSessionId) {
    refreshQRToken(currentSessionId);
  }
}

// Helper escHtml (au cas où app.js pas chargé)
function escHtml(str) {
  if (!str) return '';
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}
