#!/usr/bin/env python3
"""Dump all Ghidra-derived supplement files that the AIL extraction pipeline depends on.

Requires: Ghidra MCP connection (run via Claude Code with Ghidra open).

Outputs (bin_to_omf.py dependencies):
  workspace/ail_extract/raw/crt_data_symbols.json      — CRT data extern symbols
  workspace/ail_extract/raw/fd2common_rel32_sites.json  — fd2common E8/E9 rel32 sites
  workspace/ail_extract/audit/ghidra_pool_snapshot.json — fd2common fn pool metadata

Outputs (dump_ail_set.py dependencies):
  workspace/ail_extract/raw/ail_fn_metadata.json        — 428 AIL fn metadata
  workspace/ail_extract/raw/ail_data_items.json         — AIL data items (data_ail_* labels)

Outputs (split_to_objs.py / per-item mode):
  workspace/ail_extract/raw/ail_data_owners.json        — data item → owner fn map

Outputs (enumerate_pcrel32_sites.py):
  workspace/ail_extract/raw/pcrel32_sites_raw.json      — 1631 AIL rel32 instruction sites

Each snippet writes directly to its target file path.  Run via
mcp__ghidra__run_script_inline, or paste into Ghidra Script Manager.
Snippets that write to file use FileWriter with a <REPO> placeholder base;
replace <REPO> with your repo root before running.
"""

# === Snippet 1: crt_data_symbols.json ===
# Lists all Ghidra labels starting with "data_crt_", sorted by address.
JAVA_CRT_DATA_SYMBOLS = r"""
import ghidra.program.model.symbol.SymbolType;
import java.util.ArrayList;
import java.util.Collections;

var st = currentProgram.getSymbolTable();
var it = st.getAllSymbols(true);
ArrayList list = new ArrayList();

while (it.hasNext()) {
    var sym = it.next();
    if (sym.getSymbolType() != SymbolType.LABEL) continue;
    String name = sym.getName();
    if (!name.startsWith("data_crt_")) continue;
    String addr = String.format("%08x", sym.getAddress().getOffset());
    list.add(addr + "|" + name);
}
Collections.sort(list);

StringBuilder sb = new StringBuilder();
sb.append("[\n");
for (int i = 0; i < list.size(); i++) {
    String[] parts = ((String)list.get(i)).split("\\|", 2);
    if (i > 0) sb.append(",\n");
    sb.append("{\"addr\":\"" + parts[0] + "\",\"name\":\"" + parts[1] + "\"}");
}
sb.append("\n]");
println(sb.toString());
"""

# === Snippet 2: fd2common_rel32_sites.json ===
# Scans fd2common function bodies for E8/E9/0F8x rel32 instructions.
JAVA_FD2COMMON_REL32 = r"""
import ghidra.program.model.listing.Instruction;

String[] fd2names = {
    "fd2_dpmi_alloc_dos_memory", "fd2_dpmi_free_dos_memory",
    "fd2_dpmi_lock_region", "fd2_dpmi_unlock_region",
    "fd2_dpmi_lock_size", "fd2_dpmi_unlock_size",
    "crt_equivalent_get_eflags", "crt_equivalent_get_eflags_thunk"
};

var fm = currentProgram.getFunctionManager();
var sb = new StringBuilder();
sb.append("[\n");
boolean first = true;

for (String fname : fd2names) {
    var fit = fm.getFunctions(true);
    while (fit.hasNext()) {
        var fn = fit.next();
        if (!fn.getName().equals(fname)) continue;
        var body = fn.getBody();
        var listing = currentProgram.getListing();
        var instrIt = listing.getInstructions(body, true);
        long fnEntry = fn.getEntryPoint().getOffset();
        while (instrIt.hasNext()) {
            var inst = instrIt.next();
            byte[] bytes = inst.getBytes();
            int len = inst.getLength();
            long instAddr = inst.getAddress().getOffset();
            int opcode = bytes[0] & 0xFF;
            boolean isRel32 = false;
            int dispOff = 0;
            String opcodeStr = "";

            if ((opcode == 0xE8 || opcode == 0xE9) && len == 5) {
                isRel32 = true;
                dispOff = 1;
                opcodeStr = String.format("%02X", opcode);
            } else if (opcode == 0x0F && len == 6 && bytes.length >= 2) {
                int op2 = bytes[1] & 0xFF;
                if (op2 >= 0x80 && op2 <= 0x8F) {
                    isRel32 = true;
                    dispOff = 2;
                    opcodeStr = String.format("%02X%02X", opcode, op2);
                }
            }

            if (isRel32) {
                int disp32 = 0;
                for (int i = 0; i < 4; i++)
                    disp32 |= ((bytes[dispOff + i] & 0xFF) << (i * 8));
                long target = instAddr + len + disp32;
                if (!first) sb.append(",\n");
                first = false;
                sb.append(String.format(
                    "{\"inst_addr\":\"%08x\",\"opcode\":\"%s\",\"inst_size\":%d," +
                    "\"disp32_offset_in_inst\":%d,\"disp32\":%d," +
                    "\"target_addr\":\"%08x\",\"parent_fn\":\"%s\"," +
                    "\"parent_fn_entry\":\"%08x\"}",
                    instAddr, opcodeStr, len, dispOff, disp32,
                    target, fname, fnEntry));
            }
        }
        break;
    }
}
sb.append("\n]");
println(sb.toString());
"""

