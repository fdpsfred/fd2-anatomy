// Phase F bulk fixup — clear mis-classified data + disassemble + create functions.
//
// Reads workspace/function_review/phase_f_candidates.json and processes each
// candidate inside its own transaction. On per-candidate Bad Instruction
// bookmark violation, the transaction is rolled back and any leftover
// bookmark is manually removed.
//
// Output: workspace/function_review/phase_f_log.json
//
// Run via Ghidra's `run_script_inline` MCP tool (paste the file content).

import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.symbol.SourceType;
import java.util.*;
import java.io.*;

class Util {
    String escapeJsonStr(String s) {
        StringBuilder out = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '"') out.append("\\\"");
            else if (c == '\\') out.append("\\\\");
            else if (c == '\n') out.append("\\n");
            else if (c == '\r') out.append("\\r");
            else if (c == '\t') out.append("\\t");
            else if (c < 0x20) out.append(String.format("\\u%04x", (int) c));
            else out.append(c);
        }
        return out.toString();
    }

    void emitLogEntry(StringBuilder buf, Map<String,Object> result, boolean isLast) {
        buf.append("  {");
        boolean first = true;
        for (Map.Entry<String,Object> e : result.entrySet()) {
            if (!first) buf.append(", ");
            first = false;
            buf.append("\"").append(e.getKey()).append("\": ");
            Object v = e.getValue();
            if (v instanceof String) {
                buf.append("\"").append(escapeJsonStr((String) v)).append("\"");
            } else if (v instanceof Number) {
                buf.append(v.toString());
            } else if (v instanceof List) {
                buf.append("[");
                List l = (List) v;
                for (int j=0; j<l.size(); j++) {
                    if (j>0) buf.append(",");
                    Object itm = l.get(j);
                    if (itm instanceof String) buf.append("\"").append(escapeJsonStr((String) itm)).append("\"");
                    else if (itm instanceof Map) {
                        buf.append("{");
                        Map m = (Map) itm;
                        boolean f2 = true;
                        for (Object key : m.keySet()) {
                            if (!f2) buf.append(",");
                            f2 = false;
                            buf.append("\"").append(key).append("\":");
                            Object mv = m.get(key);
                            if (mv instanceof String) buf.append("\"").append(escapeJsonStr((String) mv)).append("\"");
                            else buf.append(mv.toString());
                        }
                        buf.append("}");
                    }
                }
                buf.append("]");
            } else {
                buf.append("\"").append(escapeJsonStr(v.toString())).append("\"");
            }
        }
        buf.append("}").append(isLast ? "" : ",").append("\n");
    }

    boolean looksLikePrologue(byte[] b) {
        if (b.length < 2) return false;
        int b0 = b[0] & 0xff;
        int b1 = b.length > 1 ? (b[1] & 0xff) : -1;
        int b2 = b.length > 2 ? (b[2] & 0xff) : -1;
        int b3 = b.length > 3 ? (b[3] & 0xff) : -1;
        int b4 = b.length > 4 ? (b[4] & 0xff) : -1;
        if (b0 == 0x56 && b1 == 0x57 && b2 == 0x55) return true;
        if (b0 == 0x57 && b1 == 0x55) return true;
        if (b0 == 0x53 && b1 == 0x55 && b2 == 0x89 && b3 == 0xe5) return true;
        if (b0 == 0x55 && b1 == 0x89 && b2 == 0xe5) return true;
        if (b0 == 0x53 && b1 == 0x56 && b2 == 0x55 && b3 == 0x89 && b4 == 0xe5) return true;
        if (b0 == 0x57 && b1 == 0x56 && b2 == 0x52 && b3 == 0x51 && b4 == 0x53) return true;
        if (b0 == 0x56 && b1 == 0x57 && b2 == 0x52 && b3 == 0x53) return true;
        if (b0 == 0x8d && b1 == 0x80 && b2 == 0x00 && b3 == 0x00 && b4 == 0x00) return true;
        if (b0 == 0x8d && b1 == 0x40 && b2 == 0x00) return true;
        if (b0 == 0x8d && b1 == 0x44 && b2 == 0x20 && b3 == 0x00) return true;
        if (b0 == 0x8b && b1 == 0x44 && b2 == 0x24) return true;
        if (b0 == 0x8b && b1 == 0x06) return true;
        if (b0 == 0x8b && b1 == 0x42) return true;
        if (b0 == 0x8b && b1 == 0xc0) return true;
        if (b0 == 0x68) return true;
        if (b0 == 0x6a && b1 == 0x01) return true;
        if (b0 == 0xc7 && b1 == 0x05) return true;
        if (b0 == 0x3b && b1 == 0x35) return true;
        if (b0 == 0x80 && b1 == 0x3d) return true;
        if (b0 == 0x1e && b1 == 0x06) return true;
        if (b0 == 0x60) return true;
        if (b0 == 0xb0 && b1 == 0x03 && b2 == 0x55) return true;
        if (b0 == 0x53 && b1 == 0x83) return true;
        if (b0 == 0xe8) return true;  // CALL imm32 — many helpers start with a leading CALL
        return false;
    }
}

