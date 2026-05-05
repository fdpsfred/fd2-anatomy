// Phase G bulk rename — apply mechanical naming for clear vendor_* categories.
//
// Reads workspace/function_review/phase_g_rename_plan.json and:
//   1. align_nop:         rename vendor_* → align_nop_<addr> + plate comment
//   2. int_stub:          rename → crt_dpmi_int_<NN> (NN derived from addr)
//   3. int3_stub:         rename → crt_dpmi_int_03
//   4. dispatcher_inner:  rename → crt_dpmi_int_invoke_inner
//   5. dispatcher_outer:  rename → crt_dpmi_int_invoke
//
// Also handles structural_fix entries (named→vendor wrong-splits): delete vendor_*,
// extend prev function's body to absorb the freed bytes.
//
// Invariant after run:
//   - all renamed functions have plate comments without "AUTO-CREATED Phase F" marker
//   - bookmark check: 0 new Bad Instruction
//   - Output log: workspace/function_review/phase_g_bulk_log.json

import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.SourceType;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.cmd.function.DeleteFunctionCmd;
import java.util.*;
import java.io.*;

class Helper {
    String escape(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '"') b.append("\\\"");
            else if (c == '\\') b.append("\\\\");
            else if (c == '\n') b.append("\\n");
            else if (c == '\r') b.append("\\r");
            else if (c < 0x20) b.append(String.format("\\u%04x", (int) c));
            else b.append(c);
        }
        return b.toString();
    }

    // Parse a flat address-like string from JSON (within the simple plan structure)
    String extractStr(String obj, String key) {
        String pat = "\"" + key + "\":\"";
        int p = obj.indexOf(pat);
        if (p < 0) return null;
        p += pat.length();
        int q = obj.indexOf("\"", p);
        if (q < 0) return null;
        return obj.substring(p, q);
    }
}

Helper H = new Helper();
Listing listing = currentProgram.getListing();
FunctionManager fm = currentProgram.getFunctionManager();
BookmarkManager bm = currentProgram.getBookmarkManager();
AddressFactory af = currentProgram.getAddressFactory();
AddressSpace defSpace = af.getDefaultAddressSpace();
SymbolTable symTab = currentProgram.getSymbolTable();

// Load plan JSON
String planPath = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\function_review\\phase_g_rename_plan.json";
StringBuilder jsonText = new StringBuilder();
BufferedReader br = new BufferedReader(new FileReader(planPath));
String line;
while ((line = br.readLine()) != null) jsonText.append(line).append("\n");
br.close();
String jt = jsonText.toString();

// Parse: for each top-level key (align_nop, int_stub, int3_stub, dispatcher_inner, dispatcher_outer),
// extract the corresponding [..] array and within it the {addr,name,...} objects.
// Simple state-machine parse — relies on plan JSON's stable shape.

class Parser {
    List<Map<String,String>> parseSection(String section, String src) {
        List<Map<String,String>> out = new ArrayList<>();
        String key = "\"" + section + "\":";
        int p = src.indexOf(key);
        if (p < 0) return out;
        p = src.indexOf("[", p);
        if (p < 0) return out;
        // Find matching ]
        int depth = 0;
        int end = p;
        for (int i = p; i < src.length(); i++) {
            char c = src.charAt(i);
            if (c == '[') depth++;
            else if (c == ']') { depth--; if (depth == 0) { end = i; break; } }
        }
        String body = src.substring(p+1, end);
        // Extract objects { ... }
        int i = 0;
        while (i < body.length()) {
            int objStart = body.indexOf('{', i);
            if (objStart < 0) break;
            // Find matching }, tracking nested {}
            int d = 0;
            int objEnd = objStart;
            boolean inStr = false;
            for (int j = objStart; j < body.length(); j++) {
                char c = body.charAt(j);
                if (c == '"' && (j == 0 || body.charAt(j-1) != '\\')) inStr = !inStr;
                if (inStr) continue;
                if (c == '{') d++;
                else if (c == '}') { d--; if (d == 0) { objEnd = j; break; } }
            }
            String obj = body.substring(objStart, objEnd+1);
            Map<String,String> m = new HashMap<>();
            String addr = H.extractStr(obj, "addr");
            String name = H.extractStr(obj, "name");
            if (addr != null) m.put("addr", addr);
            if (name != null) m.put("name", name);
            if (!m.isEmpty()) out.add(m);
            i = objEnd + 1;
        }
        return out;
    }
}