# (Snippet 3 removed: stack_pad.json replaced by Ghidra data item
#  data_ail_isr_private_stack_gap_8b @ 0x535f4)

# === Snippet 4: ghidra_pool_snapshot.json ===
# Dumps ALL functions with metadata (name, entry, cc, signature, body info).
# bin_to_omf.py filters this to the 8 fd2common functions at runtime.
JAVA_POOL_SNAPSHOT = r"""
import ghidra.program.model.listing.Function;
import java.util.ArrayList;
import java.util.Collections;

var fm = currentProgram.getFunctionManager();
var fit = fm.getFunctions(true);
ArrayList lines = new ArrayList();

while (fit.hasNext()) {
    Function fn = fit.next();
    if (fn.isExternal()) continue;
    String name = fn.getName();
    long entry = fn.getEntryPoint().getOffset();
    String cc = "__cdecl";
    if (fn.getCallingConventionName() != null &&
        fn.getCallingConventionName().contains("register")) {
        cc = "__watcall";
    }
    String sig = fn.getSignature().getPrototypeString(false);
    boolean isThunk = fn.isThunk();

    var body = fn.getBody();
    long bodyMin = body.getMinAddress().getOffset();
    long bodyMax = body.getMaxAddress().getOffset();
    long bodySize = 0;
    StringBuilder ranges = new StringBuilder();
    ranges.append("[");
    boolean firstRange = true;
    var rangeIt = body.iterator();
    while (rangeIt.hasNext()) {
        var range = rangeIt.next();
        long rMin = range.getMinAddress().getOffset();
        long rMax = range.getMaxAddress().getOffset();
        bodySize += (rMax - rMin + 1);
        if (!firstRange) ranges.append(",");
        firstRange = false;
        ranges.append(String.format("[\"%08x\",\"%08x\"]", rMin, rMax));
    }
    ranges.append("]");

    String line = String.format(
        "{\"name\":\"%s\",\"entry\":\"%08x\",\"cc\":\"%s\"," +
        "\"signature\":\"%s\",\"is_thunk\":%s,\"is_external\":false," +
        "\"body_min\":\"%08x\",\"body_max\":\"%08x\",\"body_size\":%d," +
        "\"body_ranges\":%s}",
        name, entry, cc,
        sig.replace("\\", "\\\\").replace("\"", "\\\""),
        isThunk ? "true" : "false",
        bodyMin, bodyMax, bodySize, ranges.toString());
    lines.add(String.format("%08x|%s", entry, line));
}

Collections.sort(lines);
StringBuilder sb = new StringBuilder();
sb.append("[\n");
for (int i = 0; i < lines.size(); i++) {
    if (i > 0) sb.append(",\n");
    String s = (String)lines.get(i);
    sb.append(s.substring(s.indexOf('|') + 1));
}
sb.append("\n]");
println(sb.toString());
"""

