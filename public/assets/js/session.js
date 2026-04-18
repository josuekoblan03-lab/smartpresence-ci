/* ============================================================
   session.js — Gestion des sessions + QR Code live
   SMARTPRESENCE CI
   ============================================================ */

let activeSessionId = null;
let countdownTimer  = null;
let qrInstance      = null;
let fraudCount      = 0;  // Compteur global des fraudes de la session

document.addEventListener('DOMContentLoaded', async () => {
  const user = await requireAuth();
  if (!user) return;
  fillUserInfo(user);

  await loadSessions();
  await loadFilieres();

  // Détecter si session_id en URL
  const params = new URLSearchParams(window.location.search);
  const sid = params.get('id');
  if (sid) showQRPanel(parseInt(sid));

  // Form création
  document.getElementById('create-form').addEventListener('submit', createSession);
});

// ─────────────────────────────────────────
// Charger les sessions
// ─────────────────────────────────────────
async function loadSessions() {
  const container = document.getElementById('sessions-container');
  const statut    = document.getElementById('filter-statut').value;

  try {
    const res  = await apiFetch('/api/sessions');
    if (!res) return;
    const data = await res.json();

    let sessions = data.sessions || [];
    if (statut) sessions = sessions.filter(s => s.statut === statut);

    if (!sessions.length) {
      container.innerHTML = `
        <div style="text-align:center;padding:60px 20px;color:var(--text-muted);">
          <div style="font-size:3rem;margin-bottom:16px;">📅</div>
          <h3 style="color:var(--text-secondary);">Aucune session</h3>
          <p style="margin-top:8px;">Créez votre première session de cours.</p>
          <button class="btn btn-primary" style="margin-top:20px;" onclick="openCreateModal()">➕ Créer une session</button>
        </div>`;
      return;
    }

    container.innerHTML = sessions.map(s => sessionCard(s)).join('');
  } catch(e) {
    console.error('[Sessions]', e);
    container.innerHTML = `<div class="alert alert-error">Erreur de chargement des sessions.</div>`;
  }
}

// ─────────────────────────────────────────
// Carte d'une session
// ─────────────────────────────────────────
function sessionCard(s) {
  const isOpen = s.statut === 'ouverte';
  return `
    <div class="card" style="margin-bottom:16px;border-left:3px solid ${isOpen?'var(--color-accent)':'var(--border)'};"
         onclick="showQRPanel(${s.id})" style="cursor:pointer;">
      <div style="display:flex;align-items:flex-start;justify-content:space-between;gap:16px;">
        <div style="flex:1;">
          <div style="display:flex;align-items:center;gap:10px;margin-bottom:6px;cursor:pointer;" >
            <span class="badge ${isOpen?'badge-success':'badge-info'}">
              ${isOpen ? '🟢 Ouverte' : '🔴 Fermée'}
            </span>
            <span style="font-size:0.78rem;color:var(--text-muted);">#${s.id}</span>
          </div>
          <div style="font-weight:700;font-size:1rem;color:var(--text-primary);margin-bottom:4px;">${escHtml(s.titre)}</div>
          <div style="font-size:0.83rem;color:var(--text-muted);">
            📖 ${escHtml(s.matiere)} &nbsp;·&nbsp;
            🎓 ${escHtml(s.filiere_nom) || 'Toutes filières'} &nbsp;·&nbsp;
            👨‍🏫 ${escHtml(s.enseignant_nom)}
          </div>
          <div style="font-size:0.8rem;color:var(--text-muted);margin-top:6px;">
            🕐 ${formatDate(s.date_debut)} &nbsp;·&nbsp;
            ✅ ${s.presences_count} présence(s)
          </div>
        </div>
        <div style="display:flex;flex-direction:column;gap:8px;">
          ${isOpen ? `
            <button class="btn btn-accent btn-sm" onclick="event.stopPropagation();showQRPanel(${s.id})">🔲 QR Code</button>
            <button class="btn btn-danger btn-sm" onclick="event.stopPropagation();confirmClose(${s.id})">🔒 Fermer</button>
          ` : `
            <a href="/reports.html?session=${s.id}" class="btn btn-ghost btn-sm">📊 Rapport</a>
          `}
        </div>
      </div>
    </div>`;
}

// ─────────────────────────────────────────
// Afficher le panneau QR Code
// ─────────────────────────────────────────
async function showQRPanel(sessionId) {
  activeSessionId = sessionId;
  const panel = document.getElementById('qr-panel');
  panel.style.display = 'block';

  try {
    // Infos session
    const sRes  = await apiFetch(`/api/sessions/${sessionId}`);
    if (!sRes) return;
    const sData = await sRes.json();
    if (!sData.success) return;

    const s = sData.session;
    document.getElementById('qr-session-title').textContent = s.titre;
    document.getElementById('qr-session-details').textContent =
      s.filiere_nom + ' · ' + s.enseignant_nom + ' · ' + s.presences_count + ' présence(s)';

    // Lien page scan
    const scanLink = document.getElementById('scan-page-link');
    if (scanLink) {
      scanLink.href = `/scan.html?sid=${sessionId}`;
    }

    if (s.statut !== 'ouverte') {
      document.getElementById('qr-panel').innerHTML +=
        '<div class="alert alert-warning" style="margin-top:16px;">⚠️ Session fermée — QR Code inactif</div>';
      return;
    }

    // Récupérer le token
    await refreshQRDisplay(sessionId);

    // Écouter les renouvellements SSE
    listenSSEQR(sessionId);

  } catch(e) {
    console.error('[QR Panel]', e);
  }
}

