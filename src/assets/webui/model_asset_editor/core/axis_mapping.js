// Model Asset Editor portable core. Physical extraction wave7C: axis_mapping.

function activeLodAxisMapping(lod){return sourceBasisMappingFromPreset(lod?.sourceBasisPreset||'game_current');}
function activeLodDirectAxisMapping(lod){return directAxisMappingFromSemantic(activeLodAxisMapping(lod));}
function axisFamily(token){return String(token||'').slice(-1);}
function axisMappingSummary(mapping){const line=axis=>{const target=mapping[axis.toLowerCase()],opposite=oppositeAxisToken(target);return `SOURCE +${axis} → GAME ${target} · ${gameAxisMeaning(target)}   |   SOURCE -${axis} → GAME ${opposite} · ${gameAxisMeaning(opposite)}`;};return `${line('X')}\n${line('Y')}\n${line('Z')}`;}
function axisRotationStepLabel(axis,degrees){const d=Number(degrees);return `${String(axis).toUpperCase()} ${d>0?'+':''}${d}°`;}
function directAxisMappingFromSemantic(mapping){const out={x:null,y:null,z:null};const assign=(sourceToken,gameToken)=>{const family=axisFamily(sourceToken).toLowerCase();out[family]=String(sourceToken).startsWith('-')?oppositeAxisToken(gameToken):gameToken;};assign(mapping.right,'+X');assign(mapping.up,'+Y');assign(mapping.forward,'-Z');return out;}
function directAxisShort(mapping){return `X→${mapping.x} · Y→${mapping.y} · Z→${mapping.z}`;}
function directMappingDeterminant(mapping){const unit=t=>{const v={X:[1,0,0],Y:[0,1,0],Z:[0,0,1]}[axisFamily(t)]||[0,0,0],sign=String(t).startsWith('-')?-1:1;return v.map(x=>x*sign);},a=unit(mapping.x),b=unit(mapping.y),c=unit(mapping.z);return a[0]*(b[1]*c[2]-b[2]*c[1])-b[0]*(a[1]*c[2]-a[2]*c[1])+c[0]*(a[1]*b[2]-a[2]*b[1]);}
function gameAxisMeaning(token){return String(gameAxisLabels[token]||token).split('·').slice(1).join('·').trim();}
function oppositeAxisToken(token){const t=String(token||'');return t.startsWith('-')?'+'+t.slice(1):'-'+t.slice(1);}
function rotateDirectMapping(mapping,axis,degrees){let turns=((Math.round(Number(degrees)/90)%4)+4)%4,out={...mapping};for(let i=0;i<turns;i++){out={x:rotateGameAxisTokenQuarterTurn(out.x,axis),y:rotateGameAxisTokenQuarterTurn(out.y,axis),z:rotateGameAxisTokenQuarterTurn(out.z,axis)};}return out;}
function rotateGameAxisTokenQuarterTurn(token,axis){const family=axisFamily(token),negative=String(token).startsWith('-'),v={X:[1,0,0],Y:[0,1,0],Z:[0,0,1]}[family]||[0,0,0];if(negative){v[0]*=-1;v[1]*=-1;v[2]*=-1;}let r=v;if(axis==='x')r=[v[0],-v[2],v[1]];else if(axis==='y')r=[v[2],v[1],-v[0]];else if(axis==='z')r=[-v[1],v[0],v[2]];const names=['X','Y','Z'];for(let i=0;i<3;i++)if(r[i])return `${r[i]<0?'-':'+'}${names[i]}`;return token;}
function sourceAxisToken(axis,negative=false){return `${negative?'-':'+'}${axis}`;}
function sourceBasisMappingFromPreset(preset){const p=String(preset||'game_current');if(p==='blender_model')return{right:'+X',up:'+Z',forward:'-Y'};if(p==='game_current')return{right:'+X',up:'+Y',forward:'-Z'};if(p.startsWith('axis:')){const parts=p.slice(5).split(',');if(parts.length===3&&parts.every(x=>axisDirectionTokens.includes(x)))return{right:parts[0],up:parts[1],forward:parts[2]};}return{right:'+X',up:'+Y',forward:'-Z'};}
function validAxisMapping(mapping){const values=[mapping.right,mapping.up,mapping.forward];return values.every(v=>axisDirectionTokens.includes(v))&&new Set(values.map(axisFamily)).size===3;}
function validDirectAxisMapping(mapping){const values=[mapping.x,mapping.y,mapping.z];return values.every(v=>axisDirectionTokens.includes(v))&&new Set(values.map(axisFamily)).size===3;}

export {activeLodAxisMapping,activeLodDirectAxisMapping,axisFamily,axisMappingSummary,axisRotationStepLabel,directAxisMappingFromSemantic,directAxisShort,directMappingDeterminant,gameAxisMeaning,oppositeAxisToken,rotateDirectMapping,rotateGameAxisTokenQuarterTurn,sourceAxisToken,sourceBasisMappingFromPreset,validAxisMapping,validDirectAxisMapping};
