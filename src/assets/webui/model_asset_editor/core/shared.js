// Model Asset Editor portable core. Physical extraction wave7A.

function escapeMarkup(value){return String(value??'').replace(/[&<>"']/g,ch=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));}
function diagnosticValue(error){if(error instanceof Error)return {message:error.message,stack:error.stack||''};if(typeof error==='string')return {message:error,stack:''};try{return {message:JSON.stringify(error),stack:''};}catch{return {message:String(error),stack:''};}}
function formatBytes(value){const n=Number(value)||0;if(n<1024)return `${n} B`;if(n<1024*1024)return `${(n/1024).toFixed(1)} KiB`;if(n<1024*1024*1024)return `${(n/(1024*1024)).toFixed(2)} MiB`;return `${(n/(1024*1024*1024)).toFixed(2)} GiB`;}
function formatMeters(value){const n=Number(value)||0;if(n>=1000)return `${(n/1000).toFixed(n>=10000?1:2)} km`;if(n>=10)return `${n.toFixed(1)} m`;if(n>=1)return `${n.toFixed(2)} m`;if(n>=0.01)return `${(n*100).toFixed(1)} cm`;return `${(n*1000).toFixed(1)} mm`;}
function formatSourceUnits(value){const n=Number(value)||0,a=Math.abs(n);if(a>=10000)return `${n.toFixed(0)} u`;if(a>=100)return `${n.toFixed(1)} u`;if(a>=1)return `${n.toFixed(3)} u`;return `${n.toPrecision(3)} u`;}
function formatRelativeLodError(value){const n=Number(value);if(!Number.isFinite(n)||n<0)return 'SSE —';if(n===0)return 'SSE 0';const pct=n*100;return `SSE ${pct>=1?pct.toFixed(2):pct>=.1?pct.toFixed(3):pct.toPrecision(3)}%`;}
function formatProjectedCharacteristic(value){const n=Number(value);if(!Number.isFinite(n)||n<=0)return '—';return `${n>=1000?(n/1000).toFixed(2)+'k':n>=100?n.toFixed(0):n.toFixed(1)} px obj`;}

export {diagnosticValue, escapeMarkup, formatBytes, formatProjectedCharacteristic, formatRelativeLodError, formatSourceUnits};
