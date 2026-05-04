// Phase 1 / 5 — dump per-function ABI evidence from Ghidra to audit.json.
//
// Pipeline phase: this is step 1 (and re-run as step 5 -> audit_after.json) of the
// calling-convention audit. See `_index.md` in this directory for the full
// pipeline overview and `program_info/calling_convention.md` for the ABI rules
// being inferred from this dump.
//
// Run via mcp__ghidra__run_script_inline with this file's content as `code`.
// No script arguments. Writes audit.json + prints summary line.
//
// Per function emits (single JSON document, list of objects):
//     addr, name, current_cc, is_thunk, thunked_addr, body_size,
//     param_count_current, last_insn_kind ('RET0' / 'RETN' / 'TAIL_JMP' /
//     'OTHER' / 'NONE'), last_insn_mnem, last_insn_n, tail_jmp_target,
//     prologue (first 16 instr text up to JMP/RET stop),
//     reads_eax/edx/ecx (read-before-write before frame setup, with CALL
//     treated as clobbering 3 caller-saved regs and self-zero XOR
//     not counted as a read; this script was authored under the prior
//     Borland-cc assumption — under Watcom watcall, EBX is also a reg
//     arg / caller-saved and should be tracked alongside EAX/EDX/ECX),
//     caller_count, callers_add_esp_seen, callers_add_esp_max,
//     callers_add_esp_min, callers_set_eax/edx/ecx, callers_push_only,
//     callers (per-caller detail array),
//     existing_signature
//
// To produce audit_after.json (Phase 5), edit OUT_PATH below to point at
// audit_after.json before running.

import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.scalar.*;
import ghidra.program.model.mem.*;
import java.nio.file.*;
import java.util.*;

// ---- CONFIG: edit these paths if you relocate the workdir ------------------
final String OUT_PATH = "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\calling_convention_audit\\audit.json";
// ----------------------------------------------------------------------------

final long CRT_FRAME_SETUP_ADDR = 0x36cd7L;
final int PROLOGUE_MAX = 16;
final int MAX_CALLERS_SAMPLE = 6;
final int CALLER_PRE_LOOKBACK = 6;

Listing listing = currentProgram.getListing();
FunctionManager fm = currentProgram.getFunctionManager();
ReferenceManager rm = currentProgram.getReferenceManager();

StringBuilder out = new StringBuilder();
out.append("{\n");
out.append("  \"program\": \"").append(currentProgram.getName().replace("\\","\\\\").replace("\"","\\\"")).append("\",\n");
out.append("  \"function_count\": ").append(fm.getFunctionCount()).append(",\n");
out.append("  \"crt_frame_setup_addr\": \"0x36cd7\",\n");
out.append("  \"functions\": [\n");

