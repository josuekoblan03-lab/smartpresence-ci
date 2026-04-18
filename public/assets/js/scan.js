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
  let deviceId = '';
  try {
    deviceId = localStorage.getItem('smartpresence_device_id');
    if (!deviceId) {
      deviceId = 'DEV_' + Math.random().toString(36).substring(2, 15) + Date.now().toString(36);
      localStorage.setItem('smartpresence_device_id', deviceId);
    }
  } catch(err) {
    // Falback si Navigation Privée stricte (qui bloque localStorage)
    deviceId = sessionStorage.getItem('sp_session_device');
    if (!deviceId) {
      deviceId = 'NPDEV_' + Math.random().toString(36).substring(2, 15) + Date.now().toString(36);
      try { sessionStorage.setItem('sp_session_device', deviceId); } catch(e){}
    }
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
      // ✅ Succès : animation confetti verte + grisage auto 3s
      showSuccess(data.etudiant_nom, data.horodatage);
    } else if (res.status === 403) {
      // Déterminer le type d'erreur pour afficher le bon écran
      const errMsg = data.error || '';
      const isFraudDevice = errMsg.includes('téléphone') ||
                            errMsg.includes('appareil') ||
                            errMsg.includes('Empreinte') ||
                            errMsg.includes('Device');
      if (isFraudDevice) {
        // 🔴 Appareil déjà utilisé = rouge pulsant + cadenas
        showFraudBlocked(errMsg);
      } else {
        // Autre erreur de sécurité (code faux, QR expiré, etc.)
        showError(errMsg);
        btn.disabled = false;
        btnText.textContent = '✓ Marquer ma présence';
        spinner.classList.add('hidden');
      }
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

// ─────────────────────────────────────────
// ✅ SUCCÈS : Animation confetti + grisage auto
// ─────────────────────────────────────────
function showSuccess(nom, horodatage) {
  // Cacher le formulaire pour éviter nouvelle soumission
  document.getElementById('presence-form-container').style.display = 'none';

  const result = document.getElementById('scan-result');
  result.className = 'scan-result'; // Reset
  result.classList.remove('hidden');

  result.innerHTML = `
    <div id="confetti-zone" style="
      text-align:center;
      padding:40px 20px;
      background: linear-gradient(135deg,#0d2e1a,#0a3d21);
      border-radius:16px;
      border:2px solid #22c55e;
      animation: fadeIn 0.4s ease;
    ">
      <div style="font-size:4rem;animation:successBounce 0.6s ease;">✅</div>
      <h2 style="color:#22c55e;margin:12px 0 6px;font-size:1.5rem;">Présence enregistrée !</h2>
      <p style="color:#86efac;font-size:1rem;">
        <strong>${escHtml(nom)}</strong><br>
        <span style="font-size:0.85rem;color:#4ade80;">&#128336; ${escHtml(horodatage || new Date().toLocaleString('fr-FR'))}</span>
      </p>
      <div style="margin-top:16px;padding:12px;background:rgba(34,197,94,0.1);border-radius:8px;border:1px solid #22c55e;">
        <p style="color:#86efac;font-size:0.85rem;margin:0;">&#10003; Les 7 boucliers de sécurité validés</p>
        <p style="color:#4ade80;font-size:0.8rem;margin:4px 0 0;font-weight:700;">Vous pouvez fermer cette page.</p>
      </div>
      <div id="lock-countdown" style="margin-top:14px;color:#86efac;font-size:0.8rem;">Page verrouillée dans 3s...</div>
    </div>
  `;

  // Lancer les confetti
  lancerConfetti();

  showToast('success', '✅ Présence validée !', 5000);

  // Griser et verrouiller la page au bout de 3s
  let t = 3;
  const cd = setInterval(() => {
    t--;
    const el = document.getElementById('lock-countdown');
    if (el) el.textContent = t > 0 ? `Page verrouillée dans ${t}s...` : 'Page verrouillée.';
    if (t <= 0) {
      clearInterval(cd);
      verrouiller('success');
    }
  }, 1000);
}

// ─────────────────────────────────────────
// 🔴 FRAUDE APPAREIL : Fond rouge pulsant + cadenas animé
// ─────────────────────────────────────────
function showFraudBlocked(message) {
  // Cacher complètement le formulaire
  document.getElementById('presence-form-container').style.display = 'none';

  const result = document.getElementById('scan-result');
  result.className = 'scan-result';
  result.classList.remove('hidden');

  result.innerHTML = `
    <div style="
      text-align:center;
      padding:40px 20px;
      background: linear-gradient(135deg,#2d0a0a,#450a0a);
      border-radius:16px;
      border:2px solid #ef4444;
      animation: pulseFraud 1.5s ease infinite;
    ">
      <div style="font-size:4rem;animation:lockShake 0.5s ease;">&#128274;</div>
      <h2 style="color:#ef4444;margin:12px 0 6px;font-size:1.4rem;">🔴 Tentative de fraude détectée</h2>
      <p style="color:#fca5a5;font-size:0.9rem;line-height:1.5;">
        Cet appareil a déjà été utilisé pour valider une présence dans cette session.<br><br>
        <strong style="color:#f87171;">Tentative signalée à l'enseignant</strong><br>
        <span style="font-size:0.78rem;color:#fca5a5;">${escHtml(message)}</span>
      </p>
      <div style="margin-top:16px;padding:12px;background:rgba(239,68,68,0.15);border-radius:8px;border:1px solid #ef4444;">
        <p style="color:#fca5a5;font-size:0.82rem;margin:0;">⛔ Appareil blacklisté automatiquement</p>
        <p style="color:#f87171;font-size:0.82rem;margin:4px 0 0;font-weight:700;">Aucune autre action n'est possible.</p>
      </div>
    </div>
  `;

  // Vibration d'alerte
  if ('vibrate' in navigator) navigator.vibrate([200, 100, 200, 100, 400]);

  showToast('error', '🔴 Fraude détectée ! Enseignant notifié.', 8000);

  // Verrouiller immédiatement
  verrouiller('fraud');
}

// ─────────────────────────────────────────
// Afficher une erreur simple (code faux, QR expiré...)
// ─────────────────────────────────────────
function showError(message) {
  const errDiv  = document.getElementById('form-error');
  const errText = document.getElementById('error-text');
  errText.textContent = message;
  errDiv.classList.remove('hidden');
  // Vibration simple
  if ('vibrate' in navigator) navigator.vibrate([100, 50, 100]);
}

// ─────────────────────────────────────────
// Verrouiller complètement la page
// ─────────────────────────────────────────
function verrouiller(type) {
  // Bloquer tout élément interactif sur la page
  document.querySelectorAll('button, input, select, textarea').forEach(el => {
    el.disabled = true;
  });
  // Empêcher le clic partout
  document.body.style.pointerEvents = 'none';
  // Ajouter une overlay gris/rouge selon le type
  const overlay = document.createElement('div');
  overlay.style.cssText = `
    position:fixed;top:0;left:0;width:100%;height:100%;z-index:9999;
    ${ type === 'fraud'
      ? 'background:rgba(239,68,68,0.15);'
      : 'background:rgba(0,0,0,0.4);'}
    pointer-events:all;
  `;
  document.body.appendChild(overlay);
}

// ─────────────────────────────────────────
// Lancer des confetti (animation CSS dynamique)
// ─────────────────────────────────────────
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
      animation: confettiFall ${Math.random()*2+1.5}s ease forwards;
      opacity:0.85;
    `;
    document.body.appendChild(el);
    setTimeout(() => el.remove(), 4000);
  }
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
