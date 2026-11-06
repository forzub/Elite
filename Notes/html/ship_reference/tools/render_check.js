// Headless render check of the ship reference: runs data.js + app.js with a stub DOM.
const fs = require("fs"), vm = require("vm"), path = require("path");
const dir = process.argv[2];
function el() { return { innerHTML: "", textContent: "", href: "", dataset: {}, classList: { toggle() {} }, querySelector: () => el(), querySelectorAll: () => [], set onclick(f) {}, set oninput(f) {} }; }
function makeCtx(page, search) {
  const els = {};
  const document = { body: { dataset: { page } }, querySelector: s => (els[s] ??= el()), addEventListener() {} };
  const ctx = { window: {}, document, location: { search }, history: { replaceState() {} }, URLSearchParams, Intl, console, Map, Set, Object, Math, Number, String };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(dir, "data.js"), "utf8"), ctx);
  vm.runInContext(fs.readFileSync(path.join(dir, "app.js"), "utf8") + ";this.__api={renderMain,renderShipPage,mainShipCard,coverageHTML,partCard,partMap,D};", ctx);
  return { ctx, els };
}
const bad = /undefined|NaN|\[object Object\]/;
let problems = 0;
const { ctx, els } = makeCtx("index", "");
const A = ctx.__api;
A.renderMain();
const cov = els["#mainContent"].innerHTML;
if (!cov.includes("Покрытие флота") || bad.test(cov)) { problems++; console.log("coverage view problem", cov.match(bad)); }
console.log("index default view = coverage:", cov.includes("Покрытие флота"), "| sidebar buttons:", (els["#shipList"].innerHTML.match(/class="shipBtn/g) || []).length, "| meta:", els["#refMeta"].textContent);
for (const s of A.D.ships) {
  const h = A.mainShipCard(s);
  if (bad.test(h)) { problems++; console.log("main card", s.id, h.match(bad)[0], h.slice(Math.max(0, h.search(bad) - 120), h.search(bad) + 20)); }
  const sp = makeCtx("ship", "?id=" + encodeURIComponent(s.id));
  sp.ctx.__api.renderShipPage();
  const tree = sp.els["#equipTree"].innerHTML;
  if (sp.els["#shipTitle"].textContent !== s.name) { problems++; console.log("ship page title mismatch", s.id); }
  if (bad.test(tree)) { problems++; console.log("tree", s.id, tree.match(bad)[0]); }
  for (const g of s.functional_groups) for (const it of g.items) {
    const p = A.partMap.get(it.part_id);
    if (!p) { problems++; console.log("unresolved", s.id, it.part_id); continue; }
    const pc = A.partCard(p, s, it);
    if (/NaN|\[object Object\]/.test(pc)) { problems++; console.log("part card", s.id, it.part_id); }
  }
}
const props = A.D.ships.filter(s => s.status === "PROPOSAL (reference)");
console.log("ships", A.D.ships.length, "proposals", props.length, "parts", A.D.parts.length, "problems", problems);