// Mettre à jour l'affichage QR
async function refreshQRDisplay(sessionId) {
  try {
    const res  = await fetch(`/api/qrcode/${sessionId}/token`);
    const data = await res.json();
    if (!data.success) return;

    const token = data.token;
    const expIn = data.expires_in;

    // Afficher le token
    document.getElementById('qr-token-text').textContent = token;

    // Générer le QR Code
    const canvas = document.getElementById('qr-canvas');
    canvas.innerHTML = '';

    const scanUrl = `${window.location.origin}/scan.html?token=${encodeURIComponent(token)}&sid=${sessionId}`;

    if (typeof QRCode !== 'undefined') {
      qrInstance = new QRCode(canvas, {
        text: scanUrl,
        width: 200,
        height: 200,
        colorDark: '#1a1827',
        colorLight: '#ffffff',
        correctLevel: QRCode.CorrectLevel.H
      });
    }

    // Countdown
    startQRCountdown(expIn, sessionId);
  } catch(e) {
    console.error('[QR Refresh]', e);
  }
}

// Compte à rebours QR
function startQRCountdown(seconds, sessionId) {
  if (countdownTimer) clearInterval(countdownTimer);

  const el  = document.getElementById('qr-countdown');
  const bar = document.getElementById('qr-countdown-bar');
  const total = 30;
  let left = Math.max(0, seconds);

  if (bar) bar.style.width = (left / total * 100) + '%';

  countdownTimer = setInterval(async () => {
    left--;
    if (el)  el.textContent = left + 's';
    if (bar) {
      bar.style.width = Math.max(0, left / total * 100) + '%';
      bar.style.background = left < 10 ?
        'linear-gradient(90deg,#ef4444,#dc2626)' : 'var(--gradient-primary)';
    }

    if (left <= 0) {
      clearInterval(countdownTimer);
      await refreshQRDisplay(sessionId);
    }
  }, 1000);
}

// ─────────────────────────────────────────
// 🚨 Ajouter une alerte fraude dans le panel
// ─────────────────────────────────────────
function addFraudAlert(data) {
  // Masquer le message "aucune fraude" s'il est visible
  const emptyMsg = document.getElementById('fraud-empty');
  if (emptyMsg) emptyMsg.style.display = 'none';

  // Incrémenter le compteur et mettre à jour le badge
  fraudCount++;
  const badge = document.getElementById('fraud-badge');
  if (badge) {
    badge.textContent = `${fraudCount} tentative${fraudCount > 1 ? 's' : ''}`;
    // Animation flash sur le badge
    badge.style.animation = 'none';
    setTimeout(() => { badge.style.animation = 'pulseFraud 1s ease 3'; }, 10);
  }

  // Créer la carte alerte
  const card = document.createElement('div');
  card.style.cssText = `
    padding:14px 16px;
    background:rgba(239,68,68,0.08);
    border:1px solid rgba(239,68,68,0.3);
    border-left:4px solid #ef4444;
    border-radius:10px;
    animation:fraudSlideIn 0.3s ease;
    display:flex;
    align-items:flex-start;
    gap:12px;
  `;

  // Heure de la tentative
  const heure = data.heure || new Date().toLocaleTimeString('fr-FR', {hour:'2-digit', minute:'2-digit'});
  const matricule = data.matricule || '????';
  const typeFraude = data.type_fraude || 'Inconnu';
  const description = data.description || 'Tentative suspecte';

  card.innerHTML = `
    <div style="font-size:1.4rem;">🔴</div>
    <div style="flex:1;">
      <div style="display:flex;align-items:center;gap:10px;margin-bottom:4px;">
        <span class="badge" style="background:rgba(239,68,68,0.2);color:#ef4444;border:1px solid rgba(239,68,68,0.4);font-size:0.75rem;">FRAUDE DÉTECTÉE</span>
        <span style="font-size:0.78rem;color:var(--text-muted);font-family:var(--font-mono);">⏰ ${escHtml(heure)}</span>
        <span style="font-size:0.78rem;color:var(--text-muted);">IP: ${escHtml(data.ip || '?')}</span>
      </div>
      <div style="font-weight:700;color:#f87171;font-size:0.9rem;">Matricule&nbsp;: ${escHtml(matricule)}</div>
      <div style="font-size:0.82rem;color:#fca5a5;margin-top:2px;">${escHtml(typeFraude)}</div>
      <div style="font-size:0.79rem;color:var(--text-muted);margin-top:3px;">${escHtml(description)}</div>
    </div>
  `;

  // Insérer EN HAUT de la liste (plus récent en premier)
  const list = document.getElementById('fraud-list');
  if (list) list.insertBefore(card, list.firstChild);

  // Toast d'alerte pour le professeur
  showToast('error', `🛑 Fraude — Matricule ${matricule} — ${typeFraude}`, 6000);
}

