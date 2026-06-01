// Ghidra script: dump all fd2_* and crt_equivalent_* function names + addresses to JSON.
// Run from Ghidra Script Manager or via MCP run_ghidra_script.
// Output: <repo>/workspace/emit/emit_functions.json
//
// @category FD2
// @author fd2-anatomy

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.File;
import java.io.FileWriter;

public class dump_emit_functions extends GhidraScript {

    private static final String OUT_PATH =
        "C:\\Users\\fdpsf\\Documents\\fd2-anatomy\\workspace\\emit\\emit_functions.json";

    @Override
    protected void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        FunctionIterator iter = fm.getFunctions(true);

        StringBuilder sb = new StringBuilder();
        sb.append("[\n");
        boolean first = true;
        int count = 0;

        while (iter.hasNext()) {
            Function f = iter.next();
            String name = f.getName();
            if (name.startsWith("fd2_") || name.startsWith("crt_equivalent_")) {
                if (!first) {
                    sb.append(",\n");
                }
                first = false;
                sb.append("  {\"name\": \"");
                sb.append(name);
                sb.append("\", \"address\": \"");
                sb.append(f.getEntryPoint().toString());
                sb.append("\"}");
                count++;
            }
        }

        sb.append("\n]\n");

        File outFile = new File(OUT_PATH);
        FileWriter fw = new FileWriter(outFile);
        fw.write(sb.toString());
        fw.close();

        println("Wrote " + count + " functions to " + outFile.getAbsolutePath());
    }
}
