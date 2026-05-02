// Phase 4 worker — apply ONE batch of cc + rename changes inside a single Ghidra transaction.
//
// Pipeline phase: this is step 4 of 5 in the calling-convention audit. The Python
// orchestrator (`apply_batch.py prepare`) writes batch_input.tsv before running this
// script via mcp__ghidra__run_script_inline; afterwards `apply_batch.py consume`
// merges the results into progress.json. See `_index.md` for the full pipeline.
//
// Reads:  workspace/calling_convention_audit/batch_input.tsv
//   columns: addr<TAB>target_cc<TAB>suggested_name<TAB>set_varargs<TAB>recs_idx
//   target_cc may be "-" meaning "no cc change, only rename / varargs"
//   suggested_name may be "" meaning no rename
// Writes: workspace/calling_convention_audit/batch_result.tsv
//   columns: addr<TAB>status<TAB>actions_csv<TAB>message
//   status in {ok, error, skipped}
//   actions_csv subset of {cc_changed, renamed, varargs_set}
//
// On cc change we drop custom variable storage so Ghidra recomputes parameter
// storage from the new convention; existing parameter NAMES at slots
// [0, min(old, new)) are preserved.

import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.data.*;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.util.*;

// ---- CONFIG: edit these paths if you relocate the workdir ------------------
final String IN_PATH  = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\calling_convention_audit\\batch_input.tsv";
final String OUT_PATH = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\calling_convention_audit\\batch_result.tsv";
// ----------------------------------------------------------------------------

FunctionManager fm = currentProgram.getFunctionManager();
AddressFactory af = currentProgram.getAddressFactory();

List<String> resultLines = new ArrayList<>();
int okCount=0, errCount=0, ccCount=0, renCount=0;

List<String> input;
try {
  input = Files.readAllLines(Paths.get(IN_PATH));
} catch (Exception e) {
  println("FATAL: cannot read " + IN_PATH + " : " + e.getMessage());
  return;
}

for (String raw : input) {
  if (raw == null || raw.trim().isEmpty()) continue;
  String[] parts = raw.split("\t", -1);
  if (parts.length < 4) {
    resultLines.add(raw + "\terror\t\tmalformed input row");
    errCount++;
    continue;
  }
  String addrStr = parts[0];
  String targetCc = parts[1];
  String suggestedName = parts[2];
  boolean setVarargs = "true".equalsIgnoreCase(parts[3]);

  Address addr;
  try {
    addr = af.getAddress(addrStr);
  } catch (Exception e) {
    resultLines.add(addrStr + "\terror\t\tinvalid address: " + e.getMessage());
    errCount++;
    continue;
  }
  Function f = fm.getFunctionAt(addr);
  if (f == null) {
    resultLines.add(addrStr + "\terror\t\tno function at address");
    errCount++;
    continue;
  }

  List<String> actions = new ArrayList<>();
  String message = "";
  try {
    if (!"-".equals(targetCc)) {
      String currentCc = f.getCallingConventionName();
      if (currentCc == null || !currentCc.equals(targetCc)) {
        if (f.hasCustomVariableStorage()) f.setCustomVariableStorage(false);
        f.setCallingConvention(targetCc);
        actions.add("cc_changed");
        ccCount++;
      }
    }
    if (setVarargs && !f.hasVarArgs()) {
      f.setVarArgs(true);
      actions.add("varargs_set");
    }
    if (suggestedName != null && !suggestedName.isEmpty() && !suggestedName.equals(f.getName())) {
      String oldName = f.getName();
      f.setName(suggestedName, SourceType.USER_DEFINED);
      actions.add("renamed");
      renCount++;
      message = oldName + " -> " + suggestedName;
    }
    String actionsCsv = String.join(",", actions);
    if (actions.isEmpty()) {
      resultLines.add(addrStr + "\tskipped\t\tno change needed");
    } else {
      resultLines.add(addrStr + "\tok\t" + actionsCsv + "\t" + message);
      okCount++;
    }
  } catch (Exception e) {
    String aCsv = String.join(",", actions);
    resultLines.add(addrStr + "\terror\t" + aCsv + "\t" + e.getClass().getSimpleName() + ": " + e.getMessage());
    errCount++;
  }
}

StringBuilder body = new StringBuilder();
for (String line : resultLines) body.append(line).append("\n");
Files.writeString(Paths.get(OUT_PATH), body.toString());

println("APPLY DONE: ok=" + okCount + " err=" + errCount + " cc_changes=" + ccCount + " renames=" + renCount + " rows=" + resultLines.size());
