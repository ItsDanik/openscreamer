//Write every function of the program as a line "entry size" (size = the bytes of its body) to a file.
//usage: -postScript ExportFuncs.java <out.txt>
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class ExportFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0])) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true))
                w.printf("0x%x %d%n", f.getEntryPoint().getOffset(), f.getBody().getNumAddresses());
        }
    }
}
