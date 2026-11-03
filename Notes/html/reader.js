(()=>{"use strict";
const src=document.getElementById("md-source");if(!src)return;
const sourcePath=window.ELITE_LORE_SOURCE||"", loreSet=new Set(window.ELITE_LORE_PATHS||[]);
const md=src.textContent.replace(/<\\/script>/gi,"</script>");
const esc=s=>String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");
const norm=p=>{const o=[];for(const x of p.split("/")){if(!x||x===".")continue;if(x==="..")o.pop();else o.push(x)}return o.join("/")};
const dirname=p=>p.split("/").slice(0,-1).join("/");
const slug=s=>s.toLowerCase().replace(/<[^>]*>/g,"").replace(/[^\p{L}\p{N}\s_-]/gu,"").trim().replace(/\s+/g,"-")||"section";
const used=new Map();
const inline=s=>{const code=[];s=s.replace(/`([^`]+)`/g,(_,c)=>"\u0000C"+(code.push("<code>"+esc(c)+"</code>")-1)+"\u0000");s=esc(s);
s=s.replace(/\[([^\]]+)\]\(([^)\s]+)(?:\s+&quot;[^&]*&quot;)?\)/g,(_,t,h)=>'<a href="'+h+'">'+t+'</a>');
s=s.replace(/\*\*([^*]+)\*\*/g,"<strong>$1</strong>").replace(/__([^_]+)__/g,"<strong>$1</strong>").replace(/~~([^~]+)~~/g,"<del>$1</del>");
s=s.replace(/(^|[\s(])\*([^*\n]+)\*(?=$|[\s).,;:!?])/g,"$1<em>$2</em>").replace(/(^|[\s(])_([^_\n]+)_(?=$|[\s).,;:!?])/g,"$1<em>$2</em>");
return s.replace(/\u0000C(\d+)\u0000/g,(_,n)=>code[+n]);};
const lines=md.replace(/\r\n/g,"\n").split("\n"),out=[];let p=[],list=null,quote=[],code=null,lang="";
const fp=()=>{if(p.length){out.push("<p>"+inline(p.join(" "))+"</p>");p=[]}},fl=()=>{if(list){out.push("</"+list+">");list=null}},fq=()=>{if(quote.length){out.push("<blockquote><p>"+inline(quote.join(" "))+"</p></blockquote>");quote=[]}},close=()=>{fp();fl();fq()};
const sep=s=>{const c=s.trim().replace(/^\||\|$/g,"").split("|").map(x=>x.trim());return c.length&&c.every(x=>/^:?-{3,}:?$/.test(x))};
for(let i=0;i<lines.length;i++){let x=lines[i],m=x.match(/^\s*```(.*)$/);if(m){if(code===null){close();code=[];lang=m[1].trim()}else{out.push('<pre><code'+(lang?' class="language-'+esc(lang)+'"':'')+'>'+esc(code.join("\n"))+"</code></pre>");code=null;lang=""}continue}if(code!==null){code.push(x);continue}
if(x.includes("|")&&i+1<lines.length&&sep(lines[i+1])){close();const row=s=>s.trim().replace(/^\||\|$/g,"").split("|").map(v=>v.trim()),h=row(x),a=row(lines[i+1]).map(c=>c.startsWith(":")&&c.endsWith(":")?"center":c.endsWith(":")?"right":c.startsWith(":")?"left":"");out.push('<div class="table-wrap"><table><thead><tr>');h.forEach((c,j)=>out.push("<th"+(a[j]?' style="text-align:'+a[j]+'"':'')+">"+inline(c)+"</th>"));out.push("</tr></thead><tbody>");i+=2;while(i<lines.length&&lines[i].includes("|")&&lines[i].trim()){const r=row(lines[i]);out.push("<tr>");h.forEach((_,j)=>out.push("<td"+(a[j]?' style="text-align:'+a[j]+'"':'')+">"+inline(r[j]||"")+"</td>"));out.push("</tr>");i++}i--;out.push("</tbody></table></div>");continue}
m=x.match(/^(#{1,6})\s+(.+)$/);if(m){close();let id=slug(m[2]),n=used.get(id)||0;used.set(id,n+1);if(n)id+="-"+(n+1);out.push("<h"+m[1].length+' id="'+id+'">'+inline(m[2])+"</h"+m[1].length+">");continue}
if(/^\s*(---+|\*\*\*+|___+)\s*$/.test(x)){close();out.push("<hr>");continue}
m=x.match(/^\s*>\s?(.*)$/);if(m){fp();fl();quote.push(m[1]);continue}else fq();
let u=x.match(/^\s*[-*+]\s+(.+)$/),o=x.match(/^\s*\d+[.)]\s+(.+)$/);if(u||o){fp();fq();let t=u?"ul":"ol";if(list!==t){fl();out.push("<"+t+">");list=t}let it=(u||o)[1].replace(/^\[ \]\s+/,"☐ ").replace(/^\[[xX]\]\s+/,"☑ ");out.push("<li>"+inline(it)+"</li>");continue}else fl();
if(!x.trim()){fp();continue}if(/^\s*<\/?[a-zA-Z][^>]*>/.test(x)){fp();out.push(x);continue}p.push(x.trim())}close();if(code!==null)out.push("<pre><code>"+esc(code.join("\n"))+"</code></pre>");
const content=document.getElementById("content");content.innerHTML=out.join("\n");
for(const a of content.querySelectorAll("a[href]")){const raw=a.getAttribute("href");if(!raw||/^[a-z]+:/i.test(raw)||raw.startsWith("#"))continue;const hash=raw.indexOf("#"),part=hash>=0?raw.slice(0,hash):raw,frag=hash>=0?raw.slice(hash):"";if(!part.toLowerCase().endsWith(".md"))continue;let decoded=part;try{decoded=decodeURIComponent(part)}catch{}const target=norm(dirname(sourcePath)+"/"+decoded);if(loreSet.has(target))a.setAttribute("href",part.slice(0,-3)+".html"+frag)}
})();