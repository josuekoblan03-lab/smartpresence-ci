/* ============================================================
   app.js — Bootstrap global : auth, navigation, toasts
   SMARTPRESENCE CI
   ============================================================ */

// ─────────────────────────────────────────
// Toast Notifications
// ─────────────────────────────────────────
function showToast(type, message, duration = 4000) {
  const container = document.getElementById('toast-container');
  if (!container) return;

  const icons = { success: '✅', error: '❌', warning: '⚠️', info: 'ℹ️' };
  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  toast.innerHTML = `
    <span style="font-size:1.1rem;">${icons[type] || 'ℹ️'}</span>
    <span style="flex:1;">${message}</span>
    <button onclick="this.parentElement.remove()" style="background:none;border:none;color:var(--text-muted);cursor:pointer;font-size:1rem;padding:0 4px;">✕</button>
  `;
  container.appendChild(toast);

  // Auto-remove
  setTimeout(() => {
    if (toast.parentElement) {
      toast.style.animation = 'fadeIn 0.3s reverse both';
      setTimeout(() => toast.remove(), 300);
    }
  }, duration);
}

// ─────────────────────────────────────────
// Gestion Auth
// ─────────────────────────────────────────
function getUser() {
  try {
    return JSON.parse(localStorage.getItem('sp_user') || 'null');
  } catch { return null; }
}

function getToken() {
  return localStorage.getItem('sp_token') || '';
}

function isAuth() {
  return !!getToken() && !!getUser();
}

async function logout() {
  try {
    await fetch('/api/auth/logout', {
      method: 'POST',
      credentials: 'include'
    });
  } catch(e) {}
  localStorage.removeItem('sp_user');
  localStorage.removeItem('sp_token');
  window.location.href = '/index.html';
}

// Protéger les pages dashboard (redirection si non connecté)
async function requireAuth() {
  const token = getToken();
  if (!token) {
    window.location.href = '/index.html';
    return null;
  }
  try {
    const res = await fetch('/api/auth/me', { credentials: 'include' });
    const data = await res.json();
    if (!data.success) {
      localStorage.clear();
      window.location.href = '/index.html';
      return null;
    }
    return data.user;
  } catch (e) {
    window.location.href = '/index.html';
    return null;
  }
}

// ─────────────────────────────────────────
// Fetch avec auth automatique
// ─────────────────────────────────────────
async function apiFetch(url, options = {}) {
  const defaults = {
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json',
      ...(options.headers || {})
    }
  };
  const res = await fetch(url, { ...defaults, ...options });
  if (res.status === 401) {
    localStorage.clear();
    window.location.href = '/index.html';
    return null;
  }
  return res;
}

// ─────────────────────────────────────────
// Sidebar responsive
// ─────────────────────────────────────────
function initSidebar() {
  const menuBtn  = document.getElementById('mobile-menu-btn');
  const sidebar  = document.getElementById('sidebar');
  const overlay  = document.getElementById('sidebar-overlay');

  if (!menuBtn || !sidebar) return;

  menuBtn.addEventListener('click', () => {
    sidebar.classList.toggle('open');
    overlay.classList.toggle('visible');
  });

  if (overlay) {
    overlay.addEventListener('click', () => {
      sidebar.classList.remove('open');
      overlay.classList.remove('visible');
    });
  }
}

// ─────────────────────────────────────────
// Navigation active
// ─────────────────────────────────────────
function setActiveNav() {
  const path = window.location.pathname;
  document.querySelectorAll('.nav-item').forEach(item => {
    const href = item.getAttribute('href');
    if (href && path.includes(href.replace('.html', ''))) {
      item.classList.add('active');
    }
  });
}

// ─────────────────────────────────────────
// Remplir info utilisateur dans sidebar
// ─────────────────────────────────────────
function fillUserInfo(user) {
  if (!user) return;

  const nameEl   = document.getElementById('user-display-name');
  const roleEl   = document.getElementById('user-display-role');
  const avatarEl = document.getElementById('user-avatar');

  if (nameEl) nameEl.textContent = user.prenom + ' ' + user.nom;
  if (roleEl) {
    const roles = { admin: '👑 Administrateur', enseignant: '👨‍🏫 Enseignant' };
    roleEl.textContent = roles[user.role] || user.role;
  }
  if (avatarEl) {
    const initials = ((user.prenom || '?')[0] + (user.nom || '?')[0]).toUpperCase();
    avatarEl.textContent = initials;
  }

  // Afficher/masquer les éléments admin
  if (user.role !== 'admin') {
    document.querySelectorAll('.admin-only').forEach(el => el.remove());
  }
}

// ─────────────────────────────────────────
// Formater dates
// ─────────────────────────────────────────
function formatDate(timestamp) {
  if (!timestamp) return '—';
  return new Date(timestamp * 1000).toLocaleString('fr-FR', {
    day: '2-digit', month: '2-digit', year: 'numeric',
    hour: '2-digit', minute: '2-digit'
  });
}

function timeAgo(timestamp) {
  const diff = Math.floor(Date.now() / 1000) - timestamp;
  if (diff < 60)   return 'À l\'instant';
  if (diff < 3600) return `Il y a ${Math.floor(diff/60)} min`;
  if (diff < 86400)return `Il y a ${Math.floor(diff/3600)} h`;
  return `Il y a ${Math.floor(diff/86400)} j`;
}

// ─────────────────────────────────────────
// Escaper HTML pour sécurité
// ─────────────────────────────────────────
function escHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

// ─────────────────────────────────────────
// Init globale
// ─────────────────────────────────────────
document.addEventListener('DOMContentLoaded', function() {
  initSidebar();
  setActiveNav();
});
