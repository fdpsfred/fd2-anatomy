# Stage-2 live Ghidra dump snippets (for `run_script_inline`)

`stage2_reconcile.py` is driven by two live-Ghidra dumps that must be
regenerated from the running Ghidra immediately before each reconcile (live is
the source of truth for "applied"). Paste each as the `code` body of
`mcp__ghidra__run_script_inline` (Java). Output goes under `workspace/src_refine/`.

## 1. Function parameter names -> `ghidra_func_params.tsv`

Format per line: `addr<TAB>name<TAB>param0<TAB>param1<TAB>...`

```java
import ghidra.program.model.listing.*;
import java.io.*;
FunctionManager fm = currentProgram.getFunctionManager();
PrintWriter w = new PrintWriter(new FileWriter("<REPO>/workspace/src_refine/ghidra_func_params.tsv"));
int n = 0;
for (Function f : fm.getFunctions(true)) {
    StringBuilder sb = new StringBuilder();
    sb.append(f.getEntryPoint().toString()).append("\t").append(f.getName());
    for (Parameter p : f.getParameters()) sb.append("\t").append(p.getName());
    w.println(sb.toString());
    n++;
}
w.close();
println("wrote " + n + " functions");
```

## 2. Global/data primary symbol names -> `ghidra_global_names.tsv`

The 34 Stage-2 global addresses (worklist items n=152..185) are not functions,
so dump their primary symbol names separately. Format: `addr<TAB>name`.
Address list is the `need_global` set from a prior reconcile run; keep it in
sync if the worklist's global set changes.

```java
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
String[] addrs = {
 "00050004","00050023","00050037","00050064","00050086","0005022b","00050233",
 "00051a0c","00051a59","00051a65","00051a70","00051ef5","00051f75","00052326",
 "00052381","00052388","00052635","00052647","00053a18","00053a44","00053a4d",
 "00053a59","00053a5d","00053a79","00053a85","00053a89","00053a8d","00053ad9",
 "00053af5","00053b0f","00053be3","00053be7","0006006a","00060181"
};
SymbolTable st = currentProgram.getSymbolTable();
AddressFactory af = currentProgram.getAddressFactory();
StringBuilder sb = new StringBuilder();
for (String h : addrs) {
    Symbol s = st.getPrimarySymbol(af.getAddress(h));
    sb.append(h).append("\t").append(s == null ? "<none>" : s.getName()).append("\n");
}
println(sb.toString());
```

(Copy the printed lines into `workspace/src_refine/ghidra_global_names.tsv`, or
adapt the snippet to FileWriter as in #1.)

## Batch Ghidra param sync (PHASE B) -> two-pass rename

After the per-item src edits land, regenerate a full ordered src-param TSV
(`clean_param_sync.tsv`: `addr<TAB>name0<TAB>name1<TAB>...` = the FINAL src
param names per function, from `stage2_src_check.py`) and apply with a single
`run_script_inline` that, per function, sets all params to temp names then to
the final names (two-pass avoids shift/collision failures). The two-pass form
is the same one used for the user-approved 78-item clean sync; see git history
of `stage2_progress.json` `_batch: clean_param_sync_78`.