// ─────────────────────────────────────────
// Polling SSE avec écoute des événements fraude
// ─────────────────────────────────────────
function listenSSEQR(sessionId) {
  const es = new EventSource('/api/sse/events');

  // Événement : renouvellement QR
  es.addEventListener('qr_refresh', (e) => {
    const d = JSON.parse(e.data);
    if (d.session_id === sessionId) {
      refreshQRDisplay(sessionId);
      showToast('info', '🔄 QR Code renouvellé automatiquement');
    }
  });

  // Événement : présence validée
  es.addEventListener('presence_marked', (e) => {
    const d = JSON.parse(e.data);
    if (d.session_id === sessionId) {
      // Mettre à jour le compteur présences
      const det = document.getElementById('qr-session-details');
      if (det) {
        const text = det.textContent;
        det.textContent = text.replace(/\d+ présence\(s\)/, (m) => {
          const n = parseInt(m) + 1;
          return n + ' présence(s)';
        });
      }
      showToast('success', `✅ ${d.etudiant_nom} — présence marquée`);
    }
  });

  // 🚨 Événement FRAUDE : affichage temps réel dans le panel
  es.addEventListener('fraud_detected', (e) => {
    const d = JSON.parse(e.data);
    // Afficher pour toutes les fraudes (ou filtrer par session)
    if (!d.session_id || d.session_id === sessionId || !sessionId) {
      addFraudAlert(d);
    }
  });

  es.onerror = () => es.close();
}

// ─────────────────────────────────────────
// Fermer la session active
// ─────────────────────────────────────────
async function closeCurrentSession() {
  if (!activeSessionId) return;
  await confirmClose(activeSessionId);
}

async function confirmClose(sessionId) {
  if (!confirm(`Fermer la session #${sessionId} ? Les étudiants ne pourront plus marquer leur présence.`)) return;

  try {
    const res  = await apiFetch(`/api/sessions/${sessionId}/close`, { method: 'PUT' });
    const data = await res.json();
    if (data.success) {
      showToast('success', '✅ Session fermée avec succès');
      document.getElementById('qr-panel').style.display = 'none';
      if (countdownTimer) clearInterval(countdownTimer);
      await loadSessions();
    } else {
      showToast('error', data.error);
    }
  } catch(e) {
    showToast('error', 'Erreur lors de la fermeture');
  }
}

// ─────────────────────────────────────────
// Modal création session
// ─────────────────────────────────────────
async function loadFilieres() {
  try {
    const res  = await fetch('/api/filieres');
    const data = await res.json();
    const sel  = document.getElementById('new-filiere');
    if (!sel) return;

    data.filieres.forEach(f => {
      const opt = document.createElement('option');
      opt.value = f.id;
      opt.textContent = f.nom;
      sel.appendChild(opt);
    });
  } catch(e) {}
}

function openCreateModal() {
  document.getElementById('create-modal').classList.remove('hidden');
}

function closeCreateModal() {
  document.getElementById('create-modal').classList.add('hidden');
  document.getElementById('create-form').reset();
  document.getElementById('modal-error').classList.add('hidden');
}

async function createSession(e) {
  e.preventDefault();
  const titre    = document.getElementById('new-titre').value.trim();
  const matiere  = document.getElementById('new-matiere').value.trim();
  const filiereId= parseInt(document.getElementById('new-filiere').value) || 0;

  const btn     = document.getElementById('create-btn');
  const btnText = document.getElementById('create-btn-text');
  const spinner = document.getElementById('create-spinner');
  const errDiv  = document.getElementById('modal-error');
  const errText = document.getElementById('modal-error-text');

  btn.disabled = true;
  btnText.textContent = 'Création...';
  spinner.classList.remove('hidden');
  errDiv.classList.add('hidden');

  try {
    const res  = await apiFetch('/api/sessions', {
      method: 'POST',
      body: JSON.stringify({ titre, matiere, filiere_id: filiereId })
    });
    const data = await res.json();

    if (data.success) {
      closeCreateModal();
      showToast('success', `✅ Session "${titre}" créée !`);
      await loadSessions();
      showQRPanel(data.session_id);
    } else {
      errText.textContent = data.error || 'Erreur création';
      errDiv.classList.remove('hidden');
      btn.disabled = false;
      btnText.textContent = 'Créer et ouvrir';
      spinner.classList.add('hidden');
    }
  } catch(err) {
    errText.textContent = 'Erreur serveur';
    errDiv.classList.remove('hidden');
    btn.disabled = false;
    btnText.textContent = 'Créer et ouvrir';
    spinner.classList.add('hidden');
  }
}
