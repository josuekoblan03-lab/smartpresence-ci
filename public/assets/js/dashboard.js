/* ============================================================
   dashboard.js — Logique dashboard + SSE
   SMARTPRESENCE CI
   ============================================================ */

let pollInterval = null;
let lastEventId = 0;
let feedEmpty = true;

// ─────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────
document.addEventListener('DOMContentLoaded', async () => {
  // Auth
  const user = await requireAuth();
  if (!user) return;

  fillUserInfo(user);

  const welcomeName = document.getElementById('welcome-name');
  if (welcomeName) welcomeName.textContent = user.prenom;

  // Horloge temps réel
  updateClock();
  setInterval(updateClock, 1000);

  // Charger stats
  await loadStats();
  setInterval(loadStats, 30000);  // Rafraîchir toutes les 30s

  // Charger sessions récentes
  await loadRecentSessions();

  // Connecter Live Feed (Polling)
  connectLiveFeed();
});

// ─────────────────────────────────────────
// Horloge
// ─────────────────────────────────────────
function updateClock() {
  const el = document.getElementById('current-time');
  if (!el) return;
  el.textContent = new Date().toLocaleString('fr-FR', {
    weekday: 'long', day: '2-digit', month: 'long', year: 'numeric',
    hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
}

// ─────────────────────────────────────────
// Charger les statistiques
// ─────────────────────────────────────────
async function loadStats() {
  try {
    const res  = await apiFetch('/api/dashboard/stats');
    if (!res) return;
    const data = await res.json();
    if (!data.success) return;

    const s = data.stats;

    // Animer les compteurs
    animateCounter('stat-etudiants', s.total_etudiants);
    animateCounter('stat-presences', s.presences_today);
    animateCounter('stat-sessions',  s.total_sessions);
    animateCounter('stat-fraudes',   s.tentatives_fraude);
    document.getElementById('stat-taux').textContent = s.taux_presence + '%';

    // Tendances
    const presenceTrend = document.getElementById('stat-trend-presence');
    if (presenceTrend) {
      presenceTrend.textContent = s.presences_today > 0 ?
        `↑ ${s.presences_today} aujourd'hui` : 'Aucune présence aujourd\'hui';
    }

    const sessionOpen = document.getElementById('stat-sessions-open');
    if (sessionOpen) {
      sessionOpen.textContent = s.sessions_ouvertes > 0 ?
        `🟢 ${s.sessions_ouvertes} session(s) ouverte(s)` : 'Aucune session ouverte';
    }

    // Badge fraudes
    const badge = document.getElementById('fraud-count');
    if (badge) {
      badge.textContent = s.tentatives_fraude;
      badge.style.display = s.tentatives_fraude > 0 ? '' : 'none';
    }

  } catch(e) {
    console.error('[Stats] Erreur:', e);
  }
}

// ─────────────────────────────────────────
// Animer compteur
// ─────────────────────────────────────────
function animateCounter(id, target) {
  const el = document.getElementById(id);
  if (!el) return;

  const current = parseInt(el.textContent) || 0;
  const diff = target - current;
  const steps = 20;
  const step  = diff / steps;
  let count = current;
  let i = 0;

  const interval = setInterval(() => {
    count += step;
    i++;
    el.textContent = Math.round(i >= steps ? target : count);
    if (i >= steps) clearInterval(interval);
  }, 30);
}

// ─────────────────────────────────────────
// Sessions récentes
// ─────────────────────────────────────────
async function loadRecentSessions() {
  try {
    const res  = await apiFetch('/api/sessions');
    if (!res) return;
    const data = await res.json();
    const el   = document.getElementById('sessions-list');
    if (!el) return;

    if (!data.success || !data.sessions.length) {
      el.innerHTML = `
        <div style="text-align:center;padding:40px 0;color:var(--text-muted);">
          <div style="font-size:2rem;margin-bottom:12px;">📅</div>
          <p>Aucune session pour le moment</p>
          <a href="/session.html" class="btn btn-primary btn-sm" style="margin-top:16px;">Créer une session</a>
        </div>`;
      return;
    }

    const sessions = data.sessions.slice(0, 5);
    el.innerHTML = sessions.map(s => `
      <div class="feed-item" style="border-left-color:${s.statut==='ouverte'?'var(--color-accent)':'var(--border)'};">
        <div class="feed-icon ${s.statut==='ouverte'?'presence':''}">
          ${s.statut === 'ouverte' ? '🟢' : '🔴'}
        </div>
        <div class="feed-content">
          <div class="feed-title">${escHtml(s.titre)}</div>
          <div class="feed-desc">${escHtml(s.filiere_nom)} · ${escHtml(s.enseignant_nom)}</div>
          <div class="feed-time">${formatDate(s.date_debut)} · ${s.presences_count} présence(s)</div>
        </div>
        <a href="/session.html?id=${s.id}" class="btn btn-ghost btn-sm">Voir →</a>
      </div>
    `).join('');

  } catch(e) {
    console.error('[Sessions] Erreur:', e);
  }
}

// ─────────────────────────────────────────
// TEMPS RÉEL — Polling (Railway Fallback)
// ─────────────────────────────────────────
function connectLiveFeed() {
  if (pollInterval) clearInterval(pollInterval);
  
  // Faire un premier appel immédiatement
  fetchEvents();
  
  // Poller toutes les 3 secondes
  pollInterval = setInterval(fetchEvents, 3000);
}

async function fetchEvents() {
  try {
    const res = await apiFetch(`/api/events?last_id=${lastEventId}`);
    if (!res) {
      updateLiveStatus(false);
      return;
    }
    const data = await res.json();
    updateLiveStatus(true);
    
    if (data.success && data.events && data.events.length > 0) {
      data.events.forEach(ev => {
        // Mettre à jour lastEventId
        if (ev.id > lastEventId) lastEventId = ev.id;
        
        // Traiter l'événement
        processEvent(ev.event, ev.data);
      });
    }
  } catch (e) {
    console.error('[LiveFeed] Erreur:', e);
    updateLiveStatus(false);
  }
}

function processEvent(eventName, data) {
  if (eventName === 'presence_marked') {
    addFeedItem('presence', '✅',
      `${escHtml(data.etudiant_nom)} — présence enregistrée`,
      `${escHtml(data.matricule)} · Session #${data.session_id}`,
      data.timestamp || new Date().toLocaleTimeString('fr-FR')
    );
    loadStats();
    showToast('success', `✅ Présence : ${data.etudiant_nom}`);
  } 
  else if (eventName === 'fraud_detected') {
    addFeedItem('fraud', '🚨',
      `Fraude détectée : ${escHtml(data.type_fraude)}`,
      `${escHtml(data.description)} · IP: ${escHtml(data.ip)}`,
      new Date().toLocaleTimeString('fr-FR')
    );
    loadStats();
    showToast('error', `🚨 Fraude : ${data.type_fraude}`, 6000);
  }
  else if (eventName === 'qr_refresh') {
    addFeedItem('qr', '🔄',
      `QR Code renouvelé — Session #${data.session_id}`,
      `Nouveau token actif · Expire dans ${data.expires_in}s`,
      new Date().toLocaleTimeString('fr-FR')
    );
  }
}

function updateLiveStatus(connected) {
  const el = document.getElementById('sse-status');
  if (!el) return;
  if (connected) {
    el.innerHTML = `<div class="live-dot"></div> <span style="color:var(--color-accent);font-size:0.78rem;font-weight:600;">LIVE</span>`;
  } else {
    el.innerHTML = `<div class="live-dot" style="background:var(--text-muted);animation:none;"></div> <span style="color:var(--text-muted);font-size:0.78rem;">Reconnexion...</span>`;
  }
}

// ─────────────────────────────────────────
// Live Feed DOM
// ─────────────────────────────────────────
function addFeedItem(type, icon, title, desc, time) {
  const feed = document.getElementById('live-feed');
  if (!feed) return;

  // Vider le placeholder
  if (feedEmpty) {
    feed.innerHTML = '';
    feedEmpty = false;
  }

  const item = document.createElement('div');
  item.className = `feed-item ${type}`;
  item.innerHTML = `
    <div class="feed-icon ${type}">${icon}</div>
    <div class="feed-content">
      <div class="feed-title">${title}</div>
      <div class="feed-desc">${desc}</div>
    </div>
    <div class="feed-time">${time || new Date().toLocaleTimeString('fr-FR')}</div>
  `;

  // Insérer en haut
  feed.insertBefore(item, feed.firstChild);

  // Limiter à 50 éléments
  const items = feed.querySelectorAll('.feed-item');
  if (items.length > 50) {
    items[items.length - 1].remove();
  }
}

function clearFeed() {
  const feed = document.getElementById('live-feed');
  if (feed) {
    feed.innerHTML = `
      <div style="text-align:center;padding:40px 0;color:var(--text-muted);">
        <div style="font-size:2rem;margin-bottom:12px;">📡</div>
        <p>Flux vidé — En attente d'événements...</p>
      </div>`;
    feedEmpty = true;
  }
}

// ─────────────────────────────────────────
// Panel fraudes
// ─────────────────────────────────────────
async function showFraudsPanel() {
  const panel = document.getElementById('frauds-panel');
  if (!panel) return;
  panel.classList.remove('hidden');
  panel.scrollIntoView({ behavior: 'smooth' });

  try {
    const res  = await apiFetch('/api/frauds');
    if (!res) return;
    const data = await res.json();
    const body = document.getElementById('frauds-body');

    if (!data.success || !data.frauds.length) {
      body.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:20px;color:var(--text-muted);">Aucune tentative de fraude enregistrée</td></tr>';
      return;
    }

    body.innerHTML = data.frauds.map(f => `
      <tr>
        <td style="color:var(--text-primary);font-size:0.82rem;">${escHtml(f.horodatage_fmt)}</td>
        <td style="color:var(--text-primary);font-weight:500;">${escHtml(f.etudiant_nom) || '—'}</td>
        <td><span class="badge badge-danger">${escHtml(f.type_fraude)}</span></td>
        <td style="color:var(--text-muted);font-size:0.82rem;max-width:200px;">${escHtml(f.description)}</td>
        <td style="font-family:var(--font-mono);font-size:0.8rem;color:var(--color-warning);">${escHtml(f.ip_client)}</td>
      </tr>
    `).join('');
  } catch(e) {
    console.error('[Frauds]', e);
  }
}

function closeFraudsPanel() {
  const panel = document.getElementById('frauds-panel');
  if (panel) panel.classList.add('hidden');
}
