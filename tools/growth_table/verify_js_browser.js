/* Exhaustive JS-vs-Python growth-table cross-check (browser).
 *
 * WHY browser: no Node on this machine, and the claude-in-chrome javascript_tool
 * runs in an ISOLATED world (cannot read the page's JS globals like COMPUTED), so
 * this is SELF-CONTAINED: it re-implements the page's compute functions VERBATIM,
 * reads the page's embedded compact JSON from the DOM (#growthData), and fetches the
 * Python generator's full output (growth_data.json, same origin) to diff every value.
 *
 * HOW TO RUN:
 *   1. Serve workspace/growth_table/ on the host and load fd2_growth_tables.html in the
 *      (Joe-DT) Chrome the extension is connected to.
 *   2. Paste this whole file into that tab's javascript_tool and execute.
 *   3. Expect: { ... mismatches: 0, verdict: "PASS-all-identical" }.
 *
 * NOTE: uses TOP-LEVEL await + a final object-literal expression (do NOT wrap in an
 * async IIFE -- the tool would return the Promise as "{}").
 */
const STATS=["hp","mp","ap","dp","dx"];
function baseRows(base, growth, join, cap){
  const rows=[];
  for(let L=join; L<=cap; L++){ const mn={},mx={};
    for(const s of STATS){ const g=growth[s], gmin=g[0], gmax=g[1], b=base[s];
      if(s==="hp"||s==="mp"){ const spawn=b+gmin*(join-1); mn[s]=b+gmin*(L-1); mx[s]=spawn+gmax*(L-join); }
      else{ const spawn=b+gmin*join; mn[s]=b+gmin*L; mx[s]=spawn+gmax*(L-join); } }
    rows.push({lv:L,min:mn,max:mx}); }
  return rows;
}
function promoRows(fmin, fmax, growth, cap){
  const rows=[];
  for(let N=1;N<=cap;N++){ const mn={},mx={};
    for(const s of STATS){ const g=growth[s]; mn[s]=fmin[s]+g[0]*N; mx[s]=fmax[s]+g[1]*N; }
    rows.push({lv:N,min:mn,max:mx}); }
  return rows;
}
function computeChar(c){
  const out=[]; const baseSeg=c.segments[0];
  const brows=baseRows(c.base, baseSeg.growth, c.join, c.cap);
  out.push({seg:baseSeg, rows:brows});
  const lv40=brows.find(r=>r.lv===40);
  for(let i=1;i<c.segments.length;i++){ const s=c.segments[i];
    out.push({seg:s, rows:promoRows(lv40.min, lv40.max, s.growth, s.to)}); }
  return out;
}
const compact = JSON.parse(document.getElementById('growthData').textContent);
const full = await (await fetch('growth_data.json',{cache:'no-store'})).json();
const fullById={}; full.characters.forEach(c=>fullById[c.char_id]=c);
let checks=0,mism=0; const examples=[]; let segTotal=0, rowTotal=0;
for(const c of compact.chars){
  const js=computeChar(c), py=fullById[c.id];
  if(js.length!==py.segments.length){ mism++; examples.push('SEGCOUNT id'+c.id); continue; }
  for(let si=0; si<js.length; si++){ segTotal++;
    const jr=js[si].rows, pr=py.segments[si].rows;
    if(jr.length!==pr.length){ mism++; examples.push('ROWCOUNT id'+c.id+' seg'+si); continue; }
    for(let ri=0; ri<jr.length; ri++){ rowTotal++;
      if(jr[ri].lv!==pr[ri].lv){ mism++; if(examples.length<20) examples.push('LV id'+c.id+' seg'+si); }
      for(const s of STATS){ for(const v of ['min','max']){ checks++;
        if(jr[ri][v][s]!==pr[ri][v][s]){ mism++;
          if(examples.length<20) examples.push('id'+c.id+' seg'+si+' lv'+jr[ri].lv+' '+s+'.'+v+' js='+jr[ri][v][s]+' py='+pr[ri][v][s]); } }}
    }
  }
}
({chars:compact.chars.length, segments:segTotal, levelRows:rowTotal, valuesChecked:checks,
  mismatches:mism, examples, verdict: mism===0 ? 'PASS-all-identical' : 'FAIL'})
