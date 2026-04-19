/* app.js — Shared utilities (Chocolate & White Theme) */
'use strict';

const API = 'http://localhost:8080';

/* ── Auth ─────────────────────────────────────────────────── */
const getToken    = () => localStorage.getItem('set_token');
const getUsername = () => localStorage.getItem('set_user');
const getUserId   = () => localStorage.getItem('set_uid');
function setAuth(token, username, uid) {
  localStorage.setItem('set_token', token);
  localStorage.setItem('set_user',  username);
  localStorage.setItem('set_uid',   uid || '');
}
function clearAuth() { ['set_token','set_user','set_uid'].forEach(k => localStorage.removeItem(k)); }
function requireAuth() { return true; }
function logout() { clearAuth(); window.location.href = '/'; }

/* ── API Client ───────────────────────────────────────────── */
async function apiCall(method, path, body = null) {
  const headers = { 'Content-Type': 'application/json' };
  const tok = getToken();
  if (tok) headers['Authorization'] = 'Bearer ' + tok;
  const opts = { method, headers };
  if (body) opts.body = JSON.stringify(body);
  try {
    const r = await fetch(API + path, opts);
    return await r.json();
  } catch (e) {
    console.error('API:', e);
    return { success: false, message: String(e) };
  }
}
const api = {
  get:    path      => apiCall('GET',    path),
  post:   (path, b) => apiCall('POST',   path, b),
  put:    (path, b) => apiCall('PUT',    path, b),
  delete: path      => apiCall('DELETE', path),
};

/* ── Theme ────────────────────────────────────────────────── */
function getTheme() { return localStorage.getItem('theme') || 'light'; }
function applyTheme(t) {
  document.documentElement.setAttribute('data-theme', t);
  localStorage.setItem('theme', t);
  const ico = document.getElementById('theme-ico');
  const lbl = document.getElementById('theme-lbl');
  if (ico) ico.textContent = t === 'dark' ? '🍫' : '🥛';
  if (lbl) lbl.textContent = t === 'dark' ? 'Dark Theme' : 'Light Theme';
}
function toggleTheme() { applyTheme(getTheme() === 'dark' ? 'light' : 'dark'); }

/* ── Toast ────────────────────────────────────────────────── */
function showToast(msg, type = 'info') {
  let c = document.getElementById('toasts');
  if (!c) { c = Object.assign(document.createElement('div'), { id:'toasts', className:'toast-container' }); document.body.appendChild(c); }
  const icons = { success:'✅', error:'❌', info:'ℹ️', warning:'⚠️' };
  const t = document.createElement('div');
  t.className = 'toast t-' + type;
  t.innerHTML = `<span>${icons[type]||icons.info}</span><span>${msg}</span>`;
  c.appendChild(t);
  setTimeout(() => { t.style.animation = 'fadeOut 0.3s ease forwards'; setTimeout(() => t.remove(), 320); }, 3200);
}

/* ── Animated Counter ─────────────────────────────────────── */
function animateCounter(el, target, dur = 950) {
  const start = performance.now();
  (function tick(now) {
    const p = Math.min((now - start) / dur, 1);
    const ease = 1 - Math.pow(1 - p, 4);
    el.textContent = fmtCurrency(target * ease);
    if (p < 1) requestAnimationFrame(tick);
  })(start);
}

/* ── Formatters ───────────────────────────────────────────── */
function fmtCurrency(n, sym = '₹') {
  return sym + new Intl.NumberFormat('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(n || 0);
}
function fmtDate(d) {
  if (!d) return '';
  try { return new Date(d).toLocaleDateString('en-IN', { day:'2-digit', month:'short', year:'numeric' }); }
  catch { return d; }
}
function fmtMonth(m) {
  if (!m) return '';
  const [y, mo] = m.split('-');
  const names = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  return (names[parseInt(mo)-1] || mo) + ' ' + y;
}
function todayISO() {
  const d = new Date();
  return d.getFullYear() + '-' + String(d.getMonth()+1).padStart(2,'0') + '-' + String(d.getDate()).padStart(2,'0');
}

/* ── Categories (Chocolate Palette) ────────────────── */
const CATS = [
  { name:'Food',          ico:'🍔', color:'#6B4226' },
  { name:'Shopping',      ico:'🛍️', color:'#8B5E3C' },
  { name:'Travel',        ico:'✈️', color:'#A07848' },
  { name:'Bills',         ico:'📋', color:'#4A2010' },
  { name:'Rent',          ico:'🏠', color:'#5C2D0E' },
  { name:'Education',     ico:'📚', color:'#7C3E18' },
  { name:'Entertainment', ico:'🎬', color:'#B07848' },
  { name:'Health',        ico:'🏥', color:'#8B4513' },
  { name:'Other',         ico:'💡', color:'#2C1206' },
];
function catIco(name)   { const c = CATS.find(x=>x.name===name); return c ? c.ico   : '💰'; }
function catColor(name) { const c = CATS.find(x=>x.name===name); return c ? c.color : '#4A2010'; }

/* ── Sidebar ──────────────────────────────────────────────── */
function renderSidebar(activePage) {
  const navItems = [
    { label:'Dashboard',       ico:'📊', href:'/dashboard.html',       page:'dashboard' },
    { label:'Add Transaction', ico:'✨', href:'/add_transaction.html', page:'add' },
    { label:'History',         ico:'📋', href:'/history.html',         page:'history' },
    { label:'Analytics',       ico:'📈', href:'/analytics.html',       page:'analytics' },
  ];
  const user  = getUsername() || 'User';
  const init  = user.charAt(0).toUpperCase();
  const theme = getTheme();
  return `
  <aside class="sidebar">
    <div class="sidebar-brand">
      <div class="sidebar-brand-icon">🍫</div>
      <div class="sidebar-brand-text">Smart Expense<span>Management</span></div>
    </div>
    <nav class="sidebar-nav">
      <div class="nav-label">Navigation</div>
      ${navItems.map(n => `
        <a href="${n.href}" class="nav-item ${activePage===n.page?'active':''}">
          <span class="nav-icon">${n.ico}</span><span>${n.label}</span>
        </a>`).join('')}
    </nav>
    <div class="sidebar-footer">
      <div class="user-chip">
        <div class="user-avatar">${init}</div>
        <div>
          <div class="user-name">${user}</div>
          <div class="user-role">Personal Profile</div>
        </div>
      </div>
      <div class="theme-toggle" onclick="toggleTheme()">
        <span class="theme-toggle-lbl">
          <span id="theme-ico">${theme==='dark'?'🍫':'🥛'}</span>
          <span id="theme-lbl">${theme==='dark'?'Dark Mode':'Clean Mode'}</span>
        </span>
        <div class="toggle-pill"></div>
      </div>
      <div class="divider" style="margin:8px 0; background:rgba(255,255,255,0.08)"></div>
      <div class="status-vault">
        <div class="status-vault-ico">🔐</div>
        <div class="status-vault-info">
          <div class="status-vault-title">Secure Vault</div>
          <div class="status-vault-sub">Local Portfolio Active</div>
        </div>
      </div>
    </div>
  </aside>`;
}

/* ── Page Init ────────────────────────────────────────────── */
function initPage(page) {
  applyTheme(getTheme());
  
  // Auto-set Admin session if none exists
  if (!getToken()) {
      setAuth('local-session-active', 'Admin', '1');
  }

  const sc = document.getElementById('sidebar-root');
  if (sc) sc.innerHTML = renderSidebar(page);
}