# === Snippet 5: ail_fn_metadata.json ===
# Dumps all 428 AIL_* functions with metadata for dump_ail_set.py.
JAVA_FN_METADATA = r"""
import ghidra.program.model.listing.Function;
import java.util.ArrayList;
import java.util.Collections;
import java.io.FileWriter;

var fm = currentProgram.getFunctionManager();
var fit = fm.getFunctions(true);
ArrayList lines = new ArrayList();
while (fit.hasNext()) {
    Function fn = fit.next();
    if (fn.isExternal()) continue;
    if (!fn.getName().startsWith("AIL_")) continue;
    long entry = fn.getEntryPoint().getOffset();
    String cc = fn.getCallingConventionName();
    if (cc == null || (!cc.equals("__watcall") && !cc.equals("__cdecl"))) cc = "__cdecl";
    String sig = fn.getSignature().getPrototypeString(false);
    sig = sig.replace("\\", "\\\\").replace("\"", "\\\"");
    boolean isThunk = fn.isThunk();
    var body = fn.getBody();
    long bodyMin = body.getMinAddress().getOffset();
    long bodyMax = body.getMaxAddress().getOffset();
    long bodySize = 0; int numRanges = 0;
    StringBuilder ranges = new StringBuilder(); ranges.append("[");
    var rangeIt = body.iterator();
    while (rangeIt.hasNext()) {
        var range = rangeIt.next();
        long rMin = range.getMinAddress().getOffset();
        long rMax = range.getMaxAddress().getOffset();
        bodySize += (rMax - rMin + 1);
        if (numRanges > 0) ranges.append(",");
        ranges.append(String.format("[\"%08x\",\"%08x\"]", rMin, rMax));
        numRanges++;
    }
    ranges.append("]");
    lines.add(String.format("%08x|{\"name\":\"%s\",\"entry\":\"%08x\",\"body_min\":\"%08x\","
        + "\"body_max\":\"%08x\",\"body_ranges\":%s,\"num_ranges\":%d,\"body_size\":%d,"
        + "\"signature\":\"%s\",\"cc\":\"%s\",\"is_thunk\":%s,\"is_external\":false}",
        entry, fn.getName(), entry, bodyMin, bodyMax, ranges.toString(),
        numRanges, bodySize, sig, cc, isThunk ? "true" : "false"));
}
Collections.sort(lines);
String outPath = "<REPO>/workspace/ail_extract/raw/ail_fn_metadata.json";
FileWriter fw = new FileWriter(outPath);
fw.write("[\n");
for (int i = 0; i < lines.size(); i++) {
    if (i > 0) fw.write(",\n");
    String s = (String)lines.get(i);
    fw.write(s.substring(s.indexOf('|') + 1));
}
fw.write("\n]\n"); fw.close();
println("Wrote " + lines.size() + " fn to " + outPath);
"""

# === Snippet 6: ail_data_items.json ===
# Dumps all data_ail_* labeled data items for dump_ail_set.py.
JAVA_DATA_ITEMS = r"""
import ghidra.program.model.symbol.SymbolType;
import ghidra.program.model.listing.Data;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.io.FileWriter;

var st = currentProgram.getSymbolTable();
var listing = currentProgram.getListing();
ArrayList lines = new ArrayList(); HashSet seen = new HashSet();
var it = st.getAllSymbols(true);
while (it.hasNext()) {
    var sym = it.next();
    if (sym.getSymbolType() != SymbolType.LABEL) continue;
    String name = sym.getName();
    if (!name.startsWith("data_ail_")) continue;
    long addr = sym.getAddress().getOffset();
    String addrStr = String.format("%08x", addr);
    if (seen.contains(addrStr)) continue; seen.add(addrStr);
    String typeName = "undefined"; int size = 0;
    Data data = listing.getDataAt(sym.getAddress());
    if (data != null) { typeName = data.getDataType().getName(); size = data.getLength(); }
    lines.add(addrStr + "|{\"addr\":\"" + addrStr + "\",\"name\":\"" + name
        + "\",\"type\":\"" + typeName + "\",\"size\":" + size + ",\"source\":[\"name\"]}");
}
Collections.sort(lines);
String outPath = "<REPO>/workspace/ail_extract/raw/ail_data_items.json";
FileWriter fw = new FileWriter(outPath);
fw.write("[\n");
for (int i = 0; i < lines.size(); i++) {
    if (i > 0) fw.write(",\n");
    String s = (String)lines.get(i);
    fw.write(s.substring(s.indexOf('|') + 1));
}
fw.write("\n]\n"); fw.close();
println("Wrote " + lines.size() + " items to " + outPath);
"""

