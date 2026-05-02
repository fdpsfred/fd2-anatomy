// Phase 7 — adjust each function's parameter COUNT to match its caller-derived expected count.
//
// Pipeline phase: this runs once after Phase 6 (`ghidra_param_cleanup.java`).
// `param_count_classify.py` (this directory) reads audit.json + recommendations.json
// and writes param_count_input.tsv based on caller signals (ADD ESP / RET N /
// EAX-EDX-ECX setting). This Java script consumes that TSV inside a single Ghidra
// transaction. See `_index.md` for the full pipeline.
//
// Reads:  workspace/calling_convention_audit/param_count_input.tsv
//   columns: addr<TAB>expected_count<TAB>confidence
// Writes: workspace/calling_convention_audit/param_count_result.tsv
//   columns: addr<TAB>status<TAB>old_count<TAB>new_count<TAB>message
//   status: ok | skipped | error
//
// On reduction: removeParameter() from the end. On expansion: addParameter()
// with name "param_<idx+1>" and `unsigned int` (4-byte) type. Existing parameter
// names and types of slots [0, min(old, new)) are preserved (the prior Phase 6
// cleanup already gave them neutral names).

import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.*;
import java.nio.file.*;
import java.util.*;

// ---- CONFIG: edit these paths if you relocate the workdir ------------------
final String IN_PATH  = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\calling_convention_audit\\param_count_input.tsv";
final String OUT_PATH = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\calling_convention_audit\\param_count_result.tsv";
// ----------------------------------------------------------------------------

FunctionManager fm = currentProgram.getFunctionManager();
AddressFactory af = currentProgram.getAddressFactory();
DataTypeManager dtm = currentProgram.getDataTypeManager();

DataType paramType = dtm.getDataType("/uint");
if (paramType == null) paramType = dtm.getDataType("/unsigned int");
if (paramType == null) paramType = UnsignedIntegerDataType.dataType;

List<String> input;
try { input = Files.readAllLines(Paths.get(IN_PATH)); }
catch (Exception e) { println("FATAL: " + e.getMessage()); return; }

List<String> resultLines = new ArrayList<>();
int okCount=0, errCount=0, skipCount=0;
int adds=0, removes=0;

for (String raw : input) {
  if (raw == null || raw.trim().isEmpty()) continue;
  String[] parts = raw.split("\t", -1);
  if (parts.length < 3) { resultLines.add(raw + "\terror\t\t\tmalformed input"); errCount++; continue; }
  String addrStr = parts[0];
  int expected;
  try { expected = Integer.parseInt(parts[1]); }
  catch (Exception e) { resultLines.add(addrStr + "\terror\t\t\tbad expected count"); errCount++; continue; }
  String confidence = parts[2];

  Address addr;
  try { addr = af.getAddress(addrStr); }
  catch (Exception e) { resultLines.add(addrStr + "\terror\t\t\tinvalid addr"); errCount++; continue; }
  Function f = fm.getFunctionAt(addr);
  if (f == null) { resultLines.add(addrStr + "\terror\t\t\tno function"); errCount++; continue; }

  int oldCount = f.getParameterCount();
  if (oldCount == expected) { resultLines.add(addrStr + "\tskipped\t" + oldCount + "\t" + expected + "\talready match"); skipCount++; continue; }

  try {
    if (oldCount > expected) {
      while (f.getParameterCount() > expected) {
        int idx = f.getParameterCount() - 1;
        f.removeParameter(idx);
        removes++;
      }
    } else {
      while (f.getParameterCount() < expected) {
        int idx = f.getParameterCount();
        String pname = "param_" + (idx + 1);
        ParameterImpl p = new ParameterImpl(pname, paramType, currentProgram);
        f.addParameter(p, SourceType.USER_DEFINED);
        adds++;
      }
    }
    int newCount = f.getParameterCount();
    if (newCount != expected) {
      resultLines.add(addrStr + "\terror\t" + oldCount + "\t" + newCount + "\tcount drifted (expected " + expected + ")");
      errCount++;
    } else {
      resultLines.add(addrStr + "\tok\t" + oldCount + "\t" + newCount + "\t" + confidence);
      okCount++;
    }
  } catch (Exception e) {
    int curCount = f.getParameterCount();
    resultLines.add(addrStr + "\terror\t" + oldCount + "\t" + curCount + "\t" + e.getClass().getSimpleName() + ": " + e.getMessage());
    errCount++;
  }
}

StringBuilder body = new StringBuilder();
for (String line : resultLines) body.append(line).append("\n");
Files.writeString(Paths.get(OUT_PATH), body.toString());

println("APPLY: ok=" + okCount + " skipped=" + skipCount + " err=" + errCount + " adds=" + adds + " removes=" + removes);
