// Workshop storage of the ship reference: model-ready flags, prompt edits, sketches (images + notes),
// ship pictures. A static page cannot write files by itself, so:
//  - the browser's IndexedDB always holds a full copy (works everywhere, survives reloads);
//  - "Подключить папку" (Chrome / Edge, File System Access API) mirrors everything into a folder on disk
//    (recommended: Notes/html/ship_reference/workshop - then the sketches are in git and shared):
//      workshop.json                    flags, prompts, sketch list with notes
//      sketches/<part id>/<sketch id>.<ext>
//      ships/<ship id>.<ext>
//    On a later visit the folder is read back and merged (union; the newer note wins).
"use strict";
const Store = (() => {
  const DB = "elite_ship_reference_workshop", V = 1;
  let db = null, dir = null;
  let folder = "none";            // none | connected | needs-permission | unsupported
  const listeners = new Set();
  const urls = new Map();         // blob id -> object URL
  const empty = () => ({ version: 1, modelReady: {}, prompts: {}, sketches: {}, shipImages: {}, reviews: {} });
  let state = empty();

  const req = r => new Promise((ok, ko) => { r.onsuccess = () => ok(r.result); r.onerror = () => ko(r.error); });
  function open() {
    return new Promise((ok, ko) => {
      const r = indexedDB.open(DB, V);
      r.onupgradeneeded = () => { r.result.createObjectStore("kv"); r.result.createObjectStore("blobs"); };
      r.onsuccess = () => ok(r.result); r.onerror = () => ko(r.error);
    });
  }
  const tx = (store, mode = "readonly") => db.transaction(store, mode).objectStore(store);
  const kvGet = k => req(tx("kv").get(k));
  const kvPut = (k, v) => req(tx("kv", "readwrite").put(v, k));
  const blobGet = id => req(tx("blobs").get(id));
  const blobPut = (id, b) => req(tx("blobs", "readwrite").put(b, id));
  const blobDel = id => req(tx("blobs", "readwrite").delete(id));

  const safe = s => String(s).replace(/[^a-zA-Z0-9._-]+/g, "_");
  const ext = b => ({ "image/png": "png", "image/jpeg": "jpg", "image/webp": "webp", "image/gif": "gif", "image/svg+xml": "svg" }[b.type] || "png");
  const emit = () => listeners.forEach(f => { try { f(); } catch (e) { console.error(e); } });

  // ---- folder (File System Access API)
  async function subdir(path, create) {
    let d = dir;
    for (const p of path) d = await d.getDirectoryHandle(p, { create });
    return d;
  }
  async function writeFile(path, data) {
    const d = await subdir(path.slice(0, -1), true);
    const f = await d.getFileHandle(path[path.length - 1], { create: true });
    const w = await f.createWritable(); await w.write(data); await w.close();
  }
  async function readFile(path) {
    try { const d = await subdir(path.slice(0, -1), false); return await (await d.getFileHandle(path[path.length - 1])).getFile(); }
    catch { return null; }
  }
  async function removeFile(path) {
    try { const d = await subdir(path.slice(0, -1), false); await d.removeEntry(path[path.length - 1]); } catch { /* already gone */ }
  }
  const sketchPath = (partId, s) => ["sketches", safe(partId), s.file];
  const shipPath = (shipId, s) => ["ships", s.file];

  function merge(into, from) {
    for (const k of ["modelReady", "prompts", "shipImages", "reviews"])
      for (const [id, v] of Object.entries((into[k] ||= {}, from[k]) || {}))
        if (!(id in into[k]) || (v && v.updated && (!into[k][id] || (into[k][id].updated || 0) < v.updated))) into[k][id] = v;
    for (const [pid, list] of Object.entries(from.sketches || {})) {
      const mine = into.sketches[pid] ||= [];
      for (const s of list) {
        const m = mine.find(x => x.id === s.id);
        if (!m) mine.push(s);
        else if ((s.updated || 0) > (m.updated || 0)) {
          // a note is the author's text: a newer record without a note never erases one
          const keep = m.note && !s.note ? m.note : null;
          Object.assign(m, s);
          if (keep) m.note = keep;
        }
      }
    }
    // a deletion recorded on either side wins
    const gone = new Set([...(into.deleted || []), ...(from.deleted || [])]);
    into.deleted = [...gone];
    for (const pid of Object.keys(into.sketches)) into.sketches[pid] = into.sketches[pid].filter(s => !gone.has(s.id));
  }

  async function syncFolder() {
    const f = await readFile(["workshop.json"]);
    if (f) { try { merge(state, JSON.parse(await f.text())); } catch (e) { console.warn("workshop.json unreadable", e); } }
    // blobs: folder -> IndexedDB where missing, IndexedDB -> folder where missing
    for (const [pid, list] of Object.entries(state.sketches))
      for (const s of list) await syncBlob(s.id, sketchPath(pid, s));
    for (const [sid, s] of Object.entries(state.shipImages)) if (s) await syncBlob(s.id, shipPath(sid, s));
    await kvPut("state", state);
    await writeFile(["workshop.json"], JSON.stringify(state, null, 1));
  }
  async function syncBlob(id, path) {
    const local = await blobGet(id), disk = await readFile(path);
    if (!local && disk) await blobPut(id, disk);
    else if (local && !disk) await writeFile(path, local);
  }

  let saveTimer = null;
  function save() {
    kvPut("state", state).catch(e => console.error("IndexedDB save failed", e));
    if (folder === "connected") {
      clearTimeout(saveTimer);
      saveTimer = setTimeout(() => writeFile(["workshop.json"], JSON.stringify(state, null, 1)).catch(folderError), 300);
    }
    emit();
  }
  function folderError(e) { console.error("folder write failed", e); folder = "needs-permission"; emit(); }

  return {
    get state() { return state; },
    get folder() { return folder; },
    get folderName() { return dir ? dir.name : ""; },
    onChange(f) { listeners.add(f); },
    async init() {
      db = await open();
      state = Object.assign(empty(), (await kvGet("state")) || {});
      if (!window.showDirectoryPicker) { folder = "unsupported"; return; }
      dir = (await kvGet("dir")) || null;
      if (!dir) return;
      try {
        if ((await dir.queryPermission({ mode: "readwrite" })) === "granted") { folder = "connected"; await syncFolder(); }
        else folder = "needs-permission";
      } catch (e) { console.warn(e); folder = "needs-permission"; }
    },
    async connectFolder() {
      if (!window.showDirectoryPicker) { alert("Этот браузер не умеет писать в папку (нужны Chrome или Edge). Данные хранятся в браузере."); return; }
      if (dir && folder === "needs-permission") {
        if ((await dir.requestPermission({ mode: "readwrite" })) !== "granted") return;
      } else {
        dir = await window.showDirectoryPicker({ id: "elite-ship-ref", mode: "readwrite" });
        await kvPut("dir", dir);
      }
      folder = "connected";
      await syncFolder();
      emit();
    },
    // ---- model-ready flags (key: part id, or "hull:<ship id>")
    isReady(key) { return !!state.modelReady[key]?.ready; },
    setReady(key, ready) { state.modelReady[key] = { ready, updated: Date.now() }; save(); },
    // ---- prompt overrides
    prompt(partId) { return state.prompts[partId]?.text ?? null; },
    promptInfo(partId) { return state.prompts[partId] || null; },
    // a review of a sketch (written into workshop.json by the assistant; read-only on the page)
    review(sketchId) { return (state.reviews || {})[sketchId] || null; },
    setPrompt(partId, text) { if (text == null) delete state.prompts[partId]; else state.prompts[partId] = { text, updated: Date.now() }; save(); },
    // ---- sketches
    sketches(partId) { return state.sketches[partId] || []; },
    async addSketch(partId, file) {
      const id = Date.now().toString(36) + Math.random().toString(36).slice(2, 7);
      const s = { id, file: id + "." + ext(file), name: file.name || "эскиз", note: "", added: Date.now(), updated: Date.now() };
      await blobPut(id, file);
      (state.sketches[partId] ||= []).push(s);
      if (folder === "connected") await writeFile(sketchPath(partId, s), file).catch(folderError);
      save();
      return s;
    },
    setNote(partId, id, note) {
      const s = this.sketches(partId).find(x => x.id === id); if (!s) return;
      s.note = note; s.updated = Date.now(); save();
    },
    async removeSketch(partId, id) {
      const list = this.sketches(partId), s = list.find(x => x.id === id); if (!s) return;
      state.sketches[partId] = list.filter(x => x.id !== id);
      (state.deleted ||= []).push(id);
      await blobDel(id);
      if (folder === "connected") await removeFile(sketchPath(partId, s));
      save();
    },
    // ---- ship pictures
    shipImage(shipId) { return state.shipImages[shipId] || null; },
    async setShipImage(shipId, file) {
      const old = state.shipImages[shipId];
      const id = "ship_" + Date.now().toString(36);
      const s = { id, file: safe(shipId) + "." + ext(file), updated: Date.now() };
      await blobPut(id, file);
      if (old) { await blobDel(old.id); urls.delete(old.id); }
      state.shipImages[shipId] = s;
      if (folder === "connected") await writeFile(shipPath(shipId, s), file).catch(folderError);
      save();
    },
    // ---- object URL of a stored image (null while missing)
    async url(id) {
      if (urls.has(id)) return urls.get(id);
      if (!db) return null;
      const b = await blobGet(id);
      if (!b) return null;
      const u = URL.createObjectURL(b); urls.set(id, u); return u;
    },
    async blob(id) { return db ? blobGet(id) : null; },
  };
})();