FunctionIterator it = fm.getFunctions(true);
int idx = 0;
int totalFuncs = 0;
while (it.hasNext()) {
  Function f = it.next();
  totalFuncs++;
  if (idx > 0) out.append(",\n");
  idx++;

  Address entry = f.getEntryPoint();
  String name = f.getName();
  String cc = f.getCallingConventionName();
  if (cc == null) cc = "<null>";
  boolean isThunk = f.isThunk();
  String thunkedAddr = "";
  if (isThunk) {
    Function tf = f.getThunkedFunction(true);
    if (tf != null) thunkedAddr = "0x" + tf.getEntryPoint().toString();
  }

  AddressSetView body = f.getBody();
  long bodySize = body.getNumAddresses();
  Address maxAddr = body.getMaxAddress();
  Instruction lastInsn = listing.getInstructionContaining(maxAddr);

  String lastKind = "NONE";
  long lastN = 0;
  String lastMnem = "";
  String tailJmpTarget = "";
  if (lastInsn != null) {
    lastMnem = lastInsn.getMnemonicString().toUpperCase();
    if (lastMnem.equals("RET") || lastMnem.equals("RETN") || lastMnem.equals("RETF")) {
      if (lastInsn.getNumOperands() == 0) lastKind = "RET0";
      else {
        lastKind = "RETN";
        Object[] ops = lastInsn.getOpObjects(0);
        if (ops != null && ops.length > 0 && ops[0] instanceof Scalar) lastN = ((Scalar) ops[0]).getUnsignedValue();
      }
    } else if (lastMnem.equals("JMP")) {
      lastKind = "TAIL_JMP";
      Address[] flows = lastInsn.getFlows();
      if (flows != null && flows.length == 1) {
        Function tgt = fm.getFunctionAt(flows[0]);
        if (tgt != null) tailJmpTarget = "0x" + flows[0].toString();
        else tailJmpTarget = "0x" + flows[0].toString() + ":not_function_entry";
      } else tailJmpTarget = "indirect";
    } else lastKind = "OTHER";
  }

  // Prologue scan: detect EAX/EDX/ECX read-before-write before frame setup
  Set<String> writtenRegs = new HashSet<>();
  Set<String> readBeforeWrite = new HashSet<>();
  StringBuilder prologueJson = new StringBuilder();
  prologueJson.append("[");
  Instruction insn = listing.getInstructionAt(entry);
  int steps = 0;
  boolean stopped = false;
  while (insn != null && steps < PROLOGUE_MAX && body.contains(insn.getAddress()) && !stopped) {
    String mnem = insn.getMnemonicString().toUpperCase();
    String operands = insn.toString();
    if (steps > 0) prologueJson.append(",");
    prologueJson.append("\"").append(operands.replace("\\","\\\\").replace("\"","\\\"")).append("\"");

    boolean isProbeCall = false;
    if (mnem.equals("CALL")) {
      Address[] flows = insn.getFlows();
      if (flows != null && flows.length == 1 && flows[0].getOffset() == CRT_FRAME_SETUP_ADDR) isProbeCall = true;
    }
    boolean isProbePush = false;
    if (mnem.equals("PUSH") && insn.getNumOperands() == 1) {
      Object[] ops = insn.getOpObjects(0);
      if (ops != null && ops.length == 1 && ops[0] instanceof Scalar) {
        Instruction nxt = insn.getNext();
        if (nxt != null && nxt.getMnemonicString().toUpperCase().equals("CALL")) {
          Address[] nflows = nxt.getFlows();
          if (nflows != null && nflows.length == 1 && nflows[0].getOffset() == CRT_FRAME_SETUP_ADDR) isProbePush = true;
        }
      }
    }

    if (!isProbeCall && !isProbePush) {
      Object[] inputs = insn.getInputObjects();
      Object[] results = insn.getResultObjects();
      boolean selfZero = false;
      if ((mnem.equals("XOR") || mnem.equals("SUB")) && insn.getNumOperands() == 2) {
        Object[] op0 = insn.getOpObjects(0);
        Object[] op1 = insn.getOpObjects(1);
        if (op0 != null && op1 != null && op0.length == 1 && op1.length == 1
            && op0[0] instanceof Register && op1[0] instanceof Register) {
          if (((Register) op0[0]).getName().equals(((Register) op1[0]).getName())) selfZero = true;
        }
      }
      if (inputs != null && !selfZero) {
        for (Object o : inputs) {
          if (o instanceof Register) {
            String rn = ((Register) o).getBaseRegister().getName().toUpperCase();
            if ((rn.equals("EAX") || rn.equals("EDX") || rn.equals("ECX")) && !writtenRegs.contains(rn))
              readBeforeWrite.add(rn);
          }
        }
      }
      if (results != null) {
        for (Object o : results) {
          if (o instanceof Register) {
            String rn = ((Register) o).getBaseRegister().getName().toUpperCase();
            if (rn.equals("EAX") || rn.equals("EDX") || rn.equals("ECX")) writtenRegs.add(rn);
          }
        }
      }
      if (mnem.equals("CALL")) {
        // 32-bit CALL clobbers caller-saved EAX/EDX/ECX (and EAX is the return register).
        // Under Watcom watcall, EBX is also a reg arg / caller-saved and should be
        // added to this set when re-running for watcall-aware analysis.
        writtenRegs.add("EAX"); writtenRegs.add("EDX"); writtenRegs.add("ECX");
      }
    }

    // Stop entry-path tracking on unconditional JMP / RET
    if (mnem.equals("JMP") || mnem.equals("RET") || mnem.equals("RETN") || mnem.equals("RETF")) stopped = true;
    insn = insn.getNext();
    steps++;
  }
  prologueJson.append("]");

  // Multi-caller aggregation: pre-CALL register-set + post-CALL ADD ESP
  int callerCount = 0;
  int callersAddEspSeen = 0;
  long callersAddEspMax = 0;
  long callersAddEspMin = -1;
  int callersSetEax = 0;
  int callersSetEdx = 0;
  int callersSetEcx = 0;
  int callersPushOnly = 0;
  StringBuilder callersJson = new StringBuilder();
  callersJson.append("[");
  ReferenceIterator rit = rm.getReferencesTo(entry);
  while (rit.hasNext() && callerCount < MAX_CALLERS_SAMPLE) {
    Reference ref = rit.next();
    if (!ref.getReferenceType().isCall()) continue;
    Address callAddr = ref.getFromAddress();
    Instruction ci = listing.getInstructionAt(callAddr);
    if (ci == null) continue;

    boolean setEax = false, setEdx = false, setEcx = false;
    boolean allPushBefore = true;
    int pushCount = 0;
    Instruction p = ci.getPrevious();
    int back = 0;
    StringBuilder preInsns = new StringBuilder();
    while (p != null && back < CALLER_PRE_LOOKBACK) {
      String pm = p.getMnemonicString().toUpperCase();
      if (preInsns.length() > 0) preInsns.insert(0, " ; ");
      preInsns.insert(0, p.toString());
      if (p.getNumOperands() >= 1) {
        Object[] op0 = p.getOpObjects(0);
        if (op0 != null && op0.length == 1 && op0[0] instanceof Register) {
          String rn = ((Register) op0[0]).getBaseRegister().getName().toUpperCase();
          if (pm.equals("MOV") || pm.equals("LEA") || pm.equals("POP") || pm.equals("XOR")) {
            if (rn.equals("EAX")) setEax = true;
            if (rn.equals("EDX")) setEdx = true;
            if (rn.equals("ECX")) setEcx = true;
          }
        }
      }
      if (pm.equals("PUSH")) pushCount++;
      else if (!pm.equals("CALL")) allPushBefore = false;
      if (pm.equals("CALL") || pm.equals("JMP") || pm.equals("RET") || pm.equals("RETN") || pm.equals("RETF")) break;
      p = p.getPrevious();
      back++;
    }

    long addEspK = -1;
    String postStr = "";
    Instruction n1 = ci.getNext();
    if (n1 != null) {
      postStr = n1.toString();
      String n1m = n1.getMnemonicString().toUpperCase();
      if (n1m.equals("ADD") && n1.getNumOperands() == 2) {
        Object[] op0 = n1.getOpObjects(0);
        Object[] op1 = n1.getOpObjects(1);
        if (op0 != null && op0.length == 1 && op0[0] instanceof Register
            && ((Register) op0[0]).getName().equalsIgnoreCase("ESP")
            && op1 != null && op1.length == 1 && op1[0] instanceof Scalar) {
          addEspK = ((Scalar) op1[0]).getUnsignedValue();
        }
      }
    }

    if (callerCount > 0) callersJson.append(",");
    callersJson.append("{");
    callersJson.append("\"addr\":\"0x").append(callAddr.toString()).append("\"");
    callersJson.append(",\"pre\":\"").append(preInsns.toString().replace("\\","\\\\").replace("\"","\\\"")).append("\"");
    callersJson.append(",\"post\":\"").append(postStr.replace("\\","\\\\").replace("\"","\\\"")).append("\"");
    callersJson.append(",\"add_esp_k\":").append(addEspK);
    callersJson.append(",\"set_eax\":").append(setEax);
    callersJson.append(",\"set_edx\":").append(setEdx);
    callersJson.append(",\"set_ecx\":").append(setEcx);
    callersJson.append(",\"push_count\":").append(pushCount);
    callersJson.append("}");
    callerCount++;

    if (addEspK >= 0) {
      callersAddEspSeen++;
      if (addEspK > callersAddEspMax) callersAddEspMax = addEspK;
      if (callersAddEspMin < 0 || addEspK < callersAddEspMin) callersAddEspMin = addEspK;
    }
    if (setEax) callersSetEax++;
    if (setEdx) callersSetEdx++;
    if (setEcx) callersSetEcx++;
    if (allPushBefore && pushCount > 0) callersPushOnly++;
  }
  callersJson.append("]");

  String sig = "";
  try { sig = f.getPrototypeString(true, false); }
  catch (Exception e) { sig = "<error:" + e.getMessage() + ">"; }

  out.append("    {");
  out.append("\"addr\":\"0x").append(entry.toString()).append("\"");
  out.append(",\"name\":\"").append(name.replace("\\","\\\\").replace("\"","\\\"")).append("\"");
  out.append(",\"current_cc\":\"").append(cc).append("\"");
  out.append(",\"is_thunk\":").append(isThunk);
  out.append(",\"thunked_addr\":\"").append(thunkedAddr).append("\"");
  out.append(",\"body_size\":").append(bodySize);
  out.append(",\"param_count_current\":").append(f.getParameterCount());
  out.append(",\"last_insn_kind\":\"").append(lastKind).append("\"");
  out.append(",\"last_insn_mnem\":\"").append(lastMnem).append("\"");
  out.append(",\"last_insn_n\":").append(lastN);
  out.append(",\"tail_jmp_target\":\"").append(tailJmpTarget).append("\"");
  out.append(",\"prologue\":").append(prologueJson.toString());
  out.append(",\"reads_eax\":").append(readBeforeWrite.contains("EAX"));
  out.append(",\"reads_edx\":").append(readBeforeWrite.contains("EDX"));
  out.append(",\"reads_ecx\":").append(readBeforeWrite.contains("ECX"));
  out.append(",\"caller_count\":").append(callerCount);
  out.append(",\"callers_add_esp_seen\":").append(callersAddEspSeen);
  out.append(",\"callers_add_esp_max\":").append(callersAddEspMax);
  out.append(",\"callers_add_esp_min\":").append(callersAddEspMin);
  out.append(",\"callers_set_eax\":").append(callersSetEax);
  out.append(",\"callers_set_edx\":").append(callersSetEdx);
  out.append(",\"callers_set_ecx\":").append(callersSetEcx);
  out.append(",\"callers_push_only\":").append(callersPushOnly);
  out.append(",\"callers\":").append(callersJson.toString());
  out.append(",\"existing_signature\":\"").append(sig.replace("\\","\\\\").replace("\"","\\\"")).append("\"");
  out.append("}");
}

out.append("\n  ]\n}\n");
Files.writeString(Paths.get(OUT_PATH), out.toString());
println("Wrote " + OUT_PATH + ": bytes=" + out.length() + " functions=" + totalFuncs);
