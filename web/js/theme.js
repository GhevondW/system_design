/**
 * Theme — dark/light mode toggle with localStorage persistence.
 */
const STORAGE_KEY = 'systemforge-theme';

export function initTheme() {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
        document.documentElement.setAttribute('data-theme', saved);
    }
    updateIcon();
}

export function toggleTheme() {
    const current = document.documentElement.getAttribute('data-theme');
    const next = current === 'light' ? 'dark' : 'light';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem(STORAGE_KEY, next);
    updateIcon();
}

function updateIcon() {
    const btn = document.getElementById('theme-toggle');
    if (!btn) return;
    const isLight = document.documentElement.getAttribute('data-theme') === 'light';
    btn.innerHTML = isLight
        ? '<i class="fas fa-moon"></i>'
        : '<i class="fas fa-sun"></i>';
    btn.title = isLight ? 'Switch to dark mode' : 'Switch to light mode';
}

// Auto-init on import
initTheme();