Util util = new Util();
Listing listing = currentProgram.getListing();
Memory mem = currentProgram.getMemory();
FunctionManager fm = currentProgram.getFunctionManager();
BookmarkManager bm = currentProgram.getBookmarkManager();
AddressFactory af = currentProgram.getAddressFactory();
AddressSpace defSpace = af.getDefaultAddressSpace();

// Read candidate JSON (line-by-line parse — simple enough for our schema)
String candPath = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\function_review\\phase_f_candidates.json";
StringBuilder jsonText = new StringBuilder();
BufferedReader br = new BufferedReader(new FileReader(candPath));
String line;
while ((line = br.readLine()) != null) jsonText.append(line).append("\n");
br.close();

// Parse — extract objects between { and }
List<Map<String,String>> candidates = new ArrayList<>();
int i = 0;
String s = jsonText.toString();
while (i < s.length()) {
    int objStart = s.indexOf('{', i);
    if (objStart < 0) break;
    int objEnd = s.indexOf('}', objStart);
    if (objEnd < 0) break;
    String obj = s.substring(objStart + 1, objEnd);
    Map<String,String> c = new HashMap<>();
    for (String kv : obj.split(",(?=\"[a-z_]+\":)")) {
        kv = kv.trim();
        int colon = kv.indexOf(':');
        if (colon < 0) continue;
        String key = kv.substring(0, colon).trim();
        String val = kv.substring(colon + 1).trim();
        if (key.startsWith("\"") && key.endsWith("\"")) key = key.substring(1, key.length() - 1);
        if (val.startsWith("\"") && val.endsWith("\"")) val = val.substring(1, val.length() - 1);
        c.put(key, val);
    }
    if (c.containsKey("addr")) candidates.add(c);
    i = objEnd + 1;
}

println("Loaded " + candidates.size() + " candidates");

