(() => {
  'use strict';

  const VERSION = 1;
  const DEFAULT_PASSWORD_LENGTH = 20;
  const PASSWORD_ALPHABET =
    'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%^&*-_=+';

  function asElement(target) {
    if (!target) return null;
    if (typeof target === 'string') return document.querySelector(target);
    return target;
  }

  function focusElement(target, selectText = false) {
    const element = asElement(target);
    if (!element || typeof element.focus !== 'function') return false;
    element.focus({ preventScroll: true });
    if (selectText && typeof element.select === 'function') element.select();
    return true;
  }

  function setBanner(target, message, tone = 'error') {
    const element = asElement(target);
    if (!element) return;

    const text = String(message || '');
    element.textContent = text;
    element.dataset.tone = text ? tone : '';
    element.hidden = !text;
    element.setAttribute('aria-hidden', text ? 'false' : 'true');
  }

  function setFieldError(fieldTarget, message) {
    const field = asElement(fieldTarget);
    if (!field) return;

    const control = field.querySelector('input, select, textarea');
    const error = field.querySelector('[data-elite-field-error]');
    const text = String(message || '');
    field.dataset.invalid = text ? 'true' : 'false';
    if (control) control.setAttribute('aria-invalid', text ? 'true' : 'false');
    if (error) {
      error.textContent = text;
      error.hidden = !text;
    }
  }

  function createNavigationShell(rootTarget) {
    const root = asElement(rootTarget);
    if (!root) throw new Error('EliteUiKit.createNavigationShell requires a root element');

    const views = new Map();
    for (const view of root.querySelectorAll('[data-elite-view]')) {
      const name = view.dataset.eliteView;
      if (!name || views.has(name)) {
        throw new Error('EliteUiKit navigation view names must be unique and non-empty');
      }
      views.set(name, view);
    }

    let currentView = '';
    function show(name, options = {}) {
      if (!views.has(name)) throw new Error(`Unknown EliteUiKit view: ${name}`);
      for (const [viewName, view] of views) {
        const active = viewName === name;
        view.hidden = !active;
        view.dataset.active = active ? 'true' : 'false';
        view.setAttribute('aria-hidden', active ? 'false' : 'true');
      }
      currentView = name;
      root.dataset.eliteRoute = name;

      const focusTarget = options.focus || views.get(name).querySelector('[data-elite-autofocus]');
      if (focusTarget) {
        requestAnimationFrame(() => focusElement(focusTarget, options.select === true));
      }
    }

    return Object.freeze({
      show,
      current: () => currentView,
      has: name => views.has(name)
    });
  }

  function showDialog(target) {
    const dialog = asElement(target);
    if (!dialog) return false;
    if (typeof dialog.showModal === 'function') dialog.showModal();
    else dialog.setAttribute('open', '');
    const focusTarget = dialog.querySelector('[data-elite-autofocus], button, input, select, textarea');
    if (focusTarget) requestAnimationFrame(() => focusElement(focusTarget));
    return true;
  }

  function closeDialog(target, returnValue = '') {
    const dialog = asElement(target);
    if (!dialog) return false;
    if (typeof dialog.close === 'function') dialog.close(returnValue);
    else dialog.removeAttribute('open');
    return true;
  }

  function bindDialogs(rootTarget = document) {
    const root = asElement(rootTarget) || rootTarget;
    for (const opener of root.querySelectorAll('[data-elite-dialog-open]')) {
      if (opener.dataset.eliteDialogBound === 'true') continue;
      opener.dataset.eliteDialogBound = 'true';
      opener.addEventListener('click', () => showDialog(opener.dataset.eliteDialogOpen));
    }
    for (const closer of root.querySelectorAll('[data-elite-dialog-close]')) {
      if (closer.dataset.eliteDialogBound === 'true') continue;
      closer.dataset.eliteDialogBound = 'true';
      closer.addEventListener('click', () => {
        const dialog = closer.closest('dialog, [data-elite-dialog]');
        if (dialog) closeDialog(dialog, closer.dataset.eliteDialogClose || '');
      });
    }
  }

  function generatePassword(length = DEFAULT_PASSWORD_LENGTH) {
    const requested = Number.isFinite(length) ? Math.trunc(length) : DEFAULT_PASSWORD_LENGTH;
    const size = Math.max(12, Math.min(64, requested));
    if (!window.crypto || typeof window.crypto.getRandomValues !== 'function') {
      throw new Error('Secure password generation requires Web Crypto');
    }

    const output = [];
    const limit = 256 - (256 % PASSWORD_ALPHABET.length);
    while (output.length < size) {
      const bytes = new Uint8Array(Math.max(32, size - output.length));
      window.crypto.getRandomValues(bytes);
      for (const value of bytes) {
        if (value >= limit) continue;
        output.push(PASSWORD_ALPHABET[value % PASSWORD_ALPHABET.length]);
        if (output.length === size) break;
      }
    }
    return output.join('');
  }

  function bindPasswordFields(rootTarget = document) {
    const root = asElement(rootTarget) || rootTarget;
    for (const field of root.querySelectorAll('[data-elite-password]')) {
      if (field.dataset.elitePasswordBound === 'true') continue;
      field.dataset.elitePasswordBound = 'true';

      const input = field.querySelector('input[type="password"], input[data-elite-password-input]');
      const toggle = field.querySelector('[data-elite-password-toggle]');
      const generate = field.querySelector('[data-elite-password-generate]');
      if (!input) continue;

      if (toggle) {
        toggle.addEventListener('click', () => {
          const reveal = input.type === 'password';
          input.type = reveal ? 'text' : 'password';
          toggle.setAttribute('aria-pressed', reveal ? 'true' : 'false');
          input.focus({ preventScroll: true });
        });
      }

      if (generate) {
        generate.addEventListener('click', () => {
          input.value = generatePassword(Number(generate.dataset.elitePasswordLength || DEFAULT_PASSWORD_LENGTH));
          input.dispatchEvent(new Event('input', { bubbles: true }));
          input.focus({ preventScroll: true });
          if (typeof input.select === 'function') input.select();
        });
      }
    }
  }

  function initialize(rootTarget = document) {
    const root = asElement(rootTarget) || rootTarget;
    bindDialogs(root);
    bindPasswordFields(root);
  }

  window.EliteUiKit = Object.freeze({
    version: VERSION,
    initialize,
    focusElement,
    setBanner,
    setFieldError,
    createNavigationShell,
    showDialog,
    closeDialog,
    bindDialogs,
    bindPasswordFields,
    generatePassword
  });
})();
