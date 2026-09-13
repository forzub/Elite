const textFallbacks=new WeakMap(),titleFallbacks=new WeakMap(),ariaFallbacks=new WeakMap(),placeholderFallbacks=new WeakMap();
function remember(map,element,value){if(!map.has(element))map.set(element,String(value??''));return map.get(element);}
function applyLocalizedDom(root,translate){
 if(!root||typeof translate!=='function')return;
 const scope=(selector)=>[...(root.matches?.(selector)?[root]:[]),...(root.querySelectorAll?.(selector)||[])];
 for(const element of scope('[data-i18n]'))element.textContent=translate(element.dataset.i18n,remember(textFallbacks,element,element.textContent));
 for(const element of scope('[data-i18n-title]'))element.title=translate(element.dataset.i18nTitle,remember(titleFallbacks,element,element.title));
 for(const element of scope('[data-i18n-aria]'))element.setAttribute('aria-label',translate(element.dataset.i18nAria,remember(ariaFallbacks,element,element.getAttribute('aria-label')||'')));
 for(const element of scope('[data-i18n-placeholder]'))element.setAttribute('placeholder',translate(element.dataset.i18nPlaceholder,remember(placeholderFallbacks,element,element.getAttribute('placeholder')||'')));
}
export {applyLocalizedDom};