# === Snippet 7: pcrel32_sites_raw.json ===
# Scans all AIL function bodies for E8/E9/0F8x rel32 instructions.
JAVA_PCREL32_SITES_RAW = r"""
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import java.util.ArrayList;
import java.util.Collections;
import java.io.FileWriter;

var fm = currentProgram.getFunctionManager();
var listing = currentProgram.getListing();
var refMgr = currentProgram.getReferenceManager();
ArrayList lines = new ArrayList();
var fit = fm.getFunctions(true);
while (fit.hasNext()) {
    Function fn = fit.next();
    if (fn.isExternal()) continue;
    if (!fn.getName().startsWith("AIL_")) continue;
    long fnEntry = fn.getEntryPoint().getOffset();
    var instrIt = listing.getInstructions(fn.getBody(), true);
    while (instrIt.hasNext()) {
        Instruction inst = instrIt.next();
        byte[] bytes = inst.getBytes(); int len = inst.getLength();
        long instAddr = inst.getAddress().getOffset();
        int opcode = bytes[0] & 0xFF;
        boolean isRel32 = false; int dispOff = 0; String opcodeStr = "";
        if ((opcode == 0xE8 || opcode == 0xE9) && len == 5) {
            isRel32 = true; dispOff = 1; opcodeStr = String.format("%02X", opcode);
        } else if (opcode == 0x0F && len == 6 && bytes.length >= 2) {
            int op2 = bytes[1] & 0xFF;
            if (op2 >= 0x80 && op2 <= 0x8F) {
                isRel32 = true; dispOff = 2; opcodeStr = String.format("%02X%02X", opcode, op2);
            }
        }
        if (!isRel32) continue;
        int disp32 = 0;
        for (int i = 0; i < 4; i++) disp32 |= ((bytes[dispOff+i] & 0xFF) << (i*8));
        long target = instAddr + len + disp32;
        String targetViaRef = String.format("%08x", target);
        Reference[] refs = refMgr.getReferencesFrom(inst.getAddress());
        for (Reference ref : refs) {
            if (ref.getReferenceType().isFlow()) {
                targetViaRef = String.format("%08x", ref.getToAddress().getOffset()); break;
            }
        }
        lines.add(String.format("%08x|{\"inst_addr\": \"%08x\", \"opcode\": \"%s\", "
            + "\"inst_size\": %d, \"disp32_offset_in_inst\": %d, \"disp32\": %d, "
            + "\"target_addr\": \"%08x\", \"target_via_ref\": \"%s\", "
            + "\"parent_fn\": \"%s\", \"parent_fn_entry\": \"%08x\"}",
            instAddr, instAddr, opcodeStr, len, dispOff, disp32, target,
            targetViaRef, fn.getName(), fnEntry));
    }
}
Collections.sort(lines);
String outPath = "<REPO>/workspace/ail_extract/raw/pcrel32_sites_raw.json";
FileWriter fw = new FileWriter(outPath);
fw.write("[\n");
for (int i = 0; i < lines.size(); i++) {
    if (i > 0) fw.write(",\n");
    String s = (String)lines.get(i);
    fw.write(s.substring(s.indexOf('|') + 1));
}
fw.write("\n]\n"); fw.close();
println("Wrote " + lines.size() + " sites to " + outPath);
"""

# === Snippet 8: ail_data_owners.json ===
# Per-item mode only. Maps data items to owner AIL functions via xrefs.
JAVA_DATA_OWNERS = r"""
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SymbolType;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Collections;
import java.io.FileWriter;

var fm = currentProgram.getFunctionManager();
var refMgr = currentProgram.getReferenceManager();
var st = currentProgram.getSymbolTable();
HashMap dataLabels = new HashMap();
var symIt = st.getAllSymbols(true);
while (symIt.hasNext()) {
    var sym = symIt.next();
    if (sym.getSymbolType() != SymbolType.LABEL) continue;
    String name = sym.getName();
    if (!name.startsWith("data_ail_") && !name.startsWith("L_AIL_") && !name.startsWith("L_data_ail_"))
        continue;
    String addrStr = String.format("%08x", sym.getAddress().getOffset());
    if (!dataLabels.containsKey(addrStr)) dataLabels.put(addrStr, name);
}
ArrayList resultKeys = new ArrayList();
HashMap resultJson = new HashMap();
for (var addrStr : dataLabels.keySet()) {
    String dataName = (String)dataLabels.get(addrStr);
    long addr = Long.parseLong((String)addrStr, 16);
    ArrayList owners = new ArrayList();
    var refs = refMgr.getReferencesTo(toAddr(addr));
    while (refs.hasNext()) {
        Reference ref = refs.next();
        Function fn = fm.getFunctionContaining(ref.getFromAddress());
        if (fn != null && fn.getName().startsWith("AIL_")) {
            String fnName = fn.getName();
            if (!owners.contains(fnName)) owners.add(fnName);
        }
    }
    if (!owners.isEmpty()) {
        Collections.sort(owners);
        StringBuilder sb = new StringBuilder();
        sb.append("{\"name\": \"" + dataName + "\", \"owners\": [");
        for (int i = 0; i < owners.size(); i++) {
            if (i > 0) sb.append(", ");
            sb.append("\"" + owners.get(i) + "\"");
        }
        sb.append("]}");
        resultKeys.add(addrStr);
        resultJson.put(addrStr, sb.toString());
    }
}
Collections.sort(resultKeys);
String outPath = "<REPO>/workspace/ail_extract/raw/ail_data_owners.json";
FileWriter fw = new FileWriter(outPath);
fw.write("{\n");
for (int i = 0; i < resultKeys.size(); i++) {
    if (i > 0) fw.write(",\n");
    String k = (String)resultKeys.get(i);
    fw.write("\"" + k + "\": " + resultJson.get(k));
}
fw.write("\n}\n"); fw.close();
println("Wrote " + resultKeys.size() + " items to " + outPath);
"""
