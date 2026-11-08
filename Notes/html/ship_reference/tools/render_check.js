// Consistency check of the ship reference data as the page uses it (run from the reference folder:
// node tools/render_check.js .): every ship has a unique model_code and its group codes are <model_code>A<k>, every module resolves to a part,
// every part category / ship class has a Russian name in app.js. The page itself is checked in Chrome.
const fs = require("fs"), vm = require("vm"), path = require("path");
const dir = process.argv[2] || ".";
const ctx = { window: {} }; vm.createContext(ctx);
for (const f of ["data.js"]) vm.runInContext(fs.readFileSync(path.join(dir, f), "utf8"), ctx);
const D = ctx.window.ELITE_SHIP_REF_DATA;
const app = fs.readFileSync(path.join(dir, "app.js"), "utf8");
const parts = new Map(D.parts.map(p => [p.id, p]));
let problems = 0; const bad = m => { problems++; console.log(m); };
const seen = new Map();
for (const s of D.ships) {
  const c = s.model_code;
  if (c == null) bad("no model code: " + s.id);
  else if (seen.has(c)) bad(`model code ${c} shared by ${seen.get(c)} and ${s.id}`); else seen.set(c, s.id);
  if (!app.includes(" " + s.economic_class + ": \"")) bad("class without a Russian name: " + s.economic_class);
  for (const g of s.functional_groups) {
    if (!g.code.startsWith(c + "A") || !/^[0-9]+$/.test(g.code.slice(c.length + 1))) bad(`section code ${g.code} of ${s.id} is not ${c}A<k>`);
    for (const it of g.items) if (!parts.has(it.part_id)) bad(`unresolved module ${it.part_id} on ${s.id}`);
  }
}
for (const p of D.parts) if (!app.includes(" " + p.cat + ": [")) bad("category without a description: " + p.cat);
console.log("ships", D.ships.length, "codes", seen.size, "parts", D.parts.length, "problems", problems);
process.exit(problems ? 1 : 0);
