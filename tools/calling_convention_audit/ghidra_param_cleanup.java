// Phase 6 — clean up Ghidra parameter-name artifacts left over from old register-storage analysis.
//
// Pipeline phase: this runs once after Phase 5 (cc verify) succeeds. When Ghidra's
// auto-analysis flips a function from `__fastcall` to `__cdecl` (Phase 4), it
// recomputes parameter STORAGE (stack-based) but keeps the old NAMES like
// `arg_eax_in` / `arg_edx_in` / `arg_ecx_in` from when those parameters lived in
// EAX/EDX/ECX. Those names are now misleading and would propagate into any
// decompiled-source export. This script renames any such artifact parameter to
// `param_<ordinal+1>` — Ghidra's neutral default convention.
//
// Run via mcp__ghidra__run_script_inline with this file's content as `code`.
// No script arguments. Idempotent: running again produces 0 renames once clean.
//
// Patterns matched (case-insensitive on parameter name):
//     ^arg_(e?ax|e?dx|e?cx|e?bx|e?si|e?di|e?bp)_in(_\d+)?$
//     ^in_(EAX|EDX|ECX|EBX|ESI|EDI|EBP)(_\d+)?$
//
// Writes log to workspace/calling_convention_audit/param_rename_log.json
// (one record per rename: addr, fn, ord, old, new).

import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;
import java.util.regex.*;
import java.nio.file.*;

// ---- CONFIG: edit if you relocate the workdir ------------------------------
final String LOG_PATH = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\calling_convention_audit\\param_rename_log.json";
// ----------------------------------------------------------------------------

FunctionManager fm = currentProgram.getFunctionManager();
Pattern artifact = Pattern.compile("^arg_(e?ax|e?dx|e?cx|e?bx|e?si|e?di|e?bp)_in(_\\d+)?$", Pattern.CASE_INSENSITIVE);
Pattern altIn = Pattern.compile("^in_(EAX|EDX|ECX|EBX|ESI|EDI|EBP)(_\\d+)?$");

int renamed = 0;
int errors = 0;
StringBuilder log = new StringBuilder();
log.append("[\n");
boolean first = true;

FunctionIterator it = fm.getFunctions(true);
while (it.hasNext()) {
  Function f = it.next();
  Parameter[] params = f.getParameters();
  for (Parameter p : params) {
    String name = p.getName();
    if (name == null) continue;
    boolean dirty = artifact.matcher(name).matches() || altIn.matcher(name).matches();
    if (!dirty) continue;
    int ord = p.getOrdinal();
    String target = "param_" + (ord + 1);
    try {
      p.setName(target, SourceType.USER_DEFINED);
      renamed++;
      if (!first) log.append(",\n");
      first = false;
      log.append("  {\"addr\":\"0x").append(f.getEntryPoint().toString())
         .append("\",\"fn\":\"").append(f.getName().replace("\\","\\\\").replace("\"","\\\""))
         .append("\",\"ord\":").append(ord)
         .append(",\"old\":\"").append(name)
         .append("\",\"new\":\"").append(target).append("\"}");
    } catch (Exception e) {
      errors++;
      if (!first) log.append(",\n");
      first = false;
      log.append("  {\"addr\":\"0x").append(f.getEntryPoint().toString())
         .append("\",\"fn\":\"").append(f.getName().replace("\\","\\\\").replace("\"","\\\""))
         .append("\",\"ord\":").append(ord)
         .append(",\"old\":\"").append(name)
         .append("\",\"new\":\"").append(target)
         .append("\",\"error\":\"").append(e.getClass().getSimpleName()).append(": ").append(e.getMessage().replace("\\","\\\\").replace("\"","\\\"")).append("\"}");
    }
  }
}
log.append("\n]\n");
Files.writeString(Paths.get(LOG_PATH), log.toString());
println("Renamed: " + renamed + " params, errors: " + errors);
