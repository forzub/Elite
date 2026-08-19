(() => {
  'use strict';

  const VERSION = 1;
  const DEFAULT_PASSWORD_LENGTH = 20;
  const DOCUMENT_TRANSITION_MS = 180;
  const VIEW_TRANSITION_MS = 140;
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
    let pendingRequest = null;
    let runnerPromise = null;
    let requestId = 0;

    function focusFor(view, options) {
      const focusTarget = options.focus || view.querySelector('[data-elite-autofocus]');
      if (focusTarget)
        requestAnimationFrame(() => focusElement(focusTarget, options.select === true));
    }

    function hideView(view) {
      if (!view) return;
      view.hidden = true;
      view.dataset.active = 'false';
      view.setAttribute('aria-hidden', 'true');
      view.classList.remove('elite-view--entering', 'elite-view--leaving');
    }

    async function presentRequest(request) {
      const { name, options } = request;
      const animate = options.animate !== false;
      const nextView = views.get(name);
      const previousView = currentView ? views.get(currentView) : null;

      if (previousView === nextView) {
        root.dataset.eliteRoute = name;
        focusFor(nextView, options);
        return true;
      }

      if (previousView) {
        if (animate) {
          previousView.classList.add('elite-view--leaving');
          await waitForCssTransition(previousView, 'opacity', VIEW_TRANSITION_MS + 120);
        }
        hideView(previousView);
        currentView = '';
        root.dataset.eliteRoute = '';

        // A newer route that arrived while the old route was fading wins
        // before any superseded destination is exposed. This avoids one-frame
        // intermediate forms/buttons during rapid native state changes.
        if (pendingRequest) return false;
      }

      for (const view of views.values()) {
        if (view !== nextView) hideView(view);
      }

      nextView.hidden = false;
      nextView.dataset.active = 'true';
      nextView.setAttribute('aria-hidden', 'false');
      if (animate) nextView.classList.add('elite-view--entering');

      await nextAnimationFrame();
      if (pendingRequest) {
        hideView(nextView);
        return false;
      }

      nextView.classList.remove('elite-view--entering');
      if (animate)
        await waitForCssTransition(nextView, 'opacity', VIEW_TRANSITION_MS + 120);

      currentView = name;
      root.dataset.eliteRoute = name;
      focusFor(nextView, options);
      return true;
    }

    async function drainRequests() {
      while (pendingRequest) {
        const request = pendingRequest;
        pendingRequest = null;
        try {
          request.resolve(await presentRequest(request));
        } catch (error) {
          console.error('EliteUiKit navigation transition failed:', error);
          request.resolve(false);
        }
      }
    }

    function show(name, options = {}) {
      if (!views.has(name)) throw new Error(`Unknown EliteUiKit view: ${name}`);

      return new Promise(resolve => {
        const request = { id: ++requestId, name, options, resolve };
        if (pendingRequest) pendingRequest.resolve(false);
        pendingRequest = request;

        if (!runnerPromise) {
          runnerPromise = drainRequests().finally(() => {
            runnerPromise = null;
            if (pendingRequest) {
              runnerPromise = drainRequests().finally(() => { runnerPromise = null; });
            }
          });
        }
      });
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


  function reducedMotion() {
    return !!(window.matchMedia &&
      window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  }

  function sleepForTransition(ms) {
    if (reducedMotion() || !ms) return Promise.resolve();
    return new Promise(resolve => window.setTimeout(resolve, ms));
  }

  function nextAnimationFrame() {
    return new Promise(resolve => requestAnimationFrame(() => resolve()));
  }

  function waitForCssTransition(element, propertyName, fallbackMs) {
    if (!element || reducedMotion()) return Promise.resolve();
    return new Promise(resolve => {
      let finished = false;
      const finish = () => {
        if (finished) return;
        finished = true;
        element.removeEventListener('transitionend', onEnd);
        window.clearTimeout(timer);
        resolve();
      };
      const onEnd = event => {
        if (event.target !== element) return;
        if (propertyName && event.propertyName !== propertyName) return;
        finish();
      };
      const timer = window.setTimeout(finish, Math.max(50, fallbackMs || 300));
      element.addEventListener('transitionend', onEnd);
    });
  }

  function restoreDocument(force = false) {
    const html = document.documentElement;
    if (!force && html.classList.contains('elite-ui-boot'))
      return;
    html.classList.remove('elite-ui-boot', 'elite-ui-leaving');
    html.classList.add('elite-ui-ready');
  }

  async function fadeOutDocument() {
    const html = document.documentElement;
    if (html.classList.contains('elite-ui-leaving')) {
      await waitForCssTransition(document.body, 'opacity', DOCUMENT_TRANSITION_MS + 150);
      return;
    }
    html.classList.remove('elite-ui-boot');
    html.classList.add('elite-ui-ready');
    await nextAnimationFrame();
    html.classList.add('elite-ui-leaving');
    await waitForCssTransition(document.body, 'opacity', DOCUMENT_TRANSITION_MS + 150);
  }

  async function waitForDocumentDependencies() {
    const waits = [];
    if (window.GameI18n && typeof window.GameI18n.ready === 'function')
      waits.push(window.GameI18n.ready());
    if (document.fonts && document.fonts.ready)
      waits.push(document.fonts.ready);
    if (waits.length) await Promise.allSettled(waits);
  }

  async function settleLayout(frameCount = 2) {
    const frames = Math.max(1, Math.min(4, Number(frameCount) || 2));
    for (let i = 0; i < frames; ++i) await nextAnimationFrame();
  }

  function forcePreparedLayout() {
    // Force style/layout without requiring a compositor frame. requestAnimationFrame
    // may be heavily throttled while the native WebView child HWND is hidden,
    // which previously turned "prepare before show" into a visible delay.
    void document.documentElement.getBoundingClientRect();
    if (document.body) void document.body.getBoundingClientRect();
  }

  async function settlePreparedLayout() {
    forcePreparedLayout();
    await Promise.resolve();
    await new Promise(resolve => window.setTimeout(resolve, 0));
    forcePreparedLayout();
  }

  function revealPreparedDocument() {
    restoreDocument(true);
  }

  async function revealDocumentWhenReady() {
    await waitForDocumentDependencies();
    await settleLayout(2);
    revealPreparedDocument();
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
    generatePassword,
    fadeOutDocument,
    restoreDocument,
    waitForDocumentDependencies,
    settleLayout,
    settlePreparedLayout,
    waitForCssTransition,
    revealPreparedDocument,
    revealDocumentWhenReady
  });
})();
