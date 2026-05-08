// Runs auto-analysis on all programs in /watcom_libs so functions get
// disassembled. After import via AutoImporter, OMF .obj programs only have
// PUBDEF symbols at entry points - no instructions, so FidHasher excludes
// every function. This pass fixes that.

import ghidra.app.script.GhidraScript;
import ghidra.app.plugin.core.analysis.AutoAnalysisManager;
import ghidra.framework.model.DomainFile;
import ghidra.framework.model.DomainFolder;
import ghidra.framework.model.Project;
import ghidra.program.model.listing.Program;
import ghidra.program.model.listing.FunctionManager;

public class FidAnalyzeAll extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] argv = getScriptArgs();
        if (argv.length < 1) {
            println("ERR: need 1 arg: libsFolder");
            return;
        }
        String libsFolder = argv[0];
        int startIdx = argv.length >= 2 ? Integer.parseInt(argv[1]) : 0;
        int endIdx = argv.length >= 3 ? Integer.parseInt(argv[2]) : Integer.MAX_VALUE;

        Project project = state.getProject();
        DomainFolder libs = navigate(project.getProjectData().getRootFolder(), libsFolder);
        if (libs == null) {
            println("ERR: folder not found: " + libsFolder);
            return;
        }
        DomainFile[] files = libs.getFiles();
        if (endIdx > files.length) endIdx = files.length;
        println("Total files: " + files.length + "  range=[" + startIdx + "," + endIdx + ")");

        long t0 = System.currentTimeMillis();
        int analyzed = 0, skipped = 0, failed = 0;
        for (int i = startIdx; i < endIdx; i++) {
            DomainFile df = files[i];
            Program p;
            try {
                p = (Program) df.getDomainObject(this, false, false, monitor);
            } catch (Throwable t) {
                println("FAIL[" + i + "] open " + df.getName() + ": " + t.getMessage());
                failed++;
                continue;
            }
            try {
                FunctionManager fm = p.getFunctionManager();
                int fc = fm.getFunctionCount();
                long sumBody = 0;
                for (var f : fm.getFunctions(true)) {
                    sumBody += f.getBody().getNumAddresses();
                }
                // Skip if functions already have meaningful bodies (sum > fcount means
                // disassembly already happened).
                if (fc > 0 && sumBody > fc) {
                    skipped++;
                    continue;
                }
                AutoAnalysisManager mgr = AutoAnalysisManager.getAnalysisManager(p);
                int tx = p.startTransaction("auto-analyze");
                boolean ok = false;
                try {
                    mgr.initializeOptions();
                    mgr.reAnalyzeAll(null);
                    mgr.startAnalysis(monitor);
                    ok = true;
                } finally {
                    p.endTransaction(tx, ok);
                }
                if (p.isChanged()) {
                    df.save(monitor);
                }
                analyzed++;
            } catch (Throwable t) {
                println("FAIL[" + i + "] analyze " + df.getName() + ": " + t.getMessage());
                failed++;
            } finally {
                p.release(this);
            }
            if ((i - startIdx + 1) % 50 == 0) {
                long dt = (System.currentTimeMillis() - t0) / 1000;
                println("  progress " + (i - startIdx + 1) + "/" + (endIdx - startIdx)
                        + "  analyzed=" + analyzed + " skipped=" + skipped + " failed=" + failed
                        + "  elapsed=" + dt + "s");
            }
        }
        long dtSec = (System.currentTimeMillis() - t0) / 1000;
        println("DONE  analyzed=" + analyzed + " skipped=" + skipped + " failed=" + failed
                + " elapsed=" + dtSec + "s");
    }

    private DomainFolder navigate(DomainFolder root, String path) {
        if (path.equals("/") || path.isEmpty()) return root;
        String[] parts = path.split("/");
        DomainFolder cur = root;
        for (String p : parts) {
            if (p.isEmpty()) continue;
            cur = cur.getFolder(p);
            if (cur == null) return null;
        }
        return cur;
    }
}
