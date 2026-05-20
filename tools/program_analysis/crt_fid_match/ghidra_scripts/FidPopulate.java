// Builds 4 .fidb files (one per Watcom version) from previously-imported
// /watcom_libs/* programs. Uses the manifest to figure out which DomainFile
// belongs in which version.
//
// Args:
//   manifestPath  - JSON manifest
//   libsFolder    - Ghidra project folder containing imported .obj programs
//   outDir        - filesystem dir to write watcom_<ver>.fidb into
//   languageId    - LanguageID for the FidDb library record (must match FD2.LE)

import java.io.File;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import ghidra.app.script.GhidraScript;
import ghidra.feature.fid.db.FidDB;
import ghidra.feature.fid.db.FidFile;
import ghidra.feature.fid.db.FidFileManager;
import ghidra.feature.fid.db.LibraryRecord;
import ghidra.feature.fid.service.FidPopulateResult;
import ghidra.feature.fid.service.FidService;
import ghidra.framework.model.DomainFile;
import ghidra.framework.model.DomainFolder;
import ghidra.framework.model.Project;
import ghidra.program.model.lang.LanguageID;

public class FidPopulate extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] argv = getScriptArgs();
        if (argv.length < 4) {
            println("ERR: need 4 args: manifestPath libsFolder outDir languageId");
            return;
        }
        String manifestPath = argv[0];
        String libsFolder = argv[1];
        String outDir = argv[2];
        LanguageID langId = new LanguageID(argv[3]);

        Project project = state.getProject();
        DomainFolder root = project.getProjectData().getRootFolder();
        DomainFolder libs = navigate(root, libsFolder);
        if (libs == null) {
            println("ERR: folder not found: " + libsFolder);
            return;
        }

        // Build name->DomainFile index for fast lookup.
        Map<String, DomainFile> byName = new HashMap<>();
        for (DomainFile df : libs.getFiles()) {
            byName.put(df.getName(), df);
        }
        println("Library folder " + libs.getPathname() + " contains " + byName.size() + " files");

        // Parse manifest, group module keys by version.
        String manifestText = new String(Files.readAllBytes(new File(manifestPath).toPath()));
        JsonObject manifest = JsonParser.parseString(manifestText).getAsJsonObject();
        JsonArray modules = manifest.getAsJsonArray("modules");

        Map<String, List<DomainFile>> byVersion = new HashMap<>();
        Map<String, Set<String>> missingByVersion = new HashMap<>();
        for (String v : new String[]{"9.5", "9.5a", "9.5b", "9.5c"}) {
            byVersion.put(v, new ArrayList<>());
            missingByVersion.put(v, new TreeSet<>());
        }

        for (int i = 0; i < modules.size(); i++) {
            JsonObject m = modules.get(i).getAsJsonObject();
            String key = m.get("key").getAsString();
            String fileName = key + ".obj";
            DomainFile df = byName.get(fileName);
            JsonArray apps = m.getAsJsonArray("appearances");
            Set<String> versions = new HashSet<>();
            for (int j = 0; j < apps.size(); j++) {
                versions.add(apps.get(j).getAsJsonObject().get("version").getAsString());
            }
            for (String v : versions) {
                if (df != null) {
                    byVersion.get(v).add(df);
                } else {
                    missingByVersion.get(v).add(key);
                }
            }
        }

        File outDirFile = new File(outDir);
        outDirFile.mkdirs();

        FidFileManager mgr = FidFileManager.getInstance();
        FidService service = new FidService();

        for (String v : new String[]{"9.5", "9.5a", "9.5b", "9.5c"}) {
            List<DomainFile> progs = byVersion.get(v);
            Set<String> missing = missingByVersion.get(v);
            File fidbFile = new File(outDirFile, "watcom_" + v + ".fidb");
            if (fidbFile.exists()) fidbFile.delete();

            println("== Version " + v + " ==");
            println("  programs: " + progs.size() + "  missing(import-failed): " + missing.size());
            mgr.createNewFidDatabase(fidbFile);
            FidFile fidFile = mgr.addUserFidFile(fidbFile);
            FidDB fidDb = fidFile.getFidDB(true);
            try {
                FidPopulateResult result = service.createNewLibraryFromPrograms(
                        fidDb,
                        "watcom_" + v,
                        v,
                        "DOS-32",
                        progs,
                        null,
                        langId,
                        new ArrayList<LibraryRecord>(),
                        new ArrayList<String>(),
                        monitor);
                println("  populate result:");
                println("    attempted: " + result.getTotalAttempted());
                println("    excluded:  " + result.getTotalExcluded());
                println("    added:     " + result.getTotalAdded());
                println("    unresolved symbols: " + result.getUnresolvedSymbols().size());
            } finally {
                fidDb.saveDatabase("populate", monitor);
                fidDb.close();
            }
            mgr.removeUserFile(fidFile); // detach so subsequent queries don't auto-include
            println("  wrote: " + fidbFile.getAbsolutePath() + " (" + fidbFile.length() + " bytes)");
        }
        println("DONE");
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
