function baseLocale(value){return String(value||'').split(/[-_]/)[0];}
function localeCandidates(locale){const full=String(locale||''),base=baseLocale(full),out=[];for(const item of [full,base])if(item&&!out.includes(item))out.push(item);return out;}
function formatLocalized(value,vars={}){return String(value??'').replace(/\{([A-Za-z0-9_]+)\}/g,(match,key)=>vars[key]??match);}
function translateCatalog(catalog,locale,key,fallback=key,vars={}){
 const map=catalog?.strings?.[key];
 if(map&&typeof map==='object'){
  for(const candidate of localeCandidates(locale))if(map[candidate]!==undefined&&map[candidate]!==null&&String(map[candidate]).length)return formatLocalized(map[candidate],vars);
  if(map.en!==undefined&&map.en!==null&&String(map.en).length)return formatLocalized(map.en,vars);
 }
 return formatLocalized(fallback,vars);
}
function catalogMissingLocales(catalog,locales=catalog?.locale_order||[]){const missing=[];for(const [key,row] of Object.entries(catalog?.strings||{}))for(const locale of locales)if(!String(row?.[locale]??'').trim())missing.push({key,locale});return missing;}
export {baseLocale,formatLocalized,translateCatalog,catalogMissingLocales};