// Process each candidate
StringBuilder logJson = new StringBuilder();
logJson.append("[\n");
int totalCreated = 0;
int totalFailed = 0;
int totalRolledback = 0;
int totalSkipped = 0;
int candIdx = 0;
for (Map<String,String> c : candidates) {
    candIdx++;
    long addr = Long.parseLong(c.get("addr"), 16);
    long len = Long.parseLong(c.get("len"));
    String kind = c.get("kind");
    String pool = c.get("pool");

    Map<String,Object> result = new LinkedHashMap<>();
    result.put("addr", c.get("addr"));
    result.put("len", len);
    result.put("kind", kind);
    result.put("pool", pool);

    // Skip raw_undefined small fragments — they are alignment leftover
    if (kind.equals("raw_undefined") && len <= 2) {
        result.put("status", "skipped_raw_undef");
        result.put("created", new ArrayList<>());
        util.emitLogEntry(logJson, result, candIdx == candidates.size());
        totalSkipped++;
        continue;
    }

    Address candStart = defSpace.getAddress(addr);
    Address candEnd = defSpace.getAddress(addr + len - 1);
    AddressSet candRange = new AddressSet(candStart, candEnd);

    // Baseline bookmarks
    Set<Long> baselineBkmks = new HashSet<>();
    Iterator<Bookmark> baseIter = bm.getBookmarksIterator("Error");
    while (baseIter.hasNext()) {
        Bookmark bk = baseIter.next();
        if (bk.getCategory().equals("Bad Instruction") &&
            candRange.contains(bk.getAddress())) {
            baselineBkmks.add(bk.getAddress().getOffset());
        }
    }

    int txId = currentProgram.startTransaction("fixup_phase_f " + c.get("addr"));
    boolean commit = true;
    List<Map<String,Object>> created_in_tx = new ArrayList<>();
    List<String> failed_in_tx = new ArrayList<>();
    List<String> bookmark_violations = new ArrayList<>();

    try {
        // Step 1: clear data + disassemble (only for byte_array_*)
        if (kind.startsWith("byte_array_")) {
            listing.clearCodeUnits(candStart, candEnd, false);
            DisassembleCommand dis = new DisassembleCommand(candRange, null, true);
            dis.applyTo(currentProgram);
        }

        // Step 2: iterate over instructions, try createFunction at each potential entry
        // Strategy: at candStart, try create. If success, jump past body, else advance 1 instr.
        Address cursor = candStart;
        int attempts = 0;
        while (cursor != null && cursor.compareTo(candEnd) <= 0 && attempts < 200) {
            attempts++;
            // Verify cursor has an instruction
            Instruction inst = listing.getInstructionAt(cursor);
            if (inst == null) {
                // advance to next instruction
                Instruction next = listing.getInstructionAfter(cursor);
                if (next == null || next.getMinAddress().compareTo(candEnd) > 0) break;
                cursor = next.getMinAddress();
                continue;
            }

            // Skip if cursor is already inside an existing function (e.g. a prior createFunction
            // already covered this range)
            Function existing = fm.getFunctionContaining(cursor);
            if (existing != null) {
                Address bodyEnd = existing.getBody().getMaxAddress();
                Instruction nextInst = listing.getInstructionAfter(bodyEnd);
                if (nextInst == null) break;
                cursor = nextInst.getMinAddress();
                continue;
            }

            String hex = String.format("%x", cursor.getOffset());
            String tentName = "vendor_" + pool + "_" + hex;

            try {
                CreateFunctionCmd cmd = new CreateFunctionCmd(tentName, cursor, null,
                    SourceType.USER_DEFINED);
                boolean ok = cmd.applyTo(currentProgram);
                if (ok) {
                    Function f = fm.getFunctionAt(cursor);
                    if (f != null) {
                        // plate comment
                        String pc = "AUTO-CREATED Phase F.\n"
                            + "Source: " + kind + " @ 0x" + c.get("addr")
                            + ", len=" + len + "\n"
                            + "Pool: " + pool + "\n"
                            + "Pre-rename placeholder — Phase G renames + updates this comment.";
                        f.setComment(pc);

                        Map<String,Object> entry = new LinkedHashMap<>();
                        entry.put("addr", String.format("%08x", cursor.getOffset()));
                        entry.put("name", tentName);
                        entry.put("body_size", f.getBody().getNumAddresses());
                        created_in_tx.add(entry);
                        totalCreated++;

                        // Advance past body
                        Address bodyEnd = f.getBody().getMaxAddress();
                        Instruction nextInst = listing.getInstructionAfter(bodyEnd);
                        if (nextInst == null) break;
                        cursor = nextInst.getMinAddress();
                        continue;
                    } else {
                        failed_in_tx.add("create_returned_null:" + hex);
                    }
                } else {
                    failed_in_tx.add("create_cmd_failed:" + hex);
                }
            } catch (Exception ex) {
                failed_in_tx.add(hex + ":" + ex.getMessage());
            }

            // createFunction failed at cursor — advance 1 instruction
            Instruction nextInst = listing.getInstructionAfter(cursor);
            if (nextInst == null || nextInst.getMinAddress().compareTo(candEnd) > 0) break;
            cursor = nextInst.getMinAddress();
        }

        // Step 3: bookmark check
        Iterator<Bookmark> postIter = bm.getBookmarksIterator("Error");
        while (postIter.hasNext()) {
            Bookmark bk = postIter.next();
            if (bk.getCategory().equals("Bad Instruction") &&
                candRange.contains(bk.getAddress())) {
                long off = bk.getAddress().getOffset();
                if (!baselineBkmks.contains(off)) {
                    bookmark_violations.add(String.format("%08x", off));
                }
            }
        }
        if (!bookmark_violations.isEmpty()) {
            commit = false;
        }
    } finally {
        currentProgram.endTransaction(txId, commit);
    }

    // Manual cleanup of any leftover bookmarks after rollback
    if (!commit) {
        for (String bkAddrStr : bookmark_violations) {
            long bkAddr = Long.parseLong(bkAddrStr, 16);
            Address bkAddrObj = defSpace.getAddress(bkAddr);
            Bookmark[] bks = bm.getBookmarks(bkAddrObj);
            if (bks != null) {
                for (Bookmark bk : bks) {
                    if (bk.getTypeString().equals("Error") && bk.getCategory().equals("Bad Instruction")) {
                        bm.removeBookmark(bk);
                    }
                }
            }
        }
        totalRolledback++;
        result.put("status", "rolled_back");
    } else if (!failed_in_tx.isEmpty()) {
        totalFailed++;
        result.put("status", "ok_with_partial_failures");
    } else {
        result.put("status", "ok");
    }

    result.put("created", created_in_tx);
    result.put("failed", failed_in_tx);
    result.put("bookmark_violations", bookmark_violations);

    util.emitLogEntry(logJson, result, candIdx == candidates.size());

    if (candIdx % 20 == 0) {
        println("[" + candIdx + "/" + candidates.size() + "]  created=" + totalCreated
            + " failed=" + totalFailed + " rb=" + totalRolledback + " skip=" + totalSkipped);
    }
}

logJson.append("]\n");
String logPath = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\function_review\\phase_f_log.json";
FileWriter fw = new FileWriter(logPath);
fw.write(logJson.toString());
fw.close();

println("\n=== DONE ===");
println("Total candidates: " + candidates.size());
println("Functions created: " + totalCreated);
println("Candidates with partial failures: " + totalFailed);
println("Candidates rolled back (bookmark violation): " + totalRolledback);
println("Candidates skipped: " + totalSkipped);
println("Log: " + logPath);
