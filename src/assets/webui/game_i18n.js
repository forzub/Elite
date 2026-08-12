(function () {
  const STORAGE_KEY = 'elite.ui.locale';
  let data = null;
  let allowedLocales = ['en'];
  const queryLocale = new URLSearchParams(location.search).get('locale');
  let locale = queryLocale || localStorage.getItem(STORAGE_KEY) || 'en';
  if (queryLocale) localStorage.setItem(STORAGE_KEY, queryLocale);

  function baseLocale(value) {
    return String(value || '').split(/[-_]/)[0];
  }

  function resolve(map, fallback) {
    if (!map || typeof map !== 'object') return fallback;
    if (map[locale]) return map[locale];
    const base = baseLocale(locale);
    if (base !== locale && map[base]) return map[base];
    if (map.en) return map.en;
    return fallback;
  }

  function t(key, fallback) {
    if (!data || !data.strings) return fallback || key;
    return resolve(data.strings[key], fallback || key);
  }

  function ensureIndicator() {
    let el = document.getElementById('gameUiLanguageIndicator');
    if (el) return el;
    el = document.createElement('div');
    el.id = 'gameUiLanguageIndicator';
    Object.assign(el.style, {
      position: 'fixed', right: '10px', bottom: '8px', zIndex: '99999',
      opacity: '0.52', fontSize: '11px', letterSpacing: '0.08em',
      pointerEvents: 'none', color: '#9bb8d7', fontFamily: 'Arial, sans-serif'
    });
    document.body.appendChild(el);
    return el;
  }

  function apply(root) {
    root = root || document;
    root.querySelectorAll('[data-i18n]').forEach(el => {
      const key = el.getAttribute('data-i18n');
      el.textContent = t(key, el.textContent);
    });
    document.documentElement.lang = locale;
    const indicator = ensureIndicator();
    const langMap = data?.languages?.[locale];
    indicator.textContent = `UI: ${resolve(langMap, locale)}`;
    window.dispatchEvent(new CustomEvent('game-ui-language-changed', {
      detail: { locale }
    }));
  }

  async function load() {
    try {
      const response = await fetch('/localization/ui_strings.json', { cache: 'no-store' });
      data = await response.json();
      allowedLocales = Array.isArray(data.locale_order) && data.locale_order.length
        ? data.locale_order.slice()
        : ['en'];
      if (!allowedLocales.includes(locale)) locale = data.default_locale || 'en';
    } catch (e) {
      console.warn('GameI18n load failed:', e);
    }
    apply(document);
  }

  function setLocale(nextLocale) {
    const requested = nextLocale || 'en';
    locale = allowedLocales.includes(requested)
      ? requested
      : ((data && data.default_locale) || 'en');
    localStorage.setItem(STORAGE_KEY, locale);
    apply(document);
  }

  window.GameI18n = { load, t, apply, setLocale, locale: () => locale };
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', load, { once: true });
  } else {
    load();
  }
})();