Parser P = new Parser();
List<Map<String,String>> alignNop = P.parseSection("align_nop", jt);
List<Map<String,String>> intStub  = P.parseSection("int_stub", jt);
List<Map<String,String>> int3Stub = P.parseSection("int3_stub", jt);
List<Map<String,String>> dispInner = P.parseSection("dispatcher_inner", jt);
List<Map<String,String>> dispOuter = P.parseSection("dispatcher_outer", jt);
List<Map<String,String>> structFix = P.parseSection("structural_fix", jt);

println("loaded: align_nop=" + alignNop.size() + " int_stub=" + intStub.size()
    + " int3_stub=" + int3Stub.size() + " disp_inner=" + dispInner.size()
    + " disp_outer=" + dispOuter.size() + " struct_fix=" + structFix.size());

// Setup result log
StringBuilder log = new StringBuilder();
log.append("[\n");
boolean firstLog = true;

class Renamer {
    int total = 0;
    int success = 0;
    int failed = 0;

    boolean rename(String addrStr, String newName, String plateComment) {
        total++;
        try {
            Address a = defSpace.getAddress(addrStr);
            Function f = fm.getFunctionAt(a);
            if (f == null) {
                logEntry(addrStr, "FAILED", "no function at addr", null, newName);
                failed++;
                return false;
            }
            String oldName = f.getName();
            f.setName(newName, SourceType.USER_DEFINED);
            f.setComment(plateComment);
            logEntry(addrStr, "RENAMED", null, oldName, newName);
            success++;
            return true;
        } catch (Exception e) {
            logEntry(addrStr, "FAILED", e.getMessage(), null, newName);
            failed++;
            return false;
        }
    }

    void logEntry(String addrStr, String status, String err, String oldName, String newName) {
        if (!firstLog) log.append(",\n");
        firstLog = false;
        log.append("  {\"addr\":\"" + addrStr + "\"");
        log.append(",\"status\":\"" + status + "\"");
        log.append(",\"new_name\":\"" + (newName != null ? newName : "") + "\"");
        if (oldName != null) log.append(",\"old_name\":\"" + oldName + "\"");
        if (err != null) log.append(",\"error\":\"" + H.escape(err) + "\"");
        log.append("}");
    }
}

Renamer R = new Renamer();

