// Batch-imports OMF .obj files from a manifest into a Ghidra project folder,
// sets each program's language to x86:LE:32:watcom (matching FD2.LE), and
// saves. Step 5 of the CRT FidDb pipeline; see rebuild_info/crt/fid_match.md.
//
// Args (whitespace-separated, in order):
//   manifestPath  - JSON manifest produced by build_manifest.py
//                   (e.g. workspace/crt_fid_match/manifest.json)
//   destFolder    - Ghidra project folder (e.g. /watcom_libs)
//   startIndex    - 0-based start index in manifest.modules[]
//   endIndex      - exclusive end index
//
// Skips modules whose target DomainFile already exists, so it is safe to
// re-run for partial / resumed batches.

import java.io.File;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.importer.AutoImporter;
import ghidra.app.util.importer.MessageLog;
import ghidra.app.util.opinion.Loaded;
import ghidra.app.util.opinion.LoadResults;
import ghidra.framework.model.DomainFile;
import ghidra.framework.model.DomainFolder;
import ghidra.framework.model.DomainObject;
import ghidra.framework.model.Project;
import ghidra.program.model.lang.CompilerSpecID;
import ghidra.program.model.lang.Language;
import ghidra.program.model.lang.LanguageID;
import ghidra.program.model.lang.LanguageService;
import ghidra.program.model.listing.Program;
import ghidra.program.util.DefaultLanguageService;

public class FidImportBatch extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] argv = getScriptArgs();
        if (argv.length < 4) {
            println("ERR: need 4 args: manifestPath destFolder startIndex endIndex");
            return;
        }
        String manifestPath = argv[0];
        String destFolder = argv[1];
        int startIdx = Integer.parseInt(argv[2]);
        int endIdx = Integer.parseInt(argv[3]);

        Project project = state.getProject();
        DomainFolder root = project.getProjectData().getRootFolder();
        DomainFolder dest = ensureFolder(root, destFolder);

        LanguageService svc = DefaultLanguageService.getLanguageService();
        Language watcom = svc.getLanguage(new LanguageID("x86:LE:32:watcom"));
        CompilerSpecID watcomCs = new CompilerSpecID("watcomcpp");

        String manifestText = new String(Files.readAllBytes(new File(manifestPath).toPath()));
        JsonObject manifest = JsonParser.parseString(manifestText).getAsJsonObject();
        JsonArray modules = manifest.getAsJsonArray("modules");
        int total = modules.size();
        if (endIdx > total) endIdx = total;

        // Index existing files in destFolder for skip-if-exists.
        Set<String> existing = new HashSet<>();
        for (DomainFile df : dest.getFiles()) {
            existing.add(df.getName());
        }

        long t0 = System.currentTimeMillis();
        int imported = 0, skipped = 0, failed = 0;
        MessageLog log = new MessageLog();

        for (int i = startIdx; i < endIdx; i++) {
            JsonObject m = modules.get(i).getAsJsonObject();
            String key = m.get("key").getAsString();
            String src = m.get("src").getAsString();

            if (existing.contains(key)) {
                skipped++;
                continue;
            }

            File srcFile = new File(src);
            if (!srcFile.isFile()) {
                println("MISSING: " + src);
                failed++;
                continue;
            }

            try {
                LoadResults<Program> results = AutoImporter.importByUsingBestGuess(
                        srcFile, project, dest.getPathname(), this, log, monitor);
                if (results == null || results.size() == 0) {
                    failed++;
                    if (results != null) results.release(this);
                    continue;
                }
                Iterator<Loaded<Program>> it = results.iterator();
                while (it.hasNext()) {
                    Loaded<Program> ld = it.next();
                    Program p = ld.getDomainObject(this);
                    try {
                        if (!p.getLanguageID().getIdAsString().equals("x86:LE:32:watcom")) {
                            int tx = p.startTransaction("setLang");
                            try {
                                p.setLanguage(watcom, watcomCs, false, monitor);
                                p.endTransaction(tx, true);
                            } catch (Throwable t) {
                                p.endTransaction(tx, false);
                                throw t;
                            }
                        }
                    } finally {
                        p.release(this);
                    }
                }
                results.save(monitor);
                // Rename the saved file to the manifest key.
                it = results.iterator();
                while (it.hasNext()) {
                    Loaded<Program> ld = it.next();
                    DomainFile df = ld.getSavedDomainFile();
                    if (df != null && !df.getName().equals(key)) {
                        try { df.setName(key); } catch (Throwable ignore) {}
                    }
                }
                results.release(this);
                imported++;
            } catch (Throwable t) {
                println("FAIL[" + i + "] key=" + key + " err=" + t.getClass().getSimpleName() + ": " + t.getMessage());
                failed++;
            }

            if ((i - startIdx + 1) % 50 == 0) {
                long dt = (System.currentTimeMillis() - t0) / 1000;
                println("  progress " + (i - startIdx + 1) + "/" + (endIdx - startIdx)
                        + "  imported=" + imported + " skipped=" + skipped + " failed=" + failed
                        + "  elapsed=" + dt + "s");
            }
        }

        long dtSec = (System.currentTimeMillis() - t0) / 1000;
        println("DONE  range=[" + startIdx + "," + endIdx + ")  imported=" + imported
                + " skipped=" + skipped + " failed=" + failed + " elapsed=" + dtSec + "s");
    }

    private DomainFolder ensureFolder(DomainFolder root, String path) throws Exception {
        if (path.equals("/") || path.isEmpty()) return root;
        String[] parts = path.split("/");
        DomainFolder cur = root;
        for (String p : parts) {
            if (p.isEmpty()) continue;
            DomainFolder child = cur.getFolder(p);
            if (child == null) child = cur.createFolder(p);
            cur = child;
        }
        return cur;
    }
}
