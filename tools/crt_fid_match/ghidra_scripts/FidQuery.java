// Queries one or more .fidb files against FD2.LE and writes match results
// to JSON. Uses reflection because FidQueryService is not in the exported API.
//
// Args:
//   fidbDir   - directory containing watcom_<ver>.fidb files
//   outDir    - directory to write per-version JSON results
//   threshold - score threshold (e.g. "0" for all candidates, "14.6" default)

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.feature.fid.db.FidFile;
import ghidra.feature.fid.db.FidFileManager;
import ghidra.feature.fid.db.LibraryRecord;
import ghidra.feature.fid.service.FidService;
import ghidra.feature.fid.service.FidSearchResult;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class FidQuery extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] argv = getScriptArgs();
        if (argv.length < 3) {
            println("ERR: need 3 args: fidbDir outDir threshold");
            return;
        }
        File fidbDir = new File(argv[0]);
        File outDir = new File(argv[1]);
        float threshold = Float.parseFloat(argv[2]);
        outDir.mkdirs();

        Method processProgram = null;
        for (Method m : FidService.class.getMethods()) {
            if (m.getName().equals("processProgram") && m.getParameterCount() == 4) {
                processProgram = m;
                break;
            }
        }
        if (processProgram == null) throw new RuntimeException("processProgram not found");

        // Reflective access to FidSearchResult fields.
        Field fnField = FidSearchResult.class.getDeclaredField("function");
        Field hashField = FidSearchResult.class.getDeclaredField("hashQuad");
        Field matchesField = FidSearchResult.class.getDeclaredField("matches");
        fnField.setAccessible(true);
        hashField.setAccessible(true);
        matchesField.setAccessible(true);

        File[] fidbs = fidbDir.listFiles((d, name) -> name.endsWith(".fidb"));
        if (fidbs == null || fidbs.length == 0) {
            println("ERR: no .fidb under " + fidbDir);
            return;
        }
        java.util.Arrays.sort(fidbs);
        FidService service = new FidService();
        FidFileManager mgr = FidFileManager.getInstance();

        // Capture the original active state of every installed fidb so we can
        // restore it at the end. Then deactivate everything; we only want our
        // watcom fidbs contributing to matches.
        List originalAll = mgr.getFidFiles();
        java.util.Map<FidFile, Boolean> origActive = new java.util.LinkedHashMap<>();
        for (Object o : originalAll) {
            FidFile f = (FidFile) o;
            origActive.put(f, f.isActive());
            f.setActive(false);
        }
        // Detach any pre-attached user fidbs from prior runs.
        List existing = mgr.getUserAddedFiles();
        for (Object o : existing) { try { mgr.removeUserFile((FidFile) o); } catch (Exception e) {} }

        try {
            for (File fidbFile : fidbs) {
                String version = fidbFile.getName().replaceAll("^watcom_", "").replaceAll("\\.fidb$", "");
                println("== " + fidbFile.getName() + " ==");

                FidFile fidFile = mgr.addUserFidFile(fidbFile);
                fidFile.setActive(true);
                Object qs = service.openFidQueryService(currentProgram.getLanguage(), false);

                long t0 = System.currentTimeMillis();
                List results = (List) processProgram.invoke(service, currentProgram, qs, threshold, monitor);
                long dt = System.currentTimeMillis() - t0;
                println("  matches=" + results.size() + "  elapsed=" + dt + "ms");

                File outFile = new File(outDir, "matches_" + version + ".json");
                writeJson(outFile, version, threshold, results, fnField, matchesField);
                println("  wrote: " + outFile);

                mgr.removeUserFile(fidFile);
            }
        } finally {
            // Restore active flags for installed fidbs so subsequent Ghidra
            // sessions are unaffected.
            for (java.util.Map.Entry<FidFile, Boolean> e : origActive.entrySet()) {
                try { e.getKey().setActive(e.getValue()); } catch (Exception ex) {}
            }
        }
        println("DONE");
    }

    private void writeJson(File out, String version, float threshold, List results,
                           Field fnField, Field matchesField) throws Exception {
        FileWriter w = new FileWriter(out);
        try {
            w.write("{\n");
            w.write("  \"version\": " + jsonString(version) + ",\n");
            w.write("  \"threshold\": " + threshold + ",\n");
            w.write("  \"match_count\": " + results.size() + ",\n");
            w.write("  \"matches\": [\n");

            for (int i = 0; i < results.size(); i++) {
                Object r = results.get(i);
                Function f = (Function) fnField.get(r);
                List matches = (List) matchesField.get(r);

                w.write("    {\n");
                w.write("      \"address\": " + jsonString(f.getEntryPoint().toString()) + ",\n");
                w.write("      \"current_name\": " + jsonString(f.getName()) + ",\n");
                w.write("      \"body_size\": " + f.getBody().getNumAddresses() + ",\n");
                w.write("      \"candidates\": [\n");
                for (int j = 0; j < matches.size(); j++) {
                    Object m = matches.get(j);
                    String funcName = "";
                    String domainPath = "";
                    Object fr = invokeMethod(m, "getFunctionRecord");
                    if (fr != null) {
                        Object n = invokeMethod(fr, "getName");
                        if (n != null) funcName = n.toString();
                        Object dp = invokeMethod(fr, "getDomainPath");
                        if (dp != null) domainPath = dp.toString();
                    }
                    String libVar = "";
                    String libVer = "";
                    String libFam = "";
                    Object lr = invokeMethod(m, "getLibraryRecord");
                    if (lr instanceof LibraryRecord) {
                        LibraryRecord libRec = (LibraryRecord) lr;
                        libVar = libRec.getLibraryVariant() == null ? "" : libRec.getLibraryVariant();
                        libVer = libRec.getLibraryVersion() == null ? "" : libRec.getLibraryVersion();
                        libFam = libRec.getLibraryFamilyName() == null ? "" : libRec.getLibraryFamilyName();
                    }
                    String score = invokeOrNull(m, "getOverallScore");
                    String forceSpec = invokeOrNull(m, "isForceSpecific");

                    w.write("        {");
                    w.write("\"matched_name\":" + jsonString(funcName));
                    w.write(",\"score\":" + (score != null ? score : "null"));
                    w.write(",\"source_obj\":" + jsonString(domainPath));
                    w.write(",\"library_family\":" + jsonString(libFam));
                    w.write(",\"library_version\":" + jsonString(libVer));
                    w.write(",\"library_variant\":" + jsonString(libVar));
                    if (forceSpec != null) w.write(",\"force_specific\":" + forceSpec);
                    w.write("}" + (j + 1 < matches.size() ? "," : "") + "\n");
                }
                w.write("      ]\n");
                w.write("    }" + (i + 1 < results.size() ? "," : "") + "\n");
            }
            w.write("  ]\n");
            w.write("}\n");
        } finally {
            w.close();
        }
    }

    private Object invokeMethod(Object o, String name) {
        // The impl class FidMatchImpl is in a non-exported OSGi package, so
        // calls into its methods fail with IllegalAccessException even though
        // the methods are declared public. Walk the interfaces to find a
        // publicly-accessible declaration of the method.
        Class<?> cls = o.getClass();
        // Try interfaces first.
        for (Class<?> iface : collectAllInterfaces(cls)) {
            try {
                Method m = iface.getMethod(name);
                m.setAccessible(true);
                return m.invoke(o);
            } catch (NoSuchMethodException nsme) {
                // try next iface
            } catch (Throwable t) {
                return null;
            }
        }
        // Fall back to the class, with setAccessible true.
        try {
            Method m = cls.getMethod(name);
            m.setAccessible(true);
            return m.invoke(o);
        } catch (Throwable t) {
            return null;
        }
    }

    private List<Class<?>> collectAllInterfaces(Class<?> c) {
        List<Class<?>> out = new ArrayList<>();
        while (c != null && c != Object.class) {
            for (Class<?> i : c.getInterfaces()) {
                if (!out.contains(i)) {
                    out.add(i);
                    for (Class<?> sup : i.getInterfaces()) {
                        if (!out.contains(sup)) out.add(sup);
                    }
                }
            }
            c = c.getSuperclass();
        }
        return out;
    }

    private String invokeOrNull(Object o, String name) {
        Object v = invokeMethod(o, name);
        return v == null ? null : v.toString();
    }

    private String jsonString(String s) {
        if (s == null) return "null";
        StringBuilder sb = new StringBuilder("\"");
        for (char c : s.toCharArray()) {
            if (c == '"') sb.append("\\\"");
            else if (c == '\\') sb.append("\\\\");
            else if (c == '\n') sb.append("\\n");
            else if (c == '\r') sb.append("\\r");
            else if (c == '\t') sb.append("\\t");
            else if (c < 0x20) sb.append(String.format("\\u%04x", (int) c));
            else sb.append(c);
        }
        sb.append("\"");
        return sb.toString();
    }
}