int txId = currentProgram.startTransaction("Phase G bulk rename");
boolean txOK = false;
try {
    // 1. align_nop
    for (Map<String,String> m : alignNop) {
        String addr = m.get("addr");
        String newName = "align_nop_" + addr.substring(addr.length()-8).replaceFirst("^0+","");
        if (newName.equals("align_nop_")) newName = "align_nop_0";
        String plate = "Compiler-emitted alignment NOP padding (Watcom). Not called.\n"
                     + "Auto-discovered by Phase F as orphan instruction; preserved as Function "
                     + "entity to satisfy 'every code byte belongs to a function' invariant.";
        R.rename(addr, newName, plate);
    }

    // 2. int_stub: addr 0x465f8 + N*3 → crt_dpmi_int_<NN> for N=0..255 (skip 0x03)
    for (Map<String,String> m : intStub) {
        String addr = m.get("addr");
        long a = Long.parseLong(addr, 16);
        long n = (a - 0x465f8L) / 3L;
        String nn = String.format("%02x", n);
        String newName = "crt_dpmi_int_" + nn;
        String plate = "DPMI INT vector dispatch table entry — INT 0x" + nn + " followed by RET.\n"
                     + "Reached via crt_dpmi_int_invoke_inner (computed jump) when DPMI client "
                     + "invokes a real-mode INT 0x" + nn + ".";
        R.rename(addr, newName, plate);
    }

    // 3. int3_stub
    for (Map<String,String> m : int3Stub) {
        String plate = "DPMI INT vector dispatch table entry — INT 0x03 (single-byte CC opcode) "
                     + "followed by RET.\nUses 1-byte CC encoding instead of 2-byte CD 03.";
        R.rename(m.get("addr"), "crt_dpmi_int_03", plate);
    }

    // 4. dispatcher_inner
    for (Map<String,String> m : dispInner) {
        String plate = "DPMI INT vector dispatcher trampoline.\n"
                     + "Computes target_addr = (intnum & 0xff) * 3 + 0x465f8 (the int-stub table), "
                     + "pushes target as fake return address, restores registers from regs struct, "
                     + "RETs into the indexed crt_dpmi_int_<NN> stub.";
        R.rename(m.get("addr"), "crt_dpmi_int_invoke_inner", plate);
    }

    // 5. dispatcher_outer
    for (Map<String,String> m : dispOuter) {
        String plate = "DPMI INT vector invocation wrapper (Watcom int386 equivalent).\n"
                     + "Calls crt_dpmi_int_invoke_inner to perform the INT, then captures EAX, "
                     + "EFLAGS, segment registers, and other GPRs back into the regs struct.";
        R.rename(m.get("addr"), "crt_dpmi_int_invoke", plate);
    }

    // 6. structural_fix: delete vendor_*, extend prev fn body
    for (Map<String,String> m : structFix) {
        String addr = m.get("addr");
        String name = m.get("name");
        try {
            Address a = defSpace.getAddress(addr);
            Function vf = fm.getFunctionAt(a);
            if (vf == null) {
                R.logEntry(addr, "FAILED", "no vendor function at addr", null, "(structural_fix)");
                R.failed++;
                continue;
            }
            // Find the previous function (immediately preceding instruction's containing fn)
            Instruction prev = listing.getInstructionBefore(a);
            if (prev == null) { R.logEntry(addr, "FAILED", "no prev inst", null, "(structural_fix)"); R.failed++; continue; }
            Function prevFn = fm.getFunctionContaining(prev.getAddress());
            if (prevFn == null) { R.logEntry(addr, "FAILED", "no prev fn", null, "(structural_fix)"); R.failed++; continue; }
            String prevName = prevFn.getName();
            Address prevEntry = prevFn.getEntryPoint();
            AddressSetView vfBody = vf.getBody();
            Address vfMax = vfBody.getMaxAddress();
            // Compute new body for prevFn = old body ∪ vfBody
            AddressSet newBody = new AddressSet(prevFn.getBody());
            newBody.add(vfBody);
            // Delete vendor function
            fm.removeFunction(a);
            // Set new body on prev function
            try {
                prevFn.setBody(newBody);
                R.logEntry(addr, "STRUCT_MERGED", null, name, prevName + " (extended body to 0x" + vfMax.toString() + ")");
                R.success++;
            } catch (Exception bodyErr) {
                R.logEntry(addr, "STRUCT_FAILED", "setBody: " + bodyErr.getMessage(), name, prevName);
                R.failed++;
            }
        } catch (Exception e) {
            R.logEntry(addr, "FAILED", e.getMessage(), name, "(structural_fix)");
            R.failed++;
        }
    }

    txOK = true;
} finally {
    currentProgram.endTransaction(txId, txOK);
}

log.append("\n]\n");

File outF = new File("C:/Users/fdpsf/Documents/fd2-anatomy/workspace/function_review/phase_g_bulk_log.json");
PrintWriter pw = new PrintWriter(outF);
pw.print(log.toString());
pw.close();

println("done. total=" + R.total + " success=" + R.success + " failed=" + R.failed);
println("log: " + outF.getAbsolutePath());

// Quick bookmark check
AddressSetView all = currentProgram.getMemory();
int badCount = 0;
Iterator<Bookmark> bIt = bm.getBookmarksIterator("Error");
while (bIt.hasNext()) {
    Bookmark bk = bIt.next();
    if ("Bad Instruction".equals(bk.getCategory())) badCount++;
}
println("Bad Instruction bookmarks after: " + badCount);
