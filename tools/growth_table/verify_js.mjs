// Cross-check: the web page's JS compute (copied verbatim) must reproduce the
// Python generator's precomputed growth_data.json for every char/segment/level.
import fs from "fs";
const ROOT = new URL("../../", import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1");
const compact = JSON.parse(fs.readFileSync(ROOT + "workspace/growth_table/growth_compact.json", "utf8"));
const full = JSON.parse(fs.readFileSync(ROOT + "workspace/growth_table/growth_data.json", "utf8"));
const STATS = compact.stats;

/* ---- functions copied verbatim from page_template.html ---- */
function baseRows(base, growth, join, cap){
  const rows=[];
  for(let L=join; L<=cap; L++){
    const mn={},mx={};
    for(const s of STATS){
      const g=growth[s], gmin=g[0], gmax=g[1], b=base[s];
      if(s==="hp"||s==="mp"){
        const spawn=b+gmin*(join-1);
        mn[s]=b+gmin*(L-1); mx[s]=spawn+gmax*(L-join);
      }else{
        const spawn=b+gmin*join;
        mn[s]=b+gmin*L; mx[s]=spawn+gmax*(L-join);
      }
    }
    rows.push({lv:L,min:mn,max:mx});
  }
  return rows;
}
function promoRows(fmin, fmax, growth, cap){
  const rows=[];
  for(let N=1;N<=cap;N++){
    const mn={},mx={};
    for(const s of STATS){
      const g=growth[s];
      mn[s]=fmin[s]+g[0]*N; mx[s]=fmax[s]+g[1]*N;
    }
    rows.push({lv:N,min:mn,max:mx});
  }
  return rows;
}
function computeChar(c){
  const out=[];
  const baseSeg=c.segments[0];
  const brows=baseRows(c.base, baseSeg.growth, c.join, c.cap);
  out.push({seg:baseSeg, rows:brows});
  const lv40=brows.find(r=>r.lv===40);
  for(let i=1;i<c.segments.length;i++){
    const s=c.segments[i];
    const prows=promoRows(lv40.min, lv40.max, s.growth, s.to);
    out.push({seg:s, rows:prows});
  }
  return out;
}

/* ---- compare ---- */
let checks=0, mism=0;
const fullById={}; full.characters.forEach(c=>fullById[c.char_id]=c);
for(const c of compact.chars){
  const js=computeChar(c);
  const py=fullById[c.id];
  if(js.length!==py.segments.length){ console.log("SEG COUNT MISMATCH", c.id, js.length, py.segments.length); mism++; continue; }
  for(let si=0; si<js.length; si++){
    const jr=js[si].rows, pr=py.segments[si].rows;
    if(jr.length!==pr.length){ console.log("ROW COUNT MISMATCH", c.id, si, jr.length, pr.length); mism++; continue; }
    for(let ri=0; ri<jr.length; ri++){
      if(jr[ri].lv!==pr[ri].lv){ console.log("LV MISMATCH", c.id, si, ri); mism++; }
      for(const s of STATS){
        for(const v of ["min","max"]){
          checks++;
          if(jr[ri][v][s]!==pr[ri][v][s]){
            mism++;
            if(mism<=20) console.log(`MISMATCH id=${c.id} "${c.name}" seg=${si} lv=${jr[ri].lv} ${s}.${v} js=${jr[ri][v][s]} py=${pr[ri][v][s]}`);
          }
        }
      }
    }
  }
}
console.log(`\nchecked ${checks} values across ${compact.chars.length} chars; mismatches=${mism}`);
console.log(mism===0 ? "PASS: web JS == Python generator" : "FAIL");
process.exit(mism===0?0:1);
