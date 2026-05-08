// Force-deletes every file under a project folder, releasing any cached
// consumers first. Used to clean /watcom_libs between FidDb pipeline runs.

import ghidra.app.script.GhidraScript;
import ghidra.framework.model.DomainFile;
import ghidra.framework.model.DomainFolder;
import ghidra.framework.model.DomainObject;
import ghidra.framework.model.Project;
import java.util.List;

public class FidWipeFolder extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] argv = getScriptArgs();
        if (argv.length < 1) {
            println("ERR: need 1 arg: folderPath");
            return;
        }
        String folderPath = argv[0];

        Project project = state.getProject();
        DomainFolder root = project.getProjectData().getRootFolder();
        DomainFolder f = navigate(root, folderPath);
        if (f == null) {
            println("Folder not found: " + folderPath);
            return;
        }
        DomainFile[] files = f.getFiles();
        println("Wiping " + files.length + " files under " + folderPath);

        int deleted = 0, retained = 0;
        for (DomainFile df : files) {
            // Force-release any cached consumers.
            List consumers = df.getConsumers();
            for (Object c : consumers) {
                try {
                    DomainObject d = df.getOpenedDomainObject(c);
                    if (d != null) d.release(c);
                } catch (Throwable t) {}
            }
            try {
                df.delete();
                deleted++;
            } catch (Exception e) {
                retained++;
                if (retained <= 5) println("  retain " + df.getName() + ": " + e.getMessage());
            }
        }
        println("DONE  deleted=" + deleted + "  retained=" + retained);
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
